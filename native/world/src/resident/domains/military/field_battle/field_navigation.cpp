#include "resident/model/common_model.hpp"
#include "field_navigation.hpp"
#include "field_deployment.hpp"
#include "field_integer_geometry.hpp"
#include "field_navigation_source.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <queue>
#include <tuple>
namespace hanmo::resident {
namespace {
// 坏几何不能降级为平地；调用者仍须先核同场 field_hash。
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code) {
  throw Failure{common_failure(code).error};
}
void validate(const GeneratedBattleField &field) {
  const auto &p = field.parameters;
  auto parameters = validate_field_battle_parameters(p);
  if (!parameters.ok)
    throw Failure{parameters.error};
  if (p.cell_m <= 0 || p.length_m <= 0 || p.width_m <= 0 ||
      p.length_m % p.cell_m || p.width_m % p.cell_m ||
      field.columns != p.length_m / p.cell_m ||
      field.rows != p.width_m / p.cell_m || field.columns <= 0 ||
      field.rows <= 0 ||
      __int128_t(field.columns) * field.rows != field.cells.size() ||
      __int128_t(p.length_m) * 10 > std::numeric_limits<Integer>::max() ||
      __int128_t(p.width_m) * 10 > std::numeric_limits<Integer>::max())
    fail("FieldNavigationGeometryRequired");
}
std::size_t cell(const GeneratedBattleField &field, PointI point) {
  const auto &p = field.parameters;
  if (point.x < -p.length_m * 5 || point.x >= p.length_m * 5 ||
      point.y < -p.width_m * 5 || point.y >= p.width_m * 5)
    fail("FieldNavigationPointOutsideField");
  const Integer x = (point.x + p.length_m * 5) / (p.cell_m * 10);
  const Integer y = (point.y + p.width_m * 5) / (p.cell_m * 10);
  return static_cast<std::size_t>(y * field.columns + x);
}
Integer speed(const GeneratedBattleField &field, std::size_t index,
              FieldTroopCategory category) {
  const auto kind = static_cast<std::size_t>(field.cells[index].terrain);
  const auto troop = static_cast<std::size_t>(category);
  if (kind >= field.parameters.terrain_speed_bp.size() || troop >= 3)
    fail("FieldNavigationTerrainRequired");
  const auto value = field.parameters.terrain_speed_bp[kind][troop];
  if (value < 0 || value > 10000)
    fail("FieldNavigationTerrainSpeedRequired");
  return value;
}
// 纯内部稳定次序：同 f/g 取格序小者；不消费 RNG 或改写编队任务。
struct Node {
  Integer estimate, cost;
  std::size_t index;
  bool operator>(const Node &other) const {
    return std::tie(estimate, cost, index) >
           std::tie(other.estimate, other.cost, other.index);
  }
};
} // namespace
static Outcome<FieldNavigationPath>
read_navigation_path_impl(const GeneratedBattleField &field,
                          FieldTroopCategory category, PointI from_dm,
                          PointI target_dm,
                          std::optional<std::size_t> terminal_gate) {
  try {
    validate(field);
    const auto start = cell(field, from_dm), goal = cell(field, target_dm);
    const auto path_speed = [&](std::size_t index) {
      if (terminal_gate && index == *terminal_gate && index == goal &&
          field.cells[index].terrain == FieldTerrainKind::Gate)
        return field.parameters.terrain_speed_bp[static_cast<std::size_t>(
            FieldTerrainKind::Road)][static_cast<std::size_t>(category)];
      return speed(field, index, category);
    };
    FieldNavigationPath out;
    if (!path_speed(start) || !path_speed(goal))
      return {true, std::move(out), {}};
    const auto count = field.cells.size();
    const auto infinity = std::numeric_limits<Integer>::max();
    std::vector<Integer> costs(count, infinity);
    std::vector<std::size_t> parents(count, count);
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;
    // 1000 分之一格的正交/斜向长度；启发值采用同一八邻距离下界。
    const auto estimate = [&](std::size_t index) {
      const auto x = static_cast<Integer>(index % field.columns);
      const auto y = static_cast<Integer>(index / field.columns);
      const auto gx = static_cast<Integer>(goal % field.columns);
      const auto gy = static_cast<Integer>(goal / field.columns);
      const auto dx = x > gx ? x - gx : gx - x;
      const auto dy = y > gy ? y - gy : gy - y;
      return 1414 * std::min(dx, dy) +
             1000 * (std::max(dx, dy) - std::min(dx, dy));
    };
    costs[start] = 0;
    open.push({estimate(start), 0, start});
    while (!open.empty()) {
      const auto next = open.top();
      open.pop();
      if (next.cost != costs[next.index])
        continue;
      if (next.index == goal) {
        out.reachable = true;
        for (auto index = goal;; index = parents[index]) {
          out.cells.push_back(index);
          if (index == start)
            break;
          if (parents[index] == count)
            fail("FieldNavigationParentRequired");
        }
        std::reverse(out.cells.begin(), out.cells.end());
        return {true, std::move(out), {}};
      }
      const Integer x = next.index % field.columns,
                    y = next.index / field.columns;
      for (Integer dy = -1; dy <= 1; ++dy)
        for (Integer dx = -1; dx <= 1; ++dx) {
          if ((!dx && !dy) || x + dx < 0 || x + dx >= field.columns ||
              y + dy < 0 || y + dy >= field.rows)
            continue;
          const auto index =
              static_cast<std::size_t>((y + dy) * field.columns + x + dx);
          const auto multiplier = path_speed(index);
          if (!multiplier)
            continue;
          // 斜边不能穿过两个相邻阻挡格的夹角；道路八邻仍用同一格通行表。
          if (dx && dy &&
              (!speed(field,
                      static_cast<std::size_t>(y * field.columns + x + dx),
                      category) ||
               !speed(field,
                      static_cast<std::size_t>((y + dy) * field.columns + x),
                      category)))
            continue;
          const Integer edge = (dx && dy ? 1414 : 1000) * 10000 / multiplier;
          if (next.cost > infinity - edge - estimate(index))
            fail("FieldNavigationCostOverflow");
          const auto cost = next.cost + edge;
          if (cost >= costs[index])
            continue;
          costs[index] = cost;
          parents[index] = next.index;
          open.push({cost + estimate(index), cost, index});
        }
    }
    return {true, std::move(out), {}};
  } catch (Failure &error) {
    return {false, {}, std::move(error.error)};
  }
}
Outcome<FieldNavigationPath>
read_field_navigation_path(const GeneratedBattleField &field,
                           FieldTroopCategory category, PointI from,
                           PointI target) {
  return read_navigation_path_impl(field, category, from, target, {});
}

