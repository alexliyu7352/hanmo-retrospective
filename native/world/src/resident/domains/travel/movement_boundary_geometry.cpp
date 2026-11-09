#include "movement_boundary_geometry.hpp"
#include "group_movement.hpp"
#include "movement_boundary_geometry_detail.hpp"
#include "resident/domains/military/field_battle/field_author_data.hpp"
#include "resident/domains/military/field_battle/field_boundary_geometry.hpp"
#include "resident/domains/military/field_battle/field_exact_contact_source.hpp"
#include "resident/domains/military/field_battle/field_integer_geometry.hpp"
#include "resident/domains/military/field_battle/field_world.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "spatial_carrier.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
using namespace movement_boundary_detail;
struct Piece {
  CanonicalFraction from, to;
  Point a, z;
};
// Paired with the original finite read_movement_position cases. BigNat and
// scratch vectors use their actual allocator; only fresh ordinary text is
// leased.

CanonicalFraction cursor_at(const RunningMovementMap &map,
                            const WorldMoment &at, ExactArithmeticBudget b) {
  return add(map.cursor_at_from,
             mul(map.rate,
                 take(elapsed_world_duration(map.world_from, at, b)).value, b),
             b);
}
Point local(Point point, const FieldBoundaryGeometryRead &geometry,
            ExactArithmeticBudget b) {
  const auto &a = geometry.session->anchor;
  const auto tx = value(a.tangent.x, b), ty = value(a.tangent.y, b);
  point.x =
      minus(point.x, times(value(a.point_m.x, b), value(Integer(10), b), b), b);
  point.y =
      minus(point.y, times(value(a.point_m.y, b), value(Integer(10), b), b), b);
  auto norm = plus(times(tx, tx, b), times(ty, ty, b), b);
  if (norm.negative || compare(norm.magnitude, {}, b) <= 0)
    fail("BoundaryOriginalAnchorRequired");
  auto x = plus(times(point.x, tx, b), times(point.y, ty, b), b);
  auto y = minus(times(point.y, tx, b), times(point.x, ty, b), b);
  return {divided(times(x, value(Integer(1000), b), b), norm.magnitude, b),
          divided(times(y, value(Integer(1000), b), b), norm.magnitude, b)};
}
Point terrain_point(ContactPoint p, const FieldBoundaryGeometryRead &g,
                    ExactArithmeticBudget b) {
  const auto &m = *g.terrain_metadata;
  if (m.km_per_unit.num <= 0 || m.km_per_unit.den <= 0)
    fail("BoundaryOriginalMapScaleRequired");
  auto scale = div(mul(integer(m.km_per_unit.num, b), integer(10000, b), b),
                   integer(m.km_per_unit.den, b), b);
  return local({times(value(p.x, b), {false, scale}, b),
                times(value(p.y, b), {false, scale}, b)},
               g, b);
}
CheckpointVector<Piece> terrain_pieces(const TerrainCostBasis &basis,
                                       const CanonicalFraction &from,
                                       const CanonicalFraction &to,
                                       const FieldBoundaryGeometryRead &g,
                                       ExactArithmeticBudget b) {
  if (basis.original_inputs.map_id != g.terrain_metadata->map_id)
    fail("BoundaryMapMismatch");
  CheckpointVector<Piece> result;
  const auto width = sub(to, from, b);
  if (compare(width, {}, b) <= 0)
    return result;
  for (const auto &piece : basis.pieces) {
    if (result.size() >= b.max_steps)
      fail("BoundaryGeometryBudgetExceeded");
    auto lo = compare(piece.cost_from, from, b) > 0 ? piece.cost_from : from;
    auto hi = compare(piece.cost_to, to, b) < 0 ? piece.cost_to : to;
    if (compare(lo, hi, b) >= 0 ||
        compare(piece.cost_to, piece.cost_from, b) <= 0)
      continue;
    const auto span = sub(piece.cost_to, piece.cost_from, b);
    const auto a = terrain_point(piece.segment.from, g, b),
               z = terrain_point(piece.segment.to, g, b);
    result.push_back(
        {div(sub(lo, from, b), width, b), div(sub(hi, from, b), width, b),
         interpolate(a, z, div(sub(lo, piece.cost_from, b), span, b), b),
         interpolate(a, z, div(sub(hi, piece.cost_from, b), span, b), b)});
  }
  return result;
}
CanonicalFraction progress(const RoadMovementBasis &basis,
                           const CanonicalFraction &cursor,
                           ExactArithmeticBudget b) {
  const auto &a = basis.exact_start_progress;
  const auto &z = basis.exact_end_progress;
  return compare(a, z, b) <= 0 ? add(a, mul(sub(z, a, b), cursor, b), b)
                               : sub(a, mul(sub(a, z, b), cursor, b), b);
}
CheckpointVector<Piece>
road_pieces(const FieldExactContactReadContext &x,
            const FieldBattleCatalog &catalog, const RoadMovementBasis &basis,
            const CanonicalFraction &from, const CanonicalFraction &to,
            const FieldBoundaryGeometryRead &g, ExactArithmeticBudget b) {
  auto road = x.travel.roads.find(basis.original_inputs.edge_id);
  if (!road || road->duration_ticks <= 0 ||
      road->duration_ticks > std::numeric_limits<Integer>::max() / 100)
    fail("BoundaryOriginalRoadRequired");
  auto geometry = take(read_field_road_geometry(x.travel, catalog,
                                                *g.terrain_metadata, road->id));
  if (geometry.points_dm.size() < 2)
    fail("BoundaryOriginalRoadGeometryRequired");
  CheckpointVector<Integer> lengths;
  Integer total = 0;
  for (std::size_t i = 1; i < geometry.points_dm.size(); ++i) {
    const auto length = field_integer_sqrt(field_distance_squared(
        geometry.points_dm[i - 1], geometry.points_dm[i]));
    if (length < 0 || total > std::numeric_limits<Integer>::max() - length)
      fail("BoundaryRoadLengthOverflow");
    lengths.push_back(length);
    total += length;
  }
  if (total <= 0)
    fail("BoundaryOriginalRoadGeometryRequired");
  const auto p0 = progress(basis, from, b), p1 = progress(basis, to, b);
  const bool forward = compare(p0, p1, b) < 0;
  if (compare(p0, p1, b) == 0)
    return {};
  const auto width = forward ? sub(p1, p0, b) : sub(p0, p1, b);
  const auto low = forward ? p0 : p1, high = forward ? p1 : p0;
  const auto units = integer(road->duration_ticks * 100, b),
             distance = integer(total, b);
  CheckpointVector<Piece> result;
  Integer walked = 0;
  for (std::size_t i = 0; i < lengths.size(); ++i) {
    if (i >= b.max_steps)
      fail("BoundaryGeometryBudgetExceeded");
    auto start = div(mul(integer(walked, b), units, b), distance, b);
    walked += lengths[i];
    auto end = div(mul(integer(walked, b), units, b), distance, b);
    if (!lengths[i])
      continue;
    auto lo = compare(start, low, b) > 0 ? start : low,
         hi = compare(end, high, b) < 0 ? end : high;
    if (compare(lo, hi, b) > 0)
      continue;
    auto a = geometry.points_dm[i], z = geometry.points_dm[i + 1];
    const Point first = local({value(a.x, b), value(a.y, b)}, g, b),
                last = local({value(z.x, b), value(z.y, b)}, g, b);
    const auto segment = sub(end, start, b);
    auto at_lo =
        interpolate(first, last, div(sub(lo, start, b), segment, b), b);
    auto at_hi =
        interpolate(first, last, div(sub(hi, start, b), segment, b), b);
    if (forward)
      result.push_back({div(sub(lo, p0, b), width, b),
                        div(sub(hi, p0, b), width, b), at_lo, at_hi});
    else
      result.push_back({div(sub(p0, hi, b), width, b),
                        div(sub(p0, lo, b), width, b), at_hi, at_lo});
  }
  std::sort(result.begin(), result.end(), [&](const auto &a, const auto &z) {
    return compare(a.from, z.from, b) < 0;
  });
  return result;
}
std::optional<CanonicalFraction>
intersect(const CheckpointVector<Piece> &pieces,
          const FieldBoundaryGeometryRead &geometry, ExactArithmeticBudget b) {
  const auto &p = *geometry.parameters;
  if (p.length_m > std::numeric_limits<Integer>::max() / 5 ||
      p.width_m > std::numeric_limits<Integer>::max() / 5)
    fail("BoundaryRectangleOverflow");
  for (const auto &piece : pieces) {
    auto hit =
        rectangle_entry(piece.a, piece.z, p.length_m * 5, p.width_m * 5, b);
    if (hit)
      return add(piece.from, mul(sub(piece.to, piece.from, b), *hit, b), b);
  }
  return {};
}
} // namespace
Outcome<std::optional<MovementBoundaryPrediction>>
predict_movement_field_boundary(const FieldExactContactReadContext &x,
                                const FieldBattleCatalog &catalog,
                                const StableId &battle,
                                const StableId &prefix_id,
                                ExactArithmeticBudget b) {
  return guard<std::optional<MovementBoundaryPrediction>>(
      [&]() -> std::optional<MovementBoundaryPrediction> {
        auto geometry =
            take(read_field_boundary_geometry(x, catalog, battle, b));
        const auto *p = x.movement.prefixes.find(prefix_id);
        if (!p)
          fail("BoundaryOriginalPrefixRequired", prefix_id);
        if (!std::holds_alternative<MovementActive>(p->lifecycle))
          return {};
        const auto *map =
            std::get_if<RunningMovementMap>(&p->activity_world_map);
        const auto *unit = x.movement.units.find(p->unit_ref);
        if (!map || !unit ||
            !std::holds_alternative<MovementUnitOpen>(unit->status) ||
            compare(map->rate, {}, b) <= 0 ||
            execution_key(unit->execution) != execution_key(p->execution))
          fail("BoundaryOriginalActiveUnitRequired", prefix_id);
        for (const auto &row : x.movement.boundary_contacts.rows())
          if (row.second.prefix_id == prefix_id &&
              row.second.motion_source.unit_ref == p->unit_ref &&
              row.second.original_field_source.id == battle)
            return {};
        const auto end =
            take(predict_movement_boundary(x.movement, prefix_id, b));
        if (!geometry.created_at)
          fail("BoundaryOriginalCreationMomentRequired", battle);
        const auto begin = take(compare_world_moments(
                               map->world_from, *geometry.created_at, b)) < 0
                               ? *geometry.created_at
                               : map->world_from;
        const MomentSpan span{begin, end};
        if (take(compare_world_moments(span.from, span.to, b)) >= 0)
          return {};
        const auto from = cursor_at(*map, begin, b),
                   to = cursor_at(*map, end, b);
        CheckpointVector<Piece> pieces;
        if (const auto *road = std::get_if<RoadMovementBasis>(&p->basis))
          pieces = road_pieces(x, catalog, *road, from, to, geometry, b);
        else
          pieces = terrain_pieces(std::get<TerrainCostBasis>(p->basis), from,
                                  to, geometry, b);
        const auto entry = intersect(pieces, geometry, b);
        if (!entry)
          return {};
        const auto duration =
            take(elapsed_world_duration(span.from, span.to, b));
        const auto at = take(
            add_world_duration(span.from, {mul(duration.value, *entry, b)}, b));
        if (take(compare_world_moments(at, geometry.actual_now, b)) < 0)
          fail("UnsettledPastMovementBoundary", prefix_id);


        auto sample_lease = CheckpointMemoryCharge(0);
        auto sample = *p;
        sample.cursor = cursor_at(*map, at, b);
        MovementBoundaryPrediction result;


        std::visit(
            [&](const auto &basis) {  },
            p->basis);


        (result.retained_prediction).grow(0);
        auto anchor = take(read_movement_position(sample, b));
        result.contact.prefix_id = prefix_id;
        result.contact.original_field_source = {"military",
                                                "field_battle_session", battle};
        result.contact.field_hash = geometry.session->field_hash;
        result.contact.prediction_span = span;
        result.contact.entry_fraction = *entry;
        result.contact.occurred_at = at;
        result.contact.motion_source = {p->unit_ref, basis_id(p->basis),
                                        p->activity_world_map};
        result.contact.actual_anchor = std::move(anchor);
        return result;
      });
}
Status validate_movement_boundary_geometry_source(
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    const MovementBoundaryContact &actual, ExactArithmeticBudget b) {
  return status([&] {
    const SourceRef source{"travel", "movement_boundary_contact", actual.id};
    auto geometry = take(read_field_boundary_geometry_at_original_contact(
        x, catalog, source, b));
    if (actual.field_hash != geometry.session->field_hash)
      fail("BoundaryOriginalFieldHashMismatch", actual.id);
    const auto *prefix = x.movement.prefixes.find(actual.prefix_id);
    const auto *unit = x.movement.units.find(actual.motion_source.unit_ref);
    const auto *mapping =
        std::get_if<RunningMovementMap>(&actual.motion_source.original_mapping);
    const TravelExertionBasis *effort = nullptr;
    if (unit)
      for (const auto &ref : unit->effort_basis_refs) {
        const auto *row = x.movement.exertion_bases.find(ref.id);
        if (row &&
            basis_id(row->actual_basis) == actual.motion_source.basis_ref) {
          if (effort)
            fail("BoundaryOriginalBasisAmbiguous", actual.id);
          effort = row;
        }
      }
    if (!prefix || !unit || !mapping || !effort ||
        effort->unit_ref != unit->id ||
        execution_key(prefix->execution) != execution_key(unit->execution) ||
        execution_key(effort->execution) != execution_key(unit->execution) ||
        compare(mapping->rate, {}, b) <= 0 ||
        !source_valid(mapping->rate_source))
      fail("BoundaryOriginalMotionSourceRequired", actual.id);
    const auto cut = take(read_actual_common_fact_position(x.common, source));
    const auto effort_head = take(read_actual_common_fact_position(
        x.common, {"travel", "travel_exertion_basis", effort->id}));
    if (effort_head >= cut ||
        take(compare_world_moments(effort->opened_at, mapping->world_from, b)) >
            0)
      fail("BoundaryOriginalBasisOrderMismatch", actual.id);
    bool supported =
        same_moment(effort->opened_at, mapping->world_from, b) &&
        compare(effort->opened_cursor, mapping->cursor_at_from, b) == 0 &&
        same_source(effort->original_rule_source, mapping->rate_source);
    CanonicalFraction elapsed, at_cursor = mapping->cursor_at_from;
    WorldMoment at = mapping->world_from;
    for (const auto &work : unit->activity_spans) {
      if (take(compare_world_moments(work.span.from, mapping->world_from, b)) <
          0) {
        if (take(compare_world_moments(work.span.to, mapping->world_from, b)) >
            0)
          fail("BoundaryActivityMapCutMismatch", actual.id);
        elapsed = add(
            elapsed,
            take(elapsed_world_duration(work.span.from, work.span.to, b)).value,
            b);
        continue;
      }
      if (take(compare_world_moments(work.span.from, actual.occurred_at, b)) >=
          0)
        break;
      if (work.basis_ref != actual.motion_source.basis_ref ||
          !same_moment(work.span.from, at, b) ||
          take(compare_world_moments(work.span.to, actual.occurred_at, b)) >
              0 ||
          compare(work.cursor_from, at_cursor, b) != 0 ||
          compare(work.rate, mapping->rate, b) != 0 ||
          compare(work.budget_rate, mapping->budget_rate, b) != 0 ||
          !same_source(work.rate_source, mapping->rate_source))
        fail("BoundaryActivityWitnessMismatch", actual.id);
      auto end = cursor_at(*mapping, work.span.to, b);
      if (compare(end, work.cursor_to, b) != 0)
        fail("BoundaryActivityCursorMismatch", actual.id);
      at_cursor = end;
      at = work.span.to;
      supported = true;
    }
    if (!supported) {
      if (mapping->rate_source.domain == "travel" &&
          mapping->rate_source.kind == "personal_movement_control") {
        const auto *control =
            x.movement.personal_controls.find(mapping->rate_source.id);
        supported =
            control && control->unit_ref == unit->id &&
            control->basis_ref == actual.motion_source.basis_ref &&
            control->operation == PersonalMovementControlOperation::resume &&
            same_moment(control->occurred_at, mapping->world_from, b) &&
            compare(control->cursor, mapping->cursor_at_from, b) == 0;
      } else if (mapping->rate_source.domain == "travel" &&
                 mapping->rate_source.kind == "group_movement_control") {
        const auto *control = x.groups.controls.find(mapping->rate_source.id);
        const auto *execution =
            std::get_if<GroupMovementExecution>(&unit->execution);
        supported = control && execution &&
                    control->journey_id == execution->journey_id &&
                    control->operation == GroupControlOperation::resume &&
                    same_moment(control->occurred_at, mapping->world_from, b);
        if (supported) {
          MovementPrefixState sample;
          sample.basis = effort->actual_basis;
          sample.cursor = mapping->cursor_at_from;
          sample.lifecycle = MovementActive{};
          supported = take(
              same_spatial_anchor(control->actual_anchor,
                                  take(read_movement_position(sample, b)), b));
        }
      }
      if(!supported&&mapping->rate_source.domain=="travel"&&mapping->rate_source.kind=="group_military_supply_rebind"){
        const auto* change=take(read_group_military_supply_rebind_source(x.common,x.carriers,x.movement,x.groups,mapping->rate_source,1,b));
        const auto* ledger=std::get_if<TerrainUnitBudget>(&unit->budget);
        supported=change->prefix_ref==actual.prefix_id&&change->unit_ref==unit->id&&change->basis_ref==actual.motion_source.basis_ref&&change->disposition==GroupSupplyRateDisposition::running&&same_moment(change->occurred_at,mapping->world_from,b)&&compare(change->original_cursor,mapping->cursor_at_from,b)==0&&compare(mapping->budget_rate,integer(change->after_budget,b),b)==0&&compare(mapping->rate,ledger?add(integer(change->after_budget,b),ledger->opening_credit.value,b):integer(1,b),b)==0;
      }
      if (supported && take(read_actual_common_fact_position(
                           x.common, mapping->rate_source)) >= cut)
        supported = false;
    }
    if (!supported || !same_moment(at, actual.occurred_at, b))
      fail("BoundaryActivityOriginalSourceRequired", actual.id);
    const auto unit_remaining = sub(integer(1, b), elapsed, b);
    const auto basis_remaining = div(
        sub(basis_limit(effort->actual_basis, b), mapping->cursor_at_from, b),
        mapping->rate, b);
    const auto remaining = compare(unit_remaining, basis_remaining, b) < 0
                               ? unit_remaining
                               : basis_remaining;
    const auto end =
        take(add_world_duration(mapping->world_from, {remaining}, b));
    if (!geometry.created_at)
      fail("BoundaryOriginalCreationMomentRequired", actual.id);
    const auto begin = take(compare_world_moments(mapping->world_from,
                                                  *geometry.created_at, b)) < 0
                           ? *geometry.created_at
                           : mapping->world_from;
    if (!same_moment(actual.prediction_span.from, begin, b) ||
        !same_moment(actual.prediction_span.to, end, b))
      fail("BoundaryCanonicalPredictionSpanMismatch", actual.id);
    const auto from = cursor_at(*mapping, begin, b),
               to = cursor_at(*mapping, end, b);
    CheckpointVector<Piece> pieces;
    if (const auto *road =
            std::get_if<RoadMovementBasis>(&effort->actual_basis))
      pieces = road_pieces(x, catalog, *road, from, to, geometry, b);
    else
      pieces = terrain_pieces(std::get<TerrainCostBasis>(effort->actual_basis),
                              from, to, geometry, b);
    auto hit = intersect(pieces, geometry, b);
    if (!hit || compare(*hit, actual.entry_fraction, b) != 0)
      fail("BoundaryOriginalGeometryMismatch", actual.id);
    const auto duration = take(elapsed_world_duration(begin, end, b));
    const auto moment =
        take(add_world_duration(begin, {mul(duration.value, *hit, b)}, b));
    if (!same_moment(moment, actual.occurred_at, b) ||
        take(compare_world_moments(moment, geometry.actual_now, b)) > 0)
      fail("BoundaryOriginalMomentMismatch", actual.id);


    auto sample_lease = CheckpointMemoryCharge(0);
    MovementPrefixState sample;
    sample.basis = effort->actual_basis;
    sample.cursor = cursor_at(*mapping, moment, b);
    sample.lifecycle = MovementActive{};


    auto anchor_lease = CheckpointMemoryCharge(0);
    auto anchor = take(read_movement_position(sample, b));
    if (!take(same_actual_movement_position(anchor, actual.actual_anchor, b)))
      fail("BoundaryOriginalAnchorMismatch", actual.id);
  });
}
Outcome<MovementBoundaryFieldGeometryRead>
read_movement_boundary_field_geometry(const FieldExactContactReadContext &x,
                                      const FieldBattleCatalog &catalog,
                                      const MovementBoundaryContact &actual,
                                      ExactArithmeticBudget b) {
  return guard<MovementBoundaryFieldGeometryRead>([&] {
    take(validate_movement_boundary_geometry_source(x, catalog, actual, b));
    const SourceRef source{"travel", "movement_boundary_contact", actual.id};
    const auto geometry = take(read_field_boundary_geometry_at_original_contact(
        x, catalog, source, b));
    const auto *unit = x.movement.units.find(actual.motion_source.unit_ref);
    const TravelExertionBasis *effort = nullptr;
    for (const auto &ref : unit->effort_basis_refs) {
      const auto *candidate = x.movement.exertion_bases.find(ref.id);
      if (candidate &&
          basis_id(candidate->actual_basis) == actual.motion_source.basis_ref)
        effort = candidate; // uniqueness already proved by original validator
    }
    const auto &mapping =
        std::get<RunningMovementMap>(actual.motion_source.original_mapping);
    const auto from = cursor_at(mapping, actual.prediction_span.from, b);
    const auto to = cursor_at(mapping, actual.prediction_span.to, b);
    CheckpointVector<Piece> pieces;
    if (const auto *road =
            std::get_if<RoadMovementBasis>(&effort->actual_basis))
      pieces = road_pieces(x, catalog, *road, from, to, geometry, b);
    else
      pieces = terrain_pieces(std::get<TerrainCostBasis>(effort->actual_basis),
                              from, to, geometry, b);
    std::optional<PointI> projected;
    for (const auto &piece : pieces) {
      if (compare(actual.entry_fraction, piece.from, b) < 0 ||
          compare(actual.entry_fraction, piece.to, b) > 0)
        continue;
      const auto width = sub(piece.to, piece.from, b);
      const auto u =
          compare(width, {}, b) == 0
              ? CanonicalFraction{}
              : div(sub(actual.entry_fraction, piece.from, b), width, b);
      const auto point = Point{interpolate(piece.a.x, piece.z.x, u, b),
                               interpolate(piece.a.y, piece.z.y, u, b)};
      const auto integer_projection = [&](const Signed &v) {
        auto result = take(floor_fraction(v.magnitude, b));
        return v.negative ? -result : result;
      };
      if (!projected)
        projected =
            PointI{integer_projection(point.x), integer_projection(point.y)};
      const auto dx = minus(piece.z.x, piece.a.x, b),
                 dy = minus(piece.z.y, piece.a.y, b);
      const auto magnitude = compare(dx.magnitude, dy.magnitude, b) >= 0
                                 ? dx.magnitude
                                 : dy.magnitude;
      if (compare(magnitude, {}, b) == 0)
        continue;
      // Only angle projection is quantized; actual contact M/anchor/fraction
      // remain the original rational values. Feed the original integer CORDIC
      // a scale-normalized direction, preserving sub-dm moving pieces.
      const auto scale = integer(Integer(1) << 60, b);
      const auto component = [&](const Signed &v) {
        const auto n = take(
            floor_fraction(div(mul(v.magnitude, scale, b), magnitude, b), b));
        return v.negative ? -n : n;
      };
      return MovementBoundaryFieldGeometryRead{
          {integer_projection(point.x), integer_projection(point.y)},
          field_direction_mdeg({component(dx), component(dy)})};
    }
    if (projected)
      return MovementBoundaryFieldGeometryRead{*projected, {}};
    fail("MovementBoundaryOriginalPointRequired", actual.id);
  });
}
Outcome<PointI> read_movement_boundary_field_point(
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    const MovementBoundaryContact &actual, ExactArithmeticBudget b) {
  return guard<PointI>([&] {
    return take(read_movement_boundary_field_geometry(x, catalog, actual, b))
        .point_dm;
  });
}
} // namespace hanmo::resident
