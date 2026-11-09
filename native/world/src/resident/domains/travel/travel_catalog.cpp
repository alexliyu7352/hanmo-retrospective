// 原 TravelCatalog 装载独立于 seed/checkpoint 写者；只移动原函数正文，不改变
// schema 或结果。
#include "cargo_handling.hpp"
#include "mobile_inventory_adoption.hpp"
#include "travel_decode_detail.hpp"
#include <algorithm>
namespace hanmo::resident {
using namespace travel_decode_detail;
// 一次准备地图/据点/作者计划，完整地图几何验证不进入逐tick热路由。
Outcome<TravelCatalog> decode_travel_catalog(const BoundaryValue &data,
                                             const StableId &scenario_id) {
  try {
    TravelCatalog result;
    if (const auto *containers = data.find("mobile_inventory_definitions")) {
      auto decoded = decode_mobile_inventory_definitions(*containers);
      if (!decoded.ok)
        return {false, {}, decoded.error};
      result.mobile_inventory_definitions = std::move(decoded.value);
    }
    if (const auto *rules = data.find("cargo_handling_rules")) {
      auto decoded = decode_cargo_handling_rules(*rules);
      if (!decoded.ok)
        return {false, {}, decoded.error};
      result.cargo_handling_rules = std::move(decoded.value);
    }
    const auto &scenario =
        field(field(data, "scenarios", "/data"), scenario_id, "/scenarios");
    if (auto *id = scenario.find("world_map_id"))
      result.world_map_id = typed<StableId>(*id, "/world_map_id");
    if (auto *capabilities = data.find("required_capabilities"))
      for (const auto &x : strings(*capabilities, "/required_capabilities"))
        if (x == "road_encounters")
          result.road_encounters = true;
    if (const auto *maps = data.find("world_maps"))
      for (const auto &row : typed<Dict>(*maps, "/world_maps")) {
        const auto &v = row.second;
        StableId path = "/world_maps/" + row.first;
        terrain::TerrainMap map;
        map.id = row.first;
        map.width = integer(v, "width", path);
        map.height = integer(v, "height", path);
        map.cell_size = integer(v, "cell_size", path);
        map.money_per_tick = integer(v, "money_per_tick", path);
        map.fatigue_per_tick = integer(v, "fatigue_per_tick", path);
        for (const auto &x :
             typed<Array>(field(v, "cells", path), path + "/cells")) {
          terrain::Cell cell;
          cell.terrain = text(x, "terrain", path);
          if (auto *crossing = x.find("crossing"))
            cell.crossing = typed<StableId>(*crossing, path);
          if (auto *flag = x.find("blocked_flag"))
            cell.blocked_flag = typed<StableId>(*flag, path);
          map.cells.push_back(std::move(cell));
        }
        if (auto *costs = v.find("costs"))
          for (const auto &m : typed<Dict>(*costs, path + "/costs"))
            for (const auto &rate :
                 typed<Dict>(m.second, path + "/costs/" + m.first))
              map.costs[mode(m.first, path)][rate.first] =
                  typed<Integer>(rate.second, path);
        for (const auto &budget :
             typed<Dict>(field(v, "budgets", path), path + "/budgets"))
          map.budgets[mode(budget.first, path)] =
              typed<Integer>(budget.second, path);
        if (map.budgets.size() != 3 || map.money_per_tick < 0 ||
            map.money_per_tick > 100000 || map.fatigue_per_tick < 0 ||
            map.fatigue_per_tick > 100)
          bad("InvalidTravelCosts", path);
        for (const auto &budget : map.budgets)
          if (budget.second < 1 || budget.second > 1000000)
            bad("InvalidTravelCosts", path + "/budgets");
        auto valid = terrain::validate_map(map);
        if (!valid.ok)
          return {false, {}, valid.error};
        result.maps.insert(row.first, std::move(map));
      }
    // 原单图后备仅限当前场景明确启用的作者记录，不能给其它场景记录猜图。
    std::vector<StableId> enabled_settlements;
    if (const auto *enabled = scenario.find("enabled"))
      if (const auto *ids = enabled->find("settlements"))
        enabled_settlements = strings(*ids, "/scenarios/enabled/settlements");
    for (const auto &row :
         typed<Dict>(field(data, "settlements", "/data"), "/settlements")) {
      TravelSettlement s;
      s.id = row.first;
      s.entry_place = text(row.second, "entry_place", "/settlements");
      s.map_xy = point(field(row.second, "map_xy", "/settlements"),
                       "/settlements/map_xy");
      if (const auto *map = row.second.find("world_map_id")) {
        s.world_map_id = typed<StableId>(*map, "/settlements/" + row.first +
                                                   "/world_map_id");
        if (s.world_map_id.empty())
          bad("TravelSettlementMapRequired", row.first);
      } else if (!result.world_map_id.empty() &&
                 std::find(enabled_settlements.begin(),
                           enabled_settlements.end(),
                           row.first) != enabled_settlements.end())
        s.world_map_id = result.world_map_id;
      if (!s.world_map_id.empty() && !result.maps.find(s.world_map_id))
        bad("UnknownWorldMap", s.world_map_id);
      // 既有单图旅行仍以场景图投影已启用城镇；显式异图必须在准入拒绝。
      if (!result.world_map_id.empty() &&
          std::find(enabled_settlements.begin(), enabled_settlements.end(),
                    row.first) != enabled_settlements.end() &&
          s.world_map_id != result.world_map_id)
        bad("TravelSettlementMapMismatch", row.first);
      result.settlements.insert(row.first, std::move(s));
    }
    for (const auto &row :
         typed<Dict>(field(data, "routes", "/data"), "/routes")) {
      const auto &v = row.second;
      StableId path = "/routes/" + row.first;
      TravelRoad road;
      road.id = row.first;
      road.origin = text(v, "origin", path);
      road.destination = text(v, "destination", path);
      road.duration_ticks = integer(v, "duration_ticks", path);
      if (v.find("cost_per_tick"))
        road.cost_per_tick = integer(v, "cost_per_tick", path);
      if (v.find("fatigue_per_tick"))
        road.fatigue_per_tick = integer(v, "fatigue_per_tick", path);
      if (v.find("blocked_from_tick"))
        road.blocked_from_tick = integer(v, "blocked_from_tick", path);
      if (v.find("blocked_until_tick"))
        road.blocked_until_tick = integer(v, "blocked_until_tick", path);
      if (const auto *shape = v.find("shape"))
        road.shape = points(*shape, path + "/shape");
      // 准备期守住作者schema数界，避免热推进及会面投影中的进度乘法溢出。
      if (!result.settlements.find(road.origin) ||
          !result.settlements.find(road.destination) ||
          road.duration_ticks < 1 || road.duration_ticks > 720 ||
          road.cost_per_tick < 0 || road.cost_per_tick > 100000 ||
          road.fatigue_per_tick < 0 || road.fatigue_per_tick > 100 ||
          road.blocked_from_tick < 0 || road.blocked_from_tick > 2147483647 ||
          road.blocked_until_tick < 0 || road.blocked_until_tick > 2147483647 ||
          road.shape.size() > 64)
        bad("InvalidTravelRoute", path);
      for (const auto &p : road.shape)
        if (p.x < -100000 || p.x > 100000 || p.y < -100000 || p.y > 100000)
          bad("InvalidTravelRoute", path + "/shape");
      if (const auto *map = v.find("world_map_id")) {
        road.world_map_id = typed<StableId>(*map, path + "/world_map_id");
        if (road.world_map_id.empty())
          bad("TravelRouteMapRequired", path);
        if (!result.maps.find(road.world_map_id))
          bad("UnknownWorldMap", road.world_map_id);
      }
      const auto &origin = *result.settlements.find(road.origin);
      const auto &destination = *result.settlements.find(road.destination);
      if (!origin.world_map_id.empty() && !destination.world_map_id.empty() &&
          origin.world_map_id != destination.world_map_id)
        bad("TravelRouteMapMismatch", path);
      if (!road.world_map_id.empty() &&
          (origin.world_map_id.empty() || destination.world_map_id.empty()))
        bad("TravelRouteMapRequired", path);
      if (!road.world_map_id.empty() &&
          (road.world_map_id != origin.world_map_id ||
           road.world_map_id != destination.world_map_id))
        bad("TravelRouteMapMismatch", path);
      result.roads.insert(row.first, std::move(road));
    }
    for (const auto &row :
         typed<Dict>(field(data, "places", "/data"), "/places")) {
      TravelPlace p;
      p.settlement_id = text(row.second, "settlement_id", "/places");
      p.passages =
          strings(field(row.second, "passages", "/places"), "/places/passages");
      result.places.insert(row.first, std::move(p));
    }
    for (const auto &row :
         typed<Dict>(field(data, "actors", "/data"), "/actors")) {
      const auto *v = row.second.find("travel_plan");
      if (!v)
        continue;
      TravelScheduleDefinition p;
      p.destinations = strings(field(*v, "destinations", "/travel_plan"),
                               "/travel_plan/destinations");
      p.dwell_ticks = integer(*v, "dwell_ticks", "/travel_plan");
      p.start_tick = integer(*v, "start_tick", "/travel_plan");
      result.authored_plans.insert(row.first, std::move(p));
    }
    return {true, std::move(result), {}};
  } catch (const DecodeFailure &f) {
    return {false, {}, f.error};
  } catch (...) {
    return {false,
            {},
            {{"NativeTravelDecodeFailure", "/catalog"},
             ErrorKind::internal,
             {},
             "travel",
             {},
             {}}};
  }
}

} // namespace hanmo::resident
