// 三种已闭合Growth原写者的同候选绑定；不创建永久条件账、不启用任何保护profile。
#pragma once
#include "growth_personal_condition_contract.hpp"
#include "growth_recovery_model.hpp"
namespace hanmo::resident {
struct ActorConditionCaptureIdentity;
class GrowthConditionWriteBoundary;
struct GrowthConditionBoundWrite {
    std::shared_ptr<const ActorConditionCaptureIdentity> capture_identity;
    GrowthPersonalConditionCause cause;
    GrowthPersonalConditionCaptured actual;
};
// 同一实际capture内的短栈边界，只可成功绑定一次；复制共享消费态，不跨Common/clone重用。
class GrowthConditionWriteBoundary {
public:
    struct State;
    GrowthConditionWriteBoundary()=default;
private:
    std::shared_ptr<State> state_;
    friend Outcome<GrowthConditionWriteBoundary> begin_growth_condition_write_in_candidate(CommonStore&,const GrowthPersonalConditionBusinessKey&,const DynamicStepInput&);
    friend Outcome<GrowthConditionBoundWrite> bind_growth_study_condition_in_candidate(CommonStore&,const GrowthState&,GrowthConditionWriteBoundary&);
    friend Outcome<GrowthConditionBoundWrite> bind_growth_training_condition_in_candidate(CommonStore&,const GrowthState&,GrowthConditionWriteBoundary&);
    friend Outcome<GrowthConditionBoundWrite> bind_growth_recovery_condition_in_candidate(CommonStore&,const GrowthState&,const GrowthRecoveryCatalog&,const GrowthRecoveryState&,const StableId&,GrowthConditionWriteBoundary&);
};
// 原writer写前调用，须已由中央开同Common capture，并借真实from→to/existing Action。
// Training只在原扣时至0且成功分支内开启；Study/Recovery在原扣时前开启。
// 只准Study/Training/Recovery三种当前原步骤；有效tick来自真实step，不能从fact头推测。
Outcome<GrowthConditionWriteBoundary> begin_growth_condition_write_in_candidate(
    CommonStore&,const GrowthPersonalConditionBusinessKey&,const DynamicStepInput& actual_step);
// 原载荷成功落定后立即调用，只消费起点后的全部实际人物写入。缺源/多写/少写/字段错全部拒。
Outcome<GrowthConditionBoundWrite> bind_growth_study_condition_in_candidate(CommonStore&,const GrowthState&,GrowthConditionWriteBoundary&);
Outcome<GrowthConditionBoundWrite> bind_growth_training_condition_in_candidate(CommonStore&,const GrowthState&,GrowthConditionWriteBoundary&);
Outcome<GrowthConditionBoundWrite> bind_growth_recovery_condition_in_candidate(CommonStore&,const GrowthState&,const GrowthRecoveryCatalog&,const GrowthRecoveryState&,const StableId& actual_credit,GrowthConditionWriteBoundary&);
// 不begin/commit任何Common保存点，不完成capture，不发布候选；失败原调用栈必须传播。
// Common/Growth/Recovery/未来binding账必须同外层rollback；边界不得跨该原writer栈或rollback保存。
// 返回值仅在同capture/候选内有效；未来永久消费方仍须反核原来源与中央全末态覆盖，不能信裸DTO。
}
