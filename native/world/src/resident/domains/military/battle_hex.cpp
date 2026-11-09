// 原六角几何与有限Dijkstra；不补场外格、不忽略控制区，同代价按原字符串键仲裁。
#include "battle_model.hpp"
#include "military_field_terrain.hpp"
#include "resident/model/common_model.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <set>
namespace hanmo::resident::military_hex {
namespace {
constexpr std::array<HexPoint, 6> directions{
    {{1, 0}, {0, 1}, {-1, 1}, {-1, 0}, {0, -1}, {1, -1}}};
bool point(HexPoint p) {
  return p.q >= -1024 && p.q <= 1024 && p.r >= -1024 && p.r <= 1024;
}
Status fail(const StableId &code, const StableId &path = {}) {
  return common_failure(code, path);
}
// 最近立方格全部参加射界检查，双向查询不靠浮点微扰选择边界一侧。
std::vector<HexPoint> line_cells(HexPoint a, HexPoint b, Integer index,
                                 Integer steps) {
  auto q = a.q * steps + (b.q - a.q) * index,
       r = a.r * steps + (b.r - a.r) * index;
  auto fq = static_cast<Integer>(std::floor(double(q) / steps)),
       fr = static_cast<Integer>(std::floor(double(r) / steps));
  Integer best = std::numeric_limits<Integer>::max();
  std::vector<HexPoint> result;
  for (Integer cq : std::array<Integer, 2>{fq, fq + 1})
    for (Integer cr : std::array<Integer, 2>{fr, fr + 1}) {
      auto eq = cq * steps - q, er = cr * steps - r,
           e = eq * eq + er * er + (eq + er) * (eq + er);
      if (e < best) {
        best = e;
        result.clear();
      }
      if (e == best)
        result.push_back({cq, cr});
    }
  return result;
}
} // namespace
StableId key(HexPoint p) {
  return std::to_string(p.q) + "," + std::to_string(p.r);
}
Integer distance(HexPoint a, HexPoint b) {
  auto q = b.q - a.q, r = b.r - a.r;
  return std::max(std::abs(q), std::max(std::abs(r), std::abs(q + r)));
}
Integer direction(HexPoint a, HexPoint b) {
  Integer best = 0, score = 2147483647;
  for (Integer i = 0; i < 6; ++i) {
    auto q = a.q + directions[i].q - b.q, r = a.r + directions[i].r - b.r,
         candidate = q * q + r * r + (q + r) * (q + r);
    if (candidate < score) {
      best = i;
      score = candidate;
    }
  }
  return best;
}
std::vector<HexPoint> neighbors(HexPoint p) {
  std::vector<HexPoint> result;
  for (auto d : directions)
    result.push_back({p.q + d.q, p.r + d.r});
  return result;
}
const HexCell *cell(const BattlefieldDefinition &b, HexPoint p) {
  for (auto &c : b.cells)
    if (c.q == p.q && c.r == p.r)
      return &c;
  return nullptr;
}
Integer height(const BattlefieldDefinition &b, HexPoint p) {
  auto c = cell(b, p);
  return c ? c->height.value_or(c->terrain == "hill" ? 1 : 0) : 0;
}
Integer cover(const BattlefieldDefinition &b, HexPoint p) {
  auto c = cell(b, p);
  return c ? c->cover.value_or(c->terrain == "forest" ? 20
                               : c->terrain == "hill" ? 10
                               : c->terrain == "wall" ? 45
                                                      : 0)
           : 0;
}
namespace {
// 两种既有 allocator 共用原成本算法，不复制规则或创建中间 map。
template <class Allocator>
Integer movement_cost_impl(
    const BattlefieldDefinition &b, HexPoint p,
    const std::map<StableId, Integer, std::less<StableId>, Allocator> &costs) {
  auto c = cell(b, p);
  if (!c || c->blocks_movement.value_or(c->terrain == "wall"))
    return -1;
  auto i = costs.find(c->terrain);
  return i != costs.end() && i->second >= 1 && i->second <= 24 ? i->second : -1;
}
} // namespace
Integer movement_cost(const BattlefieldDefinition &b, HexPoint p,
                      const std::map<StableId, Integer> &costs) {
  return movement_cost_impl(b, p, costs);
}
Integer movement_cost(const BattlefieldDefinition &b, HexPoint p,
                      const TroopDefinition &t) {
  if (!b.field_terrain)
    return movement_cost_impl(b, p, t.movement_costs);
  auto c = cell(b, p);
  if (!c || c->blocks_movement.value_or(c->terrain == "wall"))
    return -1;
  auto cost =
      military_field_terrain_cost(*b.field_terrain, t.category, c->terrain);
  return cost.ok ? cost.value : -1;
}
bool line_of_sight(const BattlefieldDefinition &b, HexPoint a, HexPoint z) {
  if (!point(a) || !point(z) || !cell(b, a) || !cell(b, z))
    return false;
  auto steps = distance(a, z), sight = std::max(height(b, a), height(b, z));
  for (Integer i = 1; i < steps; ++i)
    for (auto p : line_cells(a, z, i, steps)) {
      auto c = cell(b, p);
      if (!c ||
          c->blocks_sight.value_or(c->terrain == "forest" ||
                                   c->terrain == "wall") ||
          height(b, p) > sight)
        return false;
    }
  return true;
}
Status validate(const BattlefieldDefinition &b) {
  if (b.field_terrain) {
    auto valid = validate_military_field_terrain_rule(*b.field_terrain);
    if (!valid.ok)
      return valid;
  }
  if (b.cells.empty() || b.cells.size() > 1024)
    return fail("InvalidHexCells", "cells");
  std::set<StableId> seen;
  for (std::size_t i = 0; i < b.cells.size(); ++i) {
    auto &c = b.cells[i];
    auto path = "cells/" + std::to_string(i);
    if (!point({c.q, c.r}) ||
        (c.terrain != "plain" && c.terrain != "forest" && c.terrain != "hill" &&
         c.terrain != "water" && c.terrain != "wall" &&
         !(b.field_terrain && (c.terrain == "road" || c.terrain == "mud"))))
      return fail("InvalidHexCell", path);
    if (!seen.insert(key({c.q, c.r})).second)
      return fail("DuplicateHexCell", path);
    if ((c.height && (*c.height < 0 || *c.height > 100)) ||
        (c.cover && (*c.cover < 0 || *c.cover > 100)))
      return fail("InvalidHexTerrainValue", path);
  }
  for (auto name : {"spawns", "exits"}) {
    auto &rows = StableId(name) == "spawns" ? b.spawns : b.exits;
    std::set<StableId> positions;
    std::set<Integer> sides;
    for (std::size_t i = 0; i < rows.size(); ++i) {
      auto &e = rows[i];
      auto path = StableId(name) + "/" + std::to_string(i);
      if (e.side < 0 || e.side > 1 || !point(e.position) ||
          !seen.count(key(e.position)))
        return fail("InvalidHexEntry", path);
      if (StableId(name) == "spawns" && positions.count(key(e.position)))
        return fail("DuplicateHexSpawn", path);
      auto c = cell(b, e.position);
      if (c->blocks_movement.value_or(c->terrain == "wall"))
        return fail("BlockedHexEntry", path);
      positions.insert(key(e.position));
      sides.insert(e.side);
    }
    if (StableId(name) == "spawns" && sides.size() != 2)
      return fail("MissingHexSpawnSide", name);
  }
  return common_success();
}
namespace {
// 保留原路径选择、上限和遍历顺序，仅让同一算法接受已跟踪的成本容器。
template <class Allocator>
Outcome<std::vector<HexPath>> reachable_impl(
    const BattlefieldDefinition &b, HexPoint from,
    const std::map<StableId, Integer, std::less<StableId>, Allocator> &costs,
    Integer budget, const std::vector<HexPoint> &occupied,
    const std::vector<HexPoint> &stops) {
  auto error = [](Status s) {
    return Outcome<std::vector<HexPath>>{false, {}, s.error};
  };
  if (!point(from) || movement_cost_impl(b, from, costs) < 0 || budget < 0 ||
      budget > 24)
    return error(fail("InvalidHexMovement"));
  std::set<StableId> blocked, stop;
  for (auto p : occupied) {
    if (!point(p))
      return error(fail("InvalidHexOccupation"));
    if (p != from)
      blocked.insert(key(p));
  }
  for (auto p : stops) {
    if (!point(p))
      return error(fail("InvalidHexControlZone"));
    stop.insert(key(p));
  }
  auto start = key(from);
  std::map<StableId, HexPath> paths{{start, {from, {from}, 0}}};
  std::map<StableId, Integer> pending{{start, 0}};
  std::set<StableId> finished;
  while (!pending.empty()) {
    auto best = pending.begin();
    for (auto i = pending.begin(); i != pending.end(); ++i)
      if (i->second < best->second)
        best = i;
    auto current = best->first;
    pending.erase(best);
    finished.insert(current);
    auto entry = paths.at(current);
    if (current != start && stop.count(current))
      continue;
    for (auto p : neighbors(entry.position)) {
      auto k = key(p);
      if (blocked.count(k) || finished.count(k))
        continue;
      auto cost = movement_cost_impl(b, p, costs);
      if (cost < 0 || entry.cost + cost > budget)
        continue;
      auto previous = paths.find(k);
      if (previous != paths.end() && previous->second.cost <= entry.cost + cost)
        continue;
      auto path = entry.path;
      path.push_back(p);
      paths[k] = {p, std::move(path), entry.cost + cost};
      pending[k] = entry.cost + cost;
    }
  }
  std::vector<HexPath> out;
  for (auto &p : paths)
    out.push_back(std::move(p.second));
  return {true, std::move(out), {}};
}
} // namespace
Outcome<std::vector<HexPath>>
reachable(const BattlefieldDefinition &b, HexPoint from,
          const std::map<StableId, Integer> &costs, Integer budget,
          const std::vector<HexPoint> &occupied,
          const std::vector<HexPoint> &stops) {
  return reachable_impl(b, from, costs, budget, occupied, stops);
}
Outcome<std::vector<HexPath>> reachable(const BattlefieldDefinition &b,
                                        HexPoint from, const TroopDefinition &t,
                                        Integer budget,
                                        const std::vector<HexPoint> &occupied,
                                        const std::vector<HexPoint> &stops) {
  if (!b.field_terrain)
    return reachable_impl(b, from, t.movement_costs, budget, occupied, stops);
  auto valid = validate_military_field_terrain_troop(*b.field_terrain, t);
  if (!valid.ok)
    return {false, {}, valid.error};
  return reachable_impl(b, from, t.movement_costs, budget, occupied, stops);
}

} // namespace hanmo::resident::military_hex
