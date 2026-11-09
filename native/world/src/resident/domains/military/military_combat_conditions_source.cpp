#include "resident/domains/military/military_model.hpp"
// 条件真值始终由原训练/行军/驻地恢复链产生；存档只携带同实际入口被冻结的有限引用。
#include "military_combat_conditions_source.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {
namespace {
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& row:rows)if(row.id==id)return &row;return nullptr;}
template<class T>Outcome<T> failure(const StableId& code,const StableId& id={}){return {false,{},common_failure(code,id).error};}
Outcome<MilitaryCombatConditionSnapshot> original(const MilitaryState& s,const MilitaryCatalog& cat,const StableId& id){const auto* unit=find(s.military_units,id);const auto* camp=unit?find(cat.garrisons,unit->source_garrison_id):nullptr;const auto* definition=camp?find(camp->units,id):nullptr;if(!unit||!camp||!definition||definition->initial_training.has_value()!=definition->initial_fatigue.has_value())return failure<MilitaryCombatConditionSnapshot>("MilitaryCombatConditionOriginalRequired",id);MilitaryCombatConditionSnapshot out;out.unit_id=id;out.source_garrison_id=camp->id;out.training=definition->initial_training;out.fatigue=definition->initial_fatigue;return {true,std::move(out),{}};}
void project(MilitaryCombatConditionSnapshot& row,const MilitaryConditionSourceProjection& p){row.training=p.condition.training;row.fatigue=p.condition.fatigue;row.last_condition_fact_id=p.last_source_id;row.efficiency_bp=military_combat_fatigue_efficiency(p.condition.fatigue).value;}
bool same(const MilitaryCombatConditionSnapshot& a,const MilitaryCombatConditionSnapshot& b){return a.unit_id==b.unit_id&&a.source_garrison_id==b.source_garrison_id&&a.training==b.training&&a.fatigue==b.fatigue&&a.last_condition_fact_id==b.last_condition_fact_id&&a.efficiency_bp==b.efficiency_bp;}
}
Outcome<MilitaryCombatConditionState> prepare_military_combat_condition_entry(const CommonRead& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& access,const MilitaryBattleRecord& battle){
 if(!cat.combat_condition_policy||!cat.garrison_rest_policy||!battle.rout_retreat_enabled||battle.round!=1||!battle.round_receipt_ids.empty()||c.tick()==std::numeric_limits<Integer>::max()||(battle.created_tick!=c.tick()&&battle.created_tick!=c.tick()+1))return failure<MilitaryCombatConditionState>("MilitaryCombatConditionEntryRequired",battle.id);
 const auto* contact=find(s.contact_facts,battle.source_id);if(!contact||contact->siege_id||contact->node_id||contact->tick>battle.created_tick||!access.fact_exists||!access.fact_exists(contact->id))return failure<MilitaryCombatConditionState>("MilitaryCombatConditionContactRequired",battle.source_id);
 MilitaryCombatConditionState out;out.entry_tick=battle.created_tick;std::vector<StableId> ids;for(const auto& u:battle.units)ids.push_back(u.unit_id);std::sort(ids.begin(),ids.end());
 for(const auto& id:ids){auto row=original(s,cat,id);if(!row.ok)return {false,{},row.error};const auto* unit=find(s.military_units,id);if(!row.value.training){if(unit->training||unit->fatigue)return failure<MilitaryCombatConditionState>("MilitaryCombatConditionNeutralSourceMismatch",id);}else{auto source=fold_military_condition_source(s,cat,id,battle.created_tick);if(!source.ok)return {false,{},source.error};if(!unit->training||!unit->fatigue||*unit->training!=source.value.condition.training||*unit->fatigue!=source.value.condition.fatigue||(!source.value.last_source_id.empty()&&!access.fact_exists(source.value.last_source_id)))return failure<MilitaryCombatConditionState>("MilitaryCombatConditionCurrentMismatch",id);project(row.value,source.value);}out.units.push_back(std::move(row.value));}
 auto valid=validate_military_combat_condition_shape(battle,out);return valid.ok?Outcome<MilitaryCombatConditionState>{true,std::move(out),{}}:Outcome<MilitaryCombatConditionState>{false,{},valid.error};
}
Status validate_military_combat_condition_entry(const CommonStore& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryRoutEntryFact& entry,const MilitaryCombatConditionState& state,const DynamicStepInput* phase){
 auto valid=validate_military_combat_condition_shape(entry.initial,state);if(!valid.ok)return valid;
 for(const auto& stored:state.units){auto expected=original(s,cat,stored.unit_id);if(!expected.ok)return {false,{},expected.error};if(expected.value.training){auto source=query_military_condition_before_rout_entry(c,s,cat,stored.unit_id,entry.id,phase);if(!source.ok)return {false,{},source.error};project(expected.value,source.value);}if(!same(stored,expected.value))return common_failure("MilitaryCombatConditionHistoryMismatch",stored.unit_id);}
 return common_success();
}
}
