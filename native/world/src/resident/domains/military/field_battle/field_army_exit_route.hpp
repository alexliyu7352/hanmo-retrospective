// Field实际位置到原行旅成本的只读适配；不迁移Army、改变返营优先或支出。
#pragma once
#include "field_army_exit_source.hpp"
namespace hanmo::resident {
// origin用同军实际主将所在编队的原选择cut；candidate_exit仅用于真实候选格可达性。
Outcome<Integer> read_field_army_return_route_cost(
    const FieldExactContactReadContext &, const FieldBattleCatalog &,
    const FieldBattleAbilityReadContext &, const StableId &battle_id,
    const StableId &army_id, const StableId &destination_settlement_id,
    const FieldControlAcceptanceBoundary &, const FieldBattleTime &selected_at,
    std::optional<std::size_t> candidate_exit = {}, ExactArithmeticBudget = {});
} // namespace hanmo::resident
