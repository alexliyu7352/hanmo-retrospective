#include "resident/domains/military/military_model.hpp"
// 有界只读投影消除已俘部队的重复登船占用；不能凭空漏掉仍有人的单位或其船长。
#include "naval_manifest_history.hpp"
#include "resident/storage/common_store.hpp"
#include <set>
#include <algorithm>
namespace hanmo::resident {
namespace {
// 只读取实际对象，缺失来源拒绝；不构造替代军团用于存储。
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& r:rows)if(r.id==id)return &r;return nullptr;}
bool has(const std::vector<StableId>& rows,const StableId& id){return std::find(rows.begin(),rows.end(),id)!=rows.end();}
}
// 验证同一活动行旅在原编舰之后开始；只读回溯换船，不拿当前战斗占用或船损重批旧出航。
Outcome<std::optional<NavalTravelProof>> query_naval_journey_source(const CommonStore& common,const MilitaryState& military,const NavalAssetState& naval,const NavalAssetCatalog& ships,const TravelState& travel,const StableId& actor,const StableId& action_id,const StableId& plan_id,const NavalBattleState* history){
 auto occupied=query_naval_location_source(common,military,naval,ships,actor);if(!occupied.ok)return {false,{},occupied.error};if(!occupied.value)return {true,std::nullopt,{}};const auto& source=*occupied.value;const auto* army=find(military.armies,source.army_id);const auto* fleet=find(naval.fleets,source.fleet_id);const auto* person=common.actor(actor);const auto* action=common.action(action_id);const auto* params=travel.actions.find(action_id);const auto* plan=travel.journey_facts.find(plan_id);const auto* header=common.state().facts.find(plan_id);
 auto fail=[&](const StableId& code)->Outcome<std::optional<NavalTravelProof>>{return {false,{},common_failure(code,plan_id).error};};
 if(!army||!fleet||!person||!action||person->action_id!=action_id||action->actor_id!=actor||action->kind!="travel"||!params||params->plan_id!=plan_id||params->mode!="boat"||!plan||plan->actor_id!=actor||plan->action_id!=action_id||plan->mode!="boat"||plan->map_id!=source.world_map_id||plan->starting_load!=0||plan->tick<action->start_tick||plan->tick>common.tick()||!header||header->kind!="terrain_journey"||header->domain!="travel")return fail("NavalJourneySourceRequired");
 const auto& order=common.state().facts.order();auto position=[&](const StableId& id){return std::find(order.begin(),order.end(),id);};const auto at=position(plan_id),formation=position(source.formation_source_id);if(at==order.end()||formation==order.end()||formation>=at)return fail("NavalJourneyFormationOrderMismatch");auto assignments=fleet->assignments;
 auto same=[](const std::vector<NavalVesselAssignment>& a,const std::vector<NavalVesselAssignment>& b){if(a.size()!=b.size())return false;for(std::size_t i=0;i<a.size();++i)if(a[i].vessel_id!=b[i].vessel_id||a[i].unit_id!=b[i].unit_id||a[i].passenger_ids!=b[i].passenger_ids||a[i].prisoner_ids!=b[i].prisoner_ids||a[i].extra_inventory_ids!=b[i].extra_inventory_ids)return false;return true;};
 if(history)for(auto i=history->captures.rbegin();i!=history->captures.rend();++i){if(i->to_fleet_id!=fleet->id&&i->from_fleet_id!=fleet->id)continue;auto captured=position(i->id);const auto* fact=common.state().facts.find(i->id);if(captured==order.end()||!fact||fact->kind!="naval_capture"||fact->domain!="military_naval")return fail("NavalJourneyCaptureSourceRequired");if(captured<at)continue;const auto& after=i->to_fleet_id==fleet->id?i->after_assignments:i->from_after_assignments;const auto& before=i->to_fleet_id==fleet->id?i->before_assignments:i->from_before_assignments;if(!same(assignments,after))return fail("NavalJourneyCaptureChainMismatch");assignments=before;}
 if(assignments.empty())return fail("NavalJourneyFleetSourceRequired");Integer budget=INT64_MAX;std::set<StableId> seen;for(const auto& a:assignments){const auto* vessel=find(naval.vessels,a.vessel_id);const auto* authored=find(ships.vessels,a.vessel_id);const auto* hull=authored?find(ships.hulls,authored->hull_id):nullptr;if(!vessel||!authored||!hull||!seen.insert(a.vessel_id).second||vessel->hull_id!=authored->hull_id||hull->travel_budget<1)return fail("NavalJourneyHullSourceRequired");budget=std::min(budget,hull->travel_budget);}
 const auto* wallet=common.wallet(army->wallet_id);if(!wallet||wallet->owner.kind!=Owner::Kind::organization||wallet->owner.id!=army->organization_id)return fail("NavalJourneyPayerSourceRequired");NavalTravelProof proof;proof.fleet_id=fleet->id;proof.army_id=army->id;proof.commander_actor_id=actor;proof.payer_wallet_id=wallet->id;proof.travel_budget=budget;return {true,std::move(proof),{}};
}
// 原observe负责人物位置和Common重量；这里仅先验证“哪些原单位已真实离开这支舰队”。
static Outcome<MilitaryState> remaining_roster(const CommonRead& common,const MilitaryState& military,const NavalBattleState& battles,const NavalFleetRecord& fleet){
 const auto* original=find(military.armies,fleet.army_id);if(!original)return {false,{},common_failure("NavalActiveArmyMismatch",fleet.id).error};auto selected=military;
 for(auto& army:selected.armies)if(army.id==fleet.army_id){std::vector<StableId> present;for(const auto& id:army.unit_ids){bool assigned=false;for(const auto& a:fleet.assignments)if(a.unit_id==id)assigned=true;if(assigned){present.push_back(id);continue;}
  const auto* unit=find(military.military_units,id);bool captured=false;for(const auto& fact:battles.captures)if(fact.from_fleet_id==fleet.id){const auto* round=find(battles.rounds,fact.round_source_id);if(round)for(const auto& binding:round->after.bindings)if(binding.vessel_id==fact.vessel_id&&binding.unit_id==id)captured=true;}
  const auto* captain=unit?common.actor(unit->commander_actor_id):nullptr;
  if(!unit||unit->army_id!=army.id||!unit->fit.empty()||!unit->wounded.empty()||!captured||has(army.member_actor_ids,unit->commander_actor_id)||(captain&&captain->alive&&captain->available))return {false,{},common_failure("NavalOmittedUnitSourceRequired",id).error};
 }
 army.unit_ids=std::move(present);
 }
 return {true,std::move(selected),{}};
}
// 临时投影只过滤由原捕获事实移走的单位，返回Common原载荷观察，不可保存为第二军团。
Outcome<NavalFormationObservation> observe_naval_combat_manifest(const CommonRead& common,const MilitaryState& military,const NavalAssetState& naval,const NavalBattleState& battles,const NavalFleetRecord& fleet){auto selected=remaining_roster(common,military,battles,fleet);if(!selected.ok)return {false,{},selected.error};return observe_naval_manifest(common,selected.value,naval,fleet,battles);}
Outcome<NavalFormationQuote> validate_naval_combat_manifest(const CommonRead& common,const MilitaryState& military,const NavalState& naval,const NavalCatalog& ships,const NavalFleetRecord& fleet){if(!ships.combat_policy)return {false,{},common_failure("NavalCombatProfileRequired").error};auto observation=observe_naval_combat_manifest(common,military,naval,naval.battle,fleet);if(!observation.ok)return {false,{},observation.error};return preview_naval_formation(naval,ships,observation.value,{fleet.army_id,fleet.assignments},true,true);}
// 返航仍经过原任职/船员技艺/共同Action和每船容量守卫；真实损伤或权源失效仍拒绝。
Outcome<std::optional<NavalTravelProof>> preview_naval_combat_travel(const CommonRead& common,const MilitaryState& military,const MilitaryCatalog& commands,const MilitaryAccess& access,const NavalState& naval,const NavalCatalog& ships,const StableId& actor){
 if(!ships.combat_policy)return {false,{},common_failure("NavalCombatProfileRequired").error};const NavalFleetRecord* fleet=nullptr;for(const auto& f:naval.fleets)if(f.status=="active"&&f.commander_actor_id==actor){if(fleet)return {false,{},common_failure("NavalCommanderAssignedTwice",actor).error};fleet=&f;}if(!fleet)return preview_naval_travel(common,military,commands,access,naval,ships,actor,true);
 auto selected=remaining_roster(common,military,naval.battle,*fleet);if(!selected.ok)return {false,{},selected.error};return preview_naval_travel(common,selected.value,commands,access,naval,ships,actor,true);
}

}
