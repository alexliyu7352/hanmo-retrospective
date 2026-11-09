// One actual §7.8 phase in the whole unpublished Field candidate. This is not
// an Advance implementation or a global death application.
#pragma once
#include "field_commander_pulse.hpp"
namespace hanmo::resident {
class CommonStore;
struct MilitaryState;
struct FieldBattleWorldState;
struct FieldBattleAbilityReadContext;
class FieldSegmentApplicationRead;
Outcome<FieldCommanderPulseRead> apply_field_commander_step_in_candidate(
    CommonStore &, const MilitaryState &, FieldBattleWorldState &,
    const StableId &battle_id, const GeneratedBattleField &,
    const FieldPulseRead &, const FieldBattleWindowSlice &,
    const FieldBattleAbilityReadContext &, const FieldSegmentApplicationRead &,
    CheckpointMemoryCharge &);
} // namespace hanmo::resident
