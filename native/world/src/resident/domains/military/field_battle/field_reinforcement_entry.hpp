#pragma once
#include "field_reinforcement_queue.hpp"
namespace hanmo::resident {
struct FieldBattleAbilityReadContext;
// Actual automatic entry for an AI-controlled side. Player-side command
// admission has its own explicit no-task marker, never an automatic mission.
// Every failure discards the caller's whole candidate, including Common heads.
Outcome<SourceRef> admit_next_ai_field_reinforcement_in_candidate(
    CommonStore &, FieldBattleWorldState &,
    const FieldExactContactReadContext &, const FieldBattleCatalog &,
    const FieldOriginalRosterReadContext &,
    const FieldBattleAbilityReadContext &, const GeneratedBattleField &,
    const SourceRef &actual_field_contact, CheckpointMemoryCharge &,
    ExactArithmeticBudget = {});
// Same original army authority, actual queue prefix and per-formation entry.
// The formation is Moving/present but awaits an actual player task; this is
// not permission to command unadmitted identities or to emit a fake Order.
Outcome<SourceRef> admit_next_player_field_reinforcement_in_candidate(
    CommonStore &, FieldBattleWorldState &,
    const FieldExactContactReadContext &, const FieldBattleCatalog &,
    const FieldOriginalRosterReadContext &,
    const FieldBattleAbilityReadContext &, const GeneratedBattleField &,
    const SourceRef &actual_field_contact, CheckpointMemoryCharge &,
    ExactArithmeticBudget = {});
} // namespace hanmo::resident
