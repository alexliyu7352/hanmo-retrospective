// 原固定服务的单边界身体与技艺输入反读，不重验整个 Growth 或 Complete 图。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct CharacterConditionSources;
struct GrowthState;
struct LifeFixedServiceBoundary;
// 必须是同表真实行；复用原冻结head、能力结果和技艺来源的唯一校验。
Status validate_growth_fixed_boundary_actor_inputs(
    const CharacterConditionSources &, const GrowthState &,
    const LifeFixedServiceBoundary &actual_boundary);
} // namespace hanmo::resident