namespace {
FieldMotionPrefixFormation
motion_projection(const FieldFormation &f,
                  const FieldMotionPrefixRead *prefix) {
  if (!prefix)
    return {f.position_dm, Integer(f.fit.size()),
            f.state == FieldFormationState::Routing,
            f.state == FieldFormationState::Withdrawn ||
                f.state == FieldFormationState::Destroyed,
            std::holds_alternative<FieldWithdraw>(f.mission)};
  auto found = prefix->formations.find(f.formation_id);
  if (found == prefix->formations.end() || found->second.fit < 0 ||
      found->second.fit > 500)
    fail("FieldMotionOriginalPrefixRequired");
  return found->second;
}
Integer projected_radius(const FieldFormation &f,
                         const FieldMotionPrefixRead *prefix) {
  if (!prefix)
    return field_formation_radius_dm(f);
  const auto actual = motion_projection(f, prefix);
  if (f.ranks <= 0)
    fail("FieldMotionOriginalPrefixRequired");
  return field_formation_radius_dm(f, actual.fit);
}
} // namespace
// 有限线段与原外接圆求最早接触；sqrt 向上取整确保不穿过接触边界。
static Outcome<PointI> read_contact_point(const FieldBattleSession &session,
                                          const FieldFormation &formation,
                                          PointI from, PointI proposed_dm,
                                          const FieldMotionPrefixRead *prefix) {
  try {
    const auto found = session.formations.find(formation.formation_id);
    if (found == session.formations.end() || &found->second != &formation)
      fail("FieldMotionOriginalFormationRequired");

    const __int128_t vx = __int128_t(proposed_dm.x) - from.x;
    const __int128_t vy = __int128_t(proposed_dm.y) - from.y;
    // 单步输入必须可由原 Integer 距离算术表示；拒绝越界而非溢出后绕过碰撞。
    const auto absolute_input = [](__int128_t value) {
      return value < 0 ? -value : value;
    };
    if (absolute_input(vx) > 2147483647 || absolute_input(vy) > 2147483647)
      fail("FieldMotionDistanceOverflow");
    const __int128_t length_squared = vx * vx + vy * vy;
    if (!length_squared)
      return {true, from, {}};
    // 原战场尺寸界内平方根最大不超过 Integer；越界输入不扩为任意世界坐标。
    if (length_squared > std::numeric_limits<Integer>::max())
      fail("FieldMotionDistanceOverflow");
    __int128_t fraction_n = 1, fraction_d = 1;
    const Integer radius = projected_radius(formation, prefix);
    for (const auto &[id, other] : session.formations) {
      const auto actual = motion_projection(other, prefix);
      if (id == formation.formation_id || actual.fit == 0 || actual.withdrawn)
        continue;
      const __int128_t x = __int128_t(actual.position_dm.x) - from.x;
      const __int128_t y = __int128_t(actual.position_dm.y) - from.y;
      const __int128_t sum =
          __int128_t(radius) + projected_radius(other, prefix);
      if (absolute_input(x) > 2147483647 || absolute_input(y) > 2147483647 ||
          sum < 0 || sum > 2147483647)
        fail("FieldMotionDistanceOverflow");
      const __int128_t offset = x * x + y * y - sum * sum;
      const __int128_t projection = x * vx + y * vy;
      if (offset < 0)
        fail("FieldMotionInitialOverlap");
      if (projection <= 0)
        continue;
      const __int128_t discriminant =
          projection * projection - length_squared * offset;
      if (discriminant < 0)
        continue;
      __int128_t root =
          field_integer_sqrt(static_cast<__uint128_t>(discriminant));
      if (root * root < discriminant)
        ++root;
      const __int128_t numerator = std::max<__int128_t>(0, projection - root);
      if (numerator > length_squared ||
          numerator * fraction_d >= fraction_n * length_squared)
        continue;
      fraction_n = numerator;
      fraction_d = length_squared;
    }
    // 输出仍为整数分米；量化可能略偏离射线，逐格退到最后一个不重叠点。
    const auto absolute = [](__int128_t value) {
      return value < 0 ? -value : value;
    };
    const __int128_t steps = std::max(absolute(vx), absolute(vy));
    __int128_t k = steps * fraction_n / fraction_d;
    while (k >= 0) {
      const PointI point{
          static_cast<Integer>(__int128_t(from.x) + vx * k / steps),
          static_cast<Integer>(__int128_t(from.y) + vy * k / steps)};
      bool free = true;
      for (const auto &[id, other] : session.formations) {
        const auto actual = motion_projection(other, prefix);
        if (id == formation.formation_id || actual.fit == 0 || actual.withdrawn)
          continue;
        const __int128_t sum =
            __int128_t(radius) + projected_radius(other, prefix);
        if (field_distance_squared(point, actual.position_dm) < sum * sum) {
          free = false;
          break;
        }
      }
      if (free)
        return {true, point, {}};
      --k;
    }
    fail("FieldMotionInitialOverlap");
  } catch (Failure &error) {
    return {false, {}, std::move(error.error)};
  }
}
Outcome<PointI> read_field_motion_contact_point(const FieldBattleSession &s,
                                                const FieldFormation &f,
                                                PointI proposed) {
  return read_contact_point(s, f, f.position_dm, proposed, nullptr);
}
// 按真实一步的原速度与有限路径移动；到障碍立即截断，不穿过途中编队。
Outcome<FieldNavigationStep> read_field_navigation_step_at_prefix(
    const FieldBattleSession &session, const GeneratedBattleField &field,
    const StableId &formation_id, PointI target_dm,
    const FieldBattleWindowSlice &one_step,
    const FieldMotionPrefixRead *prefix) {
  try {
    validate(field);
    auto found = session.formations.find(formation_id);
    if (found == session.formations.end() ||
        (!prefix && session.state != FieldBattleState::Running) ||
        one_step.steps != 1 ||
        one_step.from.tick != (prefix ? prefix->at.tick : session.now.tick) ||
        one_step.from.t != (prefix ? prefix->at.t : session.now.t) ||
        field.field_hash != session.field_hash)
      fail("FieldMotionActualStepRequired");
    const auto &formation = found->second;
    const auto actual = motion_projection(formation, prefix);
    if (actual.fit == 0 || actual.withdrawn)
      fail("FieldMotionActiveFormationRequired");
    PointI position = actual.position_dm;
    FieldNavigationStep result{position, position, false, false};
    std::optional<std::size_t> terminal_gate;
    const auto goal = cell(field, target_dm);
    if ((actual.routing || actual.withdrawing) && formation.side >= 0 &&
        formation.side < 2 &&
        field.cells[goal].terrain == FieldTerrainKind::Gate &&
        std::find(session.sides[formation.side].exits.begin(),
                  session.sides[formation.side].exits.end(),
                  goal) != session.sides[formation.side].exits.end()) {
      for (const auto &zone : field.sides)
        if (zone.settlement_exit_cell && *zone.settlement_exit_cell == goal)
          terminal_gate = goal;
    }
    const auto path = read_navigation_path_impl(
        field, formation.category, position, target_dm, terminal_gate);
    if (!path.ok)
      throw Failure{path.error};
    if (!path.value.reachable) {
      result.blocked = true;
      return {true, result, {}};
    }
    const auto &p = field.parameters;
    const auto next_time = add_field_battle_seconds(one_step.from, p.step_s, p);
    if (!next_time.ok)
      throw Failure{next_time.error};
    if (next_time.value.tick != one_step.to.tick ||
        next_time.value.t != one_step.to.t)
      fail("FieldMotionActualStepRequired");
    Integer rate = formation.category == FieldTroopCategory::Cavalry
                       ? p.cavalry_m_per_minute
                   : formation.category == FieldTroopCategory::Archer
                       ? p.archer_m_per_minute
                       : p.infantry_m_per_minute;
    if (formation.category == FieldTroopCategory::Cavalry &&
        field_distance_squared(position, target_dm) <= 1000 * 1000)
      rate = p.charge_m_per_minute;
    const auto road_bp =
        p.terrain_speed_bp[static_cast<std::size_t>(FieldTerrainKind::Road)]
                          [static_cast<std::size_t>(formation.category)];
    const auto terrain_bp =
        terminal_gate && cell(field, position) == *terminal_gate
            ? road_bp
            : speed(field, cell(field, position), formation.category);
    const Integer rout_bp = actual.routing ? p.rout_speed_bp : 10000;
    if (rate <= 0 || rout_bp <= 0 || p.step_s <= 0)
      fail("FieldMotionSpeedRequired");
    Integer trait_bp = 10000, tactic_bp = 10000;
    if (formation.ability) {
      if (formation.category == FieldTroopCategory::Cavalry &&
          std::find(formation.ability->traits.begin(),
                    formation.ability->traits.end(),
                    FieldTrait::CavalryMaster) !=
              formation.ability->traits.end())
        trait_bp = 11500;
      for (const auto &active : formation.ability->active) {
        const bool live = active.ends &&
                          std::tie(active.started.tick, active.started.t) <=
                              std::tie(one_step.from.tick, one_step.from.t) &&
                          std::tie(one_step.from.tick, one_step.from.t) <
                              std::tie(active.ends->tick, active.ends->t);
        if (live && active.kind == FieldTacticKind::Guard) {
          result.blocked = true;
          return {true, result, {}};
        }
        if (live && active.kind == FieldTacticKind::Charge)
          tactic_bp = 15000;
      }
    }
    __int128_t scaled_distance = rate;
    for (Integer factor :
         {Integer{10}, p.step_s, terrain_bp, rout_bp, trait_bp, tactic_bp})
      if (__builtin_mul_overflow(scaled_distance, __int128_t(factor),
                                 &scaled_distance))
        fail("FieldMotionDistanceOverflow");
    const __int128_t distance =
        scaled_distance / (__int128_t{60} * 10000 * 10000 * 10000 * 10000);
    if (distance > std::numeric_limits<Integer>::max())
      fail("FieldMotionDistanceOverflow");
    Integer remaining = static_cast<Integer>(distance);
    std::vector<PointI> targets;
    for (std::size_t i = 1; i + 1 < path.value.cells.size(); ++i)
      targets.push_back(field_cell_center_dm(field, path.value.cells[i]));
    targets.push_back(target_dm);
    for (const auto point : targets) {
      // Preserve all original nonterminal travel. Only the final Gate leg
      // converts the unspent movement budget to the approved Road multiplier.
      if (terminal_gate && point.x == target_dm.x && point.y == target_dm.y) {
        if (terrain_bp <= 0 || road_bp <= 0)
          fail("FieldMotionSpeedRequired");
        const __int128_t terminal_budget =
            __int128_t(remaining) * road_bp / terrain_bp;
        if (terminal_budget > std::numeric_limits<Integer>::max())
          fail("FieldMotionDistanceOverflow");
        remaining = Integer(terminal_budget);
      }
      if (!remaining)
        break;
      const auto from = position;
      const auto squared = field_distance_squared(from, point);
      Integer length = field_integer_sqrt(static_cast<__uint128_t>(squared));
      if (__int128_t(length) * length < squared)
        ++length;
      if (!length)
        continue;
      const Integer distance = std::min(remaining, length);
      const PointI proposed{
          static_cast<Integer>(__int128_t(from.x) +
                               (__int128_t(point.x) - from.x) * distance /
                                   length),
          static_cast<Integer>(__int128_t(from.y) +
                               (__int128_t(point.y) - from.y) * distance /
                                   length)};
      const auto contact =
          read_contact_point(session, formation, position, proposed, prefix);
      if (!contact.ok)
        throw Failure{contact.error};
      position = contact.value;
      result.to_dm = contact.value;
      if (contact.value.x != proposed.x || contact.value.y != proposed.y) {
        result.blocked = true;
        break;
      }
      remaining -= distance;
      if (distance < length)
        break;
    }
    result.reached = position.x == target_dm.x && position.y == target_dm.y;
    return {true, result, {}};
  } catch (Failure &error) {
    return {false, {}, std::move(error.error)};
  }
}
Outcome<FieldNavigationStep> move_field_formation_in_candidate(
    FieldBattleSession &session, const GeneratedBattleField &field,
    const StableId &formation_id, PointI target,
    const FieldBattleWindowSlice &step, CheckpointMemoryCharge &owner) {
  auto result = read_field_navigation_step_at_prefix(
      session, field, formation_id, target, step, nullptr);
  if (!result.ok)
    return result;
  auto &formation = session.formations.at(formation_id);
  formation.position_dm = result.value.to_dm;
  auto recorded = record_field_original_motion_application(
      session, formation, field, step, result.value, owner);
  if (!recorded.ok)
    return {false, {}, recorded.error};
  return result;
}

