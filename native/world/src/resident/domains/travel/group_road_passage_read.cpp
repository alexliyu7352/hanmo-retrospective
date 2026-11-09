#include "group_movement.hpp"
#include "movement_exact_detail.hpp"
namespace hanmo::resident {
Outcome<std::optional<PassageBoundaryRef>>
read_actual_group_road_entry_boundary(const SpatialCarrierState &carriers,
                                      const MovementExactState &movement,
                                      const TravelCatalog &catalog,
                                      const GroupMovementExecution &execution,
                                      const WorldMoment &at,
                                      ExactArithmeticBudget budget) {
  return movement_detail::guard<std::optional<PassageBoundaryRef>>(
      [&]() -> std::optional<PassageBoundaryRef> {
        using namespace movement_detail;
        const auto *journey = carriers.journeys.find(execution.journey_id);
        const auto *prefix =
            journey ? movement.prefixes.find(journey->movement_prefix_ref)
                    : nullptr;
        const auto *unit =
            prefix ? movement.units.find(prefix->unit_ref) : nullptr;
        if (!journey || !prefix || !unit ||
            execution_key(prefix->execution) !=
                execution_key(MovementExecutionRef{execution}) ||
            spatial_carrier_key(journey->carrier) !=
                spatial_carrier_key(execution.carrier) ||
            !same_moment(prefix->last_settled_at, at, budget))
          fail("ActualPassageEntrySourceRequired", execution.journey_id);
        const auto *basis = std::get_if<RoadMovementBasis>(&prefix->basis);
        if (!basis)
          fail("OriginalNonRoadPassageBoundaryReaderRequired",
               execution.journey_id);
        // A new charging unit is not another physical entry into the same road.
        if (compare(basis->exact_start_progress, {}, budget) != 0 ||
            compare(prefix->cursor, {}, budget) != 0)
          return std::nullopt;
        if (compare(unit->active_elapsed.value, {}, budget) != 0)
          fail("RoadEntryActivityAlreadyConsumed", unit->id);
        const auto *road = catalog.roads.find(basis->original_inputs.edge_id);
        if (!road)
          fail("PassageOriginalRoadRequired", basis->original_inputs.edge_id);
        const auto &from = basis->original_inputs.origin;
        StableId to;
        if (road->origin == from)
          to = road->destination;
        else if (road->destination == from)
          to = road->origin;
        else
          fail("PassageRoadDirectionMismatch", road->id);
        // 明确作者 road 地图归属优先；global
        // 只保旧无具名地图目录，不改变旅行模式。
        const auto &actual_map = road->world_map_id.empty()
                                     ? catalog.world_map_id
                                     : road->world_map_id;
        PassageBoundaryRef boundary{PassageBoundaryKind::road_edge, road->id,
                                    actual_map, from, std::move(to)};
        take(read_actual_group_passage_entry(carriers, movement, catalog,
                                             execution, boundary, at, budget));
        return boundary;
      });
}
} // namespace hanmo::resident
