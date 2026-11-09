#include "group_movement.hpp"
#include "movement_exact_detail.hpp"
#include "movement_result_read.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/storage/common_store.hpp"
#include <limits>
#include <map>
namespace hanmo::resident {
using namespace movement_detail;
Outcome<std::optional<ActualPassageBoundaryCompletion>>
read_actual_group_boundary_completion(const CommonStore &common,
                                      const SpatialCarrierState &carriers,
                                      const MovementExactState &movement,
                                      const GroupMovementExecutionState& executions,
                                      const TravelCatalog &catalog,
                                      const BoundaryCrossingReceipt &crossing,
                                      ExactArithmeticBudget budget) {
  return guard<std::optional<ActualPassageBoundaryCompletion>>(
      [&]() -> std::optional<ActualPassageBoundaryCompletion> {
        if (carriers.crossings.find(crossing.receipt_id) != &crossing)
          fail("BorrowedActualCrossingRequired", crossing.receipt_id);
        const auto *header = common.state().facts.find(crossing.receipt_id);
        if (!header || header->domain != "travel" ||
            header->kind != "boundary_crossing")
          fail("GroupCrossingSourceHeadRequired", crossing.receipt_id);
        take(validate_group_boundary_crossing_source(
            common,executions,carriers, movement, catalog, crossing, budget));
        const auto &execution =
            std::get<GroupMovementExecution>(crossing.execution);
        std::map<Integer, const MovementUnitState *> ordered;
        for (const auto &row : movement.units.rows())
          if (const auto *e =
                  std::get_if<GroupMovementExecution>(&row.second.execution))
            if (e->journey_id == execution.journey_id &&
                e->segment_ordinal == execution.segment_ordinal &&
                row.second.ordinal >= crossing.entry_ordinal - 1) {
              if (!ordered.emplace(row.second.ordinal, &row.second).second)
                fail("DuplicateGroupUnitOrdinal", execution.journey_id);
            }
        Integer expected = crossing.entry_ordinal - 1;
        const MovementUnitCloseSource *previous = nullptr;
        const auto *road =
            crossing.boundary.kind == PassageBoundaryKind::road_edge
                ? catalog.roads.find(crossing.boundary.id)
                : nullptr;
        if (crossing.boundary.kind == PassageBoundaryKind::road_edge &&
            (!road || road->duration_ticks < 1 ||
             road->duration_ticks > std::numeric_limits<Integer>::max() / 100))
          fail("PassageRoadBoundaryMismatch", crossing.receipt_id);
        for (const auto &row : ordered) {
          const auto &unit = *row.second;
          if (row.first != expected || unit.effort_basis_refs.empty())
            fail("GroupCrossingContinuationGap", crossing.receipt_id);
          const auto *first =
              movement.exertion_bases.find(unit.effort_basis_refs.front().id);
          const auto *last =
              movement.exertion_bases.find(unit.effort_basis_refs.back().id);
          if (!first || !last)
            fail("GroupCrossingBasisRequired", unit.id);
          if (previous) {
            if (previous->kind == MovementCloseKind::contact) {
              const auto &source = first->original_rule_source;
              const auto *continuation =
                  source.domain == "travel" &&
                          source.kind == "movement_contact_continuation"
                      ? movement.contact_continuations.find(source.id)
                      : nullptr;
              const auto *head =
                  continuation ? common.state().facts.find(continuation->id)
                               : nullptr;
              if (!continuation || !head || head->domain != "travel" ||
                  head->kind != "movement_contact_continuation" ||
                  continuation->held || continuation->resolution_schema != 1 ||
                  continuation->actual_resolution_source.domain != "military" ||
                  continuation->actual_resolution_source.kind !=
                      "field_battle_segment" ||
                  continuation->closed_unit_ref != previous->unit_ref ||
                  continuation->new_unit_ref != unit.id ||
                  continuation->prefix_id != previous->prefix_id ||
                  previous->actual_cause.domain != "travel" ||
                  previous->actual_cause.kind != "movement_contact" ||
                  continuation->contact_ref != previous->actual_cause.id ||
                  !same_moment(first->opened_at, continuation->occurred_at,
                               budget) ||
                  take(compare_world_moments(previous->occurred_at,
                                             continuation->occurred_at,
                                             budget)) > 0)
                fail("GroupCrossingFieldContinuationSourceMismatch", unit.id);
              const auto resumed =
                  take(read_actual_common_fact_position(common, source));
              const auto resolved = take(read_actual_common_fact_position(
                  common, continuation->actual_resolution_source));
              if (take(read_actual_common_fact_position(
                      common, SourceRef{"travel", "movement_unit_close",
                                        previous->id})) >= resolved ||
                  resolved >= resumed ||
                  take(read_actual_common_fact_position(
                      common, unit.effort_basis_refs.front())) <= resumed)
                fail("GroupCrossingFieldContinuationOrderMismatch", unit.id);
            } else if (!same_source(
                           first->original_rule_source,
                           {"travel", "movement_unit_close", previous->id}) ||
                       !same_moment(first->opened_at, previous->occurred_at,
                                    budget))
              fail("GroupCrossingContinuationSourceMismatch", unit.id);
          }
          if (road) {
            const auto *b =
                std::get_if<RoadMovementBasis>(&first->actual_basis);
            if (!b || b->original_inputs.edge_id != road->id ||
                b->original_inputs.origin != crossing.boundary.from_site_id)
              fail("GroupCrossingLeftEdgeWithoutCompletion", unit.id);
            if (previous) {
              MovementPrefixState start;
              start.basis = first->actual_basis;
              start.cursor = first->opened_cursor;
              start.lifecycle = MovementActive{};
              if (!take(same_actual_movement_position(
                      take(read_movement_position(start, budget)),
                      previous->actual_anchor, budget)))
                fail("GroupCrossingContinuationAnchorMismatch", unit.id);
            }
          }
          const auto *close = movement.unit_closes.find(unit.id + ":close");
          if (!close) {
            if (!std::holds_alternative<MovementUnitOpen>(unit.status))
              fail("GroupCrossingCloseSourceRequired", unit.id);
            return {};
          }
          const auto *close_head = common.state().facts.find(close->id);
          if (!close_head || close_head->domain != "travel" ||
              close_head->kind != "movement_unit_close" ||
              execution_key(close->execution) !=
                  execution_key(unit.execution) ||
              take(compare_world_moments(close->occurred_at, crossing.entry_at,
                                         budget)) < 0)
            fail("GroupCrossingCloseSourceRequired", unit.id);
          take(read_verified_travel_exertion(movement, unit.id + ":exertion",
                                             budget));
          MovementPrefixState end;
          end.basis = last->actual_basis;
          end.cursor = close->cursor;
          end.lifecycle = MovementActive{};
          const auto true_anchor = take(read_movement_position(end, budget));
          if (!take(same_actual_movement_position(
                  true_anchor, close->actual_anchor, budget)))
            fail("GroupCrossingCloseAnchorMismatch", unit.id);
          if (road) {
            const auto *b = std::get_if<RoadMovementBasis>(&last->actual_basis);
            if (!b || b->original_inputs.edge_id != road->id ||
                b->original_inputs.origin != crossing.boundary.from_site_id)
              fail("GroupCrossingBasisChangedEdge", unit.id);
            const MovementAnchor endpoint = OriginalMovementAnchor{
                OnRouteLocation{road->id, crossing.boundary.from_site_id,
                                road->duration_ticks * 100},
                {"travel", "catalog_road", road->id}};
            if (take(same_actual_movement_position(true_anchor, endpoint,
                                                   budget)))
              return ActualPassageBoundaryCompletion{
                  {{"travel", "movement_unit_close", close->id}, 1},
                  close->occurred_at,
                  true_anchor,
                  crossing.boundary.to_site_id};
          } else if (close->kind == MovementCloseKind::arrival) {
            const auto arrival = take(read_verified_movement_arrival(
                movement, close->actual_cause, 1, budget));
            const auto *arrival_head = common.state().facts.find(arrival.id);
            if (!arrival_head || arrival_head->domain != "travel" ||
                arrival_head->kind != "movement_arrival")
              fail("GroupBoundaryArrivalHeadRequired", arrival.id);
            if (arrival.destination_settlement_id &&
                *arrival.destination_settlement_id ==
                    crossing.boundary.to_site_id)
              return ActualPassageBoundaryCompletion{
                  {close->actual_cause, 1},
                  arrival.occurred_at,
                  arrival.actual_anchor,
                  *arrival.destination_settlement_id};
            return {};
          }
          if (close->kind != MovementCloseKind::carry) {
            if (close->kind != MovementCloseKind::contact)
              return {};
            const MovementContactContinuationSource *adopted = nullptr;
            for (const auto &item : movement.contact_continuations.rows())
              if (item.second.closed_unit_ref == unit.id) {
                if (adopted)
                  fail("GroupCrossingFieldContinuationAmbiguous", unit.id);
                adopted = &item.second;
              }
            if (!adopted)
              return {};
            if (adopted->actual_resolution_source.domain != "military" ||
                adopted->actual_resolution_source.kind !=
                    "field_battle_segment")
              return {};
            const auto *next = movement.units.find(adopted->new_unit_ref);
            if (!next || unit.ordinal == std::numeric_limits<Integer>::max() ||
                next->ordinal != unit.ordinal + 1 ||
                execution_key(next->execution) != execution_key(unit.execution))
              fail("GroupCrossingFieldContinuationUnitRequired", unit.id);
          }
          previous = close;
          if (expected == std::numeric_limits<Integer>::max())
            fail("GroupUnitOrdinalOverflow", unit.id);
          ++expected;
        }
        return {};
      });
}
} // namespace hanmo::resident
