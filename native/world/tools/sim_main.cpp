// 同库CLI仅批量查询已绑定离线库存；不模拟、覆盖输入或发布部分成功。
#include "hanmo/inventory_projection.hpp"
#include "hanmo/legacy_projection.hpp"
#include "actor_commitments_result.hpp"
#include "typed_wire.hpp"
#include "build_identity.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>

namespace {
namespace fs=std::filesystem;
using Json=nlohmann::json;
struct Failure { std::string code,path; };
// 读取时即限额，拒绝目录及超过wire限额的文件。
std::string read(const fs::path& path) {
    std::error_code ec;
    if(!fs::is_regular_file(path,ec) || ec) throw Failure{"InputReadFailed",path.string()};
    std::ifstream file(path,std::ios::binary);
    if(!file) throw Failure{"InputReadFailed",path.string()};
    std::string result; char buffer[16384];
    while(file) {
        file.read(buffer,sizeof(buffer)); const auto size=file.gcount();
        if(size>0) result.append(buffer,static_cast<std::size_t>(size));
        if(result.size()>hanmo::wire_limit) throw Failure{"WireSizeLimit",path.string()};
    }
    if(!file.eof()) throw Failure{"InputReadFailed",path.string()};
    return result;
}
// 先独占创建临时目录并完整落盘，再用不覆盖目标的硬链接发布本地输出。
struct StagedFile {
    fs::path target,directory,file;
    bool linked=false;
    StagedFile(const fs::path& destination,const std::string& bytes):target(destination),directory(destination.string()+".pending"),file(directory/"payload") {
        std::error_code ec;
        if(!fs::create_directory(directory,ec) || ec) throw Failure{"OutputStageFailed",directory.string()};
        try {
            std::ofstream output(file,std::ios::binary|std::ios::trunc);
            output.write(bytes.data(),static_cast<std::streamsize>(bytes.size())); output.close();
            if(!output) throw Failure{"OutputWriteFailed",file.string()};
        } catch(...) { fs::remove(file,ec); fs::remove(directory,ec); throw; }
    }
    // 硬链接不能替换既有文件；失败时保留原目标。
    void publish() { std::error_code ec; fs::create_hard_link(file,target,ec); if(ec) throw Failure{"OutputPublishFailed",target.string()}; linked=true; }
    // 仅撤销本对象刚发布的同一inode，不删除被其他进程替换的目标。
    void rollback() { std::error_code ec; if(linked && fs::equivalent(file,target,ec) && !ec) fs::remove(target,ec); linked=false; }
    // 只清理自己独占创建的临时文件与目录，不遍历外部目录。
    ~StagedFile() { std::error_code ec; fs::remove(file,ec); fs::remove(directory,ec); }
};
// 审计从实际runtime读取安装与批次顺序，未安装不填成功值。
void runtime_audit(Json& audit,const hanmo::ProjectionAudit& installed,bool legacy) {
    audit["installed"]=installed.installed; audit["install_count"]=installed.install_count;
    audit["inventory_lot_ids"]=installed.lot_ids;
    audit["inventory_component_sha256"]=installed.component_sha256;
    if(installed.installed) audit["payload_sha256"]=installed.payload_sha256;
    if(legacy) { audit["profile"]=installed.profile; audit["record_component_sha256"]=installed.record_component_sha256; }
}
// 编译身份由CMake根据精确源码和本地已核依赖锁生成，不接受输入自报覆盖。
Json build_identity() {
    return {{"source_commit",hanmo::build::source_commit},{"source_sha256",hanmo::build::source_sha256},
        {"dependency_lock_sha256",hanmo::build::dependency_lock_sha256},{"compiler",hanmo::build::compiler},
        {"compiler_version",hanmo::build::compiler_version},{"target",hanmo::build::target},{"configuration",hanmo::build::configuration},{"cpp_standard","17"}};
}
} // CLI文件写入不进入领域核心。

