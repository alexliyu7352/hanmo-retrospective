#pragma once
#include "field_boundary_source.hpp"
namespace hanmo::resident {
struct PoliticalRelationsReadContext;
class CheckpointMemoryCharge;
// Installs the actual stopped boundary and, only for a proved side,
// PendingEntry. Does not fabricate an entered formation, resumed journey or
// player decision.
Outcome<StableId> install_field_boundary_contact_in_candidate(
    CommonStore &, FieldBattleWorldState &,
    const FieldExactContactReadContext &, const PoliticalRelationsReadContext &,
    const FieldBattleCatalog &, const SourceRef &actual_movement_boundary,
    CheckpointMemoryCharge &, ExactArithmeticBudget = {});
Status validate_field_exact_boundary_contacts(
    const FieldExactContactReadContext &, const PoliticalRelationsReadContext &,
    const FieldBattleCatalog &, ExactArithmeticBudget = {});
} // namespace hanmo::resident
