#include "movement_remaining.hpp"
#include "movement_exact_detail.hpp"
#include "movement_result_read.hpp"
#include "spatial_carrier.hpp"
#include <limits>
namespace hanmo::resident {
using namespace movement_detail;
Outcome<MovementConditionalRemaining> read_movement_conditional_remaining(
    const MovementExactState &state, const TravelCatalog &catalog,
    const TravelServices &services, const StableId &id,
    ExactArithmeticBudget b) {
  return guard<MovementConditionalRemaining>([&] {
    const auto *found = state.prefixes.find(id);
    const auto *original_unit =
        found ? state.units.find(found->unit_ref) : nullptr;
    if (!found || !original_unit)
      fail("MovementRemainingSourceRequired", id);
    MovementConditionalRemaining out{found->execution,
                                     id,
                                     original_unit->id,
                                     {},
                                     {"travel", "movement_prefix", id}};
    auto prefix = *found;
    auto unit = *original_unit;
    if (const auto *closed = std::get_if<MovementClosed>(&found->lifecycle)) {
      const auto *close = state.unit_closes.find(found->unit_ref + ":close");
      if (closed->original_result_ref.domain == "travel" &&
          closed->original_result_ref.kind == "movement_arrival") {
        const auto *arrival =
            state.arrivals.find(closed->original_result_ref.id);
        if (arrival && is_field_group_contact_arrival(state, *arrival, b)) {
          take(read_verified_movement_arrival(
              state, closed->original_result_ref, 1, b));
          if (arrival->unit_ref != found->unit_ref ||
              !take(same_spatial_anchor(arrival->actual_anchor,
                                        closed->actual_anchor, b)))
            fail("MovementArrivalLifecycleMismatch", id);
          out.actual_basis_source = closed->original_result_ref;
          return out;
        }
      }
      if (close && (close->kind == MovementCloseKind::arrival ||
                    close->kind == MovementCloseKind::cancelled))
        return out;
      if (!close || close->kind != MovementCloseKind::contact ||
          closed->original_result_ref.kind != "movement_contact")
        fail("MovementRemainingContinuationRequired", id);
      const auto *contact = state.contacts.find(closed->original_result_ref.id);
      if (!contact)
        fail("MovementContactSourceRequired", id);
      auto anchor =
          take(read_exact_contact_endpoint_anchor(state, *contact, id, b));
      const auto *effort =
          unit.effort_basis_refs.empty()
              ? nullptr
              : state.exertion_bases.find(unit.effort_basis_refs.front().id);
      if (!effort)
        fail("ContactParticipantBasisRequired", id);
      const auto identity =
          effort->original_action
              ? effort->original_action->actor_id
              : spatial_carrier_key(
                    std::get<GroupMovementExecution>(prefix.execution).carrier);
      out.actual_basis_source = closed->original_result_ref;
      unit.active_elapsed = {};
      unit.activity_spans.clear();
      unit.status = MovementUnitOpen{};
      prefix.cursor = {};
      prefix.lifecycle = MovementActive{};
      if (contact->original_hit.road) {
        const auto &ends = *contact->original_hit.road;
        const auto *end = ends.first.actor_id == identity    ? &ends.first
                          : ends.second.actor_id == identity ? &ends.second
                                                             : nullptr;
        if (!end)
          fail("MovementContactSideRequired", id);
        const auto *edge = catalog.roads.find(end->trace.edge_id);
        if (!edge || edge->duration_ticks < 1 ||
            edge->duration_ticks > std::numeric_limits<Integer>::max() / 100)
          fail("MovementRoadSourceRequired", id);
        auto trace = end->trace;
        trace.start = end->anchor.progress;
        trace.end = trace.params.returning
                        ? std::max<Integer>(0, trace.start - 100)
                        : std::min<Integer>(edge->duration_ticks * 100,
                                            trace.start + 100);
        trace.moving = true;
        prefix.basis = take(make_original_road_movement_basis(
            trace, "conditional_contact", "road_original/1", unit.id,
            out.actual_basis_source, b));
        prefix.activity_world_map = RunningMovementMap{contact->occurred_at,
                                                       {},
                                                       integer(1, b),
                                                       {},
                                                       out.actual_basis_source};
      } else {
        const auto &hit = contact->original_hit;
        const auto *end = hit.first.actor_id == identity    ? &hit.first
                          : hit.second.actor_id == identity ? &hit.second
                                                            : nullptr;
        if (!end || end->trace.params.budget <= 0)
          fail("MovementContactSideRequired", id);
        auto params = end->trace.params;
        params.travel_credit = 0;
        if (params.points.empty())
          return out;
        auto rate = integer(params.budget, b);
        prefix.basis = take(make_continued_terrain_cost_basis(
            catalog, services, anchor, params, rate, "conditional_contact",
            unit.id, out.actual_basis_source, b));
        prefix.activity_world_map = RunningMovementMap{
            contact->occurred_at, {}, rate, rate, out.actual_basis_source};
        unit.budget = TerrainUnitBudget{};
      }
    }
    auto map =
        std::holds_alternative<RunningMovementMap>(prefix.activity_world_map)
            ? std::get<RunningMovementMap>(prefix.activity_world_map)
            : RunningMovementMap{
                  prefix.last_settled_at, prefix.cursor,
                  std::get<HeldMovementMap>(prefix.activity_world_map)
                      .resume_rate,
                  std::get<HeldMovementMap>(prefix.activity_world_map)
                      .resume_budget_rate,
                  std::get<HeldMovementMap>(prefix.activity_world_map)
                      .resume_rate_source};
    auto one = integer(1, b);
    if (const auto *road = std::get_if<RoadMovementBasis>(&prefix.basis)) {
      auto remaining = div(sub(one, prefix.cursor, b), map.rate, b);
      auto params = road->original_inputs.params;
      auto progress = road->exact_end_progress;
      auto edge = catalog.roads.find(road->original_inputs.edge_id);
      if (!edge || edge->duration_ticks < 1 ||
          edge->duration_ticks > std::numeric_limits<Integer>::max() / 100)
        fail("MovementRoadSourceRequired", id);
      auto length = integer(edge->duration_ticks * 100, b);
      auto distance = params.returning ? progress : sub(length, progress, b);
      auto per_unit = div(distance, integer(100, b), b);
      auto whole = take(ceil_half_day_duration({per_unit}, b));
      remaining = add(remaining, integer(whole, b), b);
      if (!(params.returning && !params.reverse_first)) {
        for (Integer i = params.edge_index + 1; i < Integer(params.path.size());
             ++i) {
          const auto *next = catalog.roads.find(params.path[std::size_t(i)]);
          if (!next || next->duration_ticks < 1)
            fail("MovementRoadSourceRequired", id);
          remaining = add(remaining, integer(next->duration_ticks, b), b);
        }
      }
      out.conditional_remaining.value = remaining;
      return out;
    }
    const auto *original_ledger = std::get_if<TerrainUnitBudget>(&unit.budget);
    if (!original_ledger)
      fail("TerrainUnitBudgetRequired", id);
    auto ledger = *original_ledger;
    auto budget_rate = map.budget_rate;
    std::size_t work = 0;
    for (;;) {
      if (++work > b.max_steps)
        throw Failure{common_failure("MovementForecastResourceExhausted", id,
                                     ErrorKind::internal)
                          .error};
      const auto basis = std::get<TerrainCostBasis>(prefix.basis);
      auto unit_remaining = sub(one, unit.active_elapsed.value, b);
      auto basis_remaining =
          div(sub(basis.parameter_limit.value, prefix.cursor, b), map.rate, b);
      auto duration = compare(unit_remaining, basis_remaining, b) < 0
                          ? unit_remaining
                          : basis_remaining;
      auto before = take(movement_consumed_cost(basis, prefix.cursor, b));
      prefix.cursor = add(prefix.cursor, mul(map.rate, duration, b), b);
      auto after = take(movement_consumed_cost(basis, prefix.cursor, b));
      unit.active_elapsed.value = add(unit.active_elapsed.value, duration, b);
      ledger.funded.value =
          add(ledger.funded.value, mul(budget_rate, duration, b), b);
      ledger.spent.value = add(ledger.spent.value, sub(after, before, b), b);
      out.conditional_remaining.value =
          add(out.conditional_remaining.value, duration, b);
      bool basis_end =
               compare(prefix.cursor, basis.parameter_limit.value, b) == 0,
           unit_end = compare(unit.active_elapsed.value, one, b) == 0;
      if (basis.reached_points == basis.original_point_count &&
          compare(after, integer(basis.actual_consumed_total, b), b) == 0 &&
          (basis_end || unit_end))
        return out;
      auto anchor = take(read_movement_position(prefix, b));
      if (unit_end) {
        auto credit =
            sub(sub(add(ledger.opening_credit.value, ledger.funded.value, b),
                    ledger.spent.value, b),
                ledger.discarded.value, b);
        ledger = TerrainUnitBudget{};
        ledger.opening_credit.value = credit;
        unit.active_elapsed = {};
        map.rate = add(budget_rate, credit, b);
      }
      if (basis_end) {
        auto params = basis.original_inputs;
        if (basis.reached_points < 0 ||
            basis.reached_points >= Integer(params.points.size()))
          fail("MovementRemainingRouteRequired", id);
        params.points.erase(params.points.begin(),
                            params.points.begin() + basis.reached_points);
        params.route_index += basis.reached_points;
        params.budget = take(floor_fraction(budget_rate, b));
        params.travel_credit =
            take(floor_fraction(ledger.opening_credit.value, b));
        auto maximum = mul(map.rate, sub(one, unit.active_elapsed.value, b), b);
        if (compare(maximum, {}, b) == 0)
          fail("MovementForecastNoProgress", id);
        bool original = false;
        if (unit_end && compare(integer(params.travel_credit, b),
                                ledger.opening_credit.value, b) == 0) {
          if (const auto *a = std::get_if<OriginalMovementAnchor>(&anchor))
            if (const auto *p = std::get_if<WildernessLocation>(&a->location)) {
              // 群体没有 Common Actor Action；沿原接敌读面使用同一执行键，
              // 不伪造行动，也不改变个人或其余执行类型的原身份拒绝。
              auto action = execution_action(prefix.execution);
              if (std::holds_alternative<GroupMovementExecution>(
                      prefix.execution))
                action = execution_key(prefix.execution);
              auto trace = take(terrain_geometry_motion(catalog, services,
                                                        p->point, params, true,
                                                        "forecast", action));
              prefix.basis = take(make_original_terrain_cost_basis(
                  trace, "forecast", "terrain_original/1", unit.id,
                  out.actual_basis_source, b));
              original = true;
            }
        }
        if (!original)
          prefix.basis = take(make_continued_terrain_cost_basis(
              catalog, services, anchor, params, maximum, "forecast", unit.id,
              out.actual_basis_source, b));
        prefix.cursor = {};
      } else if (!unit_end)
        fail("MovementForecastNoBoundary", id);
    }
  });
}
Outcome<RunStop>
read_movement_continuation_stop(const MovementExactState &state,
                                const StableId &id,
                                ExactArithmeticBudget budget) {
  return guard<RunStop>([&] {
    const auto *prefix = state.prefixes.find(id);
    if (!prefix)
      fail("MovementPrefixRequired", id);
    if (const auto *held = std::get_if<MovementHeld>(&prefix->lifecycle)) {
      if (!source_valid(held->original_reason_source))
        fail("MovementHoldSourceRequired", id);
      return held->original_reason_source.kind == "movement_contact"
                 ? RunStop::awaiting_input
                 : RunStop::paused;
    }
    if (const auto *closed = std::get_if<MovementClosed>(&prefix->lifecycle)) {
      const auto *source = state.unit_closes.find(prefix->unit_ref + ":close");
      if (closed->original_result_ref.domain == "travel" &&
          closed->original_result_ref.kind == "movement_arrival") {
        const auto *arrival =
            state.arrivals.find(closed->original_result_ref.id);
        if (arrival &&
            is_field_group_contact_arrival(state, *arrival, budget)) {
          take(read_verified_movement_arrival(
              state, closed->original_result_ref, 1, budget));
          if (arrival->unit_ref != prefix->unit_ref ||
              !take(same_spatial_anchor(arrival->actual_anchor,
                                        closed->actual_anchor, budget)))
            fail("MovementArrivalLifecycleMismatch", id);
          return RunStop::arrived;
        }
      }
      if (source) {
        if (!same_source(source->actual_cause, closed->original_result_ref))
          fail("MovementTerminalSourceMismatch", id);
        if (source->kind == MovementCloseKind::arrival) {
          take(read_verified_movement_arrival(state, source->actual_cause, 1,
                                              budget));
          return RunStop::arrived;
        }
        if (source->kind == MovementCloseKind::contact)
          return RunStop::awaiting_input;
        if (source->kind == MovementCloseKind::cancelled)
          return RunStop::cancelled;
      }
      if (closed->original_result_ref.kind == "group_movement_control")
        return RunStop::cancelled;
      fail("MovementTerminalSourceRequired", id);
    }
    return RunStop::awaiting_permit;
  });
}

} // namespace hanmo::resident
