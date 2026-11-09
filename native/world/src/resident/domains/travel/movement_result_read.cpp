#include "movement_result_read.hpp"
#include "local_move_source.hpp"
#include "movement_exact_detail.hpp"
#include "spatial_carrier.hpp"
#include <algorithm>
namespace hanmo::resident {
using namespace movement_detail;
namespace {
const ContactEndpoint *
terminal_contact_endpoint(const MovementExactState &s,
                          const MovementUnitCloseSource &close,
                          ExactArithmeticBudget b) {
  if (close.kind != MovementCloseKind::contact ||
      close.actual_cause.domain != "travel" ||
      close.actual_cause.kind != "movement_contact")
    return nullptr;
  const auto *contact = s.contacts.find(close.actual_cause.id);
  if (!contact || contact->original_hit.road ||
      !same_moment(contact->occurred_at, close.occurred_at, b) ||
      (contact->first_prefix_id != close.prefix_id &&
       contact->second_prefix_id != close.prefix_id))
    return nullptr;
  const auto action = execution_action(close.execution);
  const auto *end = contact->original_hit.first.trace.action_id == action
                        ? &contact->original_hit.first
                    : contact->original_hit.second.trace.action_id == action
                        ? &contact->original_hit.second
                        : nullptr;
  if (!end || !end->trace.params.points.empty())
    return nullptr;
  const auto *target = std::get_if<PointI>(&end->trace.params.destination);
  if (!target || target->x != end->anchor.point.x ||
      target->y != end->anchor.point.y)
    return nullptr;
  const auto *unit = s.units.find(close.unit_ref);
  const TravelExertionBasis *effort = nullptr;
  if (unit)
    for (const auto &ref : unit->effort_basis_refs) {
      const auto *value = s.exertion_bases.find(ref.id);
      if (value)
        effort = value;
    }
  const auto *terrain =
      effort ? std::get_if<TerrainCostBasis>(&effort->actual_basis) : nullptr;
  if (!terrain || terrain->reached_points != terrain->original_point_count ||
      compare(take(movement_consumed_cost(*terrain, close.cursor, b)),
              integer(terrain->actual_consumed_total, b), b) != 0)
    return nullptr;
  const MovementAnchor actual =
      OriginalMovementAnchor{end->anchor, close.actual_cause};
  if (!take(same_spatial_anchor(actual, close.actual_anchor, b)))
    fail("ContactTerminalAnchorMismatch", close.id);
  return end;
}
} // namespace
bool is_field_group_contact_arrival(const MovementExactState &s,
                                    const MovementArrivalSource &a,
                                    ExactArithmeticBudget b) {
  const auto *anchor = std::get_if<OriginalMovementAnchor>(&a.actual_anchor);
  const auto *site =
      anchor ? std::get_if<SettlementLocation>(&anchor->location) : nullptr;
  if (!anchor || anchor->actual_source.domain != "military" ||
      anchor->actual_source.kind != "field_battle_segment")
    return false;
  const auto *close = s.unit_closes.find(a.unit_ref + ":close");
  const auto *unit = s.units.find(a.unit_ref);
  const auto *group = std::get_if<GroupMovementExecution>(&a.execution);
  const auto *contact =
      close ? s.contacts.find(close->actual_cause.id) : nullptr;
  if (!site || !group ||
      !std::holds_alternative<ArmyCarrierRef>(group->carrier) || !unit ||
      !close || !std::holds_alternative<MovementUnitFinalized>(unit->status) ||
      anchor->actual_source.id.empty() ||
      close->kind != MovementCloseKind::contact ||
      close->actual_cause.domain != "travel" ||
      close->actual_cause.kind != "movement_contact" || !contact ||
      !contact->original_hit.road ||
      (contact->first_prefix_id != a.prefix_id &&
       contact->second_prefix_id != a.prefix_id) ||
      !same_moment(contact->occurred_at, close->occurred_at, b) ||
      take(compare_world_moments(close->occurred_at, a.occurred_at, b)) > 0 ||
      a.prefix_id != close->prefix_id ||
      execution_key(a.execution) != execution_key(close->execution) ||
      execution_key(a.execution) != execution_key(unit->execution) ||
      compare(a.cursor, close->cursor, b) != 0 ||
      !a.destination_settlement_id ||
      *a.destination_settlement_id != site->settlement_id ||
      a.reached_points != 0 || a.original_point_count != 0 ||
      unit->effort_basis_refs.size() != 1)
    fail("FieldGroupContactArrivalSourceMismatch", a.id);
  if (!same_source(
          std::get<MovementUnitFinalized>(unit->status).original_close_source,
          close->actual_cause))
    fail("FieldGroupContactArrivalFinalizedSourceMismatch", a.id);
  const auto *effort =
      s.exertion_bases.find(unit->effort_basis_refs.front().id);
  const auto *road =
      effort ? std::get_if<RoadMovementBasis>(&effort->actual_basis) : nullptr;
  if (!road || a.basis_ref != road->basis_ref)
    fail("FieldGroupContactArrivalBasisRequired", a.id);
  Integer count = 0;
  for (const auto &item : s.arrivals.rows())
    if (item.second.unit_ref == a.unit_ref) {
      if (&item.second != &a || item.first != a.id)
        fail("FieldGroupContactArrivalNotUnique", a.id);
      ++count;
    }
  if (count != 1)
    fail("FieldGroupContactArrivalNotUnique", a.id);
  for (const auto &item : s.contact_continuations.rows())
    if (item.second.closed_unit_ref == a.unit_ref)
      fail("FieldGroupContactArrivalAndContinuation", a.id);
  return true;
}
Outcome<MovementArrivalSource>
read_verified_movement_arrival(const MovementExactState &s,
                               const SourceRef &ref, Integer schema,
                               ExactArithmeticBudget b) {
  return guard<MovementArrivalSource>([&] {
    if (schema != 1 || ref.domain != "travel" || ref.kind != "movement_arrival")
      fail("UnsupportedMovementSourceSchema", ref.id);
    const auto *a = s.arrivals.find(ref.id);
    const auto *close =
        a ? s.unit_closes.find(a->unit_ref + ":close") : nullptr;
    if (!a || !close ||
        (!same_moment(close->occurred_at, a->occurred_at, b) &&
         !is_field_group_contact_arrival(s, *a, b)) ||
        a->prefix_id != close->prefix_id ||
        execution_key(a->execution) != execution_key(close->execution) ||
        compare(a->cursor, close->cursor, b) != 0)
      fail("MovementArrivalSourceRequired", ref.id);
    if (close->kind == MovementCloseKind::arrival) {
      if (!same_source(close->actual_cause, ref))
        fail("MovementArrivalSourceRequired", ref.id);
    } else if (!is_field_group_contact_arrival(s, *a, b)) {
      const auto *end = terminal_contact_endpoint(s, *close, b);
      if (!end || a->destination_settlement_id ||
          a->reached_points != a->original_point_count ||
          !take(same_spatial_anchor(a->actual_anchor, close->actual_anchor, b)))
        fail("MovementContactArrivalSourceRequired", ref.id);
    }
    return *a;
  });
}
Outcome<std::optional<SourceRef>>
adopt_exact_personal_contact_arrival_in_candidate(CommonWrite &common,
                                                  MovementExactState &s,
                                                  const StableId &contact_id,
                                                  const StableId &prefix_id,
                                                  ExactArithmeticBudget b) {
  return guard<std::optional<SourceRef>>([&]() -> std::optional<SourceRef> {
    const auto *p = s.prefixes.find(prefix_id);
    const auto *close =
        p ? s.unit_closes.find(p->unit_ref + ":close") : nullptr;
    if (!p || !close ||
        !std::holds_alternative<PersonalMovementExecution>(p->execution) ||
        !same_source(close->actual_cause,
                     {"travel", "movement_contact", contact_id}))
      fail("MovementContactCloseRequired", prefix_id);
    if (!terminal_contact_endpoint(s, *close, b))
      return {};
    for (const auto &row : s.arrivals.rows())
      if (row.second.unit_ref == close->unit_ref) {
        take(read_verified_movement_arrival(
            s, {"travel", "movement_arrival", row.first}, 1, b));
        return SourceRef{"travel", "movement_arrival", row.first};
      }
    const auto *basis = std::get_if<TerrainCostBasis>(&p->basis);
    if (!basis)
      fail("ContactTerminalTerrainBasisRequired", prefix_id);
    MovementArrivalSource a{take(common.next_id("movement_arrival")),
                            prefix_id,
                            close->unit_ref,
                            p->execution,
                            close->occurred_at,
                            close->actual_anchor,
                            basis->basis_ref,
                            close->cursor,
                            basis->reached_points,
                            basis->original_point_count,
                            {}};
    const auto id = a.id;
    s.arrivals.set(id, std::move(a));
    take(common.append_fact({id, "movement_arrival", "travel"}));
    take(read_verified_movement_arrival(s, {"travel", "movement_arrival", id},
                                        1, b));
    return SourceRef{"travel", "movement_arrival", id};
  });
}

Outcome<WorldMoment> read_movement_source_moment(const MovementExactState &s,
                                                 const SourceRef &ref,
                                                 Integer schema,
                                                 ExactArithmeticBudget b) {
  return guard<WorldMoment>([&] {
    if (schema != 1 || ref.domain != "travel")
      fail("UnsupportedMovementSourceSchema", ref.id);
    if (ref.kind == "local_move") {
      const auto* row=s.local_moves.find(ref.id);
      if(!row || row->id!=ref.id) fail("LocalMoveSourceRequired",ref.id);
      take(validate_local_move_value(*row,b));
      return row->occurred_at;
    }
    if (ref.kind == "movement_arrival")
      return take(read_verified_movement_arrival(s, ref, schema, b))
          .occurred_at;
    if (ref.kind == "travel_exertion")
      return take(read_verified_travel_exertion(s, ref.id, b)).actual_span.to;
    if (ref.kind == "movement_contact") {
      const auto *c = s.contacts.find(ref.id);
      if (!c)
        fail("MovementContactSourceRequired", ref.id);
      auto mapped = take(read_exact_contact_mapped_moment(*c, b));
      if (!same_moment(mapped, c->occurred_at, b))
        fail("MovementContactMomentMismatch", ref.id);
      return c->occurred_at;
    }
    if (ref.kind == "movement_expense") {
      const auto *e = s.expenses.find(ref.id);
      const auto *u = e ? s.units.find(e->unit_ref) : nullptr;
      const auto *paid =
          u ? std::get_if<MovementPaymentConsumed>(&u->payment) : nullptr;
      if (!e || !paid || !same_source(paid->actual_expense_source, ref) ||
          paid->amount != e->amount)
        fail("MovementExpenseSourceRequired", ref.id);
      return e->occurred_at;
    }
    if (ref.kind == "travel_exertion_basis") {
      const auto *basis = s.exertion_bases.find(ref.id);
      const auto *unit = basis ? s.units.find(basis->unit_ref) : nullptr;
      if (!basis || !unit || unit->effort_basis_refs.empty() ||
          std::none_of(
              unit->effort_basis_refs.begin(), unit->effort_basis_refs.end(),
              [&](const auto &actual) { return same_source(actual, ref); }) ||
          execution_key(unit->execution) != execution_key(basis->execution) ||
          basis_id(basis->actual_basis).empty() ||
          !source_valid(basis->original_rule_source))
        fail("TravelExertionBasisSourceRequired", ref.id);
      take(encode_world_moment(basis->opened_at, b));
      return basis->opened_at;
    }
    if (ref.kind == "terrain_credit_disposition") {
      const auto *source = s.credit_dispositions.find(ref.id);
      const auto *close =
          source ? s.unit_closes.find(source->unit_ref + ":close") : nullptr;
      const auto *unit = source ? s.units.find(source->unit_ref) : nullptr;
      const auto *ledger =
          unit ? std::get_if<TerrainUnitBudget>(&unit->budget) : nullptr;
      if (!source || !close || !ledger ||
          !same_source(source->original_close_source, close->actual_cause) ||
          !same_moment(source->occurred_at, close->occurred_at, b) ||
          source->kind != close->kind ||
          compare(source->before_credit.value,
                  add(source->after_credit.value, source->discarded.value, b),
                  b) != 0 ||
          std::none_of(ledger->disposition_sources.begin(),
                       ledger->disposition_sources.end(),
                       [&](const auto &r) { return same_source(r, ref); }))
        fail("TerrainCreditDispositionSourceRequired", ref.id);
      return source->occurred_at;
    }
    if (ref.kind == "movement_unit_boundary") {
      const auto *source = s.unit_boundaries.find(ref.id);
      const auto *unit = source ? s.units.find(source->unit_ref) : nullptr;
      const auto *close =
          unit ? s.unit_closes.find(unit->id + ":close") : nullptr;
      if (!source || !unit || !close ||
          close->kind != MovementCloseKind::carry ||
          !same_source(close->actual_cause, ref) ||
          !same_moment(source->occurred_at, close->occurred_at, b) ||
          compare(unit->active_elapsed.value, integer(1, b), b) != 0 ||
          compare(source->cursor, close->cursor, b) != 0)
        fail("MovementUnitBoundarySourceRequired", ref.id);
      return source->occurred_at;
    }
    if (ref.kind == "movement_basis_transition") {
      const auto *source = s.basis_transitions.find(ref.id);
      if (!source)
        fail("MovementBasisTransitionSourceRequired", ref.id);
      return source->occurred_at;
    }
    if (ref.kind == "movement_contact_continuation") {
      const auto *source = s.contact_continuations.find(ref.id);
      const auto *next = source ? s.units.find(source->new_unit_ref) : nullptr;
      const auto *basis =
          next && !next->effort_basis_refs.empty()
              ? s.exertion_bases.find(next->effort_basis_refs.front().id)
              : nullptr;
      if (!source || !basis || !same_source(basis->original_rule_source, ref) ||
          !same_moment(basis->opened_at, source->occurred_at, b))
        fail("MovementContactContinuationSourceRequired", ref.id);
      return source->occurred_at;
    }
    if (ref.kind == "movement_unit_close") {
      const auto *c = s.unit_closes.find(ref.id);
      if (!c)
        fail("MovementCloseSourceRequired", ref.id);
      return c->occurred_at;
    }
    fail("UnknownMovementSourceKind", ref.kind);
  });
}
} // namespace hanmo::resident
