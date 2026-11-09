// 正常作者和同一玩家经原军令/编舰/舟行进入真正接敌，再用有限公开战术页产生待接管银行。
#include "resident/kernel/boundary_dispatch.hpp"
#include "resident/domains/military/naval_commands.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/domains/military/battle_model.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <algorithm>
using namespace hanmo::resident;
namespace {
using V=BoundaryValue;using A=V::Array;namespace fs=std::filesystem;
const StableId player="qingxi::actor.player";int checks=0;
// 检查不随Release关闭；失败保原错误，不能以其它拒因冒充晚失败。
void check(bool v,const StableId& why){++checks;if(!v)throw std::runtime_error(why);}
template<class T>T take(Outcome<T> v,const StableId& why){check(v.ok,why+":"+v.error.original.code+":"+v.error.original.path);return std::move(v.value);}
void take(Status v,const StableId& why){check(v.ok,why+":"+v.error.original.code);}
const V& field(const V& v,const StableId& k){return boundary::field(v,k,"probe");}
StableId text(const V& v,const StableId& k){return boundary::get_string(v,k,"probe");}
StableId json(const V& v){return take(render_boundary_json(v),"JSON");}
V read(const fs::path& p){std::ifstream f(p);check(bool(f),"input "+p.string());std::ostringstream s;s<<f.rdbuf();return take(parse_boundary_json(s.str()),"input JSON");}
void write(const fs::path& p,const V& v){std::ofstream f(p);check(bool(f),"output");f<<json(v)<<'\n';}
void write(const fs::path& p,const Bytes& v){std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(v.data()),v.size());check(bool(f),"bank output");}
bool ok(const V& v){auto p=v.find("ok");return p&&p->as<bool>()&&*p->as<bool>();}
V empty(){return V{V::Dict{}};}
V query(ResidentReplaySession& w,const StableId& id,const V& args=V{V::Dict{}}){LiveOperation op;op.source=take(w.identity(),"read identity");op.payload=boundary::obj({{"query_id",V{id}},{"args",args}});return take(w.execute_live(op),"query "+id).result;}
// 所有原生动作都携本人的当前identity、序号和版本，未代NPC或注入ArmyRecord。
V command(ResidentReplaySession& w,const StableId& kind,const V& params,A& trace){auto ctx=query(w,"command_context");check(text(ctx,"actor_id")==player,"selected commander remains player");LiveOperation op;op.kind=LiveOperationKind::command;op.source=take(w.identity(),"command identity");op.payload=boundary::obj({{"kind",V{kind}},{"params",params},{"actor_id",V{player}},{"expected_revision",field(ctx,"revision")},{"request_seq",field(ctx,"next_request_seq")}});auto result=take(w.execute_live(op),"command transport "+kind).result;trace.push_back(boundary::obj({{"input",op.payload},{"result",result}}));return result;}
void success(ResidentReplaySession& w,const StableId& kind,const V& params,A& trace){auto r=command(w,kind,params,trace);check(ok(r),kind+" refused "+json(r));}
void finish(ResidentReplaySession& w,A& trace){for(int i=0;i<16;++i){auto ctx=query(w,"action_context");auto id=ctx.find("action_id");if(!id||!id->as<StableId>()||id->as<StableId>()->empty())return;success(w,"advance",empty(),trace);}throw std::runtime_error("authored action exceeded finite duration");}
V provider(){return boundary::obj({{"place_id",V{"qingxi::place.north.camp"}},{"role_id",V{"qingxi::role.north.officer"}},{"provider_actor_id",V{"qingxi::actor.north_officer"}}});}
// 只从正式本人营务页中的可用船、真实部队和容器形成原编队请求。
NavalForm formation(const V& page){NavalForm f;f.request_key="naval-transaction-gate";f.formation.army_id=text(page,"army_id");A ships;for(const auto& v:boundary::array(field(page,"ships"),"ships"))if(field(v,"available").as<bool>()&&*field(v,"available").as<bool>())ships.push_back(v);auto& units=boundary::array(field(page,"units"),"units");check(!units.empty()&&ships.size()>=units.size(),"actual own ships cover actual units");for(std::size_t i=0;i<units.size();++i){NavalVesselAssignment a;a.vessel_id=text(ships[i],"id");a.unit_id=text(units[i],"id");if(i==0){for(const auto& p:boundary::array(field(page,"passengers"),"passengers"))a.passenger_ids.push_back(text(p,"id"));for(const auto& p:boundary::array(field(page,"prisoners"),"prisoners"))a.prisoner_ids.push_back(text(p,"id"));a.extra_inventory_ids.push_back(text(page,"army_inventory_id"));}f.formation.assignments.push_back(std::move(a));}return f;}
V params(const NavalForm& f){A assignments;for(const auto& a:f.formation.assignments){A people,prisoners,extra;for(const auto& x:a.passenger_ids)people.push_back(V{x});for(const auto& x:a.prisoner_ids)prisoners.push_back(V{x});for(const auto& x:a.extra_inventory_ids)extra.push_back(V{x});assignments.push_back(boundary::obj({{"vessel_id",V{a.vessel_id}},{"unit_id",V{a.unit_id}},{"passenger_ids",V{people}},{"prisoner_ids",V{prisoners}},{"extra_inventory_ids",V{extra}}}));}return boundary::obj({{"request_key",V{f.request_key}},{"army_id",V{f.formation.army_id}},{"assignments",V{assignments}}});}
// 战术驱动只读本人有限页和原draft报价；不消费NPC计划、随机源或修改任何战场账。
Integer number(const V& v,const StableId& key){return boundary::get_int(v,key,"journey");}
bool flag(const V& v,const StableId& key){auto p=v.find(key);return p&&p->as<bool>()&&*p->as<bool>();}
HexPoint hex(const V& v){const auto& a=boundary::array(v,"point");check(a.size()==2,"hex shape");return {boundary::integer(a[0],"q"),boundary::integer(a[1],"r")};}
V xy(HexPoint p){return V{A{V{p.q},V{p.r}}};}
V hold(const V& unit){return boundary::obj({{"action",V{"hold"}},{"target_unit_id",V{StableId{}}},{"destination",field(unit,"position")},{"heading",field(unit,"heading")}});}
V draft(ResidentReplaySession& w,const V& page,const StableId& selected,const V& orders){return query(w,"naval_battle_draft",boundary::obj({{"battle_id",field(page,"battle_id")},{"selected_id",V{selected}},{"orders",orders}}));}
V plan(ResidentReplaySession& w,const V& page){
 V::Dict entries;const auto& units=field(page,"units");for(const auto& id:boundary::array(field(page,"controlled_unit_ids"),"controlled")){const auto name=boundary::string(id,"id");entries.emplace_back(name,hold(field(units,name)));}V orders{entries};
 for(const auto& entry:entries){const auto& own=field(units,entry.first);if(flag(own,"withdrawn")||number(own,"fit_count")==0)continue;auto options=draft(w,page,entry.first,orders);struct Choice{Integer score;V order;};std::vector<Choice> choices;
  for(const auto& enemy:boundary::object(units,"units")){if(number(enemy.second,"side")==number(own,"side")||flag(enemy.second,"withdrawn"))continue;for(const auto& move:boundary::array(field(options,"movement"),"movement")){auto destination=hex(field(move,"position"));const auto distance=military_hex::distance(destination,hex(field(enemy.second,"position")));const auto here=hex(field(own,"position"));const StableId action=distance==1?"board":destination==here?"hold":"move";const auto target=distance==1?enemy.first:StableId{};choices.push_back({(distance==1?10000:-distance*100)-number(move,"cost"),boundary::obj({{"action",V{action}},{"target_unit_id",V{target}},{"destination",xy(destination)},{"heading",V{military_hex::direction(destination,hex(field(enemy.second,"position")))}}})});}}
  std::stable_sort(choices.begin(),choices.end(),[](const auto& a,const auto& b){return a.score>b.score;});for(const auto& choice:choices){auto candidate=orders;*candidate.find(entry.first)=choice.order;auto quote=draft(w,page,entry.first,candidate);if(flag(quote,"ok")){orders=std::move(candidate);break;}}
 }
 check(flag(draft(w,page,entries.front().first,orders),"ok"),"whole actual finite draft quoted");return orders;
}
V capture(const V& page,const V& claim){return boundary::obj({{"kind",V{"naval_capture"}},{"params",boundary::obj({{"battle_id",field(page,"battle_id")},{"round_source_id",field(page,"round_source_id")},{"request_key",V{"journey-capture:"+text(page,"round_source_id")+":"+text(claim,"vessel_id")}},{"vessel_id",field(claim,"vessel_id")},{"captor_unit_id",field(claim,"captor_unit_id")},{"assignments",field(claim,"assignments")}})}});}
}
// 有限场景只保存真实已发生银行；期限内未自然产生目标接管权就保失败，不补船、换人或篡改计划。
int main(int argc,char** argv){fs::path out;A trace,pages;std::unique_ptr<ResidentReplaySession> w;try{
 check(argc==4,"catalog request fresh-output");out=argv[3];check(!fs::exists(out),"fresh output required");fs::create_directories(out);auto catalog=read(argv[1]);w=create_resident_replay_session();const bool resume=fs::path(argv[2]).extension()==".hmcb";if(resume){std::ifstream input(argv[2],std::ios::binary);check(bool(input),"original checkpoint input");Bytes bank{std::istreambuf_iterator<char>(input),{}};take(w->initialize_checkpoint(catalog,bank),"actual whole bank resume");check(bank==take(w->checkpoint(),"cold original bank"),"resume leaves original bytes unchanged");}else{auto request=read(argv[2]);take(w->initialize_native(catalog,request),"formal Native admission");
 success(*w,"move_local",boundary::obj({{"place_id",V{"qingxi::place.north.camp"}}}),trace);success(*w,"office_accept",boundary::obj({{"office_id",V{"qingxi::office.north_leader"}},{"context",provider()}}),trace);finish(*w,trace);success(*w,"campaign_accept",boundary::obj({{"template_id",V{"qingxi::order.north_clear"}},{"context",provider()}}),trace);auto state=take(w->export_state(),"accepted state");StableId campaign;for(const auto& row:boundary::object(field(state,"campaigns"),"campaigns"))if(text(row.second,"actor_id")==player)campaign=row.first;check(!campaign.empty(),"original own campaign");success(*w,"army_form",boundary::obj({{"order_id",V{campaign}},{"unit_ids",V{A{V{"qingxi::unit.north.work_detail"},V{"qingxi::unit.north.blockade"}}}},{"retainer_ids",V{A{}}},{"food_quantity",V{Integer{360}}},{"money",V{Integer{100}}}}),trace);finish(*w,trace);auto form=formation(field(query(*w,"military_camp_page"),"naval"));form.request_key="naval-real-battle-journey";success(*w,"naval_form",params(form),trace);write(out/"formed.hmcb",take(w->checkpoint(),"formed bank"));success(*w,"move_local",boundary::obj({{"place_id",V{"qingxi::place.north.gate"}}}),trace);finish(*w,trace);success(*w,"campaign_depart",boundary::obj({{"campaign_id",V{campaign}}}),trace);}
 for(int step=0;step<128;++step){auto page=query(*w,"naval_battle_page");pages.push_back(page);if(flag(page,"active")){
   if(text(page,"phase")=="planning"){if(flag(page,"can_plan")){auto orders=plan(*w,page);success(*w,"naval_battle_plan",boundary::obj({{"battle_id",field(page,"battle_id")},{"request_key",V{"journey-plan:"+text(page,"battle_id")+":"+std::to_string(number(page,"round"))}},{"orders",orders}}),trace);}success(*w,"advance",empty(),trace);continue;}
   check(text(page,"phase")=="settlement","active battle has known phase");bool decided=false;for(const auto& claim:boundary::array(field(page,"claims"),"claims")){if(!flag(claim,"own_commander")||!text(claim,"decision").empty())continue;if(flag(claim,"capture_ok")){auto input=capture(page,claim);if(text(claim,"vessel_id")=="riverwatch::vessel.south.2"){write(out/"settlement.hmcb",take(w->checkpoint(),"actual settlement bank"));write(out/"capture-command.json",input);write(out/"settlement-page.json",page);write(out/"state.json",take(w->export_state(),"actual stable state"));write(out/"commands.json",V{trace});write(out/"pages.json",V{pages});std::cout<<"NAVAL_REAL_JOURNEY="<<checks<<" FAILURES=0 TARGET=ACTUAL_POPULATION_CAPTURE_SETTLEMENT\n";return 0;}success(*w,"naval_capture",field(input,"params"),trace);decided=true;break;}
    if(text(claim,"vessel_id")=="riverwatch::vessel.south.2")throw std::runtime_error("actual target capture refused: "+json(claim));check(flag(claim,"decline_ok"),"owned decision must quote");success(*w,"naval_claim_decline",boundary::obj({{"battle_id",field(page,"battle_id")},{"round_source_id",field(page,"round_source_id")},{"request_key",V{"journey-decline:"+text(page,"round_source_id")+":"+text(claim,"vessel_id")}},{"vessel_id",field(claim,"vessel_id")},{"captor_unit_id",field(claim,"captor_unit_id")}}),trace);decided=true;break;}
   if(decided)continue;success(*w,"naval_battle_continue",boundary::obj({{"battle_id",field(page,"battle_id")},{"round_source_id",field(page,"round_source_id")},{"request_key",V{"journey-continue:"+text(page,"round_source_id")}}}),trace);continue;
  }
  // 原军令抵南岸后由本人正常驶回河中巡看，不从隐藏敌舰位置补传送或造接触。
  auto current=take(w->export_state(),"current own location");const auto& person=field(field(current,"actors"),player);const auto& location=field(person,"location");if(text(location,"kind")=="Settlement"&&text(location,"settlement_id")=="qingxi::settlement.south"){auto context=query(*w,"action_context");auto action=context.find("action_id");if(!action||!action->as<StableId>()||action->as<StableId>()->empty()){success(*w,"travel",boundary::obj({{"destination",xy({110,40})}}),trace);continue;}}
  success(*w,"advance",empty(),trace);
 }
 throw std::runtime_error("actual authored journey did not reach population capture settlement within 128 original decisions");
 }catch(const std::exception& e){if(!out.empty()&&fs::exists(out)){write(out/"commands.json",V{trace});write(out/"pages.json",V{pages});write(out/"failure.json",boundary::obj({{"ok",V{false}},{"checks",V{Integer(checks)}},{"error",V{StableId(e.what())}}}));if(w){auto bank=w->checkpoint();if(bank.ok)write(out/"failure.hmcb",bank.value);auto state=w->export_state();if(state.ok)write(out/"failure-state.json",state.value);}}std::cerr<<"NAVAL_REAL_JOURNEY_FAIL checks="<<checks<<" "<<e.what()<<'\n';return 1;}}
