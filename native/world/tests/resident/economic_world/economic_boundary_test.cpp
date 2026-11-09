// 从原作者Catalog.read_prepared与native_request进入真实NativeBoundarySession；不改初态、不授予测试专用权限。
#include "resident/kernel/boundary_dispatch.hpp"
#include "resident/storage/common_codec.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
using V=BoundaryValue;Integer checks=0;
// Release同样保留检查，失败报告首个真实边界错误。
void check(bool ok,const StableId& message){++checks;if(!ok)throw std::runtime_error(message);}
// 失败不读取默认结果，不将空页解释为经济规则成功。
template<class T>T must(Outcome<T> result,const StableId& label){check(result.ok,label+": "+result.error.original.code+" "+result.error.original.path);return std::move(result.value);}
// 安装与冷恢复失败即停止，不覆盖仍有效的旧来源。
void must(Status result,const StableId& label){check(result.ok,label+": "+result.error.original.code+" "+result.error.original.path);}
// 文件来自原作者加载器成功输出；此处只做严格JSON解码。
V input(const char* path){std::ifstream file(path);check(bool(file),"Input file missing");std::ostringstream bytes;bytes<<file.rdbuf();return must(parse_boundary_json(bytes.str()),"Parse original input");}
// 请求使用实际session身份，参数只含页面所需三字段。
Outcome<LiveOperationResult> raw_query(ResidentReplaySession& session,const WorldIdentity& identity,const StableId& name,V args){return session.execute_live({LiveOperationKind::query,identity,boundary::obj({{"query_id",V{name}},{"args",std::move(args)}})});}
// 当前query成功才读取原生DTO；不建立另一份WorldBundle候选。
V query(ResidentReplaySession& session,const StableId& name,V args){auto identity=must(session.identity(),"Current query identity");return must(raw_query(session,identity,name,std::move(args)),"Query "+name).result;}
// 核对整个原状态的数值和字段序，包含钱货、时钟、随机流、请求序号和事实。
void unchanged(ResidentReplaySession& session,const V& before,const StableId& label){check(compare_boundary(before,must(session.export_state(),"Export after read")).exact(),label);}
}
int main(int argc,char** argv){try{
    check(argc==4,"Usage: economic_boundary_test CATALOG_READ_JSON NATIVE_REQUEST_JSON PAGE_OUTPUT");auto catalog=input(argv[1]),request=input(argv[2]);check(boundary::get_bool(catalog,"ok","catalog"),"Input is not successful Catalog.read_prepared output");
    auto session=create_resident_replay_session();check(bool(session),"Real native session factory missing");must(session->initialize_native(catalog,request),"Initialize unchanged author request");
    auto source=must(session->identity(),"Installed identity");auto before=must(session->export_state(),"Original native state");auto checkpoint=must(session->checkpoint(),"Original native checkpoint");
    const auto context=query(*session,"command_context",boundary::obj({}));const auto actor=boundary::get_string(context,"actor_id","context");const auto location=query(*session,"journey_location",boundary::obj({}));const auto place=boundary::get_string(location,"place_id","location");
    auto args=boundary::obj({{"place_id",V{place}}});auto page=query(*session,"local_economy",args);
    check(boundary::get_string(page,"observer_actor_id","page")==actor&&boundary::get_string(page,"place_id","page")==place,"Economy observer/location does not match actual player");
    check(boundary::get_int(boundary::field(page,"identity","page"),"execution_epoch","identity")==source.execution_epoch,"Index was not rebuilt after final install epoch");
    check(boundary::array(boundary::field(page,"rows","page"),"rows").empty()&&boundary::array(boundary::field(page,"authorities","page"),"authorities").empty(),"Original apprentice unexpectedly acquired public economic authority");
    check(boundary::get_int(page,"scanned","page")==0,"Private world tables affected this player's empty page shape");
    auto again=query(*session,"local_economy",boundary::obj({{"place_id",V{place}},{"cursor",V{}},{"authority",V{}}}));check(compare_boundary(page,again).exact(),"Repeated/null optional query changed page");
    StableId other;const auto& places=boundary::field(boundary::field(catalog,"data","catalog"),"places","data");for(const auto& row:boundary::object(places,"places"))if(row.first!=place){other=row.first;break;}check(!other.empty(),"Author input lacks second real place");
    auto remote=raw_query(*session,source,"local_economy",boundary::obj({{"place_id",V{other}}}));check(!remote.ok&&remote.error.original.code=="LocalEconomyPlaceNotObservable","Actual remote author place was observable");
    auto impersonate=args;impersonate.as<V::Dict>()->emplace_back("observer_actor_id",V{StableId("someone_else")});check(!raw_query(*session,source,"local_economy",impersonate).ok,"Boundary accepted caller-selected observer");
    auto malformed=args;malformed.as<V::Dict>()->emplace_back("cursor",V{Integer{0}});check(!raw_query(*session,source,"local_economy",malformed).ok,"Boundary accepted numeric cursor");
    unchanged(*session,before,"Reads or rejected requests changed actual game state");check(checkpoint==must(session->checkpoint(),"After readonly checkpoint"),"Readonly requests changed native checkpoint bytes");
    auto saved_cursor=boundary::obj({{"identity",boundary::field(page,"identity","page")},{"observer_actor_id",V{actor}},{"place_id",V{place}},{"authority",V{}},{"offset",V{Integer{0}}}});
    must(session->restore(checkpoint),"Restore actual checkpoint");const auto restored=must(session->identity(),"Restored identity");check(restored.execution_epoch>source.execution_epoch,"Restore did not replace execution source");
    auto old=raw_query(*session,source,"local_economy",args);check(!old.ok&&old.error.original.code=="StaleWorld","Old world source survived restore");
    auto fresh=query(*session,"local_economy",args);check(boundary::get_int(boundary::field(fresh,"identity","page"),"execution_epoch","identity")==restored.execution_epoch,"Restored economic index still used old epoch");
    auto stale_args=args;stale_args.as<V::Dict>()->emplace_back("cursor",saved_cursor);auto stale=raw_query(*session,restored,"local_economy",stale_args);check(!stale.ok&&stale.error.original.code=="StaleLocalEconomyCursor","Old page cursor survived source replacement");unchanged(*session,before,"Restore/read changed original semantic state");
    auto cold=create_resident_replay_session();must(cold->initialize_checkpoint(catalog,checkpoint),"Independent cold session install");auto cold_page=query(*cold,"local_economy",args);check(boundary::get_string(cold_page,"observer_actor_id","page")==actor,"Cold session economy observer changed");unchanged(*cold,before,"Cold session query changed original semantic state");
    std::ofstream output(argv[3]);output<<must(render_boundary_json(page),"Encode actual page")<<"\n";check(bool(output),"Cannot preserve actual economic page");
    std::cout<<"ECONOMIC_BOUNDARY "<<checks<<" checks 0 failures; original apprentice has no private/public economic sources, no permissions fabricated\n";return 0;
}catch(const std::exception& error){std::cerr<<"ECONOMIC_BOUNDARY FAIL "<<error.what()<<" after "<<checks<<" checks\n";return 1;}}
