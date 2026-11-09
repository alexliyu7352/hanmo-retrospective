#pragma once
#include "movement_boundary_source.hpp"
namespace hanmo::resident {
struct MovementBoundarySuffixRead {
  MovementBoundarySourceRead original;
  const MovementUnitCloseSource *closed = nullptr;
  const RoadMovementBasis *road = nullptr;
  const TerrainCostBasis *terrain = nullptr;
  // Actual original waypoint prefix consumed at the exact contact cursor.
  // Remaining original params.points[consumed_points..] is a borrowed suffix.
  Integer consumed_points = 0;
};
// No forecast time, new cost, new route or continuation permission is produced.
Outcome<MovementBoundarySuffixRead> read_movement_boundary_original_suffix(
    const FieldExactContactReadContext &, const FieldBattleCatalog &,
    const SourceRef &, ExactArithmeticBudget = {});
} // namespace hanmo::resident
