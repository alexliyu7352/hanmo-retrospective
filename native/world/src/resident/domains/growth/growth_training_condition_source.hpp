// 原Training真正成功完工的疲劳写源；只读子集，不能宣称已经覆盖其他人物写者。
#pragma once
#include "growth_personal_condition_contract.hpp"
#include "resident/model/dynamic_contract.hpp"
namespace hanmo::resident {
struct CommonState;
// CommonState须是原同候选typed状态；stable先验完整Growth，phase仅原actions_step已成功完工后。
Outcome<GrowthPersonalConditionCause> read_growth_training_condition_cause(
    const CommonState&,const GrowthState&,const StableId& actual_training_fact,
    const DynamicStepInput* actual_phase=nullptr);
// 本口只证明原原因与clamp算术。before还必须由完整condition初态/先前写序重放核对，不能自证。
Status validate_growth_training_condition_effects(
    const CommonState&,const GrowthState&,const GrowthPersonalConditionCause&,
    const std::vector<GrowthPersonalConditionActorEffect>&,const DynamicStepInput* actual_phase=nullptr);
// 原Training账反向枚举成功完工，失败/未完无此写源；顺序是原头序，不是实际写入序。
Outcome<std::vector<StableId>> read_growth_training_condition_cause_ids(
    const CommonState&,const GrowthState&,const DynamicStepInput* actual_phase=nullptr);
}
