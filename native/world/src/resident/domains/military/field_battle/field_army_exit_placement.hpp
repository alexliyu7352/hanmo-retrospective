// 新汇合来源复用原road/settlement锚，绝不伪造一个已Withdrawn的formation。
#pragma once
#include "field_army_exit_rally.hpp"
#include "resident/domains/travel/movement_exact_model.hpp"
namespace hanmo::resident {
// 只做原world几何投影，绝不是source授权；只有已验证真实step/ready读口可采用。
Outcome<MovementAnchor> project_field_army_exit_actual_point(
    const FieldExactContactReadContext &, const FieldBattleCatalog &,
    const FieldBattleAbilityReadContext &, const StableId &battle_id,
    Integer side, PointI actual_point, const SourceRef &actual_source,
    ExactArithmeticBudget = {});
// 验证真实deadline普通兵source后才投影其实际末点；不授S4或location mutation许可。
Outcome<MovementAnchor> read_field_army_exit_ordinary_anchor(
    const FieldExactContactReadContext &, const FieldBattleCatalog &,
    const FieldBattleAbilityReadContext &, const SourceRef &step_source,
    const StableId &actor_id, ExactArithmeticBudget = {});
} // namespace hanmo::resident
