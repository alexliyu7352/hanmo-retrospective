// 真实原单位边界才开下单位；尚未消费basis跨单位续同一成本尺。
#include "movement_exact_detail.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace hanmo::resident {
using namespace movement_detail;
namespace {
terrain::Mode mode(const StableId &id) {
  if (id == "foot")
    return terrain::Mode::foot;
  if (id == "army")
    return terrain::Mode::army;
  if (id == "boat")
    return terrain::Mode::boat;
  fail("InvalidTravelMode", id);
}
terrain::PointD point(const MovementAnchor &anchor) {
  if (const auto *t = std::get_if<TerrainContinuousAnchor>(&anchor))
    return {take(decode_movement_binary64(t->x_bits)),
            take(decode_movement_binary64(t->y_bits))};
  if (const auto *a = std::get_if<OriginalMovementAnchor>(&anchor))
    if (const auto *p = std::get_if<WildernessLocation>(&a->location))
      return {double(p->point.x), double(p->point.y)};
  fail("TerrainMovementAnchorRequired");
}
} // namespace
Outcome<TerrainCostBasis> make_continued_terrain_cost_basis(
    const TravelCatalog &catalog, const TravelServices &services,
    const MovementAnchor &start, const TerrainTravelParams &params,
    const CanonicalFraction &maximum, const StableId &id, const StableId &unit,
    const SourceRef &source, ExactArithmeticBudget b,
    CheckpointMemoryCharge *retained) {
  return guard<TerrainCostBasis>([&] {
    if (id.empty() || unit.empty() || !source_valid(source) ||
        params.points.empty() || compare(maximum, {}, b) <= 0)
      fail("ContinuedTerrainSourceRequired", id);
    const auto *map = catalog.maps.find(params.map_id);
    if (!map)
      fail("UnknownWorldMap", params.map_id);
    auto m = mode(params.mode);
    auto here = point(start);
    if (!terrain::point_valid_continuous_prepared(*map, here, m,
                                                  services.terrain_flags()))
      fail("TerrainBlocked", id);
    if (retained) {


      (*retained).grow(0);
    }
    CheckpointVector<TerrainCostPiece> pieces;
    TerrainCostBasis out;
    out.basis_ref = id;
    out.original_inputs = params;
    out.start_anchor = start;
    out.parameter_limit.value = maximum;
    out.continued = true;
    out.catalog_rule_ref = "continued_terrain/1";
    out.original_unit_ref = unit;
    out.original_point_count = Integer(params.points.size());
    Integer consumed = 0, reached = 0;
    auto push = [&](TerrainCostPieceKind kind, terrain::PointD from,
                    terrain::PointD to, const CanonicalFraction &lo,
                    const CanonicalFraction &hi, Integer point_index) {
      if (compare(lo, hi, b) == 0)
        return;
      pieces.push_back({kind,
                        {{from.x, from.y},
                         {to.x, to.y},
                         take(fraction_to_binary64(lo, b)),
                         take(fraction_to_binary64(hi, b)),
                         point_index},
                        lo,
                        hi});
    };
    while (reached < Integer(params.points.size())) {
      auto available = sub(maximum, integer(consumed, b), b);
      if (compare(available, {}, b) <= 0)
        break;
      auto target = params.points[std::size_t(reached)];
      terrain::PointD target_d{double(target.x), double(target.y)};
      auto profile = take(terrain::segment_profile_continuous_prepared(
          *map, here, target_d, m, services.terrain_flags()));
      if (profile.cost == 0) {
        ++reached;
        continue;
      }
      auto landing = target_d;
      if (compare(integer(profile.cost, b), available, b) > 0)
        landing = take(terrain::partial_point_continuous_prepared(
            *map, here, target, m, services.terrain_flags(), available, b));
      if (landing.x == here.x && landing.y == here.y)
        break;
      profile = take(terrain::segment_profile_continuous_prepared(
          *map, here, landing, m, services.terrain_flags()));
      if (compare(integer(profile.cost, b), available, b) > 0)
        fail("ContinuedTerrainCostExceedsBudget", id);
      for (const auto &p : profile.spans)
        push(TerrainCostPieceKind::moving_cost, p.from, p.to,
             take(fraction_from_binary64(double(consumed) + p.start_cost, b)),
             take(fraction_from_binary64(double(consumed) + p.end_cost, b)),
             reached);
      if (profile.raw_cost < double(profile.cost))
        push(TerrainCostPieceKind::rounding_padding, landing, landing,
             take(fraction_from_binary64(double(consumed) + profile.raw_cost,
                                         b)),
             integer(consumed + profile.cost, b),
             (landing.x == target_d.x && landing.y == target_d.y) ? reached + 1
                                                                  : reached);
      consumed += profile.cost;
      here = landing;
      if (here.x == target_d.x && here.y == target_d.y)
        ++reached;
    }
    if (compare(integer(consumed, b), maximum, b) < 0)
      push(TerrainCostPieceKind::unused_budget_tail, here, here,
           integer(consumed, b), maximum, reached);
    out.actual_consumed_total = consumed;
    out.reached_points = reached;
    out.original_closing_policy =
        reached == out.original_point_count
            ? TerrainOriginalClosingPolicy::arrival_discard
            : TerrainOriginalClosingPolicy::carry_at_unit_boundary;
    out.original_end_credit =
        reached == out.original_point_count
            ? 0
            : take(floor_fraction(sub(maximum, integer(consumed, b), b), b));
    if (retained) {


      if (here.x != std::floor(here.x) || here.y != std::floor(here.y)) {


      }
      (*retained).grow(0);
    }
    out.pieces = std::vector<TerrainCostPiece>(pieces.begin(), pieces.end());
    if (here.x == std::floor(here.x) && here.y == std::floor(here.y))
      out.end_anchor = OriginalMovementAnchor{
          WildernessLocation{
              params.map_id, params.origin, {Integer(here.x), Integer(here.y)}},
          source};
    else
      out.end_anchor =
          TerrainContinuousAnchor{params.map_id,
                                  params.origin,
                                  take(encode_movement_binary64(here.x)),
                                  take(encode_movement_binary64(here.y)),
                                  source,
                                  out.catalog_rule_ref};
    return out;
  });
}
Outcome<StableId> continue_exact_personal_terrain_unit(
    CommonWrite &common, TravelState &travel, MovementExactState &state,
    const TravelCatalog &catalog, const TravelServices &services,
    const StableId &id, const WorldMoment &at, ExactArithmeticBudget b) {
  return guard<StableId>([&] {
    const auto *old_prefix = state.prefixes.find(id);
    const auto *old_unit =
        old_prefix ? state.units.find(old_prefix->unit_ref) : nullptr;
    const auto *close =
        old_unit ? state.unit_closes.find(old_unit->id + ":close") : nullptr;
    if (!old_prefix || !old_unit || !close ||
        close->kind != MovementCloseKind::carry ||
        !same_moment(close->occurred_at, at, b) ||
        !same_moment(old_prefix->last_settled_at, at, b) ||
        !std::holds_alternative<TerrainCostBasis>(old_prefix->basis))
      fail("MovementCarrySourceRequired", id);
    take(read_verified_travel_exertion(state, old_unit->id + ":exertion", b));
    auto prefix = *old_prefix;
    const auto previous = *old_unit;
    auto terrain = std::get<TerrainCostBasis>(prefix.basis);
    const auto *old_ledger = std::get_if<TerrainUnitBudget>(&previous.budget);
    if (!old_ledger)
      fail("TerrainUnitBudgetRequired", id);
    auto credit = sub(
        sub(add(old_ledger->opening_credit.value, old_ledger->funded.value, b),
            old_ledger->spent.value, b),
        old_ledger->discarded.value, b);
    auto funding =
        std::holds_alternative<RunningMovementMap>(prefix.activity_world_map)
            ? std::get<RunningMovementMap>(prefix.activity_world_map)
                  .budget_rate
            : std::get<HeldMovementMap>(prefix.activity_world_map)
                  .resume_budget_rate;
    const auto action_id = execution_action(prefix.execution);
    const auto *action_read = common.action(action_id);
    const auto *members_read =
        action_read ? common.members(action_read->actor_id) : nullptr;
    if (!action_read || !members_read)
      fail("MovementActionRequired", action_id);
    auto action = *action_read;
    auto members = *members_read;
    auto unit_id = take(common.next_id("movement_unit"));
    SourceRef source{"travel", "movement_unit_close", previous.id + ":close"};
    TerrainTravelParams params = terrain.original_inputs;
    params.budget = take(floor_fraction(funding, b));
    params.travel_credit = take(floor_fraction(credit, b));
    if (compare(integer(params.budget, b), funding, b) != 0 ||
        params.budget < 1)
      fail("OriginalMovementBudgetRequired", id);
    if (compare(prefix.cursor, terrain.parameter_limit.value, b) == 0) {
      if (terrain.reached_points == terrain.original_point_count)
        fail("MovementArrivalRequiredBeforeCarry", id);
      params.points.erase(params.points.begin(),
                          params.points.begin() + terrain.reached_points);
      params.route_index += terrain.reached_points;
      auto start = take(read_movement_position(prefix, b));
      auto basis_id = take(common.next_id("movement_basis"));
      auto maximum = add(funding, credit, b);
      bool original = false;
      if (const auto *a = std::get_if<OriginalMovementAnchor>(&start))
        if (const auto *w = std::get_if<WildernessLocation>(&a->location)) {
          if (compare(integer(params.travel_credit, b), credit, b) == 0) {
            auto trace = take(
                terrain_geometry_motion(catalog, services, w->point, params,
                                        true, action.actor_id, action.id));
            prefix.basis = take(make_original_terrain_cost_basis(
                trace, basis_id, "terrain_original/1", unit_id, source, b));
            original = true;
          }
        }
      if (!original)
        prefix.basis = take(make_continued_terrain_cost_basis(
            catalog, services, start, params, maximum, basis_id, unit_id,
            source, b));
      prefix.cursor = {};
    }
    MovementUnitState next;
    next.id = unit_id;
    next.execution = prefix.execution;
    next.ordinal = previous.ordinal + 1;
    TerrainUnitBudget ledger;
    ledger.opening_credit.value = credit;
    next.budget = ledger;
    TravelExertionBasis effort{unit_id + ":effort",
                               unit_id,
                               prefix.execution,
                               action,
                               at,
                               members,
                               params.fatigue_per_tick,
                               source,
                               prefix.basis,
                               prefix.cursor};
    next.effort_basis_refs.push_back(
        {"travel", "travel_exertion_basis", effort.id});
    prefix.unit_ref = unit_id;
    prefix.last_settled_at = at;
    prefix.lifecycle = MovementActive{};
    prefix.activity_world_map = RunningMovementMap{
        at, prefix.cursor, add(funding, credit, b), funding, source};
    travel.actions.set(action_id, std::move(params));
    {
      const auto stored_key = effort.id;
      state.exertion_bases.set(stored_key, std::move(effort));
    }
    state.units.set(unit_id, std::move(next));
    take(common.append_fact({unit_id, "movement_unit", "travel"}));
    take(common.append_fact(
        {unit_id + ":effort", "travel_exertion_basis", "travel"}));
    {
      const auto stored_key = prefix.id;
      state.prefixes.set(stored_key, std::move(prefix));
    }
    return unit_id;
  });
}

