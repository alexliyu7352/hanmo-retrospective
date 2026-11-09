// 冻结值仅由真实入战condition史提供；此纯域叶钉住整数边界和一部一行形状。
#include "military_combat_conditions.hpp"
#include "resident/model/common_model.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
bool fields(FieldOrder given,FieldOrder required){if(given.empty())return true;std::sort(given.begin(),given.end());std::sort(required.begin(),required.end());return given==required;}
}
Status validate_military_combat_condition_rule(const MilitaryCombatConditionRule& r){return r.profile=="entry_fatigue_03_v1"?common_success():common_failure("MilitaryCombatConditionProfileMismatch");}
Outcome<Integer> military_combat_fatigue_efficiency(Integer fatigue){if(fatigue<0||fatigue>100)return {false,{},common_failure("MilitaryCombatFatigueOutOfRange").error};return {true,fatigue<40?10000:fatigue<70?9000:fatigue<90?7500:6000,{}};}
Status validate_military_combat_condition_shape(const MilitaryBattleRecord& battle,const MilitaryCombatConditionState& state){
 if(state.entry_tick<0||state.entry_tick!=battle.created_tick||state.units.size()!=battle.units.size()||state.units.empty()||state.units.size()>64||!fields(state.field_order,{"entry_tick","units"}))return common_failure("MilitaryCombatConditionShapeMismatch",battle.id);
 std::vector<StableId> ids;for(const auto& u:battle.units)ids.push_back(u.unit_id);std::sort(ids.begin(),ids.end());StableId previous;
 for(std::size_t i=0;i<state.units.size();++i){const auto& row=state.units[i];if(row.unit_id.empty()||row.unit_id!=ids[i]||(!previous.empty()&&row.unit_id<=previous)||row.source_garrison_id.empty()||row.training.has_value()!=row.fatigue.has_value())return common_failure("MilitaryCombatConditionUnitMismatch",row.unit_id);previous=row.unit_id;FieldOrder shape{"unit_id","source_garrison_id","last_condition_fact_id","efficiency_bp"};if(row.training){shape.push_back("training");shape.push_back("fatigue");if(*row.training<0||*row.training>100)return common_failure("MilitaryCombatTrainingOutOfRange",row.unit_id);auto bp=military_combat_fatigue_efficiency(*row.fatigue);if(!bp.ok)return {false,{},bp.error};if(row.efficiency_bp!=bp.value)return common_failure("MilitaryCombatConditionEfficiencyMismatch",row.unit_id);}else if(row.efficiency_bp!=10000||!row.last_condition_fact_id.empty())return common_failure("MilitaryCombatConditionNeutralSourceMismatch",row.unit_id);if(!fields(row.field_order,shape))return common_failure("MilitaryCombatConditionShapeMismatch",row.unit_id);}
 return common_success();
}
Outcome<Integer> military_combat_attack_efficiency(const MilitaryBattleRecord& battle,const StableId& unit){
 if(!battle.combat_conditions)return {true,10000,{}};auto valid=validate_military_combat_condition_shape(battle,*battle.combat_conditions);if(!valid.ok)return {false,{},valid.error};for(const auto& row:battle.combat_conditions->units)if(row.unit_id==unit)return {true,row.efficiency_bp,{}};return {false,{},common_failure("MilitaryCombatConditionUnitMissing",unit).error};
}
}