// 连续分米射线与实际格矩形求交；高度沿同一线性参数比较，不移到格中心。
Outcome<bool> read_field_line_of_sight(const GeneratedBattleField &field,
                                       PointI from_dm, PointI target_dm) {
  try {
    validate(field);
    const auto from = cell(field, from_dm), target = cell(field, target_dm);
    if (from == target)
      return {true, true, {}};
    struct Fraction {
      __int128_t n, d;
    };
    const auto less = [](Fraction a, Fraction b) {
      return a.n * b.d < b.n * a.d;
    };
    const auto &p = field.parameters;
    const Integer size = p.cell_m * 10;
    const Integer x0 = from % field.columns, y0 = from / field.columns;
    const Integer x1 = target % field.columns, y1 = target / field.columns;
    const __int128_t rise =
        __int128_t(field.cells[target].height_dm) - field.cells[from].height_dm;
    for (Integer y = std::min(y0, y1); y <= std::max(y0, y1); ++y)
      for (Integer x = std::min(x0, x1); x <= std::max(x0, x1); ++x) {
        const auto index = static_cast<std::size_t>(y * field.columns + x);
        if (index == from || index == target)
          continue;
        Fraction enter{0, 1}, leave{1, 1};
        // 闭矩形的交点也受遮挡；这个稳定内部选项不消耗随机数。
        const auto clip = [&](Integer origin, Integer end, Integer low,
                              Integer high) {
          const __int128_t delta = __int128_t(end) - origin;
          if (!delta)
            return origin >= low && origin <= high;
          Fraction near{__int128_t(low) - origin, delta};
          Fraction far{__int128_t(high) - origin, delta};
          if (delta < 0) {
            near = {-far.n, -far.d};
            far = {__int128_t(origin) - low, -delta};
          }
          if (less(enter, near))
            enter = near;
          if (less(far, leave))
            leave = far;
          return !less(leave, enter);
        };
        const Integer left = x * size - p.length_m * 5;
        const Integer bottom = y * size - p.width_m * 5;
        if (!clip(from_dm.x, target_dm.x, left, left + size) ||
            !clip(from_dm.y, target_dm.y, bottom, bottom + size))
          continue;
        const auto &value = field.cells[index];
        if (value.blocks_sight || value.terrain == FieldTerrainKind::Forest ||
            value.terrain == FieldTerrainKind::Village)
          return {true, false, {}};
        const auto lowest = rise < 0 ? leave : enter;
        const auto line_height =
            __int128_t(field.cells[from].height_dm) * lowest.d +
            rise * lowest.n;
        if (__int128_t(value.height_dm) * lowest.d > line_height)
          return {true, false, {}};
      }
    return {true, true, {}};
  } catch (Failure &error) {
    return {false, {}, std::move(error.error)};
  }
}
} // namespace hanmo::resident
