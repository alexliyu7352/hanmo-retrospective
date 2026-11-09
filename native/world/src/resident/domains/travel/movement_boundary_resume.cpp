#include "movement_boundary_resume.hpp"
#include "movement_action_time.hpp"
#include "movement_exact_detail.hpp"
#include "resident/domains/military/field_battle/field_boundary_continuation_source.hpp"
#include "resident/domains/military/field_battle/field_segment_settlement.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_store.hpp"
#include "spatial_carrier.hpp"
#include <limits>
namespace hanmo::resident {
using namespace movement_detail;
Outcome<SourceRef> resume_exact_personal_field_boundary_in_candidate(
    CommonStore &common, TravelState &travel, MovementExactState &movement,
    FieldBattleWorldState &field, const TravelCatalog &catalog,
    const TravelServices &services, const FieldExactContactReadContext &x,
    const PoliticalRelationsReadContext &relations,
    const FieldBattleCatalog &field_catalog, const SourceRef &field_contact,
    CheckpointMemoryCharge &retained, ExactArithmeticBudget b) {
  return guard<SourceRef>([&] {
    if (&common != &x.common || &movement != &x.movement ||
        &field != &x.field || &catalog != &x.travel ||
        x.travel_services != &services)
      fail("MovementBoundaryContinuationSameWorldRequired", field_contact.id);
    for (const auto &item : movement.boundary_continuations.rows())
      if (same_source(item.second.field_contact, field_contact)) {
        SourceRef source{"travel", "movement_boundary_continuation",
                         item.first};
        take(read_movement_boundary_continuation_source(
            x, relations, field_catalog, source, b));
        return source;
      }
    const auto ready = take(read_field_boundary_continuation_source(
        x, relations, field_catalog, field_contact, b));
    const auto suffix = take(read_movement_boundary_original_suffix(
        x, field_catalog,
        {"travel", "movement_boundary_contact", ready.original.contact->id},
        b));
    const auto &old_prefix = *suffix.original.prefix;
    const auto *personal =
        std::get_if<PersonalMovementExecution>(&old_prefix.execution);
    const auto *closed = std::get_if<MovementClosed>(&old_prefix.lifecycle);
    const auto *action =
        personal ? common.action(personal->action_id) : nullptr;
    const auto *members = action ? common.members(action->actor_id) : nullptr;
    const auto *binding =
        action ? movement.spatial_bindings.find("actor:" + action->actor_id)
               : nullptr;
    const auto *stable =
        binding ? std::get_if<StableMovementWriter>(&binding->writer) : nullptr;
    const SourceRef contact{"travel", "movement_boundary_contact",
                            ready.original.contact->id};
    if (!personal || !action || !members || !closed || !stable ||
        old_prefix.unit_ref != suffix.original.unit->id ||
        !same_source(closed->original_result_ref, contact) ||
        !same_source(binding->binding_source, contact) ||
        !take(same_spatial_anchor(stable->anchor, suffix.closed->actual_anchor,
                                  b)) ||
        suffix.original.unit->ordinal == std::numeric_limits<Integer>::max())
      fail("MovementBoundaryOriginalPersonalWriterRequired", field_contact.id);
    if (!suffix.original.basis->original_action ||
        suffix.original.basis->original_action->id != action->id ||
        suffix.original.basis->original_action->actor_id != action->actor_id)
      fail("MovementBoundaryOriginalActionRequired", field_contact.id);
    // Reserve the ordinary retained copies before constructing their payloads.


    (retained).grow(0);
    auto prefix = old_prefix;
    const auto id = take(common.next_id("movement_boundary_continuation"));
    const auto unit_id = take(common.next_id("movement_unit"));
    const auto basis_id_value = take(common.next_id("movement_basis"));
    const SourceRef source{"travel", "movement_boundary_continuation", id};
    MovementUnitState next;
    next.id = unit_id;
    next.execution = prefix.execution;
    next.ordinal = suffix.original.unit->ordinal + 1;
    CanonicalFraction rate, funding;
    Integer fatigue = 0;
    if (suffix.terrain) {
      if (!travel.actions.find(action->id))
        fail("MovementBoundaryOriginalTerrainActionRequired", action->id);
      auto params = suffix.terrain->original_inputs;
      if (suffix.consumed_points >= Integer(params.points.size()))
        fail("MovementBoundaryArrivalRequired", action->id);
      params.points.erase(params.points.begin(),
                          params.points.begin() + suffix.consumed_points);
      if (params.route_index >
          std::numeric_limits<Integer>::max() - suffix.consumed_points)
        fail("MovementBoundaryRouteIndexOverflow", action->id);
      params.route_index += suffix.consumed_points;
      params.travel_credit = 0;
      params.paused = false;
      params.encounter_id.clear();
      if (params.budget <= 0)
        fail("MovementBoundaryOriginalBudgetRequired", action->id);
      funding = integer(params.budget, b);
      rate = funding;
      fatigue = params.fatigue_per_tick;
      prefix.basis = take(make_continued_terrain_cost_basis(
          catalog, services, suffix.closed->actual_anchor, params, rate,
          basis_id_value, unit_id, source, b, &retained));
      next.budget = TerrainUnitBudget{};
      travel.actions.set(action->id, std::move(params));
    } else if (suffix.road) {
      if (!travel.road_actions.find(action->id))
        fail("MovementBoundaryOriginalRoadActionRequired", action->id);
      auto road = *suffix.road;
      auto params = road.original_inputs.params;
      params.paused = false;
      params.has_paused = true;
      params.encounter_id.clear();
      const auto *edge = catalog.roads.find(road.original_inputs.edge_id);
      if (!edge || edge->duration_ticks < 1 ||
          edge->duration_ticks > std::numeric_limits<Integer>::max() / 100)
        fail("MovementBoundaryOriginalRoadRequired", action->id);
      CanonicalFraction start;
      if (const auto *a = std::get_if<RoadFractionalAnchor>(
              &suffix.closed->actual_anchor)) {
        if (a->edge_id != edge->id || a->origin != road.original_inputs.origin)
          fail("MovementBoundaryRoadAnchorMismatch", action->id);
        start = a->progress;
      } else if (const auto *a = std::get_if<OriginalMovementAnchor>(
                     &suffix.closed->actual_anchor)) {
        const auto *p = std::get_if<OnRouteLocation>(&a->location);
        if (!p || p->edge_id != edge->id ||
            p->origin != road.original_inputs.origin)
          fail("MovementBoundaryRoadAnchorMismatch", action->id);
        start = integer(p->progress, b);
      } else
        fail("MovementBoundaryRoadAnchorRequired", action->id);
      const auto maximum = integer(edge->duration_ticks * 100, b),
                 step = integer(100, b);
      if (compare(start, maximum, b) > 0)
        fail("MovementBoundaryRoadProgressInvalid", action->id);
      auto end = params.returning
                     ? (compare(start, step, b) < 0 ? CanonicalFraction{}
                                                    : sub(start, step, b))
                     : add(start, step, b);
      if (compare(end, maximum, b) > 0)
        end = maximum;
      if (compare(start, end, b) == 0)
        fail("MovementBoundaryArrivalRequired", action->id);
      road.basis_ref = basis_id_value;
      road.original_unit_ref = unit_id;
      road.continued = true;
      road.exact_start_progress = start;
      road.exact_end_progress = end;
      road.start_anchor = suffix.closed->actual_anchor;
      road.end_anchor = RoadFractionalAnchor{
          edge->id, road.original_inputs.origin, end, source};
      road.original_inputs.start = take(floor_fraction(start, b));
      road.original_inputs.end = take(floor_fraction(end, b));
      road.original_inputs.params = params;
      road.original_inputs.moving = true;
      prefix.basis = std::move(road);
      rate = integer(1, b);
      fatigue = edge->fatigue_per_tick;
      travel.road_actions.set(action->id, std::move(params));
    } else
      fail("MovementBoundaryOriginalBasisRequired", action->id);
    prefix.cursor = {};
    prefix.unit_ref = unit_id;
    prefix.last_settled_at = ready.actual_now;
    prefix.lifecycle = MovementActive{};
    prefix.activity_world_map =
        RunningMovementMap{ready.actual_now, {}, rate, funding, source};
    TravelExertionBasis effort{
        unit_id + ":effort", unit_id,  prefix.execution, *action,
        ready.actual_now,    *members, fatigue,          source,
        prefix.basis,        {}};
    next.effort_basis_refs.push_back(
        {"travel", "travel_exertion_basis", effort.id});
    MovementBoundaryContinuationSource continued{
        id,
        prefix.id,
        suffix.original.unit->id,
        unit_id,
        contact,
        field_contact,
        {"military", "field_battle_segment", ready.terminal->fact_id},
        ready.actual_now,
        false};
    movement.boundary_continuations.set(id, std::move(continued));
    const auto effort_id = effort.id;
    movement.exertion_bases.set(effort_id, std::move(effort));
    movement.units.set(unit_id, std::move(next));
    const auto prefix_id = prefix.id;
    movement.prefixes.set(prefix_id, std::move(prefix));
    movement.spatial_bindings.set(
        "actor:" + action->actor_id,
        {"actor", action->actor_id, WorldMovementWriter{prefix_id}, source});
    take(common.append_fact({id, "movement_boundary_continuation", "travel"}));
    take(common.append_fact({unit_id, "movement_unit", "travel"}));
    take(common.append_fact(
        {unit_id + ":effort", "travel_exertion_basis", "travel"}));
    auto &boundary = field.boundary_contacts.at(field_contact.id);
    boundary.status = FieldBoundaryStatus::Released;
    boundary.exact_continuation_source = source;
    take(sync_exact_personal_action_time_projection(
        common, travel, movement, catalog, services, action->id, b));
    take(read_movement_boundary_continuation_source(x, relations, field_catalog,
                                                    source, b));
    return source;
  });
}
} // namespace hanmo::resident
