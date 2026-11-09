// 只用原供给和匿名流失同半日事实调整战前副本；战场本体只在原结算成功后提交。
#include "military_supply_attrition_battle.hpp"
#include "military_commander.hpp"
#include "military_rout_status.hpp"
#include "military_rout_orders.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
const BattleUnit* unit(const MilitaryBattleRecord& b,const StableId& id){for(const auto& u:b.units)if(u.unit_id==id)return &u;return nullptr;}
bool active(const BattleUnit& u){return !u.withdrawn&&!u.fit.empty()&&u.morale>0&&military_commander_loss(u)!=2;}
}
Outcome<MilitaryAttritionBattlePreparation> prepare_military_supply_attrition_battle(const MilitaryState& m,const MilitaryCatalog& cat,const MilitaryBattleRecord& source,Integer tick){
 MilitaryAttritionBattlePreparation out{source,{}};if(!cat.supply_attrition_policy)return {true,std::move(out),{}};std::set<StableId> lost;
 for(auto& u:out.battle.units){const MilitarySupplyFact* food=nullptr;for(const auto& f:m.supply_facts)if(f.tick==tick&&f.army_id==u.army_id){if(food)return {false,{},common_failure("MilitarySupplyAttritionSourceRepeated",u.army_id).error};food=&f;}if(!food)return {false,{},common_failure("MilitarySupplyAttritionSupplyRequired",u.army_id).error};if(food->shortage>0&&!u.fit.empty())u.morale=cat.rout_retreat_policy&&u.morale==0?0:std::max<Integer>(1,u.morale-8);const auto before=u.fit;auto applied=apply_military_supply_attrition_to_battle(m,cat,u,food->id,tick);if(!applied.ok)return {false,{},applied.error};if(before!=u.fit)lost.insert(u.unit_id);}
 auto normalize=[&](MilitaryOrders& rows){MilitaryOrders kept;for(auto row:rows){const auto* actor=unit(out.battle,row.first);const auto* target=row.second.target_id.empty()?nullptr:unit(out.battle,row.second.target_id);const bool actor_lost=actor&&lost.count(actor->unit_id)&&!active(*actor)&&!military_unit_routing(*actor);const bool target_lost=target&&lost.count(target->unit_id)&&!active(*target)&&!military_unit_routing(*target);if(actor_lost||target_lost){MilitaryBattleEvent event;event.kind="supply_order_interrupted";event.unit_id=row.first;event.target_id=row.second.target_id;event.code="MilitarySupplyAttritionOrderCancelled";out.interrupted_orders.push_back(std::move(event));if(actor_lost)continue;row.second={"wait",actor->position,actor->heading,{},{},{}};}kept.push_back(std::move(row));}rows=std::move(kept);};
 normalize(out.battle.pending_orders);normalize(out.battle.ai_plans);
 if(military_rout_rule_in_use(source,cat.battle_catalog)){auto progress=read_military_rout_progress(source,cat.battle_catalog);if(!progress.ok)return {false,{},progress.error};progress.value.erase(std::remove_if(progress.value.begin(),progress.value.end(),[&](const auto& p){const auto* u=unit(out.battle,p.unit_id);return !u||u->fit.empty()||u->withdrawn;}),progress.value.end());auto changed=write_military_rout_progress(out.battle,progress.value);if(!changed.ok)return {false,{},changed.error};for(auto& row:out.battle.ai_plans){const auto* u=unit(out.battle,row.first);if(u&&military_unit_routing(*u)){auto automatic=make_military_rout_order(out.battle,cat.battle_catalog,row.first);if(!automatic.ok)return {false,{},automatic.error};row.second=std::move(automatic.value);}}}
 return {true,std::move(out),{}};
}
}
