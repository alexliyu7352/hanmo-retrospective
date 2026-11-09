// 同一合法世界和完整草案经过真实会话查询；本门不编造军队、来源或军事算法。
#include "resident/kernel/world_codec.hpp"
#include "resident/kernel/boundary_dispatch.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
using V=BoundaryValue;using D=V::Dict;using A=V::Array;
// 失败连同原始代码和路径上报，不能把未准入的fixture算作对照通过。
template<class T>T must(Outcome<T> result){if(!result.ok)throw std::runtime_error(result.error.original.code+":"+result.error.original.path);return std::move(result.value);}
void require(Status result){if(!result.ok)throw std::runtime_error(result.error.original.code+":"+result.error.original.path);}
void check(bool value,const std::string& label){if(!value)throw std::runtime_error(label);}
// 输入由一次真实生产来源导出；对象字段顺序不作为UI协议，但数组次序完整保留。
V read(const char* path){std::ifstream stream(path);check(bool(stream),"input unavailable");return must(parse_boundary_json(std::string(std::istreambuf_iterator<char>(stream),{})));}
V object(D fields){return V{std::move(fields)};}
bool same(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
// 只走公开live query，普通规则拒绝仍是需跨语言比较的真实结果。
V query(ResidentReplaySession& session,const std::string& id,const V& args){auto source=must(session.identity());auto reply=session.execute_live({LiveOperationKind::query,source,object({{"query_id",V{id}},{"args",args}})});if(!reply.ok)return object({{"ok",V{false}},{"code",V{reply.error.original.code}},{"path",V{reply.error.original.path}}});check(same(source,reply.value.source_identity)&&same(source,reply.value.current_identity),"read changed identity");return std::move(reply.value.result);}
}
// 主入口一次准入全部世界，前后checkpoint和完整state必须完全不变。
int main(int argc,char** argv){try{check(argc==3,"usage: military_preview_joint_test input.json native.json");auto input=read(argv[1]);auto session=create_resident_replay_session();require(session->initialize(boundary::field(input,"catalog","input"),boundary::field(input,"state","input")));auto before=must(session->checkpoint());auto state=must(session->export_state());auto identity=must(session->identity());const auto& cases=boundary::array(boundary::field(input,"cases","input"),"cases");check(!cases.empty()&&cases.size()<=256,"finite nonempty cases required");A rows;for(const auto& row:cases){auto args=boundary::field(row,"args","case");rows.push_back(object({{"label",boundary::field(row,"label","case")},{"page",query(*session,"military_battle_page",object({{"battle_id",boundary::field(args,"battle_id","args")}}))},{"draft",query(*session,"military_battle_draft",args)}}));}check(before==must(session->checkpoint()),"queries changed checkpoint (includes RNG and counters)");check(compare_boundary(state,must(session->export_state())).exact(),"queries changed full state");check(same(identity,must(session->identity())),"queries changed world identity");auto result=object({{"ok",V{true}},{"cases",V{std::move(rows)}},{"zero_write",V{true}},{"tick",V{identity.tick}},{"revision",V{identity.revision}}});std::ofstream output(argv[2]);check(bool(output),"output unavailable");output<<must(render_boundary_json(result))<<'\n';output.close();check(bool(output),"output write failed");std::cout<<"MILITARY_PREVIEW_JOINT_NATIVE_CASES="<<cases.size()<<" ZERO_WRITE=true\n";return 0;}catch(const std::exception& error){std::cerr<<"MILITARY_PREVIEW_JOINT_NATIVE_FAIL="<<error.what()<<'\n';return 1;}}
