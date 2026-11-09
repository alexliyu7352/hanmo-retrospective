#include "resident/domains/military/military_model.hpp"
// 只在同一完整候选登记海战回合结果，不复制人口账，也不借陆战溃退制造逃兵或俘虏。
#include "naval_settlement.hpp"
#include "naval_battle_codec.hpp"
#include "naval_round_supply.hpp"
#include "naval_casualties.hpp"
#include "resident/storage/common_codec.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {
namespace {
// 查找只返回已有权威对象，缺项直接拒绝，不能用默认对象补齐来源。
template<class T>T* find(std::vector<T>& rows,const StableId& id){for(auto& r:rows)if(r.id==id)return &r;return nullptr;}
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& r:rows)if(r.id==id)return &r;return nullptr;}
const BattleUnit* unit(const NavalTacticalFrame& f,const StableId& id){for(const auto& u:f.units)if(u.unit_id==id)return &u;return nullptr;}
NavalBattleRecord* battle(NavalBattleState& s,const StableId& id){for(auto& b:s.battles)if(b.frame.id==id)return &b;return nullptr;}
template<class T>Outcome<T> fail(const StableId& code,const StableId& path={}){return {false,{},common_failure(code,path).error};}
bool has(const std::vector<StableId>& rows,const StableId& id){return std::find(rows.begin(),rows.end(),id)!=rows.end();}
// 原部队名册必须恰好等于本轮前序；不能用回合事实覆盖后来训练、增补或另一场战斗。
bool same_roster(const MilitaryUnitRecord& u,const BattleUnit& b){return u.id==b.unit_id&&u.army_id==b.army_id&&u.commander_actor_id==b.commander_actor_id&&u.troop_type_id==b.troop_type_id&&u.member_ids==b.member_ids&&u.fit==b.fit&&u.wounded==b.wounded&&u.dead==b.dead&&u.captured==b.captured&&u.deserted==b.deserted&&u.morale==b.morale;}
}
// 先核全部原始单位、船损与重放，再在值拷贝中统一落账；真人修改清单仍沿原MilitaryActorChange。
Outcome<NavalRoundSettlement> settle_naval_round_candidate(const CommonRead& common,const MilitaryState& military,const NavalState& naval,const NavalCatalog& ships,const MilitaryBattleCatalog& catalog,const NavalRoundFact& fact,Integer finish_tick){
 if(!ships.combat_policy)return fail<NavalRoundSettlement>("NavalCombatProfileRequired");
 if(const auto* previous=find(naval.battle.rounds,fact.id)){
  try{if(!compare_boundary(naval_battle_codec::encode_round(*previous),naval_battle_codec::encode_round(fact)).values_equal)return fail<NavalRoundSettlement>("NavalRoundReplayMismatch",fact.id);}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}
  return {true,{military,naval,{},true},{}};
 }
 NavalRoundSettlement next{military,naval,{},false};auto* current=battle(next.naval.battle,fact.battle_id);if(!current)return fail<NavalRoundSettlement>("UnknownNavalBattle",fact.battle_id);
 if(common.tick()==std::numeric_limits<Integer>::max()||finish_tick!=common.tick()+1||fact.tick!=finish_tick)return fail<NavalRoundSettlement>("NavalRoundWorldClockMismatch",fact.id);
 auto checked=validate_naval_supplied_round(military,naval,ships,*ships.combat_policy,catalog,*current,fact);if(!checked.ok)return {false,{},checked.error};
 for(const auto& before:fact.before.units){auto* original=find(next.military.military_units,before.unit_id);const auto* after=unit(fact.after,before.unit_id);auto* army=original?find(next.military.armies,original->army_id):nullptr;
  if(!original||!after||!army||army->status!="active"||army->last_supply_tick!=finish_tick||!same_roster(*original,before))return fail<NavalRoundSettlement>("NavalRoundRosterChanged",before.unit_id);
  if(after->captured!=before.captured||after->deserted!=before.deserted||after->member_ids!=before.member_ids)return fail<NavalRoundSettlement>("NavalRoundUnexpectedCustody",before.unit_id);
  original->fit=after->fit;original->wounded=after->wounded;original->dead=after->dead;original->morale=after->morale;
 }
 auto changes=naval_round_actor_changes(common,fact.before,fact.after);if(!changes.ok)return {false,{},changes.error};next.actor_changes=std::move(changes.value);
 for(const auto& change:next.actor_changes){const auto* person=common.actor(change.actor_id);if(!person||!person->alive)return fail<NavalRoundSettlement>("NavalCasualtyAlreadyDead",change.actor_id);if(change.status=="dead"){auto* army=find(next.military.armies,change.army_id);if(!army||!has(army->member_actor_ids,change.actor_id))return fail<NavalRoundSettlement>("NavalCasualtyArmySourceRequired",change.actor_id);army->member_actor_ids.erase(std::remove(army->member_actor_ids.begin(),army->member_actor_ids.end(),change.actor_id),army->member_actor_ids.end());}}
 for(const auto& hull:fact.hull_changes){auto* vessel=find(next.naval.vessels,hull.vessel_id);if(!vessel||vessel->integrity!=hull.before||hull.after<0||hull.after>hull.before)return fail<NavalRoundSettlement>("NavalHullPredecessorMismatch",hull.vessel_id);vessel->integrity=hull.after;}
 current->frame=fact.after;current->last_tick=fact.tick;current->rng_state=fact.rng_after;current->round_source_ids.push_back(fact.id);current->ai_orders.clear();current->pending_orders.clear();current->pending_plan_source.clear();next.naval.battle.rounds.push_back(fact);
 return {true,std::move(next),{}};
}
}
