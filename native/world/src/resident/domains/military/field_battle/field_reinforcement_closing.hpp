#pragma once
#include "field_reinforcement_entry.hpp"
namespace hanmo::resident {
// 每次原局部 STEP 前采用已到时的真实队列；玩家入场只返回等待，不替他发令。
struct FieldReinforcementClosingResult {
  bool player_input_required = false;
};
Outcome<FieldReinforcementClosingResult>
admit_due_field_reinforcements_at_closing_in_candidate(
    CommonStore &, FieldBattleWorldState &, const FieldExactContactReadContext &,
    const PoliticalRelationsReadContext &, const FieldBattleCatalog &,
    const FieldOriginalRosterReadContext &, const FieldBattleAbilityReadContext &,
    const GeneratedBattleField &, const StableId &battle,
    CheckpointMemoryCharge &, ExactArithmeticBudget = {});
} // namespace hanmo::resident
