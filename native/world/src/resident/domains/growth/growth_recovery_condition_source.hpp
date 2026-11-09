// 原个人康复 credit 的实际HP封顶写入；康复进度仍只认唯一原账。
#pragma once
#include "growth_personal_condition_contract.hpp"
#include "growth_recovery_model.hpp"
namespace hanmo::resident {
// 前置完整原Growth、伤因与Recovery史；不回调完整World，不重放第二份伤势。
Outcome<GrowthPersonalConditionCause> read_growth_recovery_condition_cause(
    const CommonState&,const GrowthState&,const GrowthRecoveryCatalog&,const GrowthRecoveryState&,
    const StableId& actual_credit,const DynamicStepInput* actual_phase=nullptr);
// before.hp须由真实捕获/完整条件重放给出；只验原credit.after伤级的HP上限，不授任何疲劳写。
Status validate_growth_recovery_condition_effects(
    const CommonState&,const GrowthState&,const GrowthRecoveryCatalog&,const GrowthRecoveryState&,
    const GrowthPersonalConditionCause&,const std::vector<GrowthPersonalConditionActorEffect>&,
    const DynamicStepInput* actual_phase=nullptr);
Outcome<std::vector<StableId>> read_growth_recovery_condition_cause_ids(
    const CommonState&,const GrowthState&,const GrowthRecoveryCatalog&,const GrowthRecoveryState&,
    const DynamicStepInput* actual_phase=nullptr);
}
