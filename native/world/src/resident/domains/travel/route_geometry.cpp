// 原道路作者几何和有理交点的纯叶；从 route_contact.cpp
// 移出，算法和调用语义不变。
#include "route_geometry.hpp"
#include "travel.hpp"
#include <algorithm>
#include <cstdlib>
namespace hanmo::resident {
// 保留起城、作者 shape、终城顺序；接触 canonical 化由旧调用方自行完成。
Outcome<std::vector<PointI>>
original_travel_road_polyline(const TravelCatalog &catalog,
                              const TravelRoad &edge) {
  const auto *origin = catalog.settlements.find(edge.origin);
  const auto *destination = catalog.settlements.find(edge.destination);
  if (!origin || !destination)
    return {false,
            {},
            common_failure("InvalidTravelPath", {}, ErrorKind::internal).error};
  return original_travel_road_polyline(*origin, edge, *destination);
}
// 作者两端与道路须逐 ID 一致，几何只由这一叶组装一次。
Outcome<std::vector<PointI>>
original_travel_road_polyline(const TravelSettlement &origin,
                              const TravelRoad &edge,
                              const TravelSettlement &destination) {
  if (origin.id != edge.origin || destination.id != edge.destination)
    return {false,
            {},
            common_failure("InvalidTravelPath", {}, ErrorKind::internal).error};
  std::vector<PointI> result{origin.map_xy};
  result.insert(result.end(), edge.shape.begin(), edge.shape.end());
  result.push_back(destination.map_xy);
  return {true, std::move(result), {}};
}

// 此叶只读同一原目录的归属，不生成地图 ID 或从几何包围盒猜范围。
Outcome<StableId>
original_travel_road_world_map(const TravelSettlement &origin,
                               const TravelRoad &road,
                               const TravelSettlement &destination) {
  if (origin.id != road.origin || destination.id != road.destination)
    return {false, {}, common_failure("InvalidTravelPath", road.id).error};
  if (origin.world_map_id.empty() || destination.world_map_id.empty())
    return {false, {}, common_failure("TravelRouteMapRequired", road.id).error};
  if (origin.world_map_id != destination.world_map_id ||
      (!road.world_map_id.empty() && road.world_map_id != origin.world_map_id))
    return {false, {}, common_failure("TravelRouteMapMismatch", road.id).error};
  return {true, origin.world_map_id, {}};
}
// 相同坐标不证明同图；只有同目录的真实端点引用能选择地图。
Outcome<StableId> original_travel_road_world_map(const TravelCatalog &catalog,
                                                 const TravelRoad &road) {
  const auto *stored = catalog.roads.find(road.id);
  const auto *origin = catalog.settlements.find(road.origin);
  const auto *destination = catalog.settlements.find(road.destination);
  if (stored != &road || !origin || !destination)
    return {false, {}, common_failure("InvalidTravelPath", road.id).error};
  auto map = original_travel_road_world_map(*origin, road, *destination);
  if (!map.ok)
    return map;
  if (!catalog.maps.find(map.value))
    return {false, {}, common_failure("UnknownWorldMap", map.value).error};
  return map;
}

namespace {
// 当前边的两端和作者折线按城市ID方向统一；同端点但不同中途路线不能相遇。
Outcome<std::vector<PointI>> line(const TravelCatalog &catalog,
                                  const TravelRoad &edge) {
  auto result = original_travel_road_polyline(catalog, edge);
  if (result.ok && edge.origin > edge.destination)
    std::reverse(result.value.begin(), result.value.end());
  return result;
}
// 整数作者点逐点比较，不能用包围框或近似浮点把不同道路当成同一条线。
bool same_line(const std::vector<PointI> &a, const std::vector<PointI> &b) {
  if (a.size() != b.size())
    return false;
  for (std::size_t i = 0; i < a.size(); ++i)
    if (a[i].x != b[i].x || a[i].y != b[i].y)
      return false;
  return true;
}
// 原整数比例交点排除据点端点，时刻保持分子分母而不量化为浮点。
std::optional<ContactHit> intersection(const TravelCatalog &catalog,
                                       const RoadTravelMotion &a,
                                       const RoadTravelMotion &b) {
  if (!a.moving && !b.moving)
    return {};
  auto ea = catalog.roads.find(a.edge_id), eb = catalog.roads.find(b.edge_id);
  if (!ea || !eb)
    return {};
  auto line_a = line(catalog, *ea), line_b = line(catalog, *eb);
  if (!line_a.ok || !line_b.ok)
    return {};
  if (!((ea->origin == eb->origin && ea->destination == eb->destination) ||
        (ea->origin == eb->destination && ea->destination == eb->origin)) ||
      !same_line(line_a.value, line_b.value))
    return {};
  Integer la = ea->duration_ticks * 100, lb = eb->duration_ticks * 100;
  Integer a0 = ea->origin < ea->destination ? a.start : la - a.start,
          a1 = ea->origin < ea->destination ? a.end : la - a.end,
          b0 = eb->origin < eb->destination ? b.start : lb - b.start,
          b1 = eb->origin < eb->destination ? b.end : lb - b.end;
  Integer gap0 = a0 * lb - b0 * la, gap1 = a1 * lb - b1 * la;
  if (gap0 != 0 && gap1 != 0 && (gap0 < 0) == (gap1 < 0))
    return {};
  Integer denominator = std::abs(gap0 - gap1), numerator = std::abs(gap0);
  if (denominator == 0 || numerator > denominator)
    return {};
  Integer pa = (a.start * denominator + (a.end - a.start) * numerator) /
               denominator,
          pb = (b.start * denominator + (b.end - b.start) * numerator) /
               denominator;
  if (pa <= 0 || pa >= la || pb <= 0 || pb >= lb)
    return {};
  ContactHit hit;
  hit.a = std::min(a.actor_id, b.actor_id);
  hit.b = std::max(a.actor_id, b.actor_id);
  hit.time_num = numerator;
  hit.time_den = denominator;
  RoadContactEndpoint first{a.actor_id, {a.edge_id, a.origin, pa}, a},
      second{b.actor_id, {b.edge_id, b.origin, pb}, b};
  if (first.actor_id != hit.a)
    std::swap(first, second);
  hit.road = RoadContactEndpoints{std::move(first), std::move(second)};
  return hit;
}
} // namespace
} // namespace hanmo::resident

namespace hanmo::resident {
std::optional<ContactHit>
original_road_contact_intersection(const TravelCatalog &catalog,
                                   const RoadTravelMotion &a,
                                   const RoadTravelMotion &b) {
  return intersection(catalog, a, b);
}
} // namespace hanmo::resident
