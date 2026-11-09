#pragma once
#include "movement_boundary_model.hpp"
namespace hanmo::resident {
struct FieldExactContactReadContext;
struct FieldBattleCatalog;
// Complete actual source graph, not a list of synthetic endpoints. The
// original Field owner validates the rectangle and author map scale; travel
// owns prefix mapping, exact entry and its immutable motion witness.
Outcome<std::optional<MovementBoundaryPrediction>>
predict_movement_field_boundary(const FieldExactContactReadContext &,
                                const FieldBattleCatalog &,
                                const StableId &battle_id,
                                const StableId &prefix_id,
                                ExactArithmeticBudget = {});
Status validate_movement_boundary_geometry_source(
    const FieldExactContactReadContext &, const FieldBattleCatalog &,
    const MovementBoundaryContact &, ExactArithmeticBudget = {});
struct MovementBoundaryFieldGeometryRead {
  PointI point_dm;
  std::optional<Integer> incoming_facing_mdeg;
};
Outcome<MovementBoundaryFieldGeometryRead>
read_movement_boundary_field_geometry(const FieldExactContactReadContext &,
                                      const FieldBattleCatalog &,
                                      const MovementBoundaryContact &,
                                      ExactArithmeticBudget = {});
// Integer-dm compatibility projection of the exact intersected local point;
// truncates toward zero like original Field local_point, never resamples time.
Outcome<PointI> read_movement_boundary_field_point(
    const FieldExactContactReadContext &, const FieldBattleCatalog &,
    const MovementBoundaryContact &, ExactArithmeticBudget = {});
} // namespace hanmo::resident
