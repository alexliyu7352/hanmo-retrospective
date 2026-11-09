#pragma once
#include "movement_boundary_continuation_source.hpp"
namespace hanmo::resident {
class CheckpointMemoryCharge;
struct GroupMovementContext;
struct GroupMovementExecutionState;
struct SpatialCarrierState;
struct FieldBattleWorldState;
// Full candidate only. The real terminal cause, exact Stable writer and old
// finalized unit must agree before the original personal Action can resume.
Outcome<SourceRef> resume_exact_personal_field_boundary_in_candidate(
    CommonStore &, TravelState &, MovementExactState &, FieldBattleWorldState &,
    const TravelCatalog &, const TravelServices &,
    const FieldExactContactReadContext &, const PoliticalRelationsReadContext &,
    const FieldBattleCatalog &, const SourceRef &actual_field_contact,
    CheckpointMemoryCharge &, ExactArithmeticBudget = {});
Outcome<SourceRef> resume_exact_group_field_boundary_in_candidate(
    CommonStore &, SpatialCarrierState &, MovementExactState &,
    GroupMovementExecutionState &, FieldBattleWorldState &,
    const GroupMovementContext &, const FieldExactContactReadContext &,
    const PoliticalRelationsReadContext &, const FieldBattleCatalog &,
    const SourceRef &actual_field_contact, CheckpointMemoryCharge &,
    ExactArithmeticBudget = {});
} // namespace hanmo::resident
