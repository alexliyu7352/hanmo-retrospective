// 单个真实C记录的校验和有效但键序损坏检查点；用于证明并回归冷恢复发布前守卫。
#include "resident/kernel/boundary_dispatch.hpp"
#include "resident/kernel/world_codec.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace hanmo::resident;
namespace {
BoundaryValue read_source(const std::string& path){std::ifstream input(path);assert(input.good());std::string bytes((std::istreambuf_iterator<char>(input)),{});auto result=parse_boundary_json(bytes);assert(result.ok);return std::move(result.value);}
template<class T> T take(Outcome<T> result){if(!result.ok){std::cerr<<result.error.original.code<<":"<<result.error.original.path<<"\n";std::abort();}return std::move(result.value);}
}
int main(int argc,char** argv){
    assert(argc==2||argc==3);bool expect_reject=argc==3&&std::string(argv[2])=="--expect-reject";std::string root=argv[1];auto catalog=read_source(root+"/expanded-catalog.json");auto index=read_source(root+"/oracle-index.json");auto source=read_source(root+"/"+boundary::get_string(*index.find("final_state"),"file","final"));auto session=create_resident_replay_session();auto installed=session->initialize(catalog,source);assert(installed.ok);auto identity=take(session->identity());auto before=take(session->export_state());
    auto prepared=take(prepare_catalog(catalog,source));auto seed=take(decode_world_seed(source,*prepared));seed.common.identity=identity;WorldBundle candidate(prepared,std::move(seed));assert(candidate.validate().ok);assert(!candidate.interaction.encounters.empty());auto& order=candidate.interaction.encounters.front().field_order;assert(order.size()>=2);order[1]=order[0];
    // 只损坏一个记录的schema键序；正常编码生成全新正确摘要，不能用checksum失败掩盖语义缺口。
    auto corrupted=take(encode_world_checkpoint(candidate));auto decoded=take(decode_world_checkpoint(corrupted,*prepared));WorldBundle decoded_candidate(prepared,std::move(decoded));assert(decoded_candidate.validate().ok);auto cold_export=export_world_state(decoded_candidate);assert(!cold_export.ok&&cold_export.error.original.code=="BoundaryFieldOrderMismatch");
    auto restored=session->restore(corrupted);auto after=session->export_state();auto next_identity=take(session->identity());
    std::cout<<"RESTORE_ORDER_PROBE checksum_valid=1 typed_validate_ok=1 cold_export_error="<<cold_export.error.original.code<<" restore_ok="<<restored.ok<<" restore_error="<<restored.error.original.code<<" after_export_ok="<<after.ok<<" epoch_changed="<<(next_identity.execution_epoch!=identity.execution_epoch)<<"\n";
    if(expect_reject){assert(!restored.ok&&restored.error.original.code=="BoundaryFieldOrderMismatch");assert(after.ok&&compare_boundary(before,after.value).exact());assert(next_identity.world_id==identity.world_id&&next_identity.execution_epoch==identity.execution_epoch&&next_identity.revision==identity.revision&&next_identity.tick==identity.tick);}
}
