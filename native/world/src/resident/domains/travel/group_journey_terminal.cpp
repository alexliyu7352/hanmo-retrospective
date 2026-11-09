#include "group_movement.hpp"
#include "movement_exact_detail.hpp"
#include "movement_result_read.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
using namespace movement_detail;
Outcome<ActualGroupJourneyTerminal> read_actual_group_journey_terminal(
    const CommonStore &common, const SpatialCarrierState &carriers,
    const MovementExactState &movement,
    const GroupMovementExecutionState &executions, const StableId &id,
    ExactArithmeticBudget budget) {
  return guard<ActualGroupJourneyTerminal>([&] {
    const auto *terminal = carriers.terminals.find(id);
    const auto *journey = carriers.journeys.find(id);
    const auto *prefix =
        journey ? movement.prefixes.find(journey->movement_prefix_ref)
                : nullptr;
    const auto *closed =
        prefix ? std::get_if<MovementClosed>(&prefix->lifecycle) : nullptr;
    const auto *unit = prefix ? movement.units.find(prefix->unit_ref) : nullptr;
    const auto *header =
        terminal
            ? common.state().facts.find(terminal->actual_terminal_source.id)
            : nullptr;
    if (!terminal || terminal->journey_id != id ||
        terminal->source_schema != 1 || !journey || !prefix || !closed ||
        !unit || !header ||
        header->domain != terminal->actual_terminal_source.domain ||
        header->kind != terminal->actual_terminal_source.kind ||
        !journey->stop_reason ||
        !same_source(*journey->stop_reason, terminal->actual_terminal_source) ||
        !same_source(closed->original_result_ref,
                     terminal->actual_terminal_source) ||
        !same_moment(journey->last_step_at, terminal->occurred_at, budget) ||
        take(compare_world_moments(prefix->last_settled_at,
                                   terminal->occurred_at, budget)) > 0)
      fail("GroupJourneyTerminalSourceMismatch", id);
    const auto *execution =
        std::get_if<GroupMovementExecution>(&prefix->execution);
    if (!execution || execution->journey_id != id ||
        execution_key(prefix->execution) != execution_key(unit->execution) ||
        spatial_carrier_key(execution->carrier) !=
            spatial_carrier_key(journey->carrier))
      fail("GroupJourneyTerminalExecutionMismatch", id);
    const auto *close = movement.unit_closes.find(unit->id + ":close");
    if (terminal->outcome == GroupJourneyTerminalOutcome::arrived) {
      if (journey->state != GroupJourneyLifecycle::arrived)
        fail("GroupJourneyTerminalStateMismatch", id);
      auto arrival = take(read_verified_movement_arrival(
          movement, terminal->actual_terminal_source, terminal->source_schema,
          budget));
      if (!same_moment(prefix->last_settled_at, terminal->occurred_at,
                       budget) ||
          arrival.prefix_id != prefix->id || arrival.unit_ref != unit->id ||
          !same_moment(arrival.occurred_at, terminal->occurred_at, budget) ||
          !take(same_spatial_anchor(arrival.actual_anchor,
                                    closed->actual_anchor, budget)) ||
          !close ||
          ((close->kind != MovementCloseKind::arrival ||
            !same_source(close->actual_cause,
                         terminal->actual_terminal_source)) &&
           !is_field_group_contact_arrival(
               movement, *movement.arrivals.find(arrival.id), budget)))
        fail("GroupArrivalTerminalMismatch", id);
    } else {
      const auto *control =
          terminal->actual_terminal_source.domain == "travel" &&
                  terminal->actual_terminal_source.kind ==
                      "group_movement_control"
              ? executions.controls.find(terminal->actual_terminal_source.id)
              : nullptr;
      if (journey->state != GroupJourneyLifecycle::cancelled || !control ||
          control->journey_id != id ||
          control->operation != GroupControlOperation::cancel ||
          !source_valid(control->authority_source) ||
          !same_moment(control->occurred_at, terminal->occurred_at, budget) ||
          !take(same_spatial_anchor(control->actual_anchor,
                                    closed->actual_anchor, budget)))
        fail("GroupCancellationTerminalMismatch", id);
      if (close && close->kind == MovementCloseKind::contact) {
        // Cancelling after the real encounter only terminates future travel.
        // The old paid/contact unit, fatigue, and source remain untouched.
        if (!std::holds_alternative<MovementUnitFinalized>(unit->status) ||
            take(compare_world_moments(close->occurred_at,
                                       terminal->occurred_at, budget)) > 0 ||
            !take(same_spatial_anchor(close->actual_anchor,
                                      control->actual_anchor, budget)))
          fail("GroupContactCancellationMismatch", id);
        if (close->actual_cause.domain == "travel" &&
            close->actual_cause.kind == "movement_contact") {
          const auto *contact = movement.contacts.find(close->actual_cause.id);
          if (!contact ||
              (contact->first_prefix_id != prefix->id && contact->second_prefix_id != prefix->id) ||
              !same_moment(contact->occurred_at, close->occurred_at, budget))
            fail("GroupContactCancellationSourceRequired", id);
        } else if (close->actual_cause.domain == "travel" &&
                   close->actual_cause.kind == "movement_boundary_contact") {
          // 已真实入场的第三军沿自身单前缀接触；不借初始双军之一作为取消来源。
          const auto *contact = movement.boundary_contacts.find(close->actual_cause.id);
          const auto *head = common.state().facts.find(close->actual_cause.id);
          if (!contact || !head || head->domain != "travel" ||
              head->kind != "movement_boundary_contact" ||
              contact->prefix_id != prefix->id || contact->motion_source.unit_ref != unit->id ||
              !same_moment(contact->occurred_at, close->occurred_at, budget) ||
              !take(same_spatial_anchor(contact->actual_anchor, close->actual_anchor, budget)))
            fail("GroupContactCancellationSourceRequired", id);
        } else {
          fail("GroupContactCancellationSourceRequired", id);
        }
      } else if (std::holds_alternative<MovementPaymentNotConsumed>(
                     unit->payment)) {
        // A cancelled, never-started responsibility has no fabricated close,
        // effort, or fare. Opening/hold time is not consumed movement.
        if (close || !unit->activity_spans.empty() ||
            compare(unit->active_elapsed.value, {}, budget) != 0 ||
            !std::holds_alternative<MovementUnitOpen>(unit->status))
          fail("UnstartedGroupCancellationHasConsumption", id);
      } else if (!close || close->kind != MovementCloseKind::cancelled ||
                 !same_source(close->actual_cause,
                              terminal->actual_terminal_source) ||
                 !same_moment(close->occurred_at, terminal->occurred_at,
                              budget) ||
                 !std::holds_alternative<MovementUnitFinalized>(unit->status))
        fail("ConsumedGroupCancellationCloseRequired", id);
    }
    return ActualGroupJourneyTerminal{*terminal, prefix->execution,
                                      closed->actual_anchor};
  });
}
Status
validate_group_journey_terminals(const CommonStore &common,
                                 const SpatialCarrierState &carriers,
                                 const MovementExactState &movement,
                                 const GroupMovementExecutionState &executions,
                                 ExactArithmeticBudget budget) {
  return status([&] {
    for (const auto &row : carriers.terminals.rows()) {
      if (row.first != row.second.journey_id)
        fail("GroupJourneyTerminalKeyMismatch", row.first);
      take(read_actual_group_journey_terminal(common, carriers, movement,
                                              executions, row.first, budget));
    }
    for (const auto &row : carriers.journeys.rows()) {
      const bool terminal =
          row.second.state == GroupJourneyLifecycle::arrived ||
          row.second.state == GroupJourneyLifecycle::cancelled;
      if (terminal != bool(carriers.terminals.find(row.first)))
        fail("GroupJourneyTerminalCoverage", row.first);
    }
  });
}
} // namespace hanmo::resident
