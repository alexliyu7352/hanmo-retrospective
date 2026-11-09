// 正式作者目录经原NativeBoundarySession检查地图位置页、实际地方移动和冷恢复；不造世界初态。
#include "resident/kernel/boundary_dispatch.hpp"
#include "resident/storage/common_codec.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
using V=BoundaryValue;namespace fs=std::filesystem;
int checks=0;
// 所有失败保留业务码，禁止未知query或未安装被当成空页通过。
void require(bool value,const std::string& label){++checks;if(!value)throw std::runtime_error(label);}
template<class T>T must(Outcome<T> value,const std::string& label){require(value.ok,label+": "+value.error.original.code+" "+value.error.original.path);return std::move(value.value);}
void must(Status value,const std::string& label){require(value.ok,label+": "+value.error.original.code+" "+value.error.original.path);}
// 输入来自既有Catalog.read_prepared/native_request的精确输出，不绕过各自原验证。
V read_json(const fs::path& path){std::ifstream stream(path,std::ios::binary);require(bool(stream),"read "+path.string());std::ostringstream text;text<<stream.rdbuf();return must(parse_boundary_json(text.str()),"parse "+path.string());}
// 仅保存有限页面或本次新checkpoint，目录必须由调用者明确指定且原先不存在。
void write_json(const fs::path& path,const V& value){std::ofstream stream(path,std::ios::binary);require(bool(stream),"write "+path.string());stream<<must(render_boundary_json(value),"render")<<'\n';require(bool(stream),"finish write");}
// 所有正常读取沿与薄SDK相同的live入口，不直接调用领域叶绕过注册或世界身份。
Outcome<LiveOperationResult> query(ResidentReplaySession& world,const StableId& name,V args=V{V::Dict{}}){LiveOperation op;op.source=must(world.identity(),"query source");op.payload=boundary::obj({{"query_id",V{name}},{"args",std::move(args)}});return world.execute_live(op);}
// 只比较运行身份，不将全态导出作为UI输入或选择未知人物。
bool same(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
// 当前页的每个姓名必须来自同一观察时刻，最多64项且不能把本人重复画为他人。
void page_checks(const V& page,const WorldIdentity& source,const StableId& observer){using namespace boundary;
    require(get_int(page,"version","page")==1&&get_string(page,"observer_actor_id","page")==observer,"page version and observer");const auto& identity=field(page,"identity","page");
    require(get_int(identity,"execution_epoch","identity")==source.execution_epoch&&get_int(identity,"revision","identity")==source.revision&&get_int(identity,"tick","identity")==source.tick,"page final installed identity");
    const auto& rows=array(field(page,"rows","page"),"rows");require(rows.size()<=64,"bounded page");std::set<StableId> seen;
    for(const auto& row:rows){const auto id=get_string(row,"actor_id","row");require(get_string(row,"kind","row")=="actor"&&id!=observer&&seen.insert(id).second&&get_int(row,"observed_tick","row")==source.tick,"unique current observed actor");require(get_string(row,"display_name","row").size()<=4096,"bounded native name");}
}
// 游标仅重用已显示身份和位置，restore后应拒绝旧epoch，即使外层live source已更新。
V cursor_from(const V& page){return boundary::obj({{"identity",boundary::field(page,"identity","page")},{"observer_actor_id",boundary::field(page,"observer_actor_id","page")},{"location",boundary::field(page,"location","page")},{"offset",V{Integer(0)}}});}
// 原场景给出已知相邻入口，再通过真实command_context和live command执行一次地方移动。
void move_local_once(ResidentReplaySession& world){using namespace boundary;
    const auto location=must(query(world,"journey_location"),"current location").result;require(get_string(location,"kind","location")=="Settlement","author starts at a settlement");const auto place=get_string(location,"place_id","location");
    const auto scene=must(query(world,"scene_view",obj({{"place_id",V{place}},{"view_kind",V{"interior"}}})),"current scene").result;
    const auto& candidates=array(field(scene,"hotspot_candidates","scene"),"candidates");std::optional<V> selected;
    for(const auto& candidate:candidates)if(get_string(candidate,"command_kind","candidate")=="move_local"){selected=field(candidate,"params","candidate");break;}
    require(bool(selected),"actual authored adjacent passage exists");const auto target=get_string(*selected,"place_id","params");
    const auto context=must(query(world,"command_context"),"command context").result;const auto identity=must(world.identity(),"move source");
    V command=obj({{"kind",V{"move_local"}},{"params",*selected},{"actor_id",field(context,"actor_id","context")},{"request_seq",field(context,"next_request_seq","context")},{"expected_revision",V{identity.revision}}});
    LiveOperation operation;operation.kind=LiveOperationKind::command;operation.source=identity;operation.payload=std::move(command);
    const auto moved=must(world.execute_live(operation),"real local move");require(get_bool(moved.result,"ok","receipt"),"local move accepted");
    const auto current=must(query(world,"map_presence"),"presence after move").result;
    require(get_string(field(current,"location","page"),"secondary","location")==target,"index follows actual committed place");
}
}
// 本门只增加一次原地方移动；旧32项旅行链不重跑，银行是新的原生checkpoint而非玩家旧存档。
int main(int argc,char** argv){try{
    require(argc==4,"usage: map_presence_live_test catalog.json request.json fresh-output");const fs::path output=argv[3];require(!fs::exists(output),"fresh output required");fs::create_directories(output);
    const auto catalog=read_json(argv[1]),request=read_json(argv[2]);auto world=create_resident_replay_session();must(world->initialize_native(catalog,request),"formal native installation");
    const auto observer=boundary::get_string(request,"founder_selection","request");const auto source=must(world->identity(),"installed identity");const auto bank_before=must(world->checkpoint(),"before-read checkpoint");
    const auto first=must(query(*world,"map_presence"),"first live presence").result;page_checks(first,source,observer);write_json(output/"first-page.json",first);
    must(query(*world,"map_presence"),"repeated pure read");require(must(world->checkpoint(),"after-read checkpoint")==bank_before&&same(must(world->identity(),"after-read identity"),source),"reading does not mutate checkpoint or identity");
    require(!query(*world,"map_presence",boundary::obj({{"actor_id",V{"unobserved"}}})).ok,"arbitrary observer argument rejected");
    move_local_once(*world);const auto after_move=must(query(*world,"map_presence"),"current map page").result;const auto before_restore=must(world->identity(),"before restore");page_checks(after_move,before_restore,observer);
    const auto bank=must(world->checkpoint(),"new checkpoint");{std::ofstream file(output/"map-presence.bank",std::ios::binary);file.write(reinterpret_cast<const char*>(bank.data()),static_cast<std::streamsize>(bank.size()));require(bool(file),"write new native checkpoint");}
    must(world->restore(bank),"same-process cold restore");const auto restored=must(world->identity(),"restored identity");require(restored.execution_epoch!=before_restore.execution_epoch&&restored.world_id==before_restore.world_id&&restored.revision==before_restore.revision&&restored.tick==before_restore.tick,"restore changes epoch only");
    const auto current=must(query(*world,"map_presence"),"presence after restore").result;page_checks(current,restored,observer);write_json(output/"restored-page.json",current);
    require(compare_boundary(boundary::field(after_move,"rows","before"),boundary::field(current,"rows","after")).exact()&&compare_boundary(boundary::field(after_move,"location","before"),boundary::field(current,"location","after")).exact(),"restored local identities and position unchanged");
    const auto stale=query(*world,"map_presence",boundary::obj({{"cursor",cursor_from(after_move)}}));require(!stale.ok&&stale.error.original.code=="StaleMapPresenceCursor","old page cursor rejected under current outer source");
    require(must(world->checkpoint(),"final checkpoint")==bank,"read and stale-cursor failure do not mutate restored checkpoint");
    std::cout<<"MAP_PRESENCE_LIVE_CHECKS="<<checks<<" FAILURES=0 SOURCE=FORMAL_AUTHOR_NATIVE_SESSION GUI=NOT_TESTED UI_SLOT_BANK=NOT_TESTED\n";return 0;
}catch(const std::exception& error){std::cerr<<"MAP_PRESENCE_LIVE_FAIL checks="<<checks<<" "<<error.what()<<'\n';return 1;}}
