#include "field_reinforcement_geometry.hpp"
#include "field_deployment.hpp"
#include "field_integer_geometry.hpp"
#include "field_navigation.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include <limits>
namespace hanmo::resident {
namespace {
using namespace movement_detail;
Integer rounded(__int128_t value, __int128_t weight) {
  if (weight <= 0)
    fail("FieldReinforcementWeightRequired");
  const bool negative = value < 0;
  const auto magnitude = negative ? -value : value;
  const auto result = (magnitude + weight / 2) / weight;
  if (result > std::numeric_limits<Integer>::max())
    fail("FieldReinforcementPositionOverflow");
  return negative ? -Integer(result) : Integer(result);
}
PointI rear(PointI center, PointI exit) {
  const auto square = field_distance_squared(center, exit);
  if (square <= __int128_t(2000) * 2000)
    return exit;
  const auto length = field_integer_sqrt(square);
  return {center.x + rounded((__int128_t(exit.x) - center.x) * 2000, length),
          center.y + rounded((__int128_t(exit.y) - center.y) * 2000, length)};
}
PointI passable(const GeneratedBattleField &field, PointI desired,
                FieldTroopCategory category, bool nearest_cell) {
  const auto kind = std::size_t(category);
  if (kind > std::size_t(FieldTroopCategory::Cavalry))
    fail("InvalidFieldTroopCategory");
  const auto allowed = [&](std::size_t n) {
    const auto terrain = std::size_t(field.cells[n].terrain);
    return field_cell_passable(field.cells[n].terrain) &&
           terrain < field.parameters.terrain_speed_bp.size() &&
           field.parameters.terrain_speed_bp[terrain][kind] > 0;
  };
  const auto &p = field.parameters;
  if (!nearest_cell && p.cell_m > 0 && field.columns > 0 &&
      desired.x >= -p.length_m * 5 && desired.x < p.length_m * 5 &&
      desired.y >= -p.width_m * 5 && desired.y < p.width_m * 5) {
    const auto n = std::size_t((desired.y + p.width_m * 5) / (p.cell_m * 10) *
                                   field.columns +
                               (desired.x + p.length_m * 5) / (p.cell_m * 10));
    if (n < field.cells.size() && allowed(n))
      return desired;
  }
  std::optional<std::size_t> found;
  __int128_t best = 0;
  for (std::size_t n = 0; n < field.cells.size(); ++n) {
    if (!allowed(n))
      continue;
    const auto point = field_cell_center_dm(field, n);
    const auto distance = field_distance_squared(point, desired);
    if (!found || distance < best || (distance == best && n < *found)) {
      found = n;
      best = distance;
    }
  }
  if (!found)
    fail("FieldReinforcementPassableCellRequired");
  return field_cell_center_dm(field, *found);
}
bool present(const FieldFormation &formation, Integer side,
             FieldTroopCategory arriving_category) {
  return formation.side == side && !formation.fit.empty() &&
         formation.state != FieldFormationState::Routing &&
         formation.state != FieldFormationState::Withdrawn &&
         formation.state != FieldFormationState::Destroyed &&
         formation.state != FieldFormationState::Deploying;
}
} // namespace
static Outcome<FieldReinforcementRallyRead>
read_rally(const FieldBattleSession &session, const GeneratedBattleField &field,
           Integer side, FieldTroopCategory arriving_category,
           const FieldMotionPrefixRead *prefix) {
  return guard<FieldReinforcementRallyRead>([&] {
    if (!prefix &&
        (session.state != FieldBattleState::Running || session.result))
      fail("FieldReinforcementRunningSessionRequired", session.battle_id);
    if (prefix) {
      if (prefix->formations.size() != session.formations.size() ||
          prefix->at.tick > session.now.tick ||
          (prefix->at.tick == session.now.tick && prefix->at.t > session.now.t))
        fail("FieldReinforcementOriginalMotionPrefixRequired",
             session.battle_id);
      for (const auto &[id, row] : session.formations)
        if (!prefix->formations.contains(id) ||
            prefix->formations.at(id).fit < 0)
          fail("FieldReinforcementOriginalMotionPrefixRequired", id);
    }
    const auto available = [&](const FieldFormation &formation) {
      if (!prefix)
        return present(formation, side, arriving_category);
      const auto &actual = prefix->formations.at(formation.formation_id);
      return formation.side == side && actual.fit > 0 && !actual.routing &&
             !actual.withdrawn;
    };
    const auto *zone = take(read_field_side_geometry(session, field, side));
    bool infantry = false, any_noncommander = false;
    for (const auto &[id, formation] : session.formations)
      if (available(formation)) {
        any_noncommander =
            any_noncommander || formation.commander_actor_id !=
                                    session.sides[side].commander_actor_id;
        infantry =
            infantry || formation.category == FieldTroopCategory::Infantry;
      }
    PointI center = zone->deployment_center_dm;
    if (any_noncommander) {
      __int128_t weight = 0, x = 0, y = 0;
      for (const auto &[id, formation] : session.formations) {
        if (!available(formation))
          continue;
        if (infantry ? formation.category != FieldTroopCategory::Infantry
                     : formation.commander_actor_id ==
                           session.sides[side].commander_actor_id)
          continue;
        const auto count = prefix ? __int128_t(prefix->formations.at(id).fit)
                                  : __int128_t(formation.fit.size());
        const auto position = prefix ? prefix->formations.at(id).position_dm
                                     : formation.position_dm;
        weight += count;
        x += count * position.x;
        y += count * position.y;
      }
      // affd39b8: the explicitly empty selection falls back to this side's
      // original deployment center; it is not a fabricated commander weight.
      if (weight) {
        center = {rounded(x, weight), rounded(y, weight)};
        center = passable(field, center, arriving_category, false);
        center = passable(field, rear(center, zone->road_exit_dm),
                          arriving_category, true);
      }
    }
    center = passable(field, center, arriving_category, false);
    return FieldReinforcementRallyRead{
        center, passable(field, rear(center, zone->road_exit_dm),
                         arriving_category, false)};
  });
}
Outcome<FieldReinforcementRallyRead>
read_field_reinforcement_rally(const FieldBattleSession &session,
                               const GeneratedBattleField &field, Integer side,
                               FieldTroopCategory category) {
  return read_rally(session, field, side, category, nullptr);
}
Outcome<FieldReinforcementRallyRead> read_field_reinforcement_rally_at_prefix(
    const FieldBattleSession &session, const GeneratedBattleField &field,
    Integer side, FieldTroopCategory category,
    const FieldMotionPrefixRead &prefix) {
  return read_rally(session, field, side, category, &prefix);
}
Outcome<bool>
read_field_reinforcement_entry_clear(const FieldBattleSession &session,
                                     PointI entry, const StableId &previous) {
  return guard<bool>([&] {
    const auto found = session.formations.find(previous);
    if (found == session.formations.end() ||
        found->second.formation_id != previous)
      fail("FieldReinforcementPreviousEntryRequired", previous);
    const auto &formation = found->second;
    // An actually removed formation has no active collision circle. This is
    // not permission to skip an unrecorded predecessor or still-live body.
    if (formation.state == FieldFormationState::Withdrawn ||
        formation.state == FieldFormationState::Destroyed ||
        formation.fit.empty())
      return true;
    if (formation.ranks <= 0)
      fail("InvalidFieldFormation", previous);
    const auto radius = field_formation_radius_dm(formation);
    return field_distance_squared(entry, formation.position_dm) >
           __int128_t(radius) * radius;
  });
}
} // namespace hanmo::resident