// 整批核验／查询成功后才发布结果；失败审计不包含部分counts。
int main(int argc,char** argv) {
    std::map<std::string,fs::path> paths;
    Json audit={{"format","HM-NATIVE-INVENTORY-AUDIT/1"},{"binding_kind","artifact"},{"ok",false},{"build",build_identity()},
        {"install_count",0},{"completed_queries",0},{"exit_code",1}};
    hanmo::InventoryProjection runtime;
    hanmo::ProjectionStore legacy_runtime;
    const bool legacy=argc>1 && std::string(argv[1])=="actor-commitments";
    if(legacy) audit["format"]="HM-NATIVE-ACTOR-COMMITMENTS-AUDIT/1";
    const auto installed_audit=[&]() { return legacy?legacy_runtime.audit():runtime.audit(); };
    bool destinations_valid=false;
    try {
        if(argc!=10 || (!legacy && std::string(argv[1])!="readonly-count")) throw Failure{"InvalidArguments","/arguments"};
        for(int i=2;i<argc;i+=2) {
            const std::string name=argv[i];
            if((name!="--input" && name!="--queries" && name!="--output" && name!="--audit") || paths.count(name)) throw Failure{"InvalidArguments","/arguments"};
            std::error_code ec; auto path=fs::weakly_canonical(fs::absolute(argv[i+1]),ec);
            if(ec) throw Failure{"InvalidPath",argv[i+1]};
            paths.emplace(name,path);
        }
        std::set<fs::path> unique;
        for(const auto& [key,path]:paths) if(!unique.insert(path).second) throw Failure{"AliasedPaths",path.string()};
        for(const auto* key:{"--output","--audit"}) {
            const auto& path=paths.at(key); std::error_code ec;
            if(fs::exists(path,ec) || ec || !fs::is_directory(path.parent_path(),ec) || ec) throw Failure{"OutputExistsOrUnavailable",path.string()};
        }
        destinations_valid=true;
        const auto input=read(paths.at("--input")); const auto queries=read(paths.at("--queries"));
        if(input.size()>hanmo::wire_limit-queries.size()) throw Failure{"WireSizeLimit","/inputs"};
        audit["input_sha256"]=hanmo::sha256(input); audit["queries_sha256"]=hanmo::sha256(queries);
        const auto installed=legacy?legacy_runtime.install_once(input):runtime.install_once(input);
        if(!installed.ok) throw Failure{installed.error.code,installed.error.path};
        runtime_audit(audit,installed_audit(),legacy);
        Json result;
        std::size_t completed_queries=0;
        if(legacy) {
            const auto rows=legacy_runtime.actor_commitments(queries);
            if(!rows.ok) throw Failure{rows.error.code,rows.error.path};
            result={{"format","HM-NATIVE-ACTOR-COMMITMENTS-RESULT/1"},{"input_sha256",audit["input_sha256"]},
                {"queries_sha256",audit["queries_sha256"]},{"results",hanmo::detail::encode_value(hanmo::detail::actor_commitments_value(rows.value))}};
            completed_queries=rows.value.size();
        } else {
            const auto counts=runtime.query_batch(queries);
            if(!counts.ok) throw Failure{counts.error.code,counts.error.path};
            Json rows=Json::array();
            for(const auto& row:counts.value) rows.push_back({{"query_id",row.query_id},{"count",std::to_string(row.count)}});
            result={{"format","HM-NATIVE-INVENTORY-RESULT/1"},{"input_sha256",audit["input_sha256"]},{"queries_sha256",audit["queries_sha256"]},{"counts",rows}};
            completed_queries=counts.value.size();
        }
        const auto output=result.dump()+"\n";
        audit["ok"]=true; audit["exit_code"]=0; audit["completed_queries"]=completed_queries; audit["result_sha256"]=hanmo::sha256(output);
        StagedFile result_file(paths.at("--output"),output);
        StagedFile audit_file(paths.at("--audit"),audit.dump(2)+"\n");
        try { result_file.publish(); audit_file.publish(); }
        catch(...) { result_file.rollback(); audit_file.rollback(); throw; }
        std::cout<<(legacy?"NATIVE_ACTOR_COMMITMENTS queries=":"NATIVE_READONLY_COUNT queries=")<<completed_queries<<" installs="<<installed_audit().install_count<<'\n';
        return 0;
    } catch(const Failure& failure) {
        audit["ok"]=false; audit["exit_code"]=1; audit["completed_queries"]=0; audit.erase("result_sha256");
        audit["error"]={{"code",failure.code},{"path",failure.path}}; runtime_audit(audit,installed_audit(),legacy);
        if(destinations_valid) {
            try { StagedFile file(paths.at("--audit"),audit.dump(2)+"\n"); file.publish(); }
            catch(const Failure& secondary) { std::cerr<<secondary.code<<": "<<secondary.path<<'\n'; }
        }
        std::cerr<<failure.code<<": "<<failure.path<<'\n'; return 1;
    } catch(const std::exception& error) {
        std::cerr<<"NativeReadFailure: "<<error.what()<<'\n'; return 1;
    }
}
