#include "field_exit_projection.hpp"
#include "field_author_data.hpp"
#include "field_contact_origin.hpp"
#include "field_integer_geometry.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {
namespace {
using Wide = __int128_t;
std::string decimal_nonnegative(Wide value) {
  char digits[40];
  auto *end = digits + sizeof(digits), *at = end;
  do {
    *--at = char('0' + value % 10);
    value /= 10;
  } while (value);
  return std::string(at, end);
}
bool dot_checked(Wide ax, Wide ay, Wide bx, Wide by, Wide &out) {
  Wide x, y;
  return !__builtin_mul_overflow(ax, bx, &x) &&
         !__builtin_mul_overflow(ay, by, &y) &&
         !__builtin_add_overflow(x, y, &out);
}
} // namespace
// 复用原道路整数弧长投影；不临时伪造formation/Withdrawn/FieldExitRecord。
Outcome<FieldPointRouteProjectionRead> read_field_point_route_projection(
    const FieldBattleWorldState &world, const TravelCatalog &travel,
    const FieldBattleCatalog &catalog, const StableId &battle, PointI actual_point,
    ExactArithmeticBudget budget, const FieldExactContactReadContext *exact_contacts) {
  auto fail = [&](const char *code) -> Outcome<FieldPointRouteProjectionRead> {
    return {false, {}, common_failure(code, battle).error};
  };
  const FieldBattleContactRecord *contact = nullptr;
  for (const auto &[id, row] : world.contacts)
    if (row.battle_id == battle) {
      if (contact)
        return fail("FieldContactSourceMismatch");
      contact = &row;
    }
  if (!contact)
    return fail("FieldContactSourceRequired");
  auto generated = regenerate_field_contact_battle(
      travel, catalog, contact->source, exact_contacts, budget);
  if (!generated.ok)
    return {false, {}, generated.error};
  const auto session = world.sessions.find(battle);
  if (session == world.sessions.end() || generated.value.field_hash != session->second.field_hash)
    return fail("FieldExitGeometryMismatch");
  const auto &p = generated.value.parameters;
  if (actual_point.x < -p.length_m * 5 || actual_point.x >= p.length_m * 5 ||
      actual_point.y < -p.width_m * 5 || actual_point.y >= p.width_m * 5)
    return fail("FieldExitGeometryMismatch");
  const auto &route = generated.value.anchor.route_id;
  auto terrain = read_field_road_terrain(travel, catalog, route);
  if (!terrain.ok)
    return {false, {}, terrain.error};
  auto geometry =
      read_field_road_geometry(travel, catalog, terrain.value->metadata, route);
  if (!geometry.ok)
    return {false, {}, geometry.error};
  const auto &points = geometry.value.points_dm;
  if (points.size() < 2)
    return fail("InvalidFieldBattleRoute");
  const auto map_point = field_point_to_map_dm(
      generated.value.anchor, actual_point);
  Integer prefix = 0;
  std::optional<CanonicalFraction> nearest_distance, nearest_arc;
  std::size_t selected_segment = 0;
  const auto fraction = [&](Wide n, Wide d = 1) {
    return normalize_fraction(decimal_nonnegative(n), decimal_nonnegative(d),
                              budget);
  };
  for (std::size_t i = 1; i < points.size(); ++i) {
    const Wide dx = Wide(points[i].x) - points[i - 1].x;
    const Wide dy = Wide(points[i].y) - points[i - 1].y;
    const Wide px = Wide(map_point.x) - points[i - 1].x;
    const Wide py = Wide(map_point.y) - points[i - 1].y;
    Wide square, projection, distance;
    if (!dot_checked(dx, dy, dx, dy, square) ||
        !dot_checked(px, py, dx, dy, projection) ||
        !dot_checked(px, py, px, py, distance))
      return fail("FieldExitGeometryArithmeticOverflow");
    if (!square)
      continue;
    const Integer length = field_integer_sqrt(__uint128_t(square));
    if (length <= 0 || prefix > std::numeric_limits<Integer>::max() - length)
      return fail("FieldExitGeometryArithmeticOverflow");
    projection = std::clamp<Wide>(projection, 0, square);
    auto parameter = fraction(projection, square);
    if (!parameter.ok)
      return {false, {}, parameter.error};
    CanonicalFraction distance_squared;
    if (projection == square) {
      const Wide ex = Wide(map_point.x) - points[i].x;
      const Wide ey = Wide(map_point.y) - points[i].y;
      if (!dot_checked(ex, ey, ex, ey, distance))
        return fail("FieldExitGeometryArithmeticOverflow");
    }
    auto d = fraction(distance);
    if (!d.ok)
      return {false, {}, d.error};
    distance_squared = std::move(d.value);
    if (projection > 0 && projection < square) {
      auto dot = fraction(projection);
      if (!dot.ok)
        return {false, {}, dot.error};
      auto reduction = multiply_fractions(dot.value, parameter.value, budget);
      if (!reduction.ok)
        return {false, {}, reduction.error};
      auto reduced =
          subtract_fractions(distance_squared, reduction.value, budget);
      if (!reduced.ok)
        return {false, {}, reduced.error};
      distance_squared = std::move(reduced.value);
    }
    bool nearer = !nearest_distance;
    if (nearest_distance) {
      auto order =
          compare_fractions(distance_squared, *nearest_distance, budget);
      if (!order.ok)
        return {false, {}, order.error};
      nearer =
          order.value < 0; // Equal projection keeps original segment order.
    }
    if (nearer) {
      auto segment_length = fraction(length), before = fraction(prefix);
      if (!segment_length.ok || !before.ok)
        return {false,
                {},
                !segment_length.ok ? segment_length.error : before.error};
      auto along =
          multiply_fractions(parameter.value, segment_length.value, budget);
      if (!along.ok)
        return {false, {}, along.error};
      auto arc = add_fractions(before.value, along.value, budget);
      if (!arc.ok)
        return {false, {}, arc.error};
      nearest_distance = std::move(distance_squared);
      nearest_arc = std::move(arc.value);
      selected_segment = i - 1;
    }
    prefix += length;
  }
  if (!nearest_arc || prefix <= 0)
    return fail("InvalidFieldBattleRoute");
  auto total = fraction(prefix);
  if (!total.ok)
    return {false, {}, total.error};
  auto progress = divide_fractions(*nearest_arc, total.value, budget);
  if (!progress.ok)
    return {false, {}, progress.error};
  return {true,
          {geometry.value.route_id, map_point, std::move(progress.value), selected_segment},
          {}};
}
// 旧formation退出入口仍先证原Withdrawn，再调用完全相同的地图投影。
Outcome<FieldRouteExitProjectionRead> read_field_route_exit_projection(
    const CommonStore &common, const FieldBattleWorldState &world,
    const TravelCatalog &travel, const FieldBattleCatalog &catalog,
    const StableId &battle, const StableId &formation, ExactArithmeticBudget budget,
    const FieldExactContactReadContext *exact_contacts) {
  auto departure = read_field_actual_departure_source(common, world, battle, formation);
  if (!departure.ok) return {false, {}, departure.error};
  auto point = read_field_point_route_projection(world, travel, catalog, battle,
      departure.value.formation->position_dm, budget, exact_contacts);
  if (!point.ok) return {false, {}, point.error};
  return {true, {std::move(departure.value), std::move(point.value.route_id),
      point.value.actual_map_point_dm, std::move(point.value.route_fraction),
      point.value.original_segment}, {}};
}
} // namespace hanmo::resident
