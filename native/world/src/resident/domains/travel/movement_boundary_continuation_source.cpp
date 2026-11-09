#include "movement_boundary_continuation_source.hpp"
#include "movement_contact_source_equal.hpp"
#include "movement_exact_detail.hpp"
#include "resident/domains/interaction/immutable_authored_flags.hpp"
#include "resident/domains/interaction/interaction_model.hpp"
#include "resident/domains/military/field_battle/field_author_data.hpp"
#include "resident/domains/military/field_battle/field_boundary_continuation_source.hpp"
#include "resident/domains/military/field_battle/field_segment_settlement.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/storage/common_store.hpp"
#include "spatial_carrier.hpp"
#include <bit>
#include <cstdint>
#include <limits>
namespace hanmo::resident {
using namespace movement_detail;
Outcome<MovementBoundaryContinuationRead>
read_movement_boundary_continuation_source(
    const FieldExactContactReadContext &x,
    const PoliticalRelationsReadContext &relations,
    const FieldBattleCatalog &catalog, const SourceRef &source,
    ExactArithmeticBudget b) {
  return guard<MovementBoundaryContinuationRead>([&] {
    if (source.domain != "travel" ||
        source.kind != "movement_boundary_continuation")
      fail("MovementBoundaryContinuationSourceRequired", source.id);
    const auto *row = x.movement.boundary_continuations.find(source.id);
    if (!row || row->id != source.id || row->held ||
        row->closed_unit_ref == row->new_unit_ref)
      fail("MovementBoundaryContinuationSourceRequired", source.id);
    const auto cause = take(read_field_boundary_continuation_source(
        x, relations, catalog, row->field_contact, b, &source));
    if (!same_source(row->boundary_contact,
                     {"travel", "movement_boundary_contact",
                      cause.original.contact->id}) ||
        !same_source(row->terminal_segment, {"military", "field_battle_segment",
                                             cause.terminal->fact_id}) ||
        !same_moment(row->occurred_at, cause.actual_now, b))
      fail("MovementBoundaryContinuationCauseMismatch", source.id);
    MovementBoundaryContinuationRead out;
    out.continuation = row;
    out.previous = take(read_movement_boundary_original_suffix(
        x, catalog, row->boundary_contact, b));
    const auto &old = *out.previous.original.unit;
    out.new_unit = x.movement.units.find(row->new_unit_ref);
    if (row->prefix_id != out.previous.original.contact->prefix_id ||
        row->closed_unit_ref != old.id || !out.new_unit ||
        old.ordinal == std::numeric_limits<Integer>::max() ||
        out.new_unit->ordinal != old.ordinal + 1 ||
        execution_key(old.execution) != execution_key(out.new_unit->execution))
      fail("MovementBoundaryContinuationUnitMismatch", source.id);
    const auto cut = take(read_actual_common_fact_position(x.common, source));
    if (take(read_actual_common_fact_position(
            x.common,
            SourceRef{"travel", "movement_unit", out.new_unit->id})) <= cut ||
        out.new_unit->effort_basis_refs.size() != 1)
      fail("MovementBoundaryContinuationOpeningRequired", source.id);
    const auto &ref = out.new_unit->effort_basis_refs.front();
    out.new_basis = x.movement.exertion_bases.find(ref.id);
    if (ref.domain != "travel" || ref.kind != "travel_exertion_basis" ||
        !out.new_basis || out.new_basis->unit_ref != out.new_unit->id ||
        execution_key(out.new_basis->execution) !=
            execution_key(old.execution) ||
        !same_source(out.new_basis->original_rule_source, source) ||
        !same_moment(out.new_basis->opened_at, row->occurred_at, b) ||
        compare(out.new_basis->opened_cursor, {}, b) != 0 ||
        take(read_actual_common_fact_position(x.common, ref)) <= cut)
      fail("MovementBoundaryContinuationOriginalBasisRequired", source.id);
    // The immutable opening starts at the real exact stop, never its integer
    // projection.
    const auto &start = std::visit(
        [](const auto &v) -> const MovementAnchor & { return v.start_anchor; },
        out.new_basis->actual_basis);
    if (!take(
            same_spatial_anchor(start, out.previous.closed->actual_anchor, b)))
      fail("MovementBoundaryContinuationDiscontinuous", source.id);
    if (std::visit([](const auto &v)
                       -> const StableId & { return v.original_unit_ref; },
                   out.new_basis->actual_basis) != out.new_unit->id)
      fail("MovementBoundaryContinuationBasisOwnerMismatch", source.id);
    if (out.previous.road) {
      const auto *road =
          std::get_if<RoadMovementBasis>(&out.new_basis->actual_basis);
      const auto &prior = *out.previous.road;
      if (!road || !road->continued || road->basis_ref.empty() ||
          road->basis_ref == prior.basis_ref ||
          road->catalog_rule_ref != prior.catalog_rule_ref ||
          road->original_inputs.actor_id != prior.original_inputs.actor_id ||
          road->original_inputs.action_id != prior.original_inputs.action_id ||
          road->original_inputs.edge_id != prior.original_inputs.edge_id ||
          road->original_inputs.origin != prior.original_inputs.origin ||
          !road->original_inputs.moving)
        fail("MovementBoundaryOriginalRoadContinuationRequired", source.id);
      const auto &a = prior.original_inputs.params;
      const auto &v = road->original_inputs.params;
      using movement_contact_detail::same;
      if (!same(a.destination, v.destination) || !same(a.via, v.via) ||
          a.path != v.path || a.edge_index != v.edge_index ||
          a.payer_wallet_id != v.payer_wallet_id ||
          a.returning != v.returning || a.reverse_first != v.reverse_first ||
          a.has_terrain != v.has_terrain ||
          a.has_reverse_first != v.has_reverse_first ||
          a.has_encounter_id != v.has_encounter_id ||
          a.has_met_actions != v.has_met_actions ||
          a.met_actions != v.met_actions || a.field_order != v.field_order ||
          v.paused || !v.has_paused || !v.encounter_id.empty())
        fail("MovementBoundaryOriginalRoadParamsChanged", source.id);
      const auto *edge = x.travel.roads.find(prior.original_inputs.edge_id);
      if (!edge || edge->duration_ticks < 1 ||
          edge->duration_ticks > std::numeric_limits<Integer>::max() / 100 ||
          out.new_basis->original_fatigue_per_unit != edge->fatigue_per_tick)
        fail("MovementBoundaryOriginalRoadRuleRequired", source.id);
      CanonicalFraction begin;
      if (const auto *anchor = std::get_if<RoadFractionalAnchor>(&start)) {
        if (anchor->edge_id != edge->id ||
            anchor->origin != prior.original_inputs.origin)
          fail("MovementBoundaryOriginalRoadAnchorMismatch", source.id);
        begin = anchor->progress;
      } else if (const auto *anchor =
                     std::get_if<OriginalMovementAnchor>(&start)) {
        const auto *point = std::get_if<OnRouteLocation>(&anchor->location);
        if (!point || point->edge_id != edge->id ||
            point->origin != prior.original_inputs.origin ||
            point->progress < 0)
          fail("MovementBoundaryOriginalRoadAnchorMismatch", source.id);
        begin = integer(point->progress, b);
      } else
        fail("MovementBoundaryOriginalRoadAnchorRequired", source.id);
      const auto maximum = integer(edge->duration_ticks * 100, b);
      const auto step = integer(100, b);
      if (compare(begin, maximum, b) > 0)
        fail("MovementBoundaryOriginalRoadProgressInvalid", source.id);
      auto end = v.returning
                     ? (compare(begin, step, b) < 0 ? CanonicalFraction{}
                                                    : sub(begin, step, b))
                     : add(begin, step, b);
      if (compare(end, maximum, b) > 0)
        end = maximum;
      const auto *last = std::get_if<RoadFractionalAnchor>(&road->end_anchor);
      if (compare(begin, end, b) == 0 ||
          compare(road->exact_start_progress, begin, b) != 0 ||
          compare(road->exact_end_progress, end, b) != 0 ||
          road->original_inputs.start != take(floor_fraction(begin, b)) ||
          road->original_inputs.end != take(floor_fraction(end, b)) || !last ||
          last->edge_id != edge->id ||
          last->origin != prior.original_inputs.origin ||
          compare(last->progress, end, b) != 0 ||
          !same_source(last->actual_source, source))
        fail("MovementBoundaryOriginalRoadSuffixMismatch", source.id);
    } else {
      const auto *terrain =
          std::get_if<TerrainCostBasis>(&out.new_basis->actual_basis);
      if (!terrain || !out.previous.terrain || !terrain->continued ||
          terrain->basis_ref.empty() ||
          terrain->basis_ref == out.previous.terrain->basis_ref ||
          terrain->catalog_rule_ref != "continued_terrain/1")
        fail("MovementBoundaryOriginalBasisKindChanged", source.id);
      const auto &a = out.previous.terrain->original_inputs;
      const auto &v = terrain->original_inputs;
      const auto consumed = out.previous.consumed_points;
      using movement_contact_detail::same;
      if (consumed < 0 || consumed >= Integer(a.points.size()) ||
          a.route_index > std::numeric_limits<Integer>::max() - consumed ||
          v.route_index != a.route_index + consumed || v.plan_id != a.plan_id ||
          v.map_id != a.map_id || v.origin != a.origin ||
          v.payer_wallet_id != a.payer_wallet_id || v.mode != a.mode ||
          !same(v.destination, a.destination) ||
          v.has_encounter_id != a.has_encounter_id ||
          v.has_met_actions != a.has_met_actions ||
          v.met_actions != a.met_actions || v.field_order != a.field_order ||
          v.paused || !v.encounter_id.empty() || v.travel_credit != 0 ||
          v.budget <= 0 || v.fatigue_per_tick != a.fatigue_per_tick ||
          v.points.size() != a.points.size() - std::size_t(consumed) ||
          terrain->original_point_count != Integer(v.points.size()) ||
          compare(terrain->parameter_limit.value, integer(v.budget, b), b) !=
              0 ||
          out.new_basis->original_fatigue_per_unit != v.fatigue_per_tick)
        fail("MovementBoundaryOriginalTerrainSuffixChanged", source.id);
      for (std::size_t n = 0; n < v.points.size(); ++n)
        if (!same(v.points[n], a.points[n + std::size_t(consumed)]))
          fail("MovementBoundaryOriginalTerrainPointsChanged", source.id);
      if (std::holds_alternative<PersonalMovementExecution>(old.execution) &&
          (v.budget != a.budget || v.money_per_tick != a.money_per_tick))
        fail("MovementBoundaryOriginalPersonalTerrainRateChanged", source.id);
      if (!x.original_flags || !x.original_interaction || !x.travel_services)
        fail("MovementBoundaryImmutableTerrainFlagsRequired", source.id);
      take(validate_immutable_interaction_flags(
          *x.original_flags, catalog.identity, *x.original_interaction));
      const auto &flags = x.travel_services->terrain_flags();
      if (flags.size() != x.original_flags->rows().size())
        fail("MovementBoundaryTerrainServicesFlagsMismatch", source.id);
      for (const auto &flag : x.original_flags->rows()) {
        const auto found = flags.find(flag.id);
        if (found == flags.end() || found->second != flag.value)
          fail("MovementBoundaryTerrainServicesFlagsMismatch", source.id);
      }
      CheckpointMemoryCharge rebuilt_memory;
      const auto expected = take(make_continued_terrain_cost_basis(
          x.travel, *x.travel_services, out.previous.closed->actual_anchor, v,
          integer(v.budget, b), terrain->basis_ref, out.new_unit->id, source, b,
          &rebuilt_memory));
      if (terrain->actual_consumed_total != expected.actual_consumed_total ||
          terrain->reached_points != expected.reached_points ||
          terrain->original_end_credit != expected.original_end_credit ||
          terrain->original_closing_policy !=
              expected.original_closing_policy ||
          terrain->pieces.size() != expected.pieces.size())
        fail("MovementBoundaryTerrainOriginalGeometryMismatch", source.id);
      const auto exact_double = [](double first, double second) {
        return std::bit_cast<std::uint64_t>(first) ==
               std::bit_cast<std::uint64_t>(second);
      };
      for (std::size_t n = 0; n < terrain->pieces.size(); ++n) {
        const auto &actual = terrain->pieces[n];
        const auto &original = expected.pieces[n];
        if (actual.kind != original.kind ||
            compare(actual.cost_from, original.cost_from, b) != 0 ||
            compare(actual.cost_to, original.cost_to, b) != 0 ||
            actual.segment.point_index != original.segment.point_index ||
            !exact_double(actual.segment.from.x, original.segment.from.x) ||
            !exact_double(actual.segment.from.y, original.segment.from.y) ||
            !exact_double(actual.segment.to.x, original.segment.to.x) ||
            !exact_double(actual.segment.to.y, original.segment.to.y) ||
            !exact_double(actual.segment.start_cost,
                          original.segment.start_cost) ||
            !exact_double(actual.segment.end_cost, original.segment.end_cost))
          fail("MovementBoundaryTerrainOriginalPieceMismatch", source.id);
      }
      if (const auto *expected_end =
              std::get_if<TerrainContinuousAnchor>(&expected.end_anchor)) {
        const auto *actual_end =
            std::get_if<TerrainContinuousAnchor>(&terrain->end_anchor);
        if (!actual_end || actual_end->map_id != expected_end->map_id ||
            actual_end->origin != expected_end->origin ||
            actual_end->x_bits != expected_end->x_bits ||
            actual_end->y_bits != expected_end->y_bits ||
            actual_end->geometry_profile_ref !=
                expected_end->geometry_profile_ref ||
            !same_source(actual_end->actual_prefix_close_source,
                         expected_end->actual_prefix_close_source))
          fail("MovementBoundaryTerrainOriginalEndpointMismatch", source.id);
      } else {
        const auto *integer_expected_end =
            std::get_if<OriginalMovementAnchor>(&expected.end_anchor);
        const auto *actual_end =
            std::get_if<OriginalMovementAnchor>(&terrain->end_anchor);
        if (!integer_expected_end || !actual_end ||
            !same_source(actual_end->actual_source,
                         integer_expected_end->actual_source) ||
            !take(same_spatial_anchor(terrain->end_anchor, expected.end_anchor,
                                      b)))
          fail("MovementBoundaryTerrainOriginalEndpointMismatch", source.id);
      }
      // Group load/cost still needs the original profile's full resource cut.
      // Exact geometry from genuine immutable flags does not prove that rate.
    }
    for (const auto &other : x.movement.boundary_continuations.rows())
      if (other.first != source.id &&
          (same_source(other.second.boundary_contact, row->boundary_contact) ||
           other.second.new_unit_ref == row->new_unit_ref))
        fail("MovementBoundaryContinuationDuplicate", source.id);
    return out;
  });
}
Status validate_movement_boundary_continuation_sources(
    const FieldExactContactReadContext &x,
    const PoliticalRelationsReadContext &relations,
    const FieldBattleCatalog &catalog, ExactArithmeticBudget b) {
  return status([&] {
    for (const auto &item : x.movement.boundary_continuations.rows()) {
      if (item.first != item.second.id)
        fail("MovementBoundaryContinuationKeyMismatch", item.first);
      take(read_movement_boundary_continuation_source(
          x, relations, catalog,
          {"travel", "movement_boundary_continuation", item.first}, b));
    }
    for (const auto &head : x.common.state().facts.rows())
      if (head.second.kind == "movement_boundary_continuation" &&
          (head.second.domain != "travel" ||
           !x.movement.boundary_continuations.find(head.first)))
        fail("MovementBoundaryContinuationUnownedHead", head.first);
    for (const auto &item : x.movement.exertion_bases.rows()) {
      const auto &basis = item.second;
      if (basis.original_rule_source.kind != "movement_boundary_continuation")
        continue;
      const auto actual = take(read_movement_boundary_continuation_source(
          x, relations, catalog, basis.original_rule_source, b));
      if (actual.new_basis != &basis)
        fail("MovementBoundaryContinuationUnownedOpening", item.first);
    }
    for (const auto &item : x.movement.spatial_bindings.rows()) {
      const auto &binding = item.second;
      if (binding.binding_source.kind != "movement_boundary_continuation")
        continue;
      const auto actual = take(read_movement_boundary_continuation_source(
          x, relations, catalog, binding.binding_source, b));
      const auto *writer = std::get_if<WorldMovementWriter>(&binding.writer);
      if (!writer || writer->prefix_id != actual.continuation->prefix_id)
        fail("MovementBoundaryContinuationWriterMismatch", item.first);
      if (const auto *personal = std::get_if<PersonalMovementExecution>(
              &actual.new_unit->execution)) {
        if (!actual.new_basis->original_action ||
            binding.subject_kind != "actor" ||
            actual.new_basis->original_action->id != personal->action_id ||
            binding.subject_id != actual.new_basis->original_action->actor_id)
          fail("MovementBoundaryContinuationWriterActorMismatch", item.first);
      } else if (const auto *group = std::get_if<GroupMovementExecution>(
                     &actual.new_unit->execution)) {
        if (item.first != spatial_carrier_key(group->carrier))
          fail("MovementBoundaryContinuationWriterCarrierMismatch", item.first);
      } else
        fail("MovementBoundaryContinuationExecutionRequired", item.first);
    }
    for (const auto &[id, row] : x.field.boundary_contacts)
      if (std::holds_alternative<FieldExactBoundaryOrigin>(row.motion) &&
          row.status == FieldBoundaryStatus::Released) {
        if (!row.exact_continuation_source)
          fail("MovementBoundaryContinuationReleaseSourceRequired", id);
        const auto result = take(read_movement_boundary_continuation_source(
            x, relations, catalog, *row.exact_continuation_source, b));
        if (result.continuation->field_contact.id != id)
          fail("MovementBoundaryContinuationReleaseOwnerMismatch", id);
      }
  });
}
} // namespace hanmo::resident
