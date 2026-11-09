#pragma once
#include "movement_boundary_suffix.hpp"
namespace hanmo::resident {
struct PoliticalRelationsReadContext;
struct MovementBoundaryContinuationRead {
  const MovementBoundaryContinuationSource *continuation = nullptr;
  MovementBoundarySuffixRead previous;
  const MovementUnitState *new_unit = nullptr;
  const TravelExertionBasis *new_basis = nullptr;
};
Outcome<MovementBoundaryContinuationRead>
read_movement_boundary_continuation_source(
    const FieldExactContactReadContext &, const PoliticalRelationsReadContext &,
    const FieldBattleCatalog &, const SourceRef &, ExactArithmeticBudget = {});
Status validate_movement_boundary_continuation_sources(
    const FieldExactContactReadContext &, const PoliticalRelationsReadContext &,
    const FieldBattleCatalog &, ExactArithmeticBudget = {});
} // namespace hanmo::resident
