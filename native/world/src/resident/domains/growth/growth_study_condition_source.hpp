// 原Study逐实际课时疲劳来源；XP迟答/取消补结不额外制造人物疲劳。
#pragma once
#include "growth_personal_condition_contract.hpp"
#include "resident/model/dynamic_contract.hpp"
namespace hanmo::resident {
struct CommonState;
Outcome<GrowthPersonalConditionCause> read_growth_study_condition_cause(
    const CommonState&,const GrowthState&,const StableId& actual_study_fact,Integer actual_lesson_ordinal,
    const DynamicStepInput* actual_phase=nullptr);
// 先验完整原Growth；学生before取原lesson，教师before须由完整条件重放给出，本口仅核+6/+2。
Status validate_growth_study_condition_effects(
    const CommonState&,const GrowthState&,const GrowthPersonalConditionCause&,
    const std::vector<GrowthPersonalConditionActorEffect>&,const DynamicStepInput* actual_phase=nullptr);
// 枚举真实原lessons，无论整门课后来成功、取消或中断；顺序不是跨业务的实际执行顺序。
Outcome<std::vector<GrowthPersonalConditionCause>> read_growth_study_condition_causes(
    const CommonState&,const GrowthState&,const DynamicStepInput* actual_phase=nullptr);
}
