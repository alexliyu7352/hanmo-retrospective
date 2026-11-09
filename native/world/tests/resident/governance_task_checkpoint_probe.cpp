// 新治理生产冷边界只消费真实bank；不构造成功世界或重新执行签发/受领/劳动。
#include "resident/kernel/boundary_dispatch.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/kernel/native_initial_state.hpp"
#include "resident/kernel/world_codec.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
using V=BoundaryValue;namespace fs=std::filesystem;
// 即使Release也保留失败检查；原业务拒绝直接记录领域码。
void require(bool value,const StableId& message){if(!value)throw std::runtime_error(message);}
template<class T>T must(Outcome<T> value,const StableId& message){require(value.ok,message+": "+value.error.original.code);return std::move(value.value);}
V read_json(const fs::path& path){std::ifstream input(path);require(bool(input),"read "+path.string());std::ostringstream data;data<<input.rdbuf();return must(parse_boundary_json(data.str()),"parse input");}
Bytes read_bank(const fs::path& path){std::ifstream input(path,std::ios::binary);require(bool(input),"read native bank");return {std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};}
void write(const fs::path& path,const V& value){std::ofstream output(path);require(bool(output),"write "+path.string());output<<must(render_boundary_json(value),"render evidence")<<'\n';}
// 此处JSON只用于故意损坏冷输入；必须先证明原完整导出可正常加载。
void remove_field(V& value,const StableId& name){auto* fields=value.as<V::Dict>();require(fields!=nullptr,"expected dictionary");auto before=fields->size();fields->erase(std::remove_if(fields->begin(),fields->end(),[&](const auto& row){return row.first==name;}),fields->end());require(fields->size()+1==before,"expected exactly one field "+name);}
// 原checkpoint选择头已通过生产检查；这里只复制其完整目录身份给同一原生情景准备器。
std::shared_ptr<const PreparedCatalog> selected_catalog(const V& catalog,const V& selection){
    const auto& source=boundary::field(selection,"catalog","selection");CatalogIdentity identity;
    identity.content_id=boundary::get_string(source,"content_id","catalog");identity.content_version=boundary::get_string(source,"content_version","catalog");identity.content_digest=boundary::get_string(source,"content_digest","catalog");identity.ruleset_id=boundary::get_string(source,"ruleset_id","catalog");identity.schema_sha256=boundary::get_string(source,"schema_sha256","catalog");identity.artifact_sha256=boundary::get_string(source,"artifact_sha256","catalog");if(const auto* digest=source.find("pack_set_digest"))identity.pack_set_digest=boundary::string(*digest,"pack_set_digest");
    for(const auto& module:boundary::array(boundary::field(source,"module_versions","catalog"),"modules"))identity.module_versions.emplace_back(boundary::get_string(module,"name","module"),boundary::get_string(module,"version","module"));
    return must(prepare_native_catalog(catalog,boundary::get_string(selection,"scenario_id","selection"),identity),"prepare original native scenario");
}
// 正常原生export不含持久world_id；JSON冷边界使用本情景目录校验和精确回导，不借legacy initialize重建不同人物表。
Status validate_exported_candidate(const V& exported,const std::shared_ptr<const PreparedCatalog>& catalog){
    auto seed=decode_world_seed(exported,*catalog);if(!seed.ok)return {false,{},seed.error};WorldBundle candidate(catalog,std::move(seed.value));auto valid=candidate.validate();if(!valid.ok)return valid;auto actual=export_world_state(candidate);if(!actual.ok)return {false,{},actual.error};if(!compare_boundary(exported,actual.value).exact())return common_failure("ProbeExportRoundtripMismatch");return common_success();
}
V query(ResidentReplaySession& world,const StableId& id){LiveOperation op;op.source=must(world.identity(),"current source");op.payload=boundary::obj({{"query_id",V{id}},{"args",V{V::Dict{}}}});return must(world.execute_live(op),"query "+id).result;}
}
int main(int argc,char** argv){try{
    require(argc==4,"usage: governance_task_checkpoint_probe CATALOG REAL_BANK OUTPUT");const auto catalog=read_json(argv[1]);const auto original=read_bank(argv[2]);const fs::path output=argv[3];fs::create_directories(output);
    auto world=create_resident_replay_session();auto selection=must(world->inspect_checkpoint_selection(original),"inspect actual bank");const auto& modules=boundary::array(boundary::field(boundary::field(selection,"catalog","selection"),"module_versions","catalog"),"module_versions");
    for(const auto* name:{"politics_governance","governance_tasks","public_relief"}){Integer count=0;for(const auto& module:modules)if(boundary::string(boundary::field(module,"name","module"),"name")==name){++count;require(boundary::string(boundary::field(module,"version","module"),"version")=="1","unexpected governance version");}require(count==1,"missing or duplicate governance marker");}
    auto installed=world->initialize_checkpoint(catalog,original);require(installed.ok,"restore actual bank: "+installed.error.original.code);require(must(world->checkpoint(),"checkpoint after restore")==original,"restore rewrote original bank");
    auto exported=must(world->export_state(),"export actual governance world");for(const auto* name:{"politics_governance","governance_tasks","public_relief"})require(exported.find(name)!=nullptr,"missing actual domain");const auto& contracts=boundary::array(boundary::field(boundary::field(exported,"governance_tasks","world"),"governance_contracts","governance_tasks"),"contracts");require(!contracts.empty(),"bank has no accepted governance task");
    write(output/"selection.json",selection);write(output/"journal.json",query(*world,"journal_text"));write(output/"action.json",query(*world,"action_context"));write(output/"manifest.json",query(*world,"task_travel_manifest"));require(compare_boundary(exported,must(world->export_state(),"after read export")).exact(),"pure reads changed world");require(must(world->checkpoint(),"after pure reads")==original,"pure reads changed checkpoint");
    auto prepared=selected_catalog(catalog,selection);auto restored=validate_exported_candidate(exported,prepared);require(restored.ok,"unchanged exported native cold input rejected: "+restored.error.original.code);
    V::Array rejections;
    for(const auto* name:{"politics_governance","governance_tasks","public_relief"}){auto broken=exported;remove_field(broken,name);auto rejected=validate_exported_candidate(broken,prepared);require(!rejected.ok,StableId{"missing domain accepted: "}+name);rejections.push_back(boundary::obj({{"case",V{StableId{"missing:"}+name}},{"code",V{rejected.error.original.code}}}));}
    // 空采购钱包本身是合法治理形状；篡改typed接受源仍必须被共同冷验证拒绝。
    auto broken=exported;const auto task_id=boundary::get_string(contracts.front(),"key","contract entry");auto* entries=broken.find("governance_tasks")->find("governance_contracts")->as<V::Array>();require(entries&&!entries->empty(),"actual contract entries missing");auto* acceptance=entries->front().find("record")->find("acceptance");require(acceptance&&acceptance->find("authority_source_fact_id")&&boundary::get_string(*acceptance,"task_id","acceptance")==task_id,"actual acceptance source missing");*acceptance->find("authority_source_fact_id")=V{StableId{"invalid.authority"}};auto rejected=validate_exported_candidate(broken,prepared);require(!rejected.ok,"forged acceptance source accepted");rejections.push_back(boundary::obj({{"case",V{StableId{"forged_acceptance"}}},{"code",V{rejected.error.original.code}}}));
    require(must(world->checkpoint(),"original remains installed")==original,"failed cold candidate changed installed world");write(output/"rejections.json",V{rejections});write(output/"result.json",boundary::obj({{"commands_submitted",V{Integer{0}}},{"actual_governance_tasks",V{static_cast<Integer>(contracts.size())}},{"original_bytes",V{static_cast<Integer>(original.size())}},{"checkpoint_equal",V{true}}}));std::cout<<"governance task cold: real bank, pure journal, unchanged bytes and missing/forged source rejection passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
