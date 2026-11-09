// 青川实际行动后银行的独立冷消费者；只使用真实生产库及银行解码的原payload，不新造初态。
#include "resident/kernel/boundary_dispatch.hpp"
#include "resident/storage/common_codec.hpp"
#include <picosha2.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
using V=BoundaryValue;
Integer checks=0;
const StableId actor="society::actor.study_swordswoman";
// 所有运行形态都执行检查，失败保留具体公共边界原因。
void check(bool value,const StableId& label){++checks;if(!value)throw std::runtime_error(label);}
// 冷证据只读原字节，禁止替坏银行或未知字段补值。
StableId read(const char* path){std::ifstream in(path,std::ios::binary);check(bool(in),StableId{"read "}+path);std::ostringstream bytes;bytes<<in.rdbuf();check(!in.bad(),StableId{"complete read "}+path);return bytes.str();}
// 真实失败不能被空载荷覆盖，错误码与路径直接进入报告。
template<class T>T must(Outcome<T> value,const StableId& label){check(value.ok,label+": "+value.error.original.code+" "+value.error.original.path);return std::move(value.value);}
// 安装与恢复同样遵循生产失败边界。
void must(Status value,const StableId& label){check(value.ok,label+": "+value.error.original.code+" "+value.error.original.path);}
// 原生序列化保留完整字段与顺序，不删字段制造等价。
StableId json(const V& value){return must(render_boundary_json(value),"render boundary JSON");}
// 读查询使用当前真实source，无运行或资金命令。
V query(ResidentReplaySession& session,const StableId& name){LiveOperation operation;operation.kind=LiveOperationKind::query;operation.source=must(session.identity(),"query identity");operation.payload=boundary::obj({{"query_id",V{name}},{"args",V{V::Dict{}}}});return must(session.execute_live(operation),"actual query "+name).result;}
// 全态与完整字节同时比较，防止只核少数关键字段掩盖原生恢复差异。
void exact(ResidentReplaySession& session,const V& expected,const StableId& raw,const Bytes& checkpoint){const auto state=must(session.export_state(),"export native restored state");check(compare_boundary(expected,state).exact(),"all world values and ordering exact");check(json(state)==raw,"entire world original bytes exact");check(must(session.checkpoint(),"checkpoint reencode")==checkpoint,"entire typed checkpoint original bytes exact");}
}
// 参数由隔离Main消费者提供；两个native空session均从同一实际银行恢复，不执行新局。
int main(int argc,char** argv){try{
    using namespace boundary;check(argc==5||argc==6,"usage: qingchuan_active_person_test CATALOG PAYLOAD WORLD REPORT [CONTINUATION]");
    const auto catalog_bytes=read(argv[1]),payload_bytes=read(argv[2]),world_bytes=read(argv[3]);
    const auto catalog=must(parse_boundary_json(catalog_bytes),"prepared catalog"),expected=must(parse_boundary_json(world_bytes),"actual saved world");
    check(get_bool(catalog,"ok","catalog"),"expanded actual prepared catalog required");
    check(get_string(catalog,"pack_set_digest","catalog")=="85073906b204952b66c9cf81a7963b5f39d6b75ad6a47de85f3112320f3914d7","actual Qingchuan 1.8 catalog");
    check(get_string(expected,"player_actor_id","world")==actor&&get_int(expected,"tick","world")>0,"actual Shen action bank after tick zero");
    const Bytes payload(payload_bytes.begin(),payload_bytes.end());Bytes continuation;if(argc==6){const auto raw=read(argv[5]);continuation.assign(raw.begin(),raw.end());}
    auto first=create_resident_replay_session();check(bool(first),"actual production session factory");check(!first->identity().ok&&first->identity().error.original.code=="NoWorld","new native session is empty");
    must(first->initialize_checkpoint_with_continuation(catalog,payload,continuation),"actual bank native install");exact(*first,expected,world_bytes,payload);
    const auto source=must(first->identity(),"first source");const auto& director=field(expected,"director","world");check(!field(director,"schedules","director").find(actor)&&!field(director,"plans","director").find(actor),"current player has no autonomous NPC schedule or plan");
    const auto context=query(*first,"command_context"),profile=query(*first,"self_profile"),stopped=query(*first,"continuation_state");
    check(get_bool(context,"ok","context")&&get_string(context,"actor_id","context")==actor,"native command actor is original Shen");check(get_string(profile,"kind","profile")=="self_profile"&&get_string(profile,"actor_id","profile")==actor,"native self profile belongs to same current actor");
    check(get_bool(stopped,"ok","continuation"),"native stopped continuation is readable");exact(*first,expected,world_bytes,payload);
    auto second=create_resident_replay_session();check(bool(second),"second production session factory");must(second->initialize_checkpoint_with_continuation(catalog,payload,continuation),"second actual bank restore");const auto cold=must(second->identity(),"second source");
    check(cold.world_id==source.world_id&&cold.tick==source.tick&&cold.revision==source.revision&&cold.execution_epoch!=source.execution_epoch,"persistent identity same and runtime epoch refreshed legally");exact(*second,expected,world_bytes,payload);
    const auto report=obj({{"ok",V{true}},{"checks",V{checks}},{"tick",V{source.tick}},{"revision",V{source.revision}},{"actor_id",V{actor}},{"world_sha256",V{picosha2::hash256_hex_string(world_bytes)}},{"payload_sha256",V{picosha2::hash256_hex_string(payload_bytes)}},{"full_world_original_bytes_exact",V{true}},{"typed_checkpoint_original_bytes_exact",V{true}},{"same_process_distinct_restore_epochs",V{true}},{"context",context},{"self_profile",profile},{"continuation",stopped}});
    std::ofstream out(argv[4],std::ios::binary);check(bool(out),"create report");out<<json(report)<<'\n';check(bool(out),"write report");
    std::cout<<"QINGCHUAN_ACTIVE_PERSON_NATIVE "<<checks<<" checks 0 failures; actual bank cold install, exact full state and typed bytes, player ownership and fresh epochs\n";return 0;
}catch(const boundary::DecodeFailure& error){std::cerr<<"QINGCHUAN_ACTIVE_PERSON_NATIVE FAIL "<<error.error.original.code<<" "<<error.error.original.path<<'\n';return 1;}catch(const std::exception& error){std::cerr<<"QINGCHUAN_ACTIVE_PERSON_NATIVE FAIL "<<error.what()<<'\n';return 1;}}