Outcome<StableId> continue_exact_terrain_basis_in_unit(
    CommonWrite &common, MovementExactState &state,
    const TravelCatalog &catalog, const TravelServices &services,
    const StableId &id, const WorldMoment &at, ExactArithmeticBudget b) {
  return guard<StableId>([&] {
    const auto *found = state.prefixes.find(id);
    const auto *unit_read = found ? state.units.find(found->unit_ref) : nullptr;
    const auto *old =
        found ? std::get_if<TerrainCostBasis>(&found->basis) : nullptr;
    const auto *running =
        found ? std::get_if<RunningMovementMap>(&found->activity_world_map)
              : nullptr;
    if (!found || !unit_read || !old || !running ||
        !std::holds_alternative<MovementActive>(found->lifecycle) ||
        !std::holds_alternative<MovementUnitOpen>(unit_read->status) ||
        unit_read->effort_basis_refs.empty() ||
        !same_moment(found->last_settled_at, at, b) ||
        compare(found->cursor, old->parameter_limit.value, b) != 0 ||
        compare(unit_read->active_elapsed.value, integer(1, b), b) >= 0 ||
        old->reached_points >= old->original_point_count)
      fail("MovementInUnitBoundaryRequired", id);
    auto prefix = *found;
    auto unit = *unit_read;
    const auto *first =
        state.exertion_bases.find(unit.effort_basis_refs.front().id);
    if (!first)
      fail("TravelExertionBasisRequired", id);
    auto actual = take(read_movement_position(prefix, b));
    auto params = old->original_inputs;
    params.points.erase(params.points.begin(),
                        params.points.begin() + old->reached_points);
    params.route_index += old->reached_points;
    params.travel_credit = 0;
    const auto transition_id =
                   take(common.next_id("movement_basis_transition")),
               next_basis = take(common.next_id("movement_basis")),
               effort_id = take(common.next_id("travel_exertion_basis"));
    const SourceRef source{"travel", "movement_basis_transition",
                           transition_id};
    const auto old_basis = basis_id(prefix.basis);
    const auto old_cursor = prefix.cursor;
    auto maximum =
        mul(running->rate, sub(integer(1, b), unit.active_elapsed.value, b), b);
    prefix.basis = take(make_continued_terrain_cost_basis(
        catalog, services, actual, params, maximum, next_basis, unit.id, source,
        b));
    prefix.cursor = {};
    prefix.activity_world_map =
        RunningMovementMap{at, {}, running->rate, running->budget_rate, source};
    TravelExertionBasis effort{effort_id,
                               unit.id,
                               prefix.execution,
                               first->original_action,
                               at,
                               first->actual_participants,
                               first->original_fatigue_per_unit,
                               source,
                               prefix.basis,
                               {}};
    MovementBasisTransitionSource transition{transition_id,
                                             id,
                                             unit.id,
                                             prefix.execution,
                                             at,
                                             old_basis,
                                             old_cursor,
                                             next_basis,
                                             actual,
                                             unit.effort_basis_refs.back()};
    unit.effort_basis_refs.push_back(
        {"travel", "travel_exertion_basis", effort_id});
    state.exertion_bases.set(effort_id, std::move(effort));
    state.basis_transitions.set(transition_id, std::move(transition));
    const auto unit_id = unit.id;
    state.units.set(unit_id, std::move(unit));
    state.prefixes.set(id, std::move(prefix));
    take(common.append_fact(
        {transition_id, "movement_basis_transition", "travel"}));
    take(common.append_fact({effort_id, "travel_exertion_basis", "travel"}));
    return transition_id;
  });
}
Outcome<WorldMoment> predict_movement_boundary(const MovementExactState &state,
                                               const StableId &id,
                                               ExactArithmeticBudget b) {
  return guard<WorldMoment>([&] {
    const auto *p = state.prefixes.find(id);
    const auto *u = p ? state.units.find(p->unit_ref) : nullptr;
    const auto *map =
        p ? std::get_if<RunningMovementMap>(&p->activity_world_map) : nullptr;
    if (!p || !u || !map ||
        !std::holds_alternative<MovementUnitOpen>(u->status) ||
        !std::holds_alternative<MovementActive>(p->lifecycle))
      fail("MovementNotRunning", id);
    auto unit_remaining = sub(integer(1, b), u->active_elapsed.value, b);
    auto basis_remaining =
        div(sub(basis_limit(p->basis, b), p->cursor, b), map->rate, b);
    auto remaining = compare(unit_remaining, basis_remaining, b) < 0
                         ? unit_remaining
                         : basis_remaining;
    return take(add_world_duration(p->last_settled_at, {remaining}, b));
  });
}
} // namespace hanmo::resident
namespace hanmo::resident {
using namespace movement_detail;
Outcome<StableId> continue_exact_personal_road_unit(
    CommonWrite &common, TravelState &travel, MovementExactState &state,
    const TravelCatalog &catalog, const TravelServices &services,
    const StableId &id, const WorldMoment &at, ExactArithmeticBudget b) {
  return guard<StableId>([&] {
    const auto *old = state.prefixes.find(id);
    const auto *old_unit = old ? state.units.find(old->unit_ref) : nullptr;
    const auto *close =
        old_unit ? state.unit_closes.find(old_unit->id + ":close") : nullptr;
    if (!old || !old_unit || !close ||
        close->kind != MovementCloseKind::carry ||
        !same_moment(close->occurred_at, at, b) ||
        !std::holds_alternative<RoadMovementBasis>(old->basis) ||
        compare(old->cursor, integer(1, b), b) != 0)
      fail("MovementRoadCarryRequired", id);
    take(read_verified_travel_exertion(state, old_unit->id + ":exertion", b));
    auto prefix = *old;
    auto prior = *old_unit;
    if (prior.ordinal == std::numeric_limits<Integer>::max())
      fail("MovementUnitOrdinalOverflow", prior.id);
    auto prior_basis = std::get<RoadMovementBasis>(prefix.basis);
    auto action_id = execution_action(prefix.execution);
    const auto *read = common.action(action_id);
    const auto *members_read = read ? common.members(read->actor_id) : nullptr;
    if (!read || !members_read)
      fail("MovementActionRequired", action_id);
    const auto action = *read;
    const auto members = *members_read;
    auto params = prior_basis.original_inputs.params;
    const auto actual_anchor = take(read_movement_position(prefix, b));
    auto location = take(project_movement_location(actual_anchor, b));
    const auto *fractional = std::get_if<RoadFractionalAnchor>(&actual_anchor);
    const bool retain_fraction =
        fractional &&
        compare(fractional->progress,
                integer(take(floor_fraction(fractional->progress, b)), b),
                b) != 0;
    // A true fractional position is strictly inside this edge. Finishing the
    // old integer projection here would silently discard its remaining part.
    if (!retain_fraction &&
        take(finish_road_leg(catalog, at.half_day_tick, params, location)))
      fail("MovementArrivalRequiredBeforeCarry", id);
    const auto *here = std::get_if<OnRouteLocation>(&location);
    const auto *edge = here ? catalog.roads.find(here->edge_id) : nullptr;
    if (!edge || edge->duration_ticks < 1 ||
        edge->duration_ticks > std::numeric_limits<Integer>::max() / 100)
      fail("MovementRoadSourceRequired", id);
    if (!params.returning && !road_is_open(*edge, at.half_day_tick))
      fail("RoadBlocked", edge->id);
    auto end = params.returning ? std::max<Integer>(0, here->progress - 100)
                                : std::min<Integer>(edge->duration_ticks * 100,
                                                    here->progress + 100);
    if (end == here->progress && !retain_fraction)
      fail("RoadBlocked", edge->id);
    auto unit_id = take(common.next_id("movement_unit")),
         basis = take(common.next_id("movement_basis"));
    SourceRef source{"travel", "movement_unit_close", prior.id + ":close"};
    RoadTravelMotion trace{action.actor_id, action.id, edge->id, here->origin,
                           here->progress,  end,       true,     params};
    auto next_basis = take(make_original_road_movement_basis(
        trace, basis, "road_original/1", unit_id, source, b));
    if (retain_fraction) {
      if (fractional->edge_id != edge->id ||
          fractional->origin != here->origin || !prior_basis.continued)
        fail("MovementOriginalFractionalCarryRequired", id);
      const auto maximum = integer(edge->duration_ticks * 100, b);
      const auto step = integer(100, b);
      if (compare(fractional->progress, maximum, b) >= 0)
        fail("MovementOriginalFractionalCarryRequired", id);
      auto exact_end = params.returning
                           ? (compare(fractional->progress, step, b) < 0
                                  ? CanonicalFraction{}
                                  : sub(fractional->progress, step, b))
                           : add(fractional->progress, step, b);
      if (compare(exact_end, maximum, b) > 0)
        exact_end = maximum;
      next_basis.continued = true;
      next_basis.exact_start_progress = fractional->progress;
      next_basis.exact_end_progress = exact_end;
      next_basis.start_anchor = actual_anchor;
      next_basis.end_anchor =
          RoadFractionalAnchor{edge->id, here->origin, exact_end, source};
      next_basis.original_inputs.end = take(floor_fraction(exact_end, b));
    }
    prefix.basis = std::move(next_basis);
    prefix.cursor = {};
    prefix.unit_ref = unit_id;
    prefix.last_settled_at = at;
    prefix.lifecycle = MovementActive{};
    prefix.activity_world_map =
        RunningMovementMap{at, {}, integer(1, b), {}, source};
    MovementUnitState unit;
    unit.id = unit_id;
    unit.execution = prefix.execution;
    unit.ordinal = prior.ordinal + 1;
    TravelExertionBasis effort{
        unit_id + ":effort",    unit_id, prefix.execution, action, at, members,
        edge->fatigue_per_tick, source,  prefix.basis,     {}};
    unit.effort_basis_refs.push_back(
        {"travel", "travel_exertion_basis", effort.id});
    travel.road_actions.set(action_id, std::move(params));
    {
      const auto stored_key = effort.id;
      state.exertion_bases.set(stored_key, std::move(effort));
    }
    state.units.set(unit_id, std::move(unit));
    take(common.append_fact({unit_id, "movement_unit", "travel"}));
    take(common.append_fact(
        {unit_id + ":effort", "travel_exertion_basis", "travel"}));
    state.prefixes.set(id, std::move(prefix));
    return unit_id;
  });
}
} // namespace hanmo::resident
