// Concrete original local phases after true pending-entry application.
// Not the public Advance command or a substitute for world effects.
#pragma once
#include "field_departures.hpp"
namespace hanmo::resident {
class CommonStore;
struct MilitaryState;
struct MilitaryCatalog;
struct TravelCatalog;
struct FieldBattleCatalog;
struct FieldBattleWorldState;
struct FieldBattleAbilityReadContext;
class FieldSegmentApplicationRead;
class CheckpointMemoryCharge;
// Exact native horizon comes only from the same World clock. The original
// local slice mathematics is shared; Common may still hold S0's opening tick.
Outcome<FieldBattleWindowSlice> read_field_window_at_actual_boundary(
    const CommonStore &, const MilitaryState &, const FieldBattleWorldState &,
    const StableId &battle, const FieldSegmentApplicationRead &,
    const FieldBattleTime &requested_to, const FieldBattleParameters &);
struct FieldLocalStepResult {
  FieldBattleWindowSlice step;
  FieldDepartureRead departures;
  bool settlement_due = false;
};
// Invoked only inside the unpublished complete World candidate. Real pending
// entries due in this step must already have been applied by their producer;
// an unconsumed entry is an ordering error, never silently skipped. The true
// segment/life/injury/population/S1/report effects remain the outer
// transaction.
Outcome<FieldLocalStepResult> apply_field_local_step_in_candidate(
    CommonStore &, const MilitaryState &, const MilitaryCatalog &,
    const TravelCatalog &, const FieldBattleCatalog &, FieldBattleWorldState &,
    const StableId &battle, const GeneratedBattleField &,
    const FieldBattleAbilityReadContext &, const FieldSegmentApplicationRead &,
    const FieldBattleWindowSlice &, CheckpointMemoryCharge &);
} // namespace hanmo::resident
