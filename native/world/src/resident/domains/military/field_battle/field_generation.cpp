// 整数道路裁剪、地形叠加、连通布阵和内容摘要；没有战斗随机数与玩法后备。
#include "field_generation.hpp"
#include "resident/model/common_model.hpp"
#include <algorithm>
#include <deque>
#include <limits>
#include <picosha2.h>
namespace hanmo::resident {
namespace {
using Wide = __int128_t;
using UWide = __uint128_t;
// 整数开方返回下界，避免不同平台浮点 sqrt 改变路段选择。
Integer root(UWide value) {
  UWide low = 0, high = std::numeric_limits<Integer>::max();
  while (low < high) {
    UWide mid = (low + high + 1) / 2;
    if (mid * mid <= value)
      low = mid;
    else
      high = mid - 1;
  }
  return static_cast<Integer>(low);
}
// 坐标只在末端整数化，距离平方保留 128 位。
Wide square_distance(PointI a, PointI b) {
  Wide x = Wide(a.x) - b.x, y = Wide(a.y) - b.y;
  return x * x + y * y;
}
Integer distance(PointI a, PointI b) {
  return root(static_cast<UWide>(square_distance(a, b)));
}
// 作者折线累计长度，零长段允许但不参与切线取样。
std::vector<Integer> lengths(const FieldRoadGeometry &road) {
  std::vector<Integer> out{0};
  for (std::size_t i = 1; i < road.points_dm.size(); ++i)
    out.push_back(out.back() +
                  distance(road.points_dm[i - 1], road.points_dm[i]));
  return out;
}
// 从原路线起点按整数路程取点与千分切线；不使用按 ID 翻转的接触 canonical 向量。
std::pair<PointI, PointI> road_point(const FieldRoadGeometry &road,
                                     const std::vector<Integer> &cumulative,
                                     Integer at) {
  at = std::clamp(at, Integer(0), cumulative.back());
  std::size_t segment = 1;
  while (segment + 1 < cumulative.size() && at >= cumulative[segment])
    ++segment;
  while (segment < cumulative.size() &&
         cumulative[segment] == cumulative[segment - 1])
    ++segment;
  if (segment == cumulative.size()) {
    segment = cumulative.size() - 1;
    while (segment > 1 && cumulative[segment] == cumulative[segment - 1])
      --segment;
  }
  const auto a = road.points_dm[segment - 1], b = road.points_dm[segment];
  const auto span = cumulative[segment] - cumulative[segment - 1];
  const Integer offset =
      std::clamp(at - cumulative[segment - 1], Integer(0), span);
  PointI point{a.x + static_cast<Integer>(Wide(b.x - a.x) * offset / span),
               a.y + static_cast<Integer>(Wide(b.y - a.y) * offset / span)};
  PointI tangent{static_cast<Integer>(Wide(b.x - a.x) * 1000 / span),
                 static_cast<Integer>(Wide(b.y - a.y) * 1000 / span)};
  return {point, tangent};
}
// 与实折线的距离采用投影平方比较，不把近路包围框当道路。
bool near_road(PointI p, const FieldRoadGeometry &road, Integer radius) {
  for (std::size_t i = 1; i < road.points_dm.size(); ++i) {
    auto a = road.points_dm[i - 1], b = road.points_dm[i];
    Wide dx = Wide(b.x) - a.x, dy = Wide(b.y) - a.y, px = Wide(p.x) - a.x,
         py = Wide(p.y) - a.y, den = dx * dx + dy * dy, dot = px * dx + py * dy;
    if (den == 0 || dot <= 0) {
      if (square_distance(p, a) <= Wide(radius) * radius)
        return true;
    } else if (dot >= den) {
      if (square_distance(p, b) <= Wide(radius) * radius)
        return true;
    } else {
      Wide cross = px * dy - py * dx;
      if (cross * cross <= Wide(radius) * radius * den)
        return true;
    }
  }
  return false;
}
// 千分向量反变换保留其实际范数，防止图外裁剪随舍入缩放。
PointI local_point(const FieldAnchor &anchor, PointI point) {
  Wide x = Wide(point.x) - anchor.point_m.x * 10,
       y = Wide(point.y) - anchor.point_m.y * 10;
  Wide norm = Wide(anchor.tangent.x) * anchor.tangent.x +
              Wide(anchor.tangent.y) * anchor.tangent.y;
  return {static_cast<Integer>((x * anchor.tangent.x + y * anchor.tangent.y) *
                               1000 / norm),
          static_cast<Integer>((-x * anchor.tangent.y + y * anchor.tangent.x) *
                               1000 / norm)};
}
bool inside(const FieldBattleParameters &p, PointI local) {
  return std::abs(local.x) <= p.length_m * 5 &&
         std::abs(local.y) <= p.width_m * 5;
}
// 道路上的第一处城门/村中心或矩形边界；沿原折线逐段找首次出界，不跳过弯曲段。
struct RoadExitGeometry {
  Integer distance = 0;
  const FieldSettlement *settlement = nullptr;
};
RoadExitGeometry road_exit(const FieldRoadGeometry &road,
                           const std::vector<Integer> &cumulative,
                           const GeneratedBattleField &field, int side) {
  const Integer direction = side == 0 ? -1 : 1;
  Integer current = field.route_distance_dm;
  std::vector<Integer> vertices;
  for (auto at : cumulative)
    if ((at - current) * direction > 0)
      vertices.push_back(at);
  if (side == 0)
    std::reverse(vertices.begin(), vertices.end());
  for (const Integer next : vertices) {
    const auto begin = road_point(road, cumulative, current).first;
    const auto end = road_point(road, cumulative, next).first;
    Integer result = next;
    const FieldSettlement *selected_settlement = nullptr;
    bool stopped = false;
    // 当前路段内矩形是凸的；二分首次出界，不按固定采样跨过短弯折。
    const auto boundary = [&](Integer target, const auto &outside) {
      Integer lo = 0, hi = std::abs(target - current);
      while (hi - lo > 1) {
        const Integer mid = (lo + hi) / 2;
        if (outside(
                road_point(road, cumulative, current + direction * mid).first))
          hi = mid;
        else
          lo = mid;
      }
      return current + direction * lo;
    };
    if (!inside(field.parameters, local_point(field.anchor, end))) {
      result = boundary(next, [&](PointI p) {
        return !inside(field.parameters, local_point(field.anchor, p));
      });
      stopped = true;
    }
    // 城占地的最接近投影先证明路段确实相交，再二分朝向锚点的门边。
    for (const auto &settlement : road.settlements) {
      Wide dx = Wide(end.x) - begin.x, dy = Wide(end.y) - begin.y;
      const Wide den = dx * dx + dy * dy;
      if (den == 0)
        continue;
      const Wide dot =
          std::clamp<Wide>((Wide(settlement.point_dm.x) - begin.x) * dx +
                               (Wide(settlement.point_dm.y) - begin.y) * dy,
                           0, den);
      const Integer closest =
          current + static_cast<Integer>(Wide(next - current) * dot / den);
      if (!settlement.city) {
        // A genuine route reaches the village centre, not merely its area
        // bounding circle. Original route endpoints are exact authored centres.
        const Wide px = Wide(settlement.point_dm.x) - begin.x;
        const Wide py = Wide(settlement.point_dm.y) - begin.y;
        const Wide along = px * dx + py * dy;
        if (along < 0 || along > den || px * dy - py * dx != 0 ||
            !inside(field.parameters,
                    local_point(field.anchor, settlement.point_dm)))
          continue;
        if (!stopped ||
            (closest - current) * direction < (result - current) * direction) {
          result = closest;
          selected_settlement = &settlement;
          stopped = true;
        }
        continue;
      }
      const auto in_city = [&](PointI p) {
        return square_distance(p, settlement.point_dm) <= Wide(8500) * 8500;
      };
      if (!in_city(road_point(road, cumulative, closest).first))
        continue;
      const Integer gate =
          in_city(begin) ? current : boundary(closest, in_city);
      if (!stopped ||
          (gate - current) * direction < (result - current) * direction) {
        result = gate;
        selected_settlement = &settlement;
        stopped = true;
      }
    }
    if (stopped)
      return {result, selected_settlement};
    current = next;
  }
  return {current, nullptr};
}
// Follow the actual directed original polyline into the selected city. A
// nearby Gate centre is not enough: the real segment must cross that cell.
// Fractions only order intersections; no world time or float is introduced.
struct GateCellOnRoad {
  std::size_t cell = 0;
  Integer distance = 0;
};
std::optional<GateCellOnRoad> first_gate_on_road(
    const FieldRoadGeometry &road, const std::vector<Integer> &cumulative,
    const GeneratedBattleField &field, const FieldSettlement &city,
    Integer from, Integer direction) {
  struct Fraction {
    Wide n = 0, d = 1;
  };
  const auto less = [](Fraction a, Fraction b) {
    return a.n * b.d < b.n * a.d;
  };
  std::vector<Integer> vertices;
  for (const auto v : cumulative)
    if ((v - from) * direction > 0)
      vertices.push_back(v);
  if (direction < 0)
    std::reverse(vertices.begin(), vertices.end());
  Integer current = from;
  const Integer size = field.parameters.cell_m * 10;
  for (const auto next : vertices) {
    const auto a =
        local_point(field.anchor, road_point(road, cumulative, current).first);
    const auto b =
        local_point(field.anchor, road_point(road, cumulative, next).first);
    std::optional<std::size_t> chosen;
    Fraction first{1, 1};
    for (std::size_t index = 0; index < field.cells.size(); ++index) {
      if (field.cells[index].terrain != FieldTerrainKind::Gate)
        continue;
      const auto center = field_cell_center_dm(field, index);
      if (square_distance(field_point_to_map_dm(field.anchor, center),
                          city.point_dm) > Wide(8500) * 8500)
        continue;
      Fraction lo{0, 1}, hi{1, 1};
      const auto clip = [&](Integer start, Integer end, Integer low,
                            Integer high) {
        Wide delta = Wide(end) - start;
        if (delta == 0)
          return start >= low && start < high;
        Fraction enter{Wide(low) - start, delta},
            leave{Wide(high) - start, delta};
        if (delta < 0) {
          enter = {Wide(start) - high, -delta};
          leave = {Wide(start) - low, -delta};
        }
        if (less(lo, enter))
          lo = enter;
        if (less(leave, hi))
          hi = leave;
        return less(lo, hi);
      };
      const Integer left = -field.parameters.length_m * 5 +
                           Integer(index % field.columns) * size;
      const Integer bottom =
          -field.parameters.width_m * 5 + Integer(index / field.columns) * size;
      if (!clip(a.x, b.x, left, left + size) ||
          !clip(a.y, b.y, bottom, bottom + size))
        continue;
      if (!chosen || less(lo, first) || (!less(first, lo) && index < *chosen)) {
        chosen = index;
        first = lo;
      }
    }
    if (chosen) {
      const Integer offset =
          Integer(Wide(std::abs(next - current)) * first.n / first.d);
      return GateCellOnRoad{*chosen, current + direction * offset};
    }
    current = next;
  }
  return {};
}
// 地形优先级严格按合同；道路过水先于山坡、树林，城市优先于道路。
FieldTerrainKind terrain_kind(const FieldTerrainSample &sample,
                              PointI map_point, const FieldRoadGeometry &road,
                              Integer slope_dm) {
  const bool on_road = near_road(map_point, road, 125);
  for (const auto &settlement : road.settlements)
    if (settlement.city &&
        square_distance(map_point, settlement.point_dm) <= Wide(8500) * 8500)
      return on_road ? FieldTerrainKind::Gate : FieldTerrainKind::Wall;
  for (const auto &settlement : road.settlements)
    if (!settlement.city &&
        square_distance(map_point, settlement.point_dm) <= Wide(3000) * 3000)
      return FieldTerrainKind::Village;
  if (on_road)
    return sample.water
               ? (sample.river_level * 2 >= 255 ? FieldTerrainKind::Bridge
                                                : FieldTerrainKind::Ford)
               : FieldTerrainKind::Road;
  if (sample.water)
    return sample.river_level * 4 < 255 ? FieldTerrainKind::Stream
                                        : FieldTerrainKind::Water;
  if (sample.river_level * 2 > 255 &&
      sample.river_distance_mm * 10 < sample.river_half_width_mm * 22)
    return FieldTerrainKind::Rough;
  if (slope_dm > 150)
    return FieldTerrainKind::Steep;
  if (slope_dm >= 75 || sample.rock > 128)
    return FieldTerrainKind::Rough;
  if (sample.forest > 128)
    return FieldTerrainKind::Forest;
  if (sample.cultivation > 128)
    return FieldTerrainKind::Field;
  return FieldTerrainKind::Plain;
}
// 所有可通行路格共同种下连通搜索，城市 Gate 绝不作为可通行种子。
std::vector<bool> road_connected(const GeneratedBattleField &field) {
  std::vector<bool> connected(field.cells.size(), false);
  std::deque<std::size_t> queue;
  for (std::size_t i = 0; i < field.cells.size(); ++i) {
    auto k = field.cells[i].terrain;
    if (k == FieldTerrainKind::Road || k == FieldTerrainKind::Bridge ||
        k == FieldTerrainKind::Ford) {
      connected[i] = true;
      queue.push_back(i);
    }
  }
  while (!queue.empty()) {
    auto at = queue.front();
    queue.pop_front();
    Integer x = at % field.columns, y = at / field.columns;
    for (Integer dy = -1; dy <= 1; ++dy)
      for (Integer dx = -1; dx <= 1; ++dx) {
        Integer nx = x + dx, ny = y + dy;
        if (nx < 0 || ny < 0 || nx >= field.columns || ny >= field.rows)
          continue;
        auto next = static_cast<std::size_t>(ny * field.columns + nx);
        if (!connected[next] &&
            field_cell_passable(field.cells[next].terrain)) {
          connected[next] = true;
          queue.push_back(next);
        }
      }
  }
  return connected;
}
// 原场界与已定最近格回退共用一个整数选择体；城市准入在后继真实外交读口判定。
std::vector<std::size_t> boundary_exit_cells(const GeneratedBattleField &field,
                                             PointI road_exit_dm,
                                             bool nearest_fallback) {
  std::vector<std::size_t> boundary;
  std::optional<std::size_t> nearest;
  Wide closest = 0;
  for (std::size_t i = 0; i < field.cells.size(); ++i) {
    const Integer x = i % field.columns, y = i / field.columns;
    if ((x != 0 && y != 0 && x + 1 != field.columns && y + 1 != field.rows) ||
        !field_cell_passable(field.cells[i].terrain))
      continue;
    boundary.push_back(i);
    const auto distance =
        square_distance(field_cell_center_dm(field, i), road_exit_dm);
    if (!nearest || distance < closest) {
      nearest = i;
      closest = distance;
    }
  }
  // 无可通行场界时不把 Water/Wall 改成出口；调用方仍可有合法城村出口。
  if (!nearest)
    return {};
  const PointI center =
      nearest_fallback ? field_cell_center_dm(field, *nearest) : road_exit_dm;
  const Wide radius = Wide(field.parameters.exit_radius_m) * 10;
  std::vector<std::size_t> result;
  for (auto i : boundary)
    if (square_distance(field_cell_center_dm(field, i), center) <=
        radius * radius)
      result.push_back(i);
  return result;
}
// 每个候选锚点一次完整生成，不混入上次候选的格或布阵缓存。
GeneratedBattleField
generate_at(const FieldTerrainData &terrain, const FieldRoadGeometry &road,
            const std::vector<Integer> &cumulative, Integer at,
            const FieldBattleParameters &parameters, const StableId &version) {
  GeneratedBattleField field;
  field.content_version = version;
  field.parameters = parameters;
  field.route_distance_dm = at;
  field.columns = parameters.length_m / parameters.cell_m;
  field.rows = parameters.width_m / parameters.cell_m;
  auto [point, tangent] = road_point(road, cumulative, at);
  field.anchor = {road.route_id, {point.x / 10, point.y / 10}, tangent};
  field.cells.resize(field.columns * field.rows);
  std::vector<FieldTerrainSample> samples;
  samples.reserve(field.cells.size());
  for (std::size_t i = 0; i < field.cells.size(); ++i)
    samples.push_back(sample_field_terrain(
        terrain,
        field_point_to_map_dm(field.anchor, field_cell_center_dm(field, i))));
  for (std::size_t i = 0; i < field.cells.size(); ++i) {
    Integer x = i % field.columns, y = i / field.columns, max_delta = 0;
    for (Integer dy = -1; dy <= 1; ++dy)
      for (Integer dx = -1; dx <= 1; ++dx) {
        Integer nx = x + dx, ny = y + dy;
        if (nx >= 0 && ny >= 0 && nx < field.columns && ny < field.rows)
          max_delta = std::max(
              max_delta, std::abs(samples[i].height_dm -
                                  samples[ny * field.columns + nx].height_dm));
      }
    // 合同坡度高差/25m；参数改格长时仍以实格长计算同一 0.3/0.6 阈值。
    const auto kind = terrain_kind(
        samples[i],
        field_point_to_map_dm(field.anchor, field_cell_center_dm(field, i)),
        road, max_delta * 25 / parameters.cell_m);
    field.cells[i] = {samples[i].height_dm, kind,
                      kind == FieldTerrainKind::Forest,
                      parameters.terrain_cover[static_cast<std::size_t>(kind)]};
  }
  const auto connected = road_connected(field);
  for (int side = 0; side < 2; ++side) {
    auto &zone = field.sides[side];
    const auto exit_source = road_exit(road, cumulative, field, side);
    Integer exit = exit_source.distance;
    std::optional<GateCellOnRoad> gate;
    if (exit_source.settlement) {
      zone.road_exit_settlement_id = exit_source.settlement->id;
      if (exit_source.settlement->city) {
        gate =
            first_gate_on_road(road, cumulative, field, *exit_source.settlement,
                               exit, side == 0 ? -1 : 1);
        if (gate)
          exit = gate->distance;
      }
    }
    auto center =
        road_point(road, cumulative,
                   std::clamp(exit + (side == 0 ? 1 : -1) *
                                         parameters.deployment_inset_m * 10,
                              Integer(0), cumulative.back()));
    zone.road_exit_dm =
        gate ? field_cell_center_dm(field, gate->cell)
             : local_point(field.anchor,
                           road_point(road, cumulative, exit).first);
    if (gate)
      zone.settlement_exit_cell = gate->cell;
    else if (exit_source.settlement && !exit_source.settlement->city) {
      const auto e = zone.road_exit_dm;
      if (e.x >= -parameters.length_m * 5 && e.x < parameters.length_m * 5 &&
          e.y >= -parameters.width_m * 5 && e.y < parameters.width_m * 5) {
        const auto cell = std::size_t(
            (e.y + parameters.width_m * 5) / (parameters.cell_m * 10) *
                field.columns +
            (e.x + parameters.length_m * 5) / (parameters.cell_m * 10));
        if (cell < field.cells.size() &&
            field.cells[cell].terrain == FieldTerrainKind::Village)
          zone.settlement_exit_cell = cell;
      }
    }
    zone.deployment_center_dm = local_point(field.anchor, center.first);
    const auto along =
        local_point(field.anchor, {center.first.x + center.second.x,
                                   center.first.y + center.second.y});
    zone.deployment_axis_dm = {along.x - zone.deployment_center_dm.x,
                               along.y - zone.deployment_center_dm.y};
    for (std::size_t i = 0; i < field.cells.size(); ++i) {
      auto local = field_cell_center_dm(field, i);
      auto map = field_point_to_map_dm(field.anchor, local);
      Wide dx = Wide(map.x) - center.first.x, dy = Wide(map.y) - center.first.y;
      Wide along = dx * center.second.x + dy * center.second.y,
           across = -dx * center.second.y + dy * center.second.x;
      if (along >= -Wide(parameters.deployment_length_m) * 5000 &&
          along <= Wide(parameters.deployment_length_m) * 5000 &&
          across >= -Wide(parameters.deployment_width_m) * 5000 &&
          across <= Wide(parameters.deployment_width_m) * 5000) {
        ++zone.rectangle_cells;
        if (connected[i])
          zone.deploy_cells.push_back(i);
      }
    }
    zone.exit_cells = boundary_exit_cells(field, zone.road_exit_dm, false);
  }
  return field;
}
// 摘要写入固定宽度小端整数和显式字符串长度，不依赖对象填充或库的 hash。
void number(std::vector<std::uint8_t> &bytes, Integer value) {
  auto v = static_cast<std::uint64_t>(value);
  for (int i = 0; i < 8; ++i)
    bytes.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
}
void text(std::vector<std::uint8_t> &bytes, const StableId &value) {
  number(bytes, value.size());
  bytes.insert(bytes.end(), value.begin(), value.end());
}
void hash_field(GeneratedBattleField &field, const FieldTerrainData &terrain) {
  std::vector<std::uint8_t> bytes;
  text(bytes, "field_battle/1/field/original-deployment-axis4");
  text(bytes, field.content_version);
  text(bytes, terrain.metadata.height_sha256);
  text(bytes, terrain.metadata.rivers_sha256);
  text(bytes, terrain.metadata.land_sha256);
  text(bytes, field.anchor.route_id);
  text(bytes, field.parameters.version);
  for (auto n :
       {field.anchor.point_m.x, field.anchor.point_m.y, field.anchor.tangent.x,
        field.anchor.tangent.y, field.route_distance_dm, field.columns,
        field.rows, field.parameters.cell_m,
        field.parameters.courier_m_per_minute})
    number(bytes, n);
  for (const auto &cell : field.cells) {
    number(bytes, cell.height_dm);
    number(bytes, static_cast<Integer>(cell.terrain));
    number(bytes, cell.blocks_sight);
    number(bytes, cell.cover);
  }
  for (const auto &side : field.sides) {
    number(bytes, side.deployment_axis_dm.x);
    number(bytes, side.deployment_axis_dm.y);
    number(bytes, bool(side.road_exit_settlement_id));
    if (side.road_exit_settlement_id)
      text(bytes, *side.road_exit_settlement_id);
    number(bytes, bool(side.settlement_exit_cell));
    if (side.settlement_exit_cell)
      number(bytes, Integer(*side.settlement_exit_cell));
    number(bytes, side.road_exit_dm.x);
    number(bytes, side.road_exit_dm.y);
    number(bytes, side.deployment_center_dm.x);
    number(bytes, side.deployment_center_dm.y);
    number(bytes, side.rectangle_cells);
    number(bytes, side.deploy_cells.size());
    for (auto c : side.deploy_cells)
      number(bytes, c);
    number(bytes, side.exit_cells.size());
    for (auto c : side.exit_cells)
      number(bytes, c);
  }
  field.field_hash = picosha2::hash256_hex_string(bytes);
}
// 比较两侧中较低覆盖率；分母为裁剪后矩形的实格数，不忽略不可通行格。
std::pair<Integer, Integer> quality(const GeneratedBattleField &field) {
  std::pair<Integer, Integer> result{std::numeric_limits<Integer>::max(), 1};
  for (const auto &side : field.sides) {
    std::pair<Integer, Integer> current{
        static_cast<Integer>(side.deploy_cells.size()),
        std::max<Integer>(1, side.rectangle_cells)};
    if (Wide(current.first) * result.second <
        Wide(result.first) * current.second)
      result = current;
  }
  return result;
}
} // namespace
// 作者参数先作有限整值与维度校验；不以截断、钳位或默认值修复坏输入。
Status validate_field_battle_parameters(const FieldBattleParameters &p) {
  const auto invalid = [](const char *name) {
    return common_failure("InvalidFieldBattleConfiguration",
                          StableId("field_battle/") + name);
  };
  if (p.version != "field_battle/1")
    return invalid("version");
  for (auto entry :
       {std::pair{"window_s", p.window_s},
        std::pair{"step_s", p.step_s},
        std::pair{"length_m", p.length_m},
        std::pair{"width_m", p.width_m},
        std::pair{"cell_m", p.cell_m},
        std::pair{"deployment_length_m", p.deployment_length_m},
        std::pair{"deployment_width_m", p.deployment_width_m},
        std::pair{"deployment_inset_m", p.deployment_inset_m},
        std::pair{"deployment_min_percent", p.deployment_min_percent},
        std::pair{"exit_radius_m", p.exit_radius_m},
        std::pair{"anchor_shift_step_m", p.anchor_shift_step_m},
        std::pair{"courier_m_per_minute", p.courier_m_per_minute},
        std::pair{"minimum_order_delay_s", p.minimum_order_delay_s},
        std::pair{"pulse_s", p.pulse_s},
        std::pair{"round_equiv_s", p.round_equiv_s},
        std::pair{"max_segments", p.max_segments},
        std::pair{"infantry_m_per_minute", p.infantry_m_per_minute},
        std::pair{"archer_m_per_minute", p.archer_m_per_minute},
        std::pair{"cavalry_m_per_minute", p.cavalry_m_per_minute},
        std::pair{"charge_m_per_minute", p.charge_m_per_minute},
        std::pair{"rout_speed_bp", p.rout_speed_bp},
        std::pair{"melee_contact_m", p.melee_contact_m},
        std::pair{"archer_max_range_m", p.archer_max_range_m},
        std::pair{"ai_eval_s", p.ai_eval_s},
        std::pair{"ai_advance_hold_s", p.ai_advance_hold_s},
        std::pair{"rejoin_distance_m", p.rejoin_distance_m},
        std::pair{"rejoin_stopped_s", p.rejoin_stopped_s},
        std::pair{"max_formations_per_side", p.max_formations_per_side}})
    if (entry.second <= 0)
      return invalid(entry.first);
  for (auto entry :
       {std::pair{"anchor_shift_limit_m", p.anchor_shift_limit_m},
        std::pair{"morale_loss_bp", p.morale_loss_bp},
        std::pair{"commander_exposure_bp", p.commander_exposure_bp},
        std::pair{"archer_min_range_m", p.archer_min_range_m},
        std::pair{"ai_retreat_fit_percent", p.ai_retreat_fit_percent},
        std::pair{"ai_retreat_morale", p.ai_retreat_morale},
        std::pair{"ai_support_morale", p.ai_support_morale}})
    if (entry.second < 0)
      return invalid(entry.first);
  if (p.window_s % p.step_s || p.pulse_s % p.step_s)
    return invalid("step_s");
  if (p.length_m % p.cell_m || p.width_m % p.cell_m ||
      p.length_m > std::numeric_limits<Integer>::max() / 10 ||
      p.width_m > std::numeric_limits<Integer>::max() / 10)
    return invalid("field_dimensions");
  if (p.deployment_min_percent > 100 || p.ai_retreat_fit_percent > 100 ||
      p.ai_retreat_morale > 100 || p.ai_support_morale > 100 ||
      p.archer_min_range_m > p.archer_max_range_m)
    return invalid("thresholds");
  for (std::size_t i = 0; i < p.terrain_speed_bp.size(); ++i) {
    if (p.terrain_cover[i] < 0 || p.terrain_cover[i] > 100)
      return invalid("terrain_cover");
    const bool passable = field_cell_passable(static_cast<FieldTerrainKind>(i));
    for (const auto speed : p.terrain_speed_bp[i])
      if ((passable && speed <= 0) || (!passable && speed != 0))
        return invalid("terrain_speed_bp");
  }
  return common_success();
}
Outcome<std::vector<std::size_t>>
read_field_boundary_exit_cells(const GeneratedBattleField &field,
                               PointI road_exit_dm, bool nearest_fallback) {
  if (field.columns <= 0 || field.rows <= 0 ||
      field.cells.size() != static_cast<std::size_t>(field.columns) *
                                static_cast<std::size_t>(field.rows))
    return {
        false,
        {},
        common_failure("InvalidFieldBattleConfiguration", "exit_cells").error};
  return {true, boundary_exit_cells(field, road_exit_dm, nearest_fallback), {}};
}

Integer field_road_length_dm(const FieldRoadGeometry &road) {
  return lengths(road).back();
}
PointI field_cell_center_dm(const GeneratedBattleField &field,
                            std::size_t index) {
  return {-field.parameters.length_m * 5 +
              static_cast<Integer>(index % field.columns) *
                  field.parameters.cell_m * 10 +
              field.parameters.cell_m * 5,
          -field.parameters.width_m * 5 +
              static_cast<Integer>(index / field.columns) *
                  field.parameters.cell_m * 10 +
              field.parameters.cell_m * 5};
}
PointI field_point_to_map_dm(const FieldAnchor &anchor, PointI p) {
  return {anchor.point_m.x * 10 +
              static_cast<Integer>((Wide(p.x) * anchor.tangent.x -
                                    Wide(p.y) * anchor.tangent.y) /
                                   1000),
          anchor.point_m.y * 10 +
              static_cast<Integer>((Wide(p.x) * anchor.tangent.y +
                                    Wide(p.y) * anchor.tangent.x) /
                                   1000)};
}
bool field_cell_passable(FieldTerrainKind kind) {
  return kind != FieldTerrainKind::Steep && kind != FieldTerrainKind::Water &&
         kind != FieldTerrainKind::Wall && kind != FieldTerrainKind::Gate;
}
Outcome<GeneratedBattleField>
generate_battle_field(const FieldTerrainData &terrain,
                      const FieldRoadGeometry &road, Integer at,
                      const FieldBattleParameters &p, const StableId &version) {
  auto parameters_valid = validate_field_battle_parameters(p);
  if (!parameters_valid.ok)
    return {false, {}, parameters_valid.error};
  if (terrain.height.empty() || terrain.rivers.empty() || terrain.land.empty())
    return {false, {}, {{"FieldTerrainMissing", "terrain"}}};
  if (road.points_dm.size() < 2 || road.route_id.empty() || p.length_m <= 0 ||
      p.width_m <= 0 || p.cell_m <= 0 || p.length_m % p.cell_m ||
      p.width_m % p.cell_m || p.anchor_shift_step_m <= 0 ||
      p.anchor_shift_limit_m < 0 || p.deployment_length_m <= 0 ||
      p.deployment_width_m <= 0 || p.deployment_min_percent < 0 ||
      p.deployment_min_percent > 100)
    return {false, {}, {{"InvalidFieldBattleConfiguration", "field_battle"}}};
  auto cumulative = lengths(road);
  if (cumulative.back() <= 0 || at < 0 || at > cumulative.back())
    return {false, {}, {{"InvalidFieldBattleRoute", "route"}}};
  auto best = generate_at(terrain, road, cumulative, at, p, version);
  auto score = quality(best);
  if (score.first * 100 < score.second * p.deployment_min_percent) {
    for (Integer shift = p.anchor_shift_step_m; shift <= p.anchor_shift_limit_m;
         shift += p.anchor_shift_step_m) {
      bool done = false;
      for (Integer sign : {Integer(1), Integer(-1)}) {
        Integer candidate = at + sign * shift * 10;
        if (candidate < 0 || candidate > cumulative.back())
          continue;
        auto field =
            generate_at(terrain, road, cumulative, candidate, p, version);
        auto next = quality(field);
        if (Wide(next.first) * score.second > Wide(score.first) * next.second) {
          best = std::move(field);
          score = next;
        }
        if (score.first * 100 >= score.second * p.deployment_min_percent) {
          done = true;
          break;
        }
      }
      if (done)
        break;
    }
  }
  hash_field(best, terrain);
  return {true, std::move(best), {}};
}
} // namespace hanmo::resident
