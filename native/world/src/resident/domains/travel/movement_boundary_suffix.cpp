#include "movement_boundary_suffix.hpp"
#include "movement_exact_detail.hpp"
#include "resident/domains/military/field_battle/field_exact_contact_source.hpp"
#include "spatial_carrier.hpp"
#include <algorithm>
namespace hanmo::resident {
using namespace movement_detail;
Outcome<MovementBoundarySuffixRead> read_movement_boundary_original_suffix(
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    const SourceRef &source, ExactArithmeticBudget b) {
  return guard<MovementBoundarySuffixRead>([&] {
    MovementBoundarySuffixRead out;
    out.original =
        take(read_movement_boundary_contact_source(x, catalog, source, b));
    const auto &hit = *out.original.contact;
    out.closed = x.movement.unit_closes.find(out.original.unit->id + ":close");
    if (!out.closed || out.closed->kind != MovementCloseKind::contact ||
        !same_source(out.closed->actual_cause, source) ||
        out.closed->unit_ref != out.original.unit->id ||
        out.closed->prefix_id != hit.prefix_id ||
        !same_moment(out.closed->occurred_at, hit.occurred_at, b) ||
        !take(same_spatial_anchor(out.closed->actual_anchor, hit.actual_anchor,
                                  b)))
      fail("MovementBoundarySuffixActualCloseRequired", source.id);
    out.road =
        std::get_if<RoadMovementBasis>(&out.original.basis->actual_basis);
    if (out.road)
      return out;
    out.terrain =
        std::get_if<TerrainCostBasis>(&out.original.basis->actual_basis);
    if (!out.terrain || out.terrain->original_point_count !=
                            Integer(out.terrain->original_inputs.points.size()))
      fail("MovementBoundarySuffixOriginalTerrainRequired", source.id);
    const auto &terrain = *out.terrain;
    const auto &cursor = out.closed->cursor;
    if (compare(cursor, terrain.parameter_limit.value, b) > 0)
      fail("MovementBoundarySuffixCursorOutsideBasis", source.id);
    Integer consumed = 0;
    for (const auto &piece : terrain.pieces) {
      if (compare(cursor, piece.cost_from, b) < 0)
        break;
      const auto index = piece.segment.point_index;
      if (index < 0 || index > terrain.original_point_count)
        fail("MovementBoundarySuffixPointIndexMismatch", source.id);
      // Original zero-length waypoints have already been consumed before this
      // actual cost piece begins. No geometric nearest-point inference is used.
      consumed = std::max(consumed, index);
      if (piece.kind == TerrainCostPieceKind::moving_cost &&
          index < terrain.original_point_count &&
          compare(cursor, piece.cost_to, b) >= 0) {
        const auto &target = terrain.original_inputs.points[std::size_t(index)];
        if (piece.segment.to.x == double(target.x) &&
            piece.segment.to.y == double(target.y))
          consumed = std::max(consumed, index + 1);
      }
    }
    if (compare(cursor, terrain.parameter_limit.value, b) == 0) {
      if (consumed > terrain.reached_points)
        fail("MovementBoundarySuffixOriginalClosingMismatch", source.id);
      consumed = terrain.reached_points;
    }
    if (consumed < 0 || consumed > terrain.original_point_count)
      fail("MovementBoundarySuffixOriginalPointsRequired", source.id);
    out.consumed_points = consumed;
    return out;
  });
}
} // namespace hanmo::resident
