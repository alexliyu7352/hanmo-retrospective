// One final AUTO-QUALITY-1/Craft-PACE-1 calculation over the original K2 gate.
#pragma once
#include "craft_recipe.hpp"
namespace hanmo::resident {
struct CraftQualityInputs {
 Integer skill_level=0,equipment_level=0,material_fit=0;
 Integer recipe_minimum=0,recipe_maximum=0;
 StableId process_policy_ref,rush_policy_ref;
};
struct CraftQualityValue {Integer performance=0,rush_penalty=0,quality_score=0;};
// 同一个公式也供原gate完整历史只读重放；裸总量只是计算输入，不是产物许可。
Outcome<CraftQualityValue> calculate_automatic_craft_quality_values(
 const CraftQualityInputs&,Integer required_normal_thirds,Integer actual_rush_normal_thirds);
// Pure computation only. The Job owner must first read the actual fully applied
// last gate and all stage results; passing this value is never output authority.
Outcome<CraftQualityValue> calculate_automatic_craft_quality(
 const CraftQualityInputs&,const SharedWorkElapsedState& actual_execution,
 Integer units_per_half_day,ExactArithmeticBudget budget={});
}
