#include "craft_quality.hpp"
namespace hanmo::resident {
Outcome<CraftQualityValue> calculate_automatic_craft_quality(const CraftQualityInputs&i,const SharedWorkElapsedState&actual,Integer units,ExactArithmeticBudget budget){
 const auto*gate=std::get_if<CraftWorkGate>(&actual.gate);const auto&work=actual.execution;
 if(!gate||actual.action_ref.empty()||i.skill_level<0||i.equipment_level<0||i.material_fit<0||i.material_fit>15||i.recipe_minimum<0||i.recipe_maximum>100||i.recipe_minimum>i.recipe_maximum||i.process_policy_ref!="AUTO-QUALITY-1"||i.rush_policy_ref!="CRAFT-PACE-1"||work.work_unit_id!="normal_thirds"||work.total_work_units<=0||work.total_work_units%3||work.completed_work_units!=work.total_work_units||gate->rush_units_committed<0||gate->rush_units_committed>work.total_work_units||!gate->last_closed_gate_result_ref)return {false,{},common_failure("CraftFinalQualityOriginalWorkRequired",actual.action_ref).error};
 auto valid=validate_craft_work_gate(work,*gate,units,budget);if(!valid.ok)return {false,{},valid.error};
 return calculate_automatic_craft_quality_values(i,work.total_work_units,gate->rush_units_committed);
}
Outcome<CraftQualityValue> calculate_automatic_craft_quality_values(const CraftQualityInputs&i,Integer required,Integer rushed){
 if(i.skill_level<0||i.equipment_level<0||i.material_fit<0||i.material_fit>15||i.recipe_minimum<0||i.recipe_maximum>100||i.recipe_minimum>i.recipe_maximum||i.process_policy_ref!="AUTO-QUALITY-1"||i.rush_policy_ref!="CRAFT-PACE-1"||required<=0||required%3||rushed<0||rushed>required)return {false,{},common_failure("InvalidCraftQualityInputs").error};
 const __int128 denominator=required;
 const __int128 numerator=static_cast<__int128>(15)*rushed;
 const Integer penalty=static_cast<Integer>((2*numerator+denominator)/(2*denominator));
 __int128 raw=40+static_cast<__int128>(8)*i.skill_level+static_cast<__int128>(5)*i.equipment_level+i.material_fit-penalty;
 if(raw<i.recipe_minimum)raw=i.recipe_minimum;if(raw>i.recipe_maximum)raw=i.recipe_maximum;
 return {true,{0,penalty,static_cast<Integer>(raw)},{}};
}
}
