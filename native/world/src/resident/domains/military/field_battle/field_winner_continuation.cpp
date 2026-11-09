#include "field_winner_continuation.hpp"
#include "field_army_exit_selection.hpp"
#include "field_author_data.hpp"
#include "field_binding_source.hpp"
#include "field_segment_settlement.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/military/training_exact.hpp"
#include "resident/domains/travel/group_movement.hpp"
#include "resident/domains/travel/movement_contact_source_equal.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/domains/travel/movement_physical_basis.hpp"
#include "resident/domains/travel/movement_result_read.hpp"
#include "resident/domains/travel/spatial_carrier.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace hanmo::resident {
using namespace movement_detail;
Outcome<FieldWinnerContinuationRead> read_field_winner_continuation(
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    const StableId &battle, const StableId &army,
    const SourceRef *actual_continuation, ExactArithmeticBudget b) {
  return guard<FieldWinnerContinuationRead>([&] {
    if (!x.periods || !x.clock.bound_to(x.common, x.military, x.field))
      fail("FieldWinnerOriginalPeriodOwnerRequired", battle);
    const auto session = x.field.sessions.find(battle);
    if (session == x.field.sessions.end() ||
        session->second.state != FieldBattleState::Ended ||
        !session->second.result || session->second.result->winner_side < 0)
      fail("FieldWinnerActualTerminalRequired", battle);
    if (take(read_field_army_terminal_exit_required(session->second, army)))
      fail("FieldWinnerArmyExitRallyRequired", army);
    FieldWinnerContinuationRead out;
    out.session = &session->second;
    const FieldBattleContactRecord *origin = nullptr;
    for (const auto &[id, row] : x.field.contacts)
      if (row.battle_id == battle) {
        if (origin || id != row.contact_id)
          fail("FieldWinnerContactAmbiguous", battle);
        origin = &row;
      }
    const auto exact =
        origin ? std::get_if<FieldExactGroupContactOrigin>(&origin->source)
               : nullptr;
    if (!exact)
      fail("FieldWinnerExactContactRequired", battle);
    out.original = take(read_field_exact_road_contact_source(
        x, exact->actual_contact, catalog.parameters, b));
    for (Integer i = 0; i < 2; ++i)
      if (origin->original_bindings[i].army_id == army) {
        if (out.binding)
          fail("FieldWinnerArmyAmbiguous", army);
        out.binding = &origin->original_bindings[i];
      }
    if (!out.binding || out.binding->battle_id != battle ||
        take(read_field_original_battle_binding(x.common, x.field, battle,
                                                army)) != out.binding)
      fail("FieldWinnerOriginalBindingRequired", army);
    bool survivor = false;
    for (const auto &[id, f] : out.session->formations)
      if (f.army_id == army) {
        if (f.side != out.session->result->winner_side)
          fail("FieldWinnerSideRequired", army);
        if (f.state == FieldFormationState::Routing)
          fail("FieldWinnerTerminalDepartureRequired", army);
        if (f.state != FieldFormationState::Withdrawn &&
            f.state != FieldFormationState::Destroyed)
          survivor = true;
      }
    if (!survivor)
      fail("FieldWinnerSurvivingFormationRequired", army);
    bool endpoint = false;
    for (Integer i = 0; i < 2; ++i)
      if (out.original.endpoints[i].army->id == army) {
        if (endpoint)
          fail("FieldWinnerEndpointAmbiguous", army);
        endpoint = true;
        out.endpoint = i;
      }
    if (!endpoint)
      fail("FieldWinnerOriginalArmyRequired", army);
    const auto &side = out.original.endpoints[out.endpoint];
    out.prefix = x.movement.prefixes.find(side.closed->prefix_id);
    out.closed_unit = x.movement.units.find(side.closed->unit_ref);
    if (!out.prefix || !out.closed_unit)
      fail("FieldWinnerOriginalUnitRequired", army);
    for (const auto &row : out.session->settlements)
      if (row.battle_fact_id) {
        if (out.terminal)
          fail("FieldWinnerTerminalAmbiguous", battle);
        const auto read = take(read_field_segment_settlement_source(
            x.common, x.military, x.field,
            {"military", "field_battle_segment", row.fact_id}, &x.clock));
        if (read.session != out.session || read.settlement != &row ||
            !read.terminal)
          fail("FieldWinnerTerminalSourceMismatch", battle);
        out.terminal = &row;
      }
    if (!out.terminal || out.terminal != &out.session->settlements.back())
      fail("FieldWinnerLastSettlementRequired", battle);
    const auto period = take(x.periods->read_training_period(
        out.terminal->applied_at.half_day_tick));
    if (period.from_tick != out.terminal->applied_at.half_day_tick ||
        period.from_tick == std::numeric_limits<Integer>::max() ||
        period.to_tick != period.from_tick + 1 ||
        period.original_opening.source_schema != 1 ||
        !same_moment(period.opening_at, out.terminal->applied_at, b))
      fail("FieldWinnerNextOpeningRequired", battle);
    const SourceRef terminal{"military", "field_battle_segment",
                             out.terminal->fact_id};
    const auto terminal_pos =
        take(read_actual_common_fact_position(x.common, terminal));
    const auto opening_pos = take(read_actual_common_fact_position(
        x.common, period.original_opening.source_ref));
    if (terminal_pos >= opening_pos)
      fail("FieldWinnerNextOpeningOrderRequired", battle);
    out.opening_at = period.opening_at;
    const auto now = take(x.clock.read_current_moment());
    if (take(compare_world_moments(now, out.opening_at, b)) < 0)
      fail("FieldWinnerOpeningInFuture", battle);
    const auto &before_road = *side.original_basis;
    auto expected_inputs = before_road.original_inputs;
    expected_inputs.params.paused = false;
    expected_inputs.params.has_paused = true;
    expected_inputs.params.encounter_id.clear();
    auto expected_anchor = side.closed->actual_anchor;
    auto expected_location =
        take(project_movement_location(expected_anchor, b));
    const auto *initial_location =
        std::get_if<OnRouteLocation>(&expected_location);
    const auto *expected_edge = x.travel.roads.find(expected_inputs.edge_id);
    if (!initial_location || !expected_edge ||
        expected_edge->duration_ticks < 1 ||
        expected_edge->duration_ticks >
            std::numeric_limits<Integer>::max() / 100)
      fail("FieldWinnerOriginalRoadRequired", army);
    CanonicalFraction expected_start;
    if (const auto *fractional =
            std::get_if<RoadFractionalAnchor>(&expected_anchor))
      expected_start = fractional->progress;
    else
      expected_start = integer(initial_location->progress, b);
    bool final_arrival = false;
    const auto edge_end = expected_inputs.params.returning
                              ? CanonicalFraction{}
                              : integer(expected_edge->duration_ticks * 100, b);
    if (compare(expected_start, edge_end, b) == 0) {
      const auto reached_site = expected_inputs.params.returning
                                    ? expected_edge->origin
                                    : expected_edge->destination;
      final_arrival =
          take(finish_road_leg(x.travel, out.opening_at.half_day_tick,
                               expected_inputs.params, expected_location));
      if (!final_arrival) {
        const auto *next_location =
            std::get_if<OnRouteLocation>(&expected_location);
        const auto *next_edge =
            next_location ? x.travel.roads.find(next_location->edge_id)
                          : nullptr;
        if (!next_edge || next_edge->origin != reached_site ||
            next_edge->id == expected_edge->id ||
            !road_is_open(*next_edge, out.opening_at.half_day_tick))
          fail("RoadBlocked", next_edge ? next_edge->id : expected_edge->id);
        expected_edge = next_edge;
        expected_inputs.edge_id = next_edge->id;
        expected_inputs.origin = next_location->origin;
        expected_start = integer(next_location->progress, b);
        expected_anchor = OriginalMovementAnchor{expected_location, terminal};
      }
    }
    if (!final_arrival && !expected_inputs.params.returning &&
        !road_is_open(*expected_edge, out.opening_at.half_day_tick))
      fail("RoadBlocked", expected_edge->id);
    if (actual_continuation && actual_continuation->domain == "travel" &&
        actual_continuation->kind == "movement_arrival") {
      const auto *row = x.movement.arrivals.find(actual_continuation->id);
      if (!row || !final_arrival ||
          !is_field_group_contact_arrival(x.movement, *row, b))
        fail("FieldWinnerActualArrivalRequired", army);
      take(read_verified_movement_arrival(x.movement, *actual_continuation, 1,
                                          b));
      const auto *anchor =
          std::get_if<OriginalMovementAnchor>(&row->actual_anchor);
      const MovementAnchor expected =
          OriginalMovementAnchor{expected_location, terminal};
      if (!anchor || !same_source(anchor->actual_source, terminal) ||
          row->prefix_id != out.prefix->id ||
          row->unit_ref != out.closed_unit->id ||
          !same_moment(row->occurred_at, out.opening_at, b) ||
          !take(same_spatial_anchor(expected, row->actual_anchor, b)) ||
          take(read_actual_common_fact_position(
              x.common, *actual_continuation)) <= opening_pos)
        fail("FieldWinnerArrivalSourceMismatch", army);
      const auto *journey_terminal =
          x.carriers.terminals.find(side.journey->journey_id);
      if (!journey_terminal || journey_terminal->source_schema != 1 ||
          journey_terminal->outcome != GroupJourneyTerminalOutcome::arrived ||
          !same_source(journey_terminal->actual_terminal_source,
                       *actual_continuation) ||
          !same_moment(journey_terminal->occurred_at, out.opening_at, b))
        fail("FieldWinnerArrivalTerminalRequired", army);
      const auto *original_effort = x.movement.exertion_bases.find(
          out.closed_unit->effort_basis_refs.front().id);
      const auto *stopped_map =
          std::get_if<HeldMovementMap>(&out.prefix->activity_world_map);
      if (!original_effort || out.prefix->unit_ref != row->unit_ref ||
          compare(out.prefix->cursor, row->cursor, b) != 0 || !stopped_map ||
          !same_source(stopped_map->actual_hold_source, *actual_continuation) ||
          !same_moment(stopped_map->effective_at, out.opening_at, b) ||
          compare(stopped_map->held_cursor, row->cursor, b) != 0 ||
          !take(same_original_movement_basis_payload(
              out.prefix->basis, original_effort->actual_basis, b)))
        fail("FieldWinnerArrivalOriginalPrefixMismatch", army);
      take(read_actual_group_journey_terminal(x.common, x.carriers, x.movement,
                                              x.groups,
                                              side.journey->journey_id, b));
      out.arrival = row;
    } else if (actual_continuation) {
      if (final_arrival)
        fail("FieldWinnerArrivalNotContinuation", army);
      const auto row =
          x.movement.contact_continuations.find(actual_continuation->id);
      if (actual_continuation->domain != "travel" ||
          actual_continuation->kind != "movement_contact_continuation" ||
          !row || row->id != actual_continuation->id || row->held ||
          row->resolution_schema != 1 ||
          !same_source(row->actual_resolution_source, terminal) ||
          row->contact_ref != out.original.contact->id ||
          row->prefix_id != out.prefix->id ||
          row->closed_unit_ref != out.closed_unit->id ||
          !same_moment(row->occurred_at, out.opening_at, b) ||
          take(read_actual_common_fact_position(
              x.common, *actual_continuation)) <= opening_pos)
        fail("FieldWinnerContinuationSourceMismatch", army);
      const auto next = x.movement.units.find(row->new_unit_ref);
      const auto effort = next && next->effort_basis_refs.size() == 1
                              ? x.movement.exertion_bases.find(
                                    next->effort_basis_refs.front().id)
                              : nullptr;
      if (!next ||
          out.closed_unit->ordinal == std::numeric_limits<Integer>::max() ||
          next->ordinal != out.closed_unit->ordinal + 1 ||
          execution_key(next->execution) !=
              execution_key(out.closed_unit->execution) ||
          !effort || !same_moment(effort->opened_at, row->occurred_at, b) ||
          !same_source(effort->original_rule_source, *actual_continuation) ||
          execution_key(effort->execution) != execution_key(next->execution) ||
          effort->unit_ref != next->id ||
          compare(effort->opened_cursor, CanonicalFraction{}, b) != 0)
        fail("FieldWinnerContinuationUnitMismatch", army);
      const auto *old_group =
          std::get_if<GroupMovementExecution>(&out.closed_unit->execution);
      const auto *new_group =
          std::get_if<GroupMovementExecution>(&next->execution);
      const auto *effort_group =
          std::get_if<GroupMovementExecution>(&effort->execution);
      if (!old_group || !new_group || !effort_group ||
          old_group->journey_id != side.journey->journey_id ||
          spatial_carrier_key(old_group->carrier) !=
              spatial_carrier_key(side.journey->carrier) ||
          spatial_carrier_key(new_group->carrier) !=
              spatial_carrier_key(old_group->carrier) ||
          spatial_carrier_key(effort_group->carrier) !=
              spatial_carrier_key(old_group->carrier))
        fail("FieldWinnerContinuationCarrierMismatch", army);
      MovementPrefixState initial;
      initial.basis = effort->actual_basis;
      initial.cursor = effort->opened_cursor;
      initial.lifecycle = MovementActive{};
      if (!take(same_spatial_anchor(
              expected_anchor, take(read_movement_position(initial, b)), b)))
        fail("FieldWinnerContinuationDiscontinuous", army);
      const auto continuation_head = take(
          read_actual_common_fact_position(x.common, *actual_continuation));
      const auto &effort_ref = next->effort_basis_refs.front();
      if (effort_ref.domain != "travel" ||
          effort_ref.kind != "travel_exertion_basis" ||
          take(read_actual_common_fact_position(x.common, effort_ref)) <=
              continuation_head ||
          take(read_actual_common_fact_position(
              x.common, SourceRef{"travel", "movement_unit", next->id})) <=
              continuation_head)
        fail("FieldWinnerContinuationOpeningOrderMismatch", army);
      out.continuation = row;
    } else {
      if (!same_moment(now, out.opening_at, b))
        fail("FieldWinnerActualOpeningScopeRequired", army);
      const auto binding = x.field.army_bindings.find(army);
      if (binding == x.field.army_bindings.end() ||
          binding->second.battle_id != battle)
        fail("FieldWinnerActiveBindingRequired", army);
    }
    // Every actual loss/exit from this stopped journey must have completed its
    // existing S1 handoff BEFORE resuming. A later handoff cannot repair an
    // earlier continuation retrospectively; wounded actors remain carried.
    const auto cut = actual_continuation
                         ? take(read_actual_common_fact_position(
                               x.common, *actual_continuation))
                         : std::numeric_limits<std::size_t>::max();
    const auto &journey = side.journey->journey_id;
    auto loss_before = [&](const SourceRef &casualty, const StableId &actor,
                           const WorldMoment &at) {
      Integer count = 0;
      for (const auto &item : x.groups.member_loss_handoffs.rows()) {
        const auto &loss = item.second;
        if (loss.journey_id != journey || loss.actor_id != actor ||
            !same_source(loss.actual_casualty, casualty))
          continue;
        if (item.first != loss.id ||
            !same_source(loss.original_contact, exact->actual_contact))
          fail("FieldWinnerLossOriginalMismatch", actor);
        const auto loss_head = take(read_actual_common_fact_position(
            x.common,
            SourceRef{"travel", "group_member_loss_handoff", loss.id}));
        if (loss_head <=
                take(read_actual_common_fact_position(x.common, casualty)) ||
            !same_moment(loss.occurred_at, at, b))
          fail("FieldWinnerLossCausalityMismatch", actor);
        if (loss_head < cut)
          ++count;
      }
      if (count != 1)
        fail("FieldWinnerActualLossHandoffRequired", actor);
    };
    for (const auto &segment : out.session->settlements) {
      const auto source = take(read_field_segment_settlement_source(
          x.common, x.military, x.field,
          {"military", "field_battle_segment", segment.fact_id}, &x.clock));
      if (source.casualties)
        for (const auto &change : source.casualties->actor_changes)
          if (change.army_id == army &&
              (change.status == "dead" || change.status == "captured" ||
               change.status == "deserted"))
            loss_before(
                {"military", "military_casualties", source.casualties->id},
                change.actor_id, segment.applied_at);
    }
    for (const auto &[id, exit] : x.field.exits)
      if (exit.battle_id == battle && exit.army_id == army)
        for (const auto &actor : exit.named_actor_ids) {
          Integer count = 0;
          for (const auto &item : x.groups.contact_member_releases.rows()) {
            const auto &release = item.second;
            if (release.journey_id == journey &&
                same_source(release.actual_release_source,
                            SourceRef{"military", "field_battle_exit", id}) &&
                same_moment(release.occurred_at, exit.applied_at, b) &&
                std::find(release.named_participants.begin(),
                          release.named_participants.end(),
                          actor) != release.named_participants.end() &&
                take(read_actual_common_fact_position(
                    x.common,
                    SourceRef{"travel", "group_contact_member_release",
                              release.id})) < cut)
              ++count;
          }
          if (count != 1)
            fail("FieldWinnerActualExitHandoffRequired", actor);
        }
    if (out.continuation) {
      const auto *next = x.movement.units.find(out.continuation->new_unit_ref);
      const auto *effort =
          x.movement.exertion_bases.find(next->effort_basis_refs.front().id);
      std::vector<StableId> expected;
      for (const auto &actor : side.opening->named_participants) {
        bool released = false;
        for (const auto &item : x.groups.contact_member_releases.rows())
          if (item.second.journey_id == journey &&
              std::find(item.second.named_participants.begin(),
                        item.second.named_participants.end(),
                        actor) != item.second.named_participants.end() &&
              take(read_actual_common_fact_position(
                  x.common, SourceRef{"travel", "group_contact_member_release",
                                      item.first})) < cut)
            released = true;
        for (const auto &item : x.groups.member_loss_handoffs.rows())
          if (item.second.journey_id == journey &&
              item.second.actor_id == actor &&
              take(read_actual_common_fact_position(
                  x.common, SourceRef{"travel", "group_member_loss_handoff",
                                      item.first})) < cut)
            released = true;
        if (!released)
          expected.push_back(actor);
      }
      if (expected != effort->actual_participants)
        fail("FieldWinnerContinuationMemberMismatch", army);
      const auto *road = std::get_if<RoadMovementBasis>(&effort->actual_basis);
      const auto &before = *side.original_basis;
      if (!road || !road->continued || road->original_unit_ref != next->id ||
          road->original_inputs.edge_id != expected_inputs.edge_id ||
          road->original_inputs.origin != expected_inputs.origin ||
          road->original_inputs.params.returning !=
              expected_inputs.params.returning ||
          compare(road->exact_start_progress, expected_start, b) != 0)
        fail("FieldWinnerOriginalRoadSuffixRequired", army);
      const auto edge = expected_edge;
      if (!edge || edge->duration_ticks < 1 ||
          edge->duration_ticks > std::numeric_limits<Integer>::max() / 100)
        fail("FieldWinnerOriginalRoadRequired", army);
      const auto maximum = integer(edge->duration_ticks * 100, b),
                 step = integer(100, b);
      auto end = expected_inputs.params.returning
                     ? (compare(road->exact_start_progress, step, b) < 0
                            ? CanonicalFraction{}
                            : sub(road->exact_start_progress, step, b))
                     : add(road->exact_start_progress, step, b);
      if (compare(end, maximum, b) > 0)
        end = maximum;
      if (compare(end, road->exact_end_progress, b) != 0 ||
          compare(end, road->exact_start_progress, b) == 0)
        fail("FieldWinnerOriginalRoadSuffixMismatch", army);
      const auto last = std::get_if<RoadFractionalAnchor>(&road->end_anchor);
      if (!last || last->edge_id != edge->id ||
          last->origin != expected_inputs.origin ||
          compare(last->progress, end, b) != 0 ||
          !same_source(last->actual_source, *actual_continuation))
        fail("FieldWinnerOriginalRoadEndAnchorMismatch", army);
      auto original_inputs = expected_inputs;
      original_inputs.start =
          take(floor_fraction(road->exact_start_progress, b));
      original_inputs.end = take(floor_fraction(end, b));
      original_inputs.moving = true;
      original_inputs.params.paused = false;
      original_inputs.params.has_paused = true;
      original_inputs.params.encounter_id.clear();
      if (!movement_contact_detail::same(original_inputs,
                                         road->original_inputs) ||
          road->basis_ref.empty() || road->basis_ref == before.basis_ref ||
          road->catalog_rule_ref != before.catalog_rule_ref ||
          effort->original_fatigue_per_unit != edge->fatigue_per_tick)
        fail("FieldWinnerOriginalRoadInputsChanged", army);
    }
    return out;
  });
}
Status
validate_field_winner_continuations(const FieldExactContactReadContext &x,
                                    const FieldBattleCatalog &catalog,
                                    ExactArithmeticBudget b) {
  return status([&] {
    std::set<std::pair<StableId, StableId>> seen;
    for (const auto &item : x.movement.contact_continuations.rows()) {
      const auto &row = item.second;
      if (row.actual_resolution_source.domain != "military" ||
          row.actual_resolution_source.kind != "field_battle_segment")
        continue;
      const auto original = take(read_field_segment_settlement_source(
          x.common, x.military, x.field, row.actual_resolution_source,
          &x.clock));
      const auto unit = x.movement.units.find(row.closed_unit_ref);
      const auto group =
          unit ? std::get_if<GroupMovementExecution>(&unit->execution)
               : nullptr;
      const auto army =
          group ? std::get_if<ArmyCarrierRef>(&group->carrier) : nullptr;
      if (!army || !original.terminal || item.first != row.id ||
          !seen.emplace(original.session->battle_id, army->army_id).second)
        fail("FieldWinnerContinuationCoverageMismatch", row.id);
      const SourceRef source{"travel", "movement_contact_continuation", row.id};
      take(read_field_winner_continuation(
          x, catalog, original.session->battle_id, army->army_id, &source, b));
      const auto active = x.field.army_bindings.find(army->army_id);
      if (active != x.field.army_bindings.end() &&
          active->second.battle_id == original.session->battle_id)
        fail("FieldWinnerContinuationStillActivelyBound", army->army_id);
    }
    for (const auto &item : x.movement.arrivals.rows()) {
      const auto &row = item.second;
      const auto *anchor =
          std::get_if<OriginalMovementAnchor>(&row.actual_anchor);
      if (!anchor || anchor->actual_source.domain != "military" ||
          anchor->actual_source.kind != "field_battle_segment")
        continue;
      const auto original = take(read_field_segment_settlement_source(
          x.common, x.military, x.field, anchor->actual_source, &x.clock));
      const auto *group = std::get_if<GroupMovementExecution>(&row.execution);
      const auto *army =
          group ? std::get_if<ArmyCarrierRef>(&group->carrier) : nullptr;
      if (!army || !original.terminal || item.first != row.id ||
          !seen.emplace(original.session->battle_id, army->army_id).second)
        fail("FieldWinnerArrivalCoverageMismatch", row.id);
      const SourceRef actual{"travel", "movement_arrival", row.id};
      take(read_field_winner_continuation(
          x, catalog, original.session->battle_id, army->army_id, &actual, b));
      const auto active = x.field.army_bindings.find(army->army_id);
      if (active != x.field.army_bindings.end() &&
          active->second.battle_id == original.session->battle_id)
        fail("FieldWinnerArrivalStillActivelyBound", army->army_id);
    }
  });
}
} // namespace hanmo::resident
