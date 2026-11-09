#include "local_move_source.hpp"
// 精确值及原来源闭包一起冷验；不能借当前prefix覆盖历史unit。
#include "movement_exact_detail.hpp"
#include "movement_result_read.hpp"
#include "spatial_carrier.hpp"
#include <algorithm>
#include <cmath>
#include <set>
namespace hanmo::resident {
using namespace movement_detail;
namespace {
void ids(const std::vector<StableId> &values) {
  std::set<StableId> unique;
  for (const auto &id : values)
    if (id.empty() || !unique.insert(id).second)
      fail("InvalidMovementParticipants", id);
}
void moment(const WorldMoment &at, ExactArithmeticBudget b) {
  take(encode_world_moment(at, b));
}
// 验证各锚点原值与来源，保持坐标和值域的原有拒绝门。
void anchor(const MovementAnchor &a, ExactArithmeticBudget b) {
  if (const auto *x = std::get_if<OriginalMovementAnchor>(&a)) {
    if (!source_valid(x->actual_source))
      fail("MovementAnchorSourceRequired");
    std::visit(
        [](const auto &l) {
          using T = std::decay_t<decltype(l)>;
          if constexpr (std::is_same_v<T, SettlementLocation>) {
            if (l.settlement_id.empty() || l.place_id.empty())
              fail("InvalidMovementSettlement");
          } else if constexpr (std::is_same_v<T, WildernessLocation>) {
            if (l.map_id.empty() || l.origin.empty() || l.point.x < 0 ||
                l.point.y < 0)
              fail("InvalidMovementTerrain");
          } else if constexpr (std::is_same_v<T, OnRouteLocation>) {
            if (l.edge_id.empty() || l.origin.empty() || l.progress < 0)
              fail("InvalidMovementRoad");
          } else
            fail("MovementPhysicalAnchorRequired");
        },
        x->location);
  } else if (const auto *x = std::get_if<RoadFractionalAnchor>(&a)) {
    if (x->edge_id.empty() || x->origin.empty() ||
        !source_valid(x->actual_source))
      fail("InvalidMovementRoad");
    take(encode_canonical_fraction(x->progress, b));
  } else {
    const auto &terrain = std::get<TerrainContinuousAnchor>(a);
    if (terrain.map_id.empty() || terrain.origin.empty() ||
        terrain.geometry_profile_ref.empty() ||
        !source_valid(terrain.actual_prefix_close_source))
      fail("InvalidMovementTerrain");
    if (take(decode_movement_binary64(terrain.x_bits)) < 0 ||
        take(decode_movement_binary64(terrain.y_bits)) < 0)
      fail("InvalidMovementTerrain");
  }
}
void basis(const MovementBasis &v, ExactArithmeticBudget b) {
  std::visit(
      [&](const auto &x) {
        if (x.basis_ref.empty() || x.original_unit_ref.empty() ||
            x.catalog_rule_ref.empty())
          fail("InvalidMovementBasis");
        anchor(x.start_anchor, b);
        anchor(x.end_anchor, b);
      },
      v);
  if (const auto *t = std::get_if<TerrainCostBasis>(&v)) {
    if (t->original_inputs.budget < 1 || t->original_inputs.travel_credit < 0 ||
        t->original_inputs.money_per_tick < 0 ||
        t->original_inputs.fatigue_per_tick < 0 ||
        t->actual_consumed_total < 0 || t->reached_points < 0 ||
        t->original_point_count < 0 ||
        t->reached_points > t->original_point_count || t->pieces.empty() ||
        compare(t->parameter_limit.value, CanonicalFraction{}, b) <= 0)
      fail("InvalidTerrainCostBasis", t->basis_ref);
    if (t->original_closing_policy !=
        (t->reached_points == t->original_point_count
             ? TerrainOriginalClosingPolicy::arrival_discard
             : TerrainOriginalClosingPolicy::carry_at_unit_boundary))
      fail("TerrainOriginalClosingPolicyMismatch", t->basis_ref);
    CanonicalFraction end;
    bool unused = false;
    ContactPoint previous = t->pieces.front().segment.from;
    for (const auto &p : t->pieces) {
      const auto &s = p.segment;
      if (!std::isfinite(s.start_cost) || !std::isfinite(s.end_cost) ||
          !std::isfinite(s.from.x) || !std::isfinite(s.from.y) ||
          !std::isfinite(s.to.x) || !std::isfinite(s.to.y) ||
          compare(p.cost_from, end, b) != 0 ||
          compare(p.cost_to, p.cost_from, b) <= 0 || s.from.x != previous.x ||
          s.from.y != previous.y || unused)
        fail("InvalidTerrainCostPiece", t->basis_ref);
      if (p.kind != TerrainCostPieceKind::moving_cost &&
          (s.from.x != s.to.x || s.from.y != s.to.y))
        fail("MovingStationaryCostPiece", t->basis_ref);
      unused = p.kind == TerrainCostPieceKind::unused_budget_tail;
      end = p.cost_to;
      previous = s.to;
    }
    if (compare(end, t->parameter_limit.value, b) != 0 ||
        compare(take(movement_consumed_cost(*t, t->parameter_limit.value, b)),
                integer(t->actual_consumed_total, b), b) != 0)
      fail("TerrainCostBasisTotalMismatch", t->basis_ref);
  } else {
    const auto &r = std::get<RoadMovementBasis>(v);
    if (r.original_inputs.edge_id.empty() || r.original_inputs.origin.empty() ||
        !r.original_inputs.moving || r.original_inputs.start < 0 ||
        r.original_inputs.end < 0)
      fail("InvalidRoadMovementBasis", r.basis_ref);
  }
}
bool location_equal(const Location &a, const Location &b) {
  if (a.index() != b.index())
    return false;
  return std::visit(
      [&](const auto &x) {
        using T = std::decay_t<decltype(x)>;
        const auto &y = std::get<T>(b);
        if constexpr (std::is_same_v<T, SettlementLocation>)
          return x.settlement_id == y.settlement_id && x.place_id == y.place_id;
        else if constexpr (std::is_same_v<T, WildernessLocation>)
          return x.map_id == y.map_id && x.origin == y.origin &&
                 x.point.x == y.point.x && x.point.y == y.point.y;
        else if constexpr (std::is_same_v<T, OnRouteLocation>)
          return x.edge_id == y.edge_id && x.origin == y.origin &&
                 x.progress == y.progress;
        else if constexpr (std::is_same_v<T, FollowingLocation>)
          return x.party_id == y.party_id;
        else if constexpr (std::is_same_v<T, InArmyLocation>)
          return x.army_id == y.army_id;
        else
          return x.carrier_kind == y.carrier_kind &&
                 x.carrier_id == y.carrier_id;
      },
      a);
}
} // namespace
Status validate_movement_exact_values(const MovementExactState &s,
                                      ExactArithmeticBudget b) {
  return status([&] {
    for(const auto& [key,row]:s.local_moves.rows()) {
      if(key!=row.id) fail("InvalidLocalMoveSourceShape",key);
      take(validate_local_move_value(row,b));
    }
    std::set<StableId> active_executions, active_units;
    for (const auto &row : s.prefixes.rows()) {
      const auto &p = row.second;
      if (row.first != p.id || p.id.empty() || p.unit_ref.empty() ||
          !s.units.find(p.unit_ref))
        fail("InvalidMovementPrefix", row.first);
      moment(p.last_settled_at, b);
      basis(p.basis, b);
      if (compare(p.cursor, basis_limit(p.basis, b), b) > 0)
        fail("MovementCursorOutsideBasis", p.id);
      const auto *u = s.units.find(p.unit_ref);
      if (execution_key(u->execution) != execution_key(p.execution))
        fail("MovementExecutionMismatch", p.id);
      if (!std::holds_alternative<MovementClosed>(p.lifecycle)) {
        if (!active_executions.insert(execution_key(p.execution)).second ||
            !active_units.insert(u->id).second)
          fail("DuplicateMovementExecution", p.id);
      }
      if (const auto *r =
              std::get_if<RunningMovementMap>(&p.activity_world_map)) {
        moment(r->world_from, b);
        if (!source_valid(r->rate_source) ||
            compare(r->rate, CanonicalFraction{}, b) <= 0 ||
            take(compare_world_moments(r->world_from, p.last_settled_at, b)) >
                0)
          fail("InvalidMovementActivityMap", p.id);
        auto actual = add(r->cursor_at_from,
                          mul(r->rate,
                              take(elapsed_world_duration(r->world_from,
                                                          p.last_settled_at, b))
                                  .value,
                              b),
                          b);
        if (compare(actual, p.cursor, b) != 0)
          fail("MovementMapCursorMismatch", p.id);
      } else {
        const auto &h = std::get<HeldMovementMap>(p.activity_world_map);
        if (!source_valid(h.actual_hold_source) ||
            !source_valid(h.resume_rate_source) ||
            compare(h.held_cursor, p.cursor, b) != 0 ||
            compare(h.resume_rate, CanonicalFraction{}, b) <= 0)
          fail("InvalidMovementHold", p.id);
      }
      anchor(take(read_movement_position(p, b)), b);
    }
    for (const auto &row : s.units.rows()) {
      const auto &u = row.second;
      if (row.first != u.id || u.id.empty() || u.ordinal < 0 ||
          compare(u.active_elapsed.value, integer(1, b), b) > 0 ||
          u.effort_basis_refs.empty())
        fail("InvalidMovementUnit", row.first);
      const auto &ref = u.effort_basis_refs.front();
      const auto *e = s.exertion_bases.find(ref.id);
      if (ref.domain != "travel" || ref.kind != "travel_exertion_basis" || !e ||
          e->unit_ref != u.id ||
          execution_key(e->execution) != execution_key(u.execution))
        fail("TravelExertionBasisRequired", u.id);
      if (const auto *ledger = std::get_if<TerrainUnitBudget>(&u.budget)) {
        sub(sub(add(ledger->opening_credit.value, ledger->funded.value, b),
                ledger->spent.value, b),
            ledger->discarded.value, b);
        if (ledger->disposition_sources.size() > 1)
          fail("DuplicateMovementCreditDisposition", u.id);
        for (const auto &d : ledger->disposition_sources) {
          const auto *actual = s.credit_dispositions.find(d.id);
          if (d.domain != "travel" || d.kind != "terrain_credit_disposition" ||
              !actual || actual->unit_ref != u.id)
            fail("MovementCreditDispositionRequired", u.id);
        }
      }
      if (const auto *paid = std::get_if<MovementPaymentConsumed>(&u.payment)) {
        const auto *e = s.expenses.find(paid->actual_expense_source.id);
        if (paid->actual_expense_source.domain != "travel" ||
            paid->actual_expense_source.kind != "movement_expense" || !e ||
            e->unit_ref != u.id ||
            e->payer_wallet_id != paid->payer_wallet_id ||
            e->amount != paid->amount ||
            execution_key(e->execution) != execution_key(u.execution))
          fail("MovementExpenseSourceMismatch", u.id);
      }
      if (const auto *done = std::get_if<MovementUnitFinalized>(&u.status)) {
        const auto *close = s.unit_closes.find(u.id + ":close");
        if (!close ||
            !same_source(close->actual_cause, done->original_close_source))
          fail("MovementCloseSourceRequired", u.id);
        if (!take(read_unstarted_held_contact_close(s, u, close->actual_cause,
                                                    b)))
          take(read_verified_travel_exertion(s, u.id + ":exertion", b));
        else if (s.exertions.find(u.id + ":exertion"))
          fail("UnstartedHeldContactHasExertion", u.id);
      }
    }
    for (const auto &row : s.basis_transitions.rows()) {
      const auto &transition = row.second;
      const auto *unit = s.units.find(transition.unit_ref);
      if (row.first != transition.id || !unit ||
          !s.prefixes.find(transition.prefix_id) ||
          !source_valid(transition.original_reason_source) ||
          execution_key(unit->execution) != execution_key(transition.execution))
        fail("MovementBasisTransitionSourceRequired", row.first);
      const TravelExertionBasis *old = nullptr;
      const TravelExertionBasis *next = nullptr;
      for (const auto &ref : unit->effort_basis_refs) {
        const auto *e = s.exertion_bases.find(ref.id);
        if (e && basis_id(e->actual_basis) == transition.old_basis_ref)
          old = e;
        if (e && basis_id(e->actual_basis) == transition.new_basis_ref)
          next = e;
      }
      if (!old || !next ||
          !same_moment(next->opened_at, transition.occurred_at, b) ||
          compare(transition.old_cursor, basis_limit(old->actual_basis, b),
                  b) != 0 ||
          compare(next->opened_cursor, {}, b) != 0)
        fail("MovementBasisTransitionBoundaryMismatch", row.first);
      MovementPrefixState ended;
      ended.basis = old->actual_basis;
      ended.cursor = transition.old_cursor;
      ended.lifecycle = MovementActive{};
      MovementPrefixState begun;
      begun.basis = next->actual_basis;
      begun.cursor = next->opened_cursor;
      begun.lifecycle = MovementActive{};
      if (!take(same_spatial_anchor(take(read_movement_position(ended, b)),
                                    transition.actual_anchor, b)) ||
          !take(same_spatial_anchor(take(read_movement_position(begun, b)),
                                    transition.actual_anchor, b)))
        fail("MovementBasisTransitionDiscontinuous", row.first);
    }
    for (const auto &row : s.exertion_bases.rows()) {
      const auto &e = row.second;
      if (row.first != e.id || !s.units.find(e.unit_ref) ||
          e.original_fatigue_per_unit < 0 ||
          !source_valid(e.original_rule_source))
        fail("InvalidTravelExertionBasis", row.first);
      const auto *original_unit = s.units.find(e.unit_ref);
      if (std::none_of(original_unit->effort_basis_refs.begin(),
                       original_unit->effort_basis_refs.end(),
                       [&](const auto &ref) {
                         return same_source(
                             ref, {"travel", "travel_exertion_basis", e.id});
                       }))
        fail("UnlinkedTravelExertionBasis", e.id);
      if (e.accepted_funding_plan) {
        const auto *execution =
            std::get_if<PersonalMovementExecution>(&e.execution);
        const auto &accepted = *e.accepted_funding_plan;
        if (!execution || execution->segment_ordinal != 0 ||
            original_unit->ordinal != 0 ||
            original_unit->effort_basis_refs.empty() ||
            !same_source(original_unit->effort_basis_refs.front(),
                         {"travel", "travel_exertion_basis", e.id}) ||
            accepted.source_schema != 1 ||
            accepted.source_ref.domain != "tasks" ||
            accepted.source_ref.kind != "task_carrier_agreement_accepted" ||
            accepted.source_ref.id.empty())
          fail("PersonalFundingOnlyOriginalOpening", e.id);
      }
      ids(e.actual_participants);
      if (e.actual_participants.empty() &&
          std::holds_alternative<PersonalMovementExecution>(e.execution))
        fail("TravelParticipantsRequired", e.id);
      moment(e.opened_at, b);
      basis(e.actual_basis, b);
      if (const auto *p =
              std::get_if<PersonalMovementExecution>(&e.execution)) {
        if (!e.original_action || e.original_action->id != p->action_id ||
            e.original_action->kind != "travel" ||
            std::find(
                e.actual_participants.begin(), e.actual_participants.end(),
                e.original_action->actor_id) == e.actual_participants.end())
          fail("TravelExertionActionMismatch", e.id);
      }
      if (const auto *terrain =
              std::get_if<TerrainCostBasis>(&e.actual_basis)) {
        if (e.original_fatigue_per_unit !=
            terrain->original_inputs.fatigue_per_tick)
          fail("TravelExertionRuleMismatch", e.id);
      }
    }
    for (const auto &row : s.expenses.rows()) {
      const auto &e = row.second;
      const auto *u = s.units.find(e.unit_ref);
      const auto *paid =
          u ? std::get_if<MovementPaymentConsumed>(&u->payment) : nullptr;
      if (row.first != e.id || e.amount < 0 || e.balance_before < 0 ||
          e.balance_after < 0 || e.amount > e.balance_before ||
          e.balance_before - e.amount != e.balance_after || !paid ||
          paid->actual_expense_source.id != e.id ||
          execution_key(e.execution) != execution_key(u->execution))
        fail("InvalidMovementExpense", row.first);
      moment(e.occurred_at, b);
    }
    for (const auto &row : s.exertions.rows()) {
      if (row.first != row.second.id)
        fail("InvalidTravelExertionKey", row.first);
      take(read_verified_travel_exertion(s, row.first, b));
    }
    for (const auto &row : s.contacts.rows()) {
      const auto &c = row.second;
      if (row.first != c.id || !s.prefixes.find(c.first_prefix_id) ||
          !s.prefixes.find(c.second_prefix_id))
        fail("InvalidMovementContact", row.first);
      auto mapped = take(read_exact_contact_mapped_moment(c, b));
      if (!same_moment(mapped, c.occurred_at, b))
        fail("MovementContactMomentMismatch", c.id);
    }
    for (const auto &row : s.arrivals.rows()) {
      const auto &a = row.second;
      const auto *u = s.units.find(a.unit_ref);
      const auto *close = s.unit_closes.find(a.unit_ref + ":close");
      if (row.first != a.id || !u || !close ||
          (!same_moment(close->occurred_at, a.occurred_at, b) &&
           !is_field_group_contact_arrival(s, a, b)) ||
          execution_key(a.execution) != execution_key(u->execution) ||
          a.reached_points != a.original_point_count)
        fail("InvalidMovementArrival", row.first);
      take(read_verified_movement_arrival(
          s, {"travel", "movement_arrival", row.first}, 1, b));
      anchor(a.actual_anchor, b);
    }
    for (const auto &row : s.spatial_bindings.rows()) {
      const auto &x = row.second;
      if (row.first != x.subject_kind + ":" + x.subject_id ||
          x.subject_id.empty() || !source_valid(x.binding_source))
        fail("InvalidSpatialWriterBinding", row.first);
      if (const auto *w = std::get_if<WorldMovementWriter>(&x.writer)) {
        const auto *p = s.prefixes.find(w->prefix_id);
        if (!p || std::holds_alternative<MovementClosed>(p->lifecycle))
          fail("WorldWriterPrefixRequired", row.first);
        if (x.subject_kind == "army" || x.subject_kind == "transport_unit" ||
            x.subject_kind == "fleet") {
          const auto *group =
              std::get_if<GroupMovementExecution>(&p->execution);
          if (!group || spatial_carrier_key(group->carrier) != row.first)
            fail("WorldWriterCarrierExecutionMismatch", row.first);
        }
      } else if (const auto *w = std::get_if<BattleMovementWriter>(&x.writer)) {
        if (w->session_id.empty() || w->handoff_id.empty() || w->generation < 1)
          fail("BattleWriterHandoffRequired", row.first);
      } else if (const auto *stable =
                     std::get_if<StableMovementWriter>(&x.writer))
        anchor(stable->anchor, b);
      else {
        const auto &carried = std::get<CarriedMovementWriter>(x.writer);
        if (!((x.subject_kind == "actor" &&
               (std::holds_alternative<TransportUnitCarrierRef>(
                    carried.carrier_ref) ||
                std::holds_alternative<ArmyCarrierRef>(carried.carrier_ref))) ||
              ((x.subject_kind == "transport_unit" ||
                x.subject_kind == "army") &&
               std::holds_alternative<FleetCarrierRef>(carried.carrier_ref))))
          fail("UnsupportedCarriedBindingLayer", row.first);
        const auto resolved =
            take(read_resolved_spatial_writer(s, carried.carrier_ref));
        if (resolved.subject_kind == x.subject_kind &&
            resolved.subject_id == x.subject_id)
          fail("SpatialCarrierBindingCycle", row.first);
      }
    }
  });
}
Status validate_movement_exact_world(
    const CommonRead &common, const TravelState &travel,
    const MovementExactState &state, ExactArithmeticBudget b,
    const ArmySpatialBindingSourceRead *army_sources) {
  return status([&] {
    if (!state.passage_modifier_adoptions.empty())
      throw Failure{
          common_failure("PassageModifierOriginalSourceConsumerRequired", {},
                         ErrorKind::unsupported_capability)
              .error};
    take(validate_movement_exact_values(state, b));
    for (const auto &row : state.spatial_bindings.rows()) {
      const auto &binding = row.second;
      if (binding.subject_kind != "actor")
        continue;
      const auto *actor = common.actor(binding.subject_id);
      if (!actor)
        fail("SpatialSubjectMissing", row.first);
      if (const auto *world =
              std::get_if<WorldMovementWriter>(&binding.writer)) {
        const auto *prefix = state.prefixes.find(world->prefix_id);
        const auto action_id = execution_action(prefix->execution);
        const auto *action = common.action(action_id);
        if (std::holds_alternative<PersonalMovementExecution>(
                prefix->execution)) {
          if (!action || action->actor_id != actor->id ||
              (!travel.actions.find(action_id) &&
               !travel.road_actions.find(action_id)))
            fail("SpatialOriginalActionMissing", row.first);
        } else if (std::holds_alternative<GroupMovementExecution>(
                       prefix->execution)) {
          const auto *unit = state.units.find(prefix->unit_ref);
          const auto *effort = unit && unit->effort_basis_refs.size() > 0
                                   ? state.exertion_bases.find(
                                         unit->effort_basis_refs.front().id)
                                   : nullptr;
          if (!effort || effort->original_action ||
              std::find(effort->actual_participants.begin(),
                        effort->actual_participants.end(),
                        actor->id) == effort->actual_participants.end())
            fail("SpatialGroupParticipantMissing", row.first);
        } else
          fail("UnsupportedSpatialExecution", row.first);
        if (!location_equal(actor->location,
                            take(project_movement_location(
                                take(read_movement_position(*prefix, b)), b))))
          fail("SpatialCompatibilityProjectionMismatch", row.first);
      } else if (const auto *carried =
                     std::get_if<CarriedMovementWriter>(&binding.writer)) {
        take(read_resolved_spatial_writer(state, carried->carrier_ref));
        if (const auto *army =
                std::get_if<ArmyCarrierRef>(&carried->carrier_ref)) {
          const auto *location = std::get_if<InArmyLocation>(&actor->location);
          if (!location || location->army_id != army->army_id)
            fail("SpatialArmyProjectionMismatch", row.first);
          if (!army_sources)
            throw Failure{
                common_failure("ArmySpatialBindingSourceReaderRequired",
                               row.first, ErrorKind::unsupported_capability)
                    .error};
          take(army_sources->validate_actor_army_binding_source(
              actor->id, *army, binding.binding_source));
        } else {
          const auto *location = std::get_if<CarriedLocation>(&actor->location);
          const auto *unit =
              std::get_if<TransportUnitCarrierRef>(&carried->carrier_ref);
          if (!location || !unit ||
              location->carrier_kind !=
                  CarriedLocation::CarrierKind::transport_unit ||
              location->carrier_id !=
                  unit->transport_unit_ref.transport_unit_id)
            fail("SpatialCarriedProjectionMismatch", row.first);
        }
      } else if (const auto *stable =
                     std::get_if<StableMovementWriter>(&binding.writer)) {
        if (!location_equal(actor->location,
                            take(project_movement_location(stable->anchor, b))))
          fail("SpatialCompatibilityProjectionMismatch", row.first);
      }
    }
  });
}
} // namespace hanmo::resident
