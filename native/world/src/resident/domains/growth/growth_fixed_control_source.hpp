// 原 Growth 命令绑定的单边界反读，不重验全世界或取得另一份 Complete 图。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct CharacterConditionSources;
struct GrowthState;
struct LifeFixedServiceBoundary;
struct CommitResult;
// boundary 必须借自同 Growth 拥有表，复用原真实 result/history、本人控制或
// 已采用 Director 路由核验。非命令 cause 明确拒绝，不能把原因头冒采用头。
// 返回的 result 是原缓存借用；其 revision 不是同候选内的先后证明。
Outcome<const CommitResult *> read_growth_fixed_control_command_source(
    const CharacterConditionSources &, const GrowthState &,
    const LifeFixedServiceBoundary &actual_boundary);
} // namespace hanmo::resident
