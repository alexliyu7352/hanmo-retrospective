#include "military_supply_lookup.hpp"
// 先核完整域候选，随后同一Common候选一次落资源、真人后果和事实，最后替换域状态。
#include "naval_battle_effects.hpp"
#include "battle_world.hpp"
#include "military_actor_condition_write.hpp"
#include "naval_plan_command.hpp"
#include "naval_round_supply.hpp"
#include "naval_casualties.hpp"
#include "resident/model/common_operations.hpp"
#include "naval_gunnery_source.hpp"
namespace hanmo::resident {
namespace {
template<class T>Outcome<T> fail(const StableId& code,const StableId& path={}){return {false,{},common_failure(code,path).error};}
const NavalBattleRecord* battle(const NavalBattleState& s,const StableId& id){for(const auto& b:s.battles)if(b.frame.id==id)return &b;return nullptr;}
}
// 同业务键先还原原事实，无新ID、资源转移、随机数或行程终止；新请求才执行资源recipe。
Outcome<NavalCaptureFact> apply_naval_capture(CommonWrite& common,MilitaryState& military,const MilitaryCatalog& catalog,const MilitaryAccess& access,NavalState& naval,const NavalCatalog& ships,const StableId& actor,const NavalCaptureCommand& command,const NavalCapturedJourneyEnd& end_journey,const NavalCaptureAssetBoundary& boundary,const MilitaryTravelEndWithSource& end_with_source){
 for(const auto& previous:naval.battle.captures)if(previous.actor_id==actor&&previous.request_key==command.request_key){auto replay=settle_naval_capture_candidate(common,military,catalog,naval,ships,actor,command,previous.id);if(!replay.ok)return {false,{},replay.error};return {true,replay.value.fact,{}};}
 if(!boundary.before_transfer||!boundary.after_transfer)return fail<NavalCaptureFact>("NavalCaptureAssetBoundaryRequired");
 auto quote=preview_naval_capture(common,military,catalog,naval,ships,naval.battle,actor,command);if(!quote.ok)return {false,{},quote.error};if(!quote.value.interrupted_action_ids.empty()&&!end_journey&&!end_with_source)return fail<NavalCaptureFact>("NavalJourneyTerminationRequired");
 auto id=common.next_id("naval_capture");if(!id.ok)return {false,{},id.error};auto prepared=settle_naval_capture_candidate(common,military,catalog,naval,ships,actor,command,id.value);if(!prepared.ok)return {false,{},prepared.error};auto& next=prepared.value;
 // 原伤亡收据使用共同新 ID；接管来源先写，人口来源随后写，晚失败一并回滚。
 auto casualty_id=common.next_id("military");if(!casualty_id.ok)return {false,{},casualty_id.error};const NavalRoundFact* round_source=nullptr;for(const auto& r:naval.battle.rounds)if(r.id==next.fact.round_source_id)round_source=&r;std::vector<StableId> units;if(round_source)for(const auto& binding:round_source->after.bindings)if(binding.vessel_id==next.fact.vessel_id)units.push_back(binding.unit_id);auto casualties=make_naval_casualty_receipt(next.military,casualty_id.value,id.value,next.fact.tick,units,next.actor_changes);if(!casualties.ok)return {false,{},casualties.error};next.fact.casualty_source_id=casualty_id.value;for(auto& capture:next.naval.battle.captures)if(capture.id==id.value)capture.casualty_source_id=casualty_id.value;
 auto checked=boundary.before_transfer(next.fact,next.resources);if(!checked.ok)return {false,{},checked.error};
 checked=common.apply_resources(next.resources);if(!checked.ok)return {false,{},checked.error};
 for(const auto& action:next.interrupted_action_ids){checked=end_with_source?end_with_source(action,{MilitaryTravelEndingSource::Kind::naval_capture,id.value}):end_journey(action);if(!checked.ok)return {false,{},checked.error};}
 checked=apply_military_actor_changes(common,next.military,catalog,access,next.actor_changes);if(!checked.ok)return {false,{},checked.error};checked=common.append_fact({id.value,"naval_capture","military_naval"});if(!checked.ok)return {false,{},checked.error};
 checked=common.append_fact({casualty_id.value,"military_casualties","military"});if(!checked.ok)return {false,{},checked.error};next.military.casualty_facts.push_back(std::move(casualties.value));auto fact=next.fact;invalidate_military_supply_lookup(military);military=std::move(next.military);naval=std::move(next.naval);checked=finish_military_actor_condition_sources(common,military,access,{casualty_id.value});if(!checked.ok)return {false,{},checked.error};
 // 回调读取已真实applied的同候选来源；它失败时中央须丢弃Common和全部域，不能只退回船账。
 checked=boundary.after_transfer(fact);if(!checked.ok)return {false,{},checked.error};return {true,std::move(fact),{}};
}
// 原半日阶段只调用一次；已处于结算页的战场不能再扣血，玩家草案也不能在此重新生成AI。
Outcome<NavalRoundFact> apply_naval_round(CommonWrite& common,MilitaryState& military,const MilitaryCatalog& catalog,const MilitaryAccess& access,NavalState& naval,const NavalCatalog& ships,const StableId& battle_id,Integer finish_tick,const NavalCapturedJourneyEnd& end_journey,const MilitaryTravelEndWithSource& end_with_source){
 if(!ships.combat_policy)return fail<NavalRoundFact>("NavalCombatProfileRequired");const auto* source=battle(naval.battle,battle_id);if(!source)return fail<NavalRoundFact>("UnknownNavalBattle",battle_id);
 // 必须先过已冻结完整命令门，再分配共同事实号；ID后的任何晚失败仍归外层整世界回滚。
 auto plan=validate_naval_pending_plan(naval,*source);if(!plan.ok)return {false,{},plan.error};auto orders=frozen_naval_orders(naval,ships,*ships.combat_policy,catalog.battle_catalog,*source,naval_gunnery_preparation(naval.gunnery,source->frame.id));if(!orders.ok)return {false,{},orders.error};auto id=common.next_id("naval_round");if(!id.ok)return {false,{},id.error};auto fact=make_naval_supplied_round(military,naval,ships,*ships.combat_policy,catalog.battle_catalog,*source,id.value,finish_tick,naval_gunnery_preparation(naval.gunnery,source->frame.id));if(!fact.ok)return {false,{},fact.error};auto prepared=settle_naval_round_candidate(common,military,naval,ships,catalog.battle_catalog,fact.value,finish_tick);if(!prepared.ok)return {false,{},prepared.error};auto& next=prepared.value;
 auto casualty_id=common.next_id("military");if(!casualty_id.ok)return {false,{},casualty_id.error};std::vector<StableId> units;for(const auto& unit:fact.value.after.units)units.push_back(unit.unit_id);auto casualties=make_naval_casualty_receipt(next.military,casualty_id.value,id.value,finish_tick,units,next.actor_changes);if(!casualties.ok)return {false,{},casualties.error};fact.value.casualty_source_id=casualty_id.value;for(auto& round:next.naval.battle.rounds)if(round.id==id.value)round.casualty_source_id=casualty_id.value;
 // 原领航者死亡先终止整条共同行动；部将死亡仍由原 ActorChange 仅解除本人参与。
 for(const auto& change:next.actor_changes)if(change.status=="dead"){const auto* actor=common.actor(change.actor_id);const auto* action=actor&&!actor->action_id.empty()?common.action(actor->action_id):nullptr;if(action&&action->actor_id==change.actor_id){if(action->kind!="travel"||(!end_journey&&!end_with_source))return fail<NavalRoundFact>("NavalJourneyTerminationRequired",change.actor_id);auto stopped=end_with_source?end_with_source(action->id,{MilitaryTravelEndingSource::Kind::naval_round,id.value}):end_journey(action->id);if(!stopped.ok)return {false,{},stopped.error};}}
 auto checked=apply_military_actor_changes(common,next.military,catalog,access,next.actor_changes);if(!checked.ok)return {false,{},checked.error};checked=common.append_fact({id.value,"naval_round","military_naval"});if(!checked.ok)return {false,{},checked.error};checked=common.append_fact({casualty_id.value,"military_casualties","military"});if(!checked.ok)return {false,{},checked.error};next.military.casualty_facts.push_back(std::move(casualties.value));invalidate_military_supply_lookup(military);military=std::move(next.military);naval=std::move(next.naval);checked=finish_military_actor_condition_sources(common,military,access,{casualty_id.value});if(!checked.ok)return {false,{},checked.error};return fact;
}
// 明确放弃只登记同一事实序列；分配ID后的拒绝仍由外层共同savepoint回滚。
Outcome<NavalClaimDeclineFact> apply_naval_decline(CommonWrite& common,const MilitaryState& military,NavalState& naval,const StableId& actor,const NavalClaimDeclineCommand& command){
 for(const auto& previous:naval.battle.declines)if(previous.actor_id==actor&&previous.request_key==command.request_key){auto replay=settle_naval_decline_candidate(common,military,naval,actor,command,previous.id);if(!replay.ok)return {false,{},replay.error};return {true,replay.value.fact,{}};}
 auto id=common.next_id("naval_decline");if(!id.ok)return {false,{},id.error};auto prepared=settle_naval_decline_candidate(common,military,naval,actor,command,id.value);if(!prepared.ok)return {false,{},prepared.error};auto checked=common.append_fact({id.value,"naval_decline","military_naval"});if(!checked.ok)return {false,{},checked.error};auto fact=prepared.value.fact;naval=std::move(prepared.value.naval);return {true,std::move(fact),{}};
}
// 继续只消耗已落半日的结算；合法原行程继续，失能行程由原Travel终止口保留锚点。
Outcome<NavalContinueFact> apply_naval_continue(CommonWrite& common,const MilitaryState& military,const MilitaryBattleCatalog& military_catalog,NavalState& naval,const NavalCatalog& catalog,const StableId& actor,const NavalContinueCommand& command,const NavalCapturedJourneyEnd& end_journey,const MilitaryTravelEndWithSource& end_with_source){
 for(const auto& previous:naval.battle.continuations)if(previous.actor_id==actor&&previous.request_key==command.request_key){auto replay=settle_naval_continue_candidate(common,military,military_catalog,naval,catalog,actor,command,previous.id);if(!replay.ok)return {false,{},replay.error};return {true,replay.value.fact,{}};}
 auto id=common.next_id("naval_continue");if(!id.ok)return {false,{},id.error};auto prepared=settle_naval_continue_candidate(common,military,military_catalog,naval,catalog,actor,command,id.value);if(!prepared.ok)return {false,{},prepared.error};if(!prepared.value.fact.interrupted_action_ids.empty()&&!end_journey&&!end_with_source)return fail<NavalContinueFact>("NavalJourneyTerminationRequired");for(const auto& action:prepared.value.fact.interrupted_action_ids){auto checked=end_with_source?end_with_source(action,{MilitaryTravelEndingSource::Kind::naval_continue,id.value}):end_journey(action);if(!checked.ok)return {false,{},checked.error};}auto checked=common.append_fact({id.value,"naval_continue","military_naval"});if(!checked.ok)return {false,{},checked.error};auto fact=prepared.value.fact;naval=std::move(prepared.value.naval);return {true,std::move(fact),{}};
}

}
