// 单军单口的原选择来源；实际Common cut、原名册和局部事件不以今日状态倒填。
#pragma once
#include "field_army_exit_model.hpp"
#include "field_ability_sources.hpp"
namespace hanmo::resident {
struct FieldExactContactReadContext;
struct FieldBattleCatalog;
// 返回选定边界的真实位置和幸存兵员；不推进会话或新建名册。
Outcome<std::vector<FieldArmyExitWeight>> read_field_army_exit_weight_prefix(
    const FieldExactContactReadContext &, const FieldBattleCatalog &,
    const FieldBattleAbilityReadContext &, const StableId &battle_id,
    const StableId &army_id, const FieldControlAcceptanceBoundary &,
    const FieldBattleTime &selected_at);
// 只作整数加权几何；eligible须由原外交/路线owner产生，不能拿本返回当许可。
Outcome<std::size_t> read_field_army_weighted_exit_cell(
    const GeneratedBattleField &, const std::vector<FieldArmyExitWeight> &,
    const std::vector<std::size_t> &eligible);
} // namespace hanmo::resident
