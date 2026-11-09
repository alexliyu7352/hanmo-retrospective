// 正常作者经任官、受令、围困和真实整编决定到公库接管；不注入军团、胜负、Owner或政治事实。
#include "resident/kernel/boundary_dispatch.hpp"
#include "resident/storage/common_codec.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
using V=BoundaryValue;using A=V::Array;using D=V::Dict;namespace fs=std::filesystem;
const StableId player="qingxi::actor.player";int checks=0;
// Release同样保留断言；真实业务首错不被改写成预期成功。
void check(bool value,const StableId& why){++checks;if(!value)throw std::runtime_error(why);}
template<class T>T take(Outcome<T> value,const StableId& why){check(value.ok,why+":"+value.error.original.code+":"+value.error.original.path);return std::move(value.value);}
void take(Status value,const StableId& why){check(value.ok,why+":"+value.error.original.code+":"+value.error.original.path);}
const V& field(const V& value,const StableId& key){return boundary::field(value,key,"journey");}
StableId text(const V& value,const StableId& key){return boundary::get_string(value,key,"journey");}
bool flag(const V& value,const StableId& key){const auto* p=value.find(key);return p&&p->as<bool>()&&*p->as<bool>();}
V empty(){return V{D{}};}
StableId json(const V& value){return take(render_boundary_json(value),"render JSON");}
V read(const fs::path& path){std::ifstream in(path);check(bool(in),"read "+path.string());std::ostringstream data;data<<in.rdbuf();return take(parse_boundary_json(data.str()),"parse JSON");}
// 只接受真实前半产生的原生银行；上限沿正式恢复协议，不把JSON转成可注入初态。
Bytes read_bank(const fs::path& path){std::ifstream in(path,std::ios::binary|std::ios::ate);check(bool(in),"read original bank");const auto length=in.tellg();check(length>0&&static_cast<std::uintmax_t>(length)<=live_checkpoint_byte_limit,"bounded original bank");Bytes bytes(static_cast<std::size_t>(length));in.seekg(0);in.read(reinterpret_cast<char*>(bytes.data()),bytes.size());check(bool(in),"complete original bank");return bytes;}
void write(const fs::path& path,const V& value){std::ofstream out(path);out<<json(value)<<'\n';check(bool(out),"write JSON");}
void write(const fs::path& path,const Bytes& value){std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(value.data()),value.size());check(bool(out),"write bank");}
// 所有操作带当前世界身份；预览和只读页面不允许推进世界。
V query(ResidentReplaySession& world,const StableId& id,const V& args=V{D{}}){LiveOperation op;op.source=take(world.identity(),"query identity");op.payload=boundary::obj({{"query_id",V{id}},{"args",args}});return take(world.execute_live(op),"query "+id).result;}
V operation(ResidentReplaySession& world,const StableId& kind,const V& params,A& trace,bool preview=false){auto context=query(world,"command_context");check(text(context,"actor_id")==player,"original player authority");LiveOperation op;op.kind=preview?LiveOperationKind::preview:LiveOperationKind::command;op.source=take(world.identity(),"command identity");op.payload=preview?boundary::obj({{"kind",V{kind}},{"params",params},{"actor_id",V{player}}}):boundary::obj({{"kind",V{kind}},{"params",params},{"actor_id",V{player}},{"expected_revision",field(context,"revision")},{"request_seq",field(context,"next_request_seq")}});auto result=take(world.execute_live(op),"transport "+kind).result;trace.push_back(boundary::obj({{"preview",V{preview}},{"input",op.payload},{"result",result}}));return result;}
void success(ResidentReplaySession& world,const StableId& kind,const V& params,A& trace){auto result=operation(world,kind,params,trace);check(flag(result,"ok"),kind+" refused "+json(result));}
// 只推进本人既有Action；有限期限没有完成就保原失败现场。
void finish(ResidentReplaySession& world,A& trace){for(int n=0;n<32;++n){auto context=query(world,"action_context");const auto* id=context.find("action_id");if(!id||!id->as<StableId>()||id->as<StableId>()->empty())return;success(world,"advance",empty(),trace);}throw std::runtime_error("authored action exceeds 32 halfdays");}
V provider(){return boundary::obj({{"place_id",V{"qingxi::place.north.camp"}},{"role_id",V{"qingxi::role.north.officer"}},{"provider_actor_id",V{"qingxi::actor.north_officer"}}});}
// 从正式玩家命令建立全部军旅来源；读取完整状态仅用于确认已经产生的本人军令。
void arrive(ResidentReplaySession& world,A& trace){
 success(world,"move_local",boundary::obj({{"place_id",V{"qingxi::place.north.camp"}}}),trace);finish(world,trace);
 success(world,"office_accept",boundary::obj({{"office_id",V{"qingxi::office.north_leader"}},{"context",provider()}}),trace);finish(world,trace);
 success(world,"campaign_accept",boundary::obj({{"template_id",V{"qingxi::order.north_clear"}},{"context",provider()}}),trace);
 auto state=take(world.export_state(),"accepted state");StableId campaign;for(const auto& row:boundary::object(field(state,"campaigns"),"campaigns"))if(text(row.second,"actor_id")==player){check(campaign.empty(),"one original player campaign");campaign=row.first;}check(!campaign.empty(),"actual accepted campaign");
 success(world,"army_form",boundary::obj({{"order_id",V{campaign}},{"unit_ids",V{A{V{"qingxi::unit.north.work_detail"},V{"qingxi::unit.north.blockade"}}}},{"retainer_ids",V{A{}}},{"food_quantity",V{Integer{360}}},{"money",V{Integer{100}}}}),trace);finish(world,trace);
 success(world,"move_local",boundary::obj({{"place_id",V{"qingxi::place.north.gate"}}}),trace);finish(world,trace);success(world,"campaign_depart",boundary::obj({{"campaign_id",V{campaign}}}),trace);finish(world,trace);
 auto page=query(world,"siege_page");if(text(page,"siege_id").empty())success(world,"siege_start",boundary::obj({{"campaign_id",V{campaign}}}),trace);
}
// 两原部队采用正常补给点封锁；每轮只发原军议并推进恰一半日，不模拟饥饿或NPC计划。
void blockade(ResidentReplaySession& world,const V& page,A& trace){check(text(field(page,"siege"),"phase")=="active","active siege required");check(boundary::array(field(field(page,"siege"),"pending_node_ids"),"pending").empty(),"no unresolved battle hidden by probe");D orders;for(const auto* unit:{"qingxi::unit.north.work_detail","qingxi::unit.north.blockade"})orders.emplace_back(unit,boundary::obj({{"action",V{"blockade"}},{"node_id",V{"supply"}}}));const auto before=take(world.identity(),"before siege halfday");success(world,"siege_plan",boundary::obj({{"siege_id",field(page,"siege_id")},{"orders",V{orders}}}),trace);success(world,"advance",empty(),trace);check(take(world.identity(),"after siege halfday").tick==before.tick+1,"exact original siege halfday");}
// 冷恢复只加载已实际发生的银行；完整JSON和银行字节逐一比等，不重发占领命令。
void save(ResidentReplaySession& world,const V& catalog,const fs::path& out,const StableId& name){auto state=take(world.export_state(),"export "+name);auto bank=take(world.checkpoint(),"checkpoint "+name);write(out/(name+".hmcb"),bank);write(out/(name+".json"),state);auto cold=create_resident_replay_session();take(cold->inspect_checkpoint_selection(bank),"title "+name);take(cold->initialize_checkpoint(catalog,bank),"cold "+name);check(compare_boundary(state,take(cold->export_state(),"cold export")).exact(),"exact cold state "+name);check(take(cold->checkpoint(),"cold checkpoint")==bank,"exact cold bytes "+name);}
// 政治批准只取当前原页面的具名可用选择； unsupported责任保明确暂停，不把通用成功当经营继受。
void approve(ResidentReplaySession& world,const V& catalog,const fs::path& out,A& trace,A& pages){
 success(world,"move_local",boundary::obj({{"place_id",V{"qingxi::place.south.camp"}}}),trace);finish(world,trace);
 auto page=query(world,"political_takeover",boundary::obj({{"place_id",V{"qingxi::place.south.camp"}}}));pages.push_back(page);check(flag(page,"enabled")&&!boundary::array(field(page,"registrations"),"registrations").empty(),"actual pending public responsibility page");
 A registrations=boundary::array(field(page,"registrations"),"registrations");for(const auto& selected:registrations){page=query(world,"political_takeover",boundary::obj({{"place_id",V{"qingxi::place.south.camp"}}}));const V* registration=nullptr;for(const auto& row:boundary::array(field(page,"registrations"),"registrations"))if(text(row,"registration_fact_id")==text(selected,"registration_fact_id"))registration=&row;check(registration&&flag(*registration,"available"),"current actual takeover authority");auto params=field(*registration,"command_params");A choices;
  for(const auto& responsibility:boundary::array(field(*registration,"responsibilities"),"responsibilities")){const V* chosen=nullptr;for(const auto& choice:boundary::array(field(responsibility,"choices"),"choices"))if(flag(choice,"available")&&text(choice,"disposition")=="assume_by_successor")chosen=&choice;if(!chosen)for(const auto& choice:boundary::array(field(responsibility,"choices"),"choices"))if(flag(choice,"available")&&text(choice,"disposition")=="suspend")chosen=&choice;check(chosen,"explicit supported choice");choices.push_back(field(*chosen,"request"));}
  *params.find("decisions")=V{std::move(choices)};if(text(selected,"registration_fact_id")==text(registrations.front(),"registration_fact_id")){save(world,catalog,out,"before-approval");write(out/"approval-command.json",boundary::obj({{"kind",V{"political_assume_public_responsibilities"}},{"params",params}}));}auto before=take(world.export_state(),"before approval");success(world,"political_assume_public_responsibilities",params,trace);auto after=take(world.export_state(),"after approval");check(compare_boundary(field(before,"wallets"),field(after,"wallets")).exact()&&compare_boundary(field(before,"inventories"),field(after,"inventories")).exact(),"political decision moves no money or goods");
 }
 save(world,catalog,out,"approved");write(out/"approved-page.json",query(world,"political_takeover",boundary::obj({{"place_id",V{"qingxi::place.south.camp"}}})));
}
}
// 此探针的终点是自然已占领且正式批准的原银行；生产/任务消费者由各域后半门继续核实。
int main(int argc,char** argv){fs::path out;A trace,pages;std::unique_ptr<ResidentReplaySession> world;try{
 const bool checkpoint=argc==5&&StableId(argv[1])=="--checkpoint";check(argc==4||checkpoint,"usage: political_takeover_journey [--checkpoint] PREPARED REQUEST_OR_BANK FRESH_OUTPUT");const int offset=checkpoint?1:0;out=argv[3+offset];check(!fs::exists(out),"fresh output required");fs::create_directories(out);auto catalog=read(argv[1+offset]);world=create_resident_replay_session();
 if(checkpoint){auto bank=read_bank(argv[3]);take(world->inspect_checkpoint_selection(bank),"original entry title");take(world->initialize_checkpoint(catalog,bank),"original north-gate bank");check(text(query(*world,"command_context"),"actor_id")==player,"bank keeps original player");check(text(query(*world,"journey_location"),"place_id")=="qingxi::place.north.gate","bank must naturally return to original north gate");auto action=query(*world,"action_context");const auto* id=action.find("action_id");check(!id||!id->as<StableId>()||id->as<StableId>()->empty(),"entry has no unfinished actor action");}
 else take(world->initialize_native(catalog,read(argv[2])),"formal authored Native initialization");
 save(*world,catalog,out,checkpoint?"original-entry":"initial");arrive(*world,trace);save(*world,catalog,out,"siege");
 bool occupied=false;for(int n=0;n<80;++n){auto page=query(*world,"siege_page");pages.push_back(page);check(!text(page,"siege_id").empty(),"real current siege");const auto& integration=field(page,"integration");auto status=integration.find("status");
  if(status&&status->as<StableId>()&&*status->as<StableId>()=="pending"){bool accepted=true;for(const auto& officer:boundary::array(field(integration,"officers"),"officers"))accepted&=text(officer,"decision")=="accept";if(accepted){check(flag(field(integration,"asset_quote"),"ok"),"actual accepted integration asset quote");save(*world,catalog,out,"before-occupation");auto params=boundary::obj({{"proposal_id",field(field(integration,"proposal"),"id")},{"proposal_key",field(field(integration,"proposal"),"proposal_key")}});write(out/"occupation-command.json",boundary::obj({{"kind",V{"siege_integration_apply"}},{"params",params}}));success(*world,"siege_integration_apply",params,trace);occupied=true;break;}}
  else if(status&&status->as<StableId>()&&*status->as<StableId>()=="rejected")throw std::runtime_error("actual officer refused integration");
  else {auto params=boundary::obj({{"siege_id",field(page,"siege_id")},{"command",boundary::obj({{"kind",V{"negotiate"}},{"terms",V{"integrate"}}})}});auto quote=operation(*world,"siege_strategy",params,trace,true);if(flag(quote,"ok")){success(*world,"siege_strategy",params,trace);continue;}const auto code=text(quote,"code");check(code=="SiegeNegotiationNotReady"||code=="SiegeTermsRejected"||code=="ParticipantBusy"||code=="ActorBusy","unexpected real negotiation refusal "+code);}
  blockade(*world,page,trace);
 }
 check(occupied,"natural occupation reached within original siege deadline");auto actual=take(world->export_state(),"occupied actual state");bool applied=false;for(const auto& row:boundary::object(field(actual,"facts"),"facts")){const auto* kind=row.second.find("kind");if(kind&&kind->as<StableId>()&&*kind->as<StableId>()=="military_occupation"&&text(row.second,"stronghold_id")=="qingxi::stronghold.south"){check(!applied,"one actual occupation");applied=flag(row.second,"assets_applied");}}check(applied,"real military occupation applied, not merely integration DTO");
 save(*world,catalog,out,"pending");auto page=query(*world,"siege_page");success(*world,"siege_close",boundary::obj({{"siege_id",field(page,"siege_id")}}),trace);approve(*world,catalog,out,trace,pages);write(out/"commands.json",V{trace});write(out/"pages.json",V{pages});write(out/"result.json",boundary::obj({{"ok",V{true}},{"checks",V{Integer(checks)}},{"scope",V{"real occupation and approval; consumers remain separate"}}}));std::cout<<"POLITICAL_TAKEOVER_JOURNEY checks="<<checks<<" failures=0\n";return 0;
 }catch(const std::exception& error){if(!out.empty()&&fs::exists(out)){write(out/"commands.json",V{trace});write(out/"pages.json",V{pages});write(out/"failure.json",boundary::obj({{"ok",V{false}},{"checks",V{Integer(checks)}},{"error",V{StableId(error.what())}}}));if(world){auto bank=world->checkpoint();if(bank.ok)write(out/"failure.hmcb",bank.value);auto state=world->export_state();if(state.ok)write(out/"failure-state.json",state.value);}}std::cerr<<"POLITICAL_TAKEOVER_JOURNEY_FAIL "<<error.what()<<'\n';return 1;}}
