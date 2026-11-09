#pragma once
#include "field_reinforcement_model.hpp"
#include "field_reinforcement_prepare.hpp"
namespace hanmo::resident {
struct FieldOriginalRosterReadContext;
class CheckpointMemoryCharge;
// Source-local identity only: exact formation, real Common entry head and
// original local Entered event. Full queue/geometry/mission validation remains
// a separate domain gate; this read never recomputes today's placement.
Outcome<const FieldReinforcementAdmission *>
read_field_reinforcement_admission_identity(const CommonStore &,
                                            const FieldBattleWorldState &,
                                            const FieldBattleSession &,
                                            const FieldFormation &);
struct FieldReinforcementNextPreparation {
  CheckpointMemoryCharge copied_formation;
  const FieldBattleBoundaryRecord *boundary = nullptr;
  const FieldBattleSession *session = nullptr;
  const FieldReinforcementQueue *queue = nullptr;
  WorldMoment actual_at;
  FieldFormation formation;
};
// No write: the original ordered prefix and actual predecessor circle, not
// arbitrary requested formation IDs, select exactly the next admission.
Outcome<FieldReinforcementNextPreparation>
prepare_next_field_reinforcement_in_candidate(
    const FieldExactContactReadContext &, const FieldBattleCatalog &,
    const FieldOriginalRosterReadContext &,
    const SourceRef &actual_field_contact, ExactArithmeticBudget = {});
// Verifies original queue identity/order at its own Common cut. It neither
// admits a formation nor substitutes current Army rows for the old roster.
Outcome<const FieldReinforcementQueue *>
read_field_original_reinforcement_queue(const FieldExactContactReadContext &,
                                        const FieldBattleCatalog &,
                                        const FieldOriginalRosterReadContext &,
                                        const SourceRef &actual_field_contact,
                                        ExactArithmeticBudget = {});
// Complete queue/header coverage, independent of the actual movement/mission
// replay gate. Not-yet-admitted identities must not appear as simulated units.
Status validate_field_reinforcement_queue_sources(
    const FieldExactContactReadContext &, const FieldBattleCatalog &,
    const FieldOriginalRosterReadContext &, ExactArithmeticBudget = {});
Outcome<SourceRef> initialize_field_reinforcement_queue_in_candidate(
    CommonStore &, FieldBattleWorldState &,
    const FieldExactContactReadContext &, const PoliticalRelationsReadContext &,
    const FieldBattleCatalog &, const FieldOriginalRosterReadContext &,
    const SourceRef &actual_field_contact, CheckpointMemoryCharge &,
    ExactArithmeticBudget = {});
} // namespace hanmo::resident
