#pragma once
#include "movement_boundary_geometry.hpp"
namespace hanmo::resident {
struct MovementBoundarySourceRead {
  const MovementBoundaryContact *contact = nullptr;
  const MovementPrefixState *prefix = nullptr;
  const MovementUnitState *unit = nullptr;
  const TravelExertionBasis *basis = nullptr;
};
Outcome<MovementBoundarySourceRead> read_movement_boundary_contact_source(
    const FieldExactContactReadContext &, const FieldBattleCatalog &,
    const SourceRef &, ExactArithmeticBudget = {});
// Publication boundary only: every original contact must have its actual
// unit close and exertion, and every corresponding Common head has one row.
// Hot prepare/read uses the single-source reader above before these writes.
Status
validate_movement_boundary_contact_sources(const FieldExactContactReadContext &,
                                           const FieldBattleCatalog &,
                                           ExactArithmeticBudget = {});
// Requires the actual prefix settled at the independently revalidated event
// M. This source alone never closes a unit, moves an actor, or releases a hold.
Outcome<SourceRef> install_movement_boundary_contact_in_candidate(
    CommonStore &, MovementExactState &, const FieldExactContactReadContext &,
    const FieldBattleCatalog &, const MovementBoundaryPrediction &,
    ExactArithmeticBudget = {});
} // namespace hanmo::resident
