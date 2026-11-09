// 只消费同军同半日的原 military_supply；缺口和重复来源一律拒绝，不能用默认满粮绕过。
#include "naval_round_supply.hpp"
#include "military_supply_attrition_rules.hpp"
#include "naval_battle_codec.hpp"
#include "resident/storage/common_codec.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace hanmo::resident {
namespace {
template<class T>Outcome<T> fail(const StableId& code,const StableId& path={}){return {false,{},common_failure(code,path).error};}
}
// 原供给已由 Military 全账验证；此处追加战场本部与当轮真实就餐名单绑定。
Outcome<NavalSuppliedBattle> naval_round_supply_source(const MilitaryState& military,const NavalBattleRecord& source,Integer tick){
 if(source.last_tick<0||source.last_tick==std::numeric_limits<Integer>::max()||tick!=source.last_tick+1)return fail<NavalSuppliedBattle>("InvalidNavalRoundClock",source.frame.id);
 NavalSuppliedBattle out;out.battle=source;std::set<StableId> armies;for(const auto& unit:source.frame.units)armies.insert(unit.army_id);
 for(const auto& army_id:armies){const MilitarySupplyFact* supply=nullptr;const ArmyRecord* army=nullptr;for(const auto& row:military.armies)if(row.id==army_id)army=&row;
  for(const auto& row:military.supply_facts)if(row.army_id==army_id&&row.tick==tick){if(supply)return fail<NavalSuppliedBattle>("NavalSupplySourceAmbiguous",army_id);supply=&row;}
  if(!army||!supply||supply->id.empty()||supply->organization_id!=army->organization_id||supply->required<0||supply->consumed<0||supply->consumed>supply->required||supply->shortage!=supply->required-supply->consumed)return fail<NavalSuppliedBattle>("NavalSupplySourceRequired",army_id);
  const std::set<StableId> mouths(supply->mouth_ids.begin(),supply->mouth_ids.end());if(mouths.size()!=supply->mouth_ids.size()||mouths.count(""))return fail<NavalSuppliedBattle>("NavalSupplyRosterMismatch",supply->id);
  for(auto& unit:out.battle.frame.units)if(unit.army_id==army_id){for(const auto* group:{&unit.fit,&unit.wounded})for(const auto& person:*group)if(!mouths.count(person))return fail<NavalSuppliedBattle>("NavalSupplyRosterMismatch",person);if(supply->shortage>0&&!unit.fit.empty())unit.morale=std::max<Integer>(1,unit.morale-8);auto attrition=apply_military_supply_attrition_to_battle(military,unit,supply->id,tick);if(!attrition.ok)return {false,{},attrition.error};}
  out.source_ids.push_back(supply->id);
 }
 return {true,std::move(out),{}};
}
// 原 AI/本人正式命令原样保留，仅把本半日已发生的缺粮士气带入同一战斗公式。
Outcome<NavalRoundFact> make_naval_supplied_round(const MilitaryState& military,const NavalAssetState& assets,const NavalAssetCatalog& ships,const NavalBattlePolicy& policy,const MilitaryBattleCatalog& battle_catalog,const NavalBattleRecord& source,const StableId& id,Integer tick,const NavalGunneryPreparation* prepared){
 auto fed=naval_round_supply_source(military,source,tick);if(!fed.ok)return {false,{},fed.error};
 // 只有真实本轮匿名逃散可使已接受命令失能；原AI不会重算，失败意图在回合事件中具名保留。
 std::set<StableId> changed;for(const auto& f:military.supply_attrition.facts)if(f.tick==tick&&!f.deserted_ids.empty()&&std::find(fed.value.source_ids.begin(),fed.value.source_ids.end(),f.supply_fact_id)!=fed.value.source_ids.end())changed.insert(f.unit_id);
 std::vector<NavalCombatEvent> interrupted;if(!changed.empty()){
  auto original=frozen_naval_orders(assets,ships,policy,battle_catalog,source,prepared);if(!original.ok)return {false,{},original.error};const BattlefieldDefinition* board=nullptr;for(const auto& row:battle_catalog.battlefields)if(row.id==source.frame.battlefield_id)board=&row;if(!board)return fail<NavalRoundFact>("UnknownNavalBattlefield",source.frame.battlefield_id);
  for(auto* rows:{&fed.value.battle.ai_orders,&fed.value.battle.pending_orders})for(auto& entry:*rows){auto quoted=preview_naval_tactical_order(assets,ships,*board,fed.value.battle.frame,entry.first,entry.second,policy.combat.tactical_navigation?&*policy.combat.tactical_navigation:nullptr,policy.combat.gunnery?&*policy.combat.gunnery:nullptr);if(quoted.ok)continue;const auto& code=quoted.error.original.code;const bool affected=(changed.count(entry.first)&&(code=="NavalCrewCannotAct"||code=="NavalShipCannotSail"||code=="NavalDestinationUnreachable"||code=="NavalSurrenderRequiresPresentCaptor"))||(changed.count(entry.second.target_unit_id)&&code=="NavalSurrenderRequiresPresentCaptor");if(!affected)return {false,{},quoted.error};const BattleUnit* unit=nullptr;for(const auto& row:fed.value.battle.frame.units)if(row.unit_id==entry.first)unit=&row;if(!unit)return fail<NavalRoundFact>("NavalSupplyRosterMismatch",entry.first);NavalCombatEvent event;event.kind="supply_order_interrupted";event.unit_id=entry.first;event.target_unit_id=entry.second.target_unit_id;event.code="NavalSupplyAttritionOrderCancelled";interrupted.push_back(std::move(event));entry.second={"hold",{},unit->position,unit->heading,{}};}
 }
 auto result=make_naval_round_fact(assets,ships,policy,battle_catalog,fed.value.battle,id,tick,prepared);if(!result.ok)return result;result.value.supply_source_ids=std::move(fed.value.source_ids);result.value.events.insert(result.value.events.begin(),interrupted.begin(),interrupted.end());return result;
}
// 冷回放依原供给事实再算，改写 before 或供给引用不能靠重算封套摘要取得合法性。
Status validate_naval_supplied_round(const MilitaryState& military,const NavalAssetState& assets,const NavalAssetCatalog& ships,const NavalBattlePolicy& policy,const MilitaryBattleCatalog& battle_catalog,const NavalBattleRecord& source,const NavalRoundFact& fact,const NavalGunneryPreparation* prepared){
 auto expected=make_naval_supplied_round(military,assets,ships,policy,battle_catalog,source,fact.id,fact.tick,prepared);if(!expected.ok)return {false,{},expected.error};expected.value.casualty_source_id=fact.casualty_source_id;try{if(!compare_boundary(naval_battle_codec::encode_round(expected.value),naval_battle_codec::encode_round(fact)).values_equal)return common_failure("NavalSuppliedRoundMismatch",fact.id);}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}return common_success();
}
}
