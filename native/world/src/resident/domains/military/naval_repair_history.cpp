// 工单不容许跨编组、接管或战斗偷补船损；每次完工只修改原船的一个完整前后值。
#include "naval_repair_history.hpp"
#include "naval_repair_codec.hpp"
#include "military_field_command_source.hpp"
#include "resident/storage/common_codec.hpp"
#include <algorithm>
#include <charconv>
#include <set>
namespace hanmo::resident {
namespace {
// 事实先后只按 Common 原顺序；缺头不能按ID字符串或当前对象补来源。
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& row:rows)if(row.id==id)return &row;return nullptr;}
bool precedes(const CommonStore& c,const StableId& a,const StableId& b){const auto& order=c.state().facts.order();auto x=std::find(order.begin(),order.end(),a),y=std::find(order.begin(),order.end(),b);return x!=order.end()&&y!=order.end()&&x<y;}
bool same(const Location& a,const Location& b){return compare_boundary(boundary::encode_location(a),boundary::encode_location(b)).values_equal;}
const NavalRepairResult* result_for(const NavalState& n,const StableId& id){for(const auto& result:n.repairs.results)if(result.action_id==id)return &result;return nullptr;}
bool aboard(const NavalAssetState& assets,const NavalFleetRecord& f,const StableId& inventory){for(const auto& binding:f.assignments){const auto* vessel=find(assets.vessels,binding.vessel_id);if(vessel&&vessel->cargo_inventory_id==inventory)return true;if(std::find(binding.extra_inventory_ids.begin(),binding.extra_inventory_ids.end(),inventory)!=binding.extra_inventory_ids.end())return true;}return false;}
// 当前事件之前尚未关闭的工单，用于拒绝行动占用期间的零时转船或交接。
const NavalRepairFact* working_before(const CommonStore& c,const NavalState& n,const StableId& fleet,const StableId& event){const NavalRepairFact* found=nullptr;for(const auto& start:n.repairs.starts)if(start.terms.fleet_id==fleet&&precedes(c,start.id,event)){const auto* end=result_for(n,start.id);if(!end||!precedes(c,end->id,event)){if(found)return &start;found=&start;}}return found;}
}
// 热开工核定原位置和人手；本口严格重建原实船产权、编组、兵员和工法，而非信任terms自述。
Status validate_naval_repair_history_event(const CommonStore& c,const MilitaryState& military,const MilitaryCatalog& commands,const MilitaryAccess& access,const NavalState& n,const NavalCatalog& catalog,const NavalAssetState& before,const StableId& kind,const StableId& id){
 if(!catalog.repair_policy)return (kind=="naval_repair_start"||kind=="naval_repair_result"||!n.repairs.starts.empty()||!n.repairs.results.empty())?common_failure("NavalRepairProfileRequired",id):common_success();
 if(kind=="naval_repair_start"){
  const auto* fact=find(n.repairs.starts,id);if(!fact||!access.rosters_before)return common_failure("NavalRepairHistoryReadRequired",id);const auto& t=fact->terms;const auto* fleet=find(before.fleets,t.fleet_id);const auto* vessel=find(before.vessels,t.vessel_id);const auto* army=find(military.armies,t.army_id);const auto* rule=find(catalog.repair_policy->rules,t.rule_id);const auto* hull=vessel?find(catalog.hulls,vessel->hull_id):nullptr;
  if(!fleet||!vessel||!army||!rule||!hull||fleet->status!="active"||fleet->army_id!=t.army_id||fleet->commander_actor_id!=t.actor_id||vessel->fleet_id!=fleet->id||vessel->hull_id!=rule->hull_id||army->formation_source!=t.formation_source_id||!owner_equal(vessel->owner,t.owner)||t.owner.kind!=Owner::Kind::organization||t.owner.id!=army->organization_id||!aboard(before,*fleet,t.inventory_id)||working_before(c,n,fleet->id,id))return common_failure("NavalRepairHistorySourceMismatch",id);
  bool bound=false;for(const auto& binding:fleet->assignments)if(binding.vessel_id==vessel->id&&binding.unit_id==t.unit_id)bound=true;if(!bound)return common_failure("NavalRepairBindingHistoryMismatch",id);
  auto roster=access.rosters_before({t.unit_id},fact->tick,id);if(!roster.ok)return {false,{},roster.error};if(roster.value.size()!=1||roster.value.front().army_id!=army->id||roster.value.front().organization_id!=army->organization_id||roster.value.front().commander_actor_id!=t.captain_actor_id||roster.value.front().fit!=t.crew_ids||Integer(t.crew_ids.size())<rule->minimum_crew)return common_failure("NavalRepairCrewHistoryMismatch",id);
  auto commander=query_military_army_command_source_before(c,military,army->id,id);if(!commander.ok)return {false,{},commander.error};if(commander.value.commander_actor_id!=t.actor_id||commander.value.command_source.id!=t.command_source_id)return common_failure("NavalRepairCommandHistoryMismatch",id);
  auto members=military_army_actor_members_before(c,military,army->id,fact->tick,id);if(!members.ok)return {false,{},members.error};if(members.value!=t.participants)return common_failure("NavalRepairParticipantsHistoryMismatch",id);
  Integer limit=hull->max_integrity;if(const auto* at=std::get_if<WildernessLocation>(&t.anchor)){if(at->map_id!=catalog.world_map_id||!access.travel_catalog||!access.travel_services)return common_failure("NavalRepairAnchorHistoryMismatch",id);const auto* map=access.travel_catalog->maps.find(at->map_id);if(!map||!terrain::point_valid_prepared(*map,at->point,terrain::Mode::boat,access.travel_services->terrain_flags()))return common_failure("NavalRepairAnchorHistoryMismatch",id);limit=rule->afloat_integrity_limit;}
  else if(const auto* at=std::get_if<SettlementLocation>(&t.anchor)){bool dock=false;for(const auto& berth:catalog.berths)if(berth.place_id==at->place_id)dock=true;const auto* place=access.travel_catalog?access.travel_catalog->places.find(at->place_id):nullptr;if(!dock||!place||place->settlement_id!=at->settlement_id)return common_failure("NavalRepairAnchorHistoryMismatch",id);}else return common_failure("NavalRepairAnchorHistoryMismatch",id);
  if(vessel->integrity!=t.integrity_before||t.integrity_before>=limit||t.integrity_after!=t.integrity_before+std::min(rule->integrity_gain,limit-t.integrity_before)||t.duration!=rule->duration||t.materials!=rule->materials)return common_failure("NavalRepairTermsHistoryMismatch",id);
  return common_success();
 }
 if(kind=="naval_repair_result"){
  const auto* result=find(n.repairs.results,id);const auto* start=result?find(n.repairs.starts,result->action_id):nullptr;if(!result||!start||!precedes(c,start->id,id))return common_failure("NavalRepairResultSourceRequired",id);const auto& t=start->terms;const auto* vessel=find(before.vessels,t.vessel_id);const auto* fleet=find(before.fleets,t.fleet_id);if(!vessel||!fleet||fleet->status!="active"||vessel->fleet_id!=fleet->id||!owner_equal(vessel->owner,t.owner)||vessel->integrity!=t.integrity_before)return common_failure("NavalRepairHullPredecessorMismatch",id);
  if(result->reason=="NavalRepairContact"){const auto* contact=find(n.battle.contacts,result->interruption_source_id);if(!contact||contact->tick!=result->tick||!precedes(c,contact->id,id)||(contact->first_fleet_id!=fleet->id&&contact->second_fleet_id!=fleet->id))return common_failure("NavalRepairContactSourceRequired",id);const auto& anchor=contact->first_fleet_id==fleet->id?contact->first_anchor:contact->second_anchor;const auto& approach=contact->first_fleet_id==fleet->id?contact->first_approach:contact->second_approach;if(!approach.stationary||!same(t.anchor,Location{anchor}))return common_failure("NavalRepairContactAnchorMismatch",id);}
  else if(result->reason=="NavalRepairCompleted"||result->reason=="NavalRepairCrewChanged"){if(!access.rosters_before)return common_failure("NavalRepairHistoryReadRequired",id);auto roster=access.rosters_before({t.unit_id},result->tick,id);if(!roster.ok)return {false,{},roster.error};if(roster.value.size()!=1||((roster.value.front().fit==t.crew_ids)!=(result->reason=="NavalRepairCompleted")))return common_failure("NavalRepairCrewHistoryMismatch",id);}
  return common_success();
 }
 // 接敌可以打断工单，但不得带着未关闭工单继续战斗、转属、退编或替换统帅。
 std::set<StableId> fleets;if(kind=="naval_voyage"){const auto* f=find(n.navigation.voyages,id);if(f)fleets.insert(f->fleet_id);}else if(kind=="naval_navigation_step"){const auto* f=find(n.navigation.steps,id);if(f)fleets.insert(f->fleet_id);}else if(kind=="naval_disband"){for(const auto& f:n.fleets)if(f.release_source_id==id)fleets.insert(f.id);}else if(kind=="naval_capture"){const auto* f=find(n.battle.captures,id);if(f){fleets.insert(f->from_fleet_id);fleets.insert(f->to_fleet_id);}}else if(kind=="naval_round"){const auto* f=find(n.battle.rounds,id);if(f)for(const auto& b:f->before.bindings)fleets.insert(b.fleet_id);}else if(kind=="military_field_command_assigned"){const auto* f=find(military.field_commands,id);if(f)fleets.insert(f->target.fleet_id);}
 for(const auto& fleet:fleets)if(working_before(c,n,fleet,id))return common_failure("NavalRepairHistoryOccupationOverlap",id);
 return common_success();
}
// 每份Action预约只属于该工单的冻结物料；钱包与合同预留继续各自原主权。
Status validate_naval_repair_reservations(const CommonStore& c,const NavalState& naval){for(const auto& row:c.state().inventories.rows())for(const auto& lot:row.second.lots){if(lot.reserved_by.kind!=ReservationRef::Kind::action||lot.reserved_by.empty())continue;const auto* start=find(naval.repairs.starts,lot.reserved_by.id);if(!start)continue;bool material=false;for(const auto& item:start->terms.materials)if(item.first==lot.item_id)material=true;if(result_for(naval,start->id)||row.first!=start->terms.inventory_id||!owner_equal(lot.owner,start->terms.owner)||!material)return common_failure("NavalRepairUnexpectedReservation",lot.id);}for(const auto& row:c.state().wallets.rows())for(const auto& r:row.second.reserved)if(r.action_id.kind==ReservationRef::Kind::action&&find(naval.repairs.starts,r.action_id.id)){const NavalRepairChargeFact* fee=nullptr;for(const auto& charge:naval.repair_charges.charges)if(charge.action_id==r.action_id.id)fee=&charge;if(!fee||result_for(naval,r.action_id.id)||row.first!=fee->terms.payer_wallet_id||r.amount!=fee->terms.amount||!owner_equal(row.second.owner,fee->terms.payer_owner))return common_failure("NavalRepairUnexpectedMoneyReservation",row.first);}for(const auto& fee:naval.repair_charges.charges)if(!result_for(naval,fee.action_id)){const auto* payer=c.wallet(fee.terms.payer_wallet_id);Integer count=0;if(payer)for(const auto& r:payer->reserved)if(r.action_id==ReservationRef{fee.action_id}&&r.amount==fee.terms.amount)++count;if(count!=1)return common_failure("NavalRepairChargeReservationMismatch",fee.action_id);}return common_success();}
// 已终止工单同样有身份；有修缮头却无本域原件时拒绝，而不是退回普通行动解释。
Outcome<std::optional<NavalRepairActionSource>> query_naval_repair_action(const CommonStore& common,const NavalState& naval,const NavalCatalog& catalog,const StableId& action){const auto* start=find(naval.repairs.starts,action);const auto* head=common.state().facts.find(action);if(!start){if(head&&head->kind=="naval_repair_start")return {false,{},common_failure("NavalRepairActionSourceRequired",action).error};return {true,std::nullopt,{}};}if(!catalog.repair_policy||!head||head->kind!="naval_repair_start"||head->domain!="military_naval"||start->tick<0||start->tick>common.tick()||start->terms.duration<1||start->terms.duration>120||start->terms.actor_id.empty())return {false,{},common_failure("NavalRepairActionSourceRequired",action).error};const auto* result=result_for(naval,action);NavalRepairActionSource source{start->id,start->id,start->terms.actor_id,result?result->id:StableId{},start->tick,start->terms.duration};return {true,std::move(source),{}};}
// 公共完成回执只能引用本域唯一结果，精确匹配原本人/行动，不取latest或猜测已到达。
Outcome<NavalRepairReceipt> query_naval_repair_completion(const CommonStore& common,const NavalState& naval,const NavalCatalog& catalog,const StableId& id,const StableId& action,const StableId& actor){const auto* result=find(naval.repairs.results,id);const auto* start=result?find(naval.repairs.starts,result->action_id):nullptr;const auto* h=common.state().facts.find(id);const auto* sh=start?common.state().facts.find(start->id):nullptr;if(!catalog.repair_policy||!result||!start||result->action_id!=action||result->actor_id!=actor||start->terms.actor_id!=actor||!h||h->kind!="naval_repair_result"||h->domain!="military_naval"||!sh||sh->kind!="naval_repair_start"||sh->domain!="military_naval"||!precedes(common,start->id,id)||result->tick>common.tick())return {false,{},common_failure("NavalRepairCompletionSourceRequired",id).error};return {true,{id,action,actor,result->reason,result->tick,true,result->success,false},{}};}
// 缓存键不是业务幂等键；先核唯一共同提交，再按原分配序区分首次与重放。
Status validate_naval_repair_cached_receipt(const CommonStore& common,const NavalState& naval,const NavalCatalog& catalog,const StableId& key,const StableId& kind,const StableId& result_id,Integer tick,const NavalRepairReceipt& receipt){
 auto invalid=[&](const StableId& code){return common_failure(code,key);};
 auto ordinal=[](const StableId& id,const StableId& prefix)->Integer{if(!prefix.empty()&&id.rfind(prefix+":",0)!=0)return 0;const auto split=id.rfind(':');if(split==StableId::npos)return 0;const auto digits=id.substr(split+1);Integer value=0;const auto parsed=std::from_chars(digits.data(),digits.data()+digits.size(),value);return parsed.ec==std::errc{}&&parsed.ptr==digits.data()+digits.size()&&value>0&&std::to_string(value)==digits?value:0;};
 auto checked=naval_repair_codec::validate_receipt(receipt,kind);if(!checked.ok)return checked;
 const auto split=key.rfind(':');if(split==StableId::npos)return invalid("NavalRepairCachedKeyMismatch");const auto actor=key.substr(0,split);const auto request=ordinal(key,{});const auto* sequence=common.state().request_sequences.find(actor);if(!request||!sequence||request>*sequence||actor!=receipt.actor_id)return invalid("NavalRepairCachedKeyMismatch");
 const HistoryRecord* selected=nullptr;const HistoryRecord* previous=nullptr;const HistoryRecord* before=nullptr;
 for(const auto& history:common.state().history){if(history.result_id==result_id){if(selected)return invalid("NavalRepairCachedHistoryMismatch");selected=&history;before=previous;}previous=&history;}
 if(!selected||selected->actor_id!=actor||selected->kind!=kind||selected->tick!=tick||tick<0||tick>common.tick())return invalid("NavalRepairCachedHistoryMismatch");
 const auto upper=ordinal(result_id,"result"),lower=before?ordinal(before->result_id,"result"):0;
 if(!upper||(before&&!lower)||upper<=lower||upper>=common.state().next_instance_id)return invalid("NavalRepairCachedAllocationMismatch");
 auto action=query_naval_repair_action(common,naval,catalog,receipt.action_id);if(!action.ok)return {false,{},action.error};if(!action.value||action.value->actor_id!=actor)return invalid("NavalRepairCachedSourceMismatch");
 const auto* start=find(naval.repairs.starts,receipt.action_id);const auto* end=result_for(naval,receipt.action_id);
 if(!start||!naval_request_key_valid(start->request_key)||start->tick>tick||!ordinal(start->id,"action")||ordinal(start->id,"action")>=upper)return invalid("NavalRepairCachedSourceMismatch");
 // 旧回执按本次结果之前的终态重建，后来才完工不能使早期未完成重放失效。
 const bool ended_before=end&&ordinal(end->id,"naval_repair")>0&&ordinal(end->id,"naval_repair")<upper;
 if(receipt.completed){if(!ended_before)return invalid("NavalRepairCachedSourceMismatch");auto completed=query_naval_repair_completion(common,naval,catalog,receipt.source_id,receipt.action_id,actor);if(!completed.ok)return {false,{},completed.error};if(completed.value.tick!=receipt.tick||completed.value.reason!=receipt.reason||completed.value.success!=receipt.success||receipt.tick>tick)return invalid("NavalRepairCachedSourceMismatch");}
 else if(ended_before||receipt.source_id!=start->id||receipt.tick!=start->tick)return invalid("NavalRepairCachedSourceMismatch");
 if(kind=="naval_repair_cancel"&&(!end||!naval_request_key_valid(end->request_key)||end->reason!="NavalRepairCancelled"))return invalid("NavalRepairCachedSourceMismatch");
 const auto source=ordinal(receipt.source_id,receipt.completed?"naval_repair":"action");
 if(!source)return invalid("NavalRepairCachedAllocationMismatch");
 if(!receipt.replayed){if(receipt.tick!=tick||source<=lower||source>=upper)return invalid("NavalRepairCachedAllocationMismatch");}
 else{
  // 先前工单可来自NPC且最早命令history可能已裁剪；永久业务事实仍须在本次前界存在。
  const auto origin=kind=="naval_repair_start"?ordinal(start->id,"action"):ordinal(end->id,"naval_repair");
  if(!before||origin>lower||source>lower)return invalid("NavalRepairCachedReplayMismatch");
 }
 return common_success();
}

}
