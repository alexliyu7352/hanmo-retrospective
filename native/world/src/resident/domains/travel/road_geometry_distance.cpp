// 只读原道路的逐段弧长与同图最短路；同距沿原据点/道路 ID 稳定顺序。
#include "road_geometry_distance.hpp"
#include "route_geometry.hpp"
#include "travel.hpp"
#include "movement_exact_detail.hpp"
#include <algorithm>
#include <limits>
#include <map>
#include <set>
namespace hanmo::resident {
namespace {
using namespace movement_detail;
using Wide = __int128_t;
using UWide = __uint128_t;
Integer segment_length(PointI a, PointI b, const StableId &id) {
  Wide dx = Wide(a.x) - b.x, dy = Wide(a.y) - b.y;
  const UWide x = static_cast<UWide>(dx < 0 ? -dx : dx);
  const UWide y = static_cast<UWide>(dy < 0 ? -dy : dy);
  const UWide maximum = std::numeric_limits<Integer>::max();
  // 先限差，再平方；连极端作者坐标也不能在 128 位乘加处先溢出。
  if (x > maximum || y > maximum) fail("RoadGeometryLengthOverflow", id);
  const UWide square = x * x + y * y;
  if (square > maximum * maximum) fail("RoadGeometryLengthOverflow", id);
  UWide low = 0, high = maximum;
  while (low < high) {
    const auto mid = low + (high - low + 1) / 2;
    if (mid * mid <= square) low = mid;
    else high = mid - 1;
  }
  return static_cast<Integer>(low);
}
struct Edge {
  const TravelRoad *road = nullptr;
  StableId map;
  Integer length = 0;
};
} // namespace
Outcome<Integer> read_original_road_geometry_length(const TravelCatalog &catalog,
                                                   const TravelRoad &road) {
  return movement_detail::guard<Integer>([&] {
    take(original_travel_road_world_map(catalog, road));
    const auto points = take(original_travel_road_polyline(catalog, road));
    Integer total = 0;
    for (std::size_t i = 1; i < points.size(); ++i) {
      const auto length = segment_length(points[i - 1], points[i], road.id);
      if (length > std::numeric_limits<Integer>::max() - total)
        fail("RoadGeometryLengthOverflow", road.id);
      total += length;
    }
    return total;
  });
}
static Outcome<OriginalRoadDistanceTree> distance_tree(
    const TravelCatalog &catalog, const Location *origin,
    const RoadFractionalAnchor *fractional, Integer tick,
    ExactArithmeticBudget budget) {
  return movement_detail::guard<OriginalRoadDistanceTree>([&] {
    if (tick < 0) fail("RoadGeometryObservationRequired");
    OriginalRoadDistanceTree out;
    std::map<StableId, OriginalRoadDistance> distances;
    std::map<StableId, Edge> edges;
    // 全目录真实道路先核；缺地图/shape 来源不被“跳过坏边”变成无路。
    for (const auto &id : catalog.roads.order()) {
      const auto *road = catalog.roads.find(id);
      if (!road || road->id != id) fail("RoadGeometryOriginalRoadRequired", id);
      const auto map = take(original_travel_road_world_map(catalog, *road));
      const auto length = take(read_original_road_geometry_length(catalog, *road));
      if (!edges.emplace(id, Edge{road, map, length}).second)
        fail("RoadGeometryOriginalRoadRepeated", id);
    }
    const auto seed = [&](const StableId &town, CanonicalFraction distance,
                          std::vector<StableId> path, bool returning) {
      const auto existing = distances.find(town);
      if (existing == distances.end() || compare(distance, existing->second.distance, budget) < 0)
        distances[town] = OriginalRoadDistance{town, std::move(distance), std::move(path), returning};
    };
    if (const auto *at = origin ? std::get_if<SettlementLocation>(origin) : nullptr) {
      const auto *town = catalog.settlements.find(at->settlement_id);
      if (!town || town->id != at->settlement_id || town->world_map_id.empty() ||
          !catalog.maps.find(town->world_map_id))
        fail("RoadGeometryOriginalOriginRequired", at->settlement_id);
      const auto *place = catalog.places.find(at->place_id);
      if (!place || place->settlement_id != town->id)
        fail("RoadGeometryOriginalOriginRequired", at->place_id);
      out.world_map_id = town->world_map_id;
      seed(town->id, integer(0, budget), {}, false);
    } else if (fractional || (origin && std::holds_alternative<OnRouteLocation>(*origin))) {
      const auto *at = origin ? std::get_if<OnRouteLocation>(origin) : nullptr;
      const auto &edge_id = fractional ? fractional->edge_id : at->edge_id;
      const auto &from = fractional ? fractional->origin : at->origin;
      const auto found = edges.find(edge_id);
      if (found == edges.end()) fail("RoadGeometryOriginalOriginRequired", edge_id);
      const auto &edge = found->second;
      const auto &road = *edge.road;
      if (from != road.origin || road.duration_ticks <= 0 ||
          road.duration_ticks > std::numeric_limits<Integer>::max() / 100 ||
          (at && at->progress < 0))
        fail("RoadGeometryOriginalOriginRequired", edge_id);
      const auto progress = fractional ? fractional->progress : integer(at->progress, budget);
      const auto extent = integer(road.duration_ticks * 100, budget);
      if (compare(progress, integer(0, budget), budget) < 0 ||
          compare(progress, extent, budget) > 0)
        fail("RoadGeometryOriginalOriginRequired", edge_id);
      out.world_map_id = edge.map;
      const auto length = integer(edge.length, budget);
      const auto travelled = mul(length, div(progress, extent, budget), budget);
      // 原 S1 允许在当前边回头，即使前向临时封闭；不额外创造一条反向道路。
      seed(road.origin, travelled, {road.id}, true);
      if (road_is_open(road, tick))
        seed(road.destination, sub(length, travelled, budget), {road.id}, false);
    } else {
      fail("RoadGeometryOriginalOriginRequired");
    }
    std::set<StableId> visited;
    while (true) {
      const OriginalRoadDistance *current = nullptr;
      for (const auto &[id, candidate] : distances)
        if (!visited.contains(id) && (!current || compare(candidate.distance, current->distance, budget) < 0))
          current = &candidate;
      if (!current) break;
      const auto settled = *current;
      visited.insert(settled.settlement_id);
      for (const auto &[id, edge] : edges) {
        const auto &road = *edge.road;
        if (edge.map != out.world_map_id || road.origin != settled.settlement_id ||
            visited.contains(road.destination) || !road_is_open(road, tick)) continue;
        auto distance = add(settled.distance, integer(edge.length, budget), budget);
        const auto old = distances.find(road.destination);
        if (old != distances.end() && compare(distance, old->second.distance, budget) >= 0) continue;
        auto path = settled.road_ids;
        path.push_back(road.id);
        distances[road.destination] = OriginalRoadDistance{
            road.destination, std::move(distance), std::move(path), settled.initial_returning};
      }
    }
    for (auto &[id, distance] : distances) out.reachable.push_back(std::move(distance));
    return out;
  });
}
Outcome<OriginalRoadDistanceTree> read_original_road_distance_tree(
    const TravelCatalog &catalog, const Location &origin, Integer tick,
    ExactArithmeticBudget budget) {
  return distance_tree(catalog, &origin, nullptr, tick, budget);
}
Outcome<OriginalRoadDistanceTree> read_original_road_distance_tree(
    const TravelCatalog &catalog, const MovementAnchor &origin, Integer tick,
    ExactArithmeticBudget budget) {
  if (const auto *ordinary = std::get_if<OriginalMovementAnchor>(&origin))
    return distance_tree(catalog, &ordinary->location, nullptr, tick, budget);
  if (const auto *road = std::get_if<RoadFractionalAnchor>(&origin))
    return distance_tree(catalog, nullptr, road, tick, budget);
  return {false, {}, common_failure("RoadGeometryOriginalOriginRequired").error};
}
} // namespace hanmo::resident
