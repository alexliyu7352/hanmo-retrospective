#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/checkpoint/projection_writer.hpp"
#include "travel_decode_detail.hpp"
#include <array>
// 行旅seed/检查点唯一字段codec；BoundaryValue仅存在初始化与诊断边界，热状态始终typed。
#include "cargo_handling.hpp"
#include "mobile_inventory_adoption.hpp"
#include "travel.hpp"
#include <stdexcept>
#include <unordered_set>
namespace hanmo::resident {
namespace {
using namespace travel_decode_detail;
// 仅存明确schema键的插入顺序；类型和值仍只在各自typed字段内。
std::vector<StableId> field_order(const Value &v) {
  std::vector<StableId> result;
  for (const auto &row : *v.as<Dict>())
    result.push_back(row.first);
  return result;
}
// 旧恢复可能改变Dictionary键序，导出遵循该输入本身，不冒称与fresh顺序一致。
Value reordered(Value value, const std::vector<StableId> &order) {
  if (order.empty())
    return value;
  auto &source = *value.as<Dict>();
  if (order.size() != source.size())
    bad("InvalidTravelFieldOrder", "/field_order");
  Dict output;
  std::unordered_set<StableId> seen;
  for (const auto &key : order) {
    if (!seen.insert(key).second)
      bad("InvalidTravelFieldOrder", "/field_order");
    auto i = std::find_if(source.begin(), source.end(),
                          [&](const auto &row) { return row.first == key; });
    if (i == source.end())
      bad("InvalidTravelFieldOrder", "/field_order");
    output.push_back(*i);
  }
  return Value{std::move(output)};
}
// 地形载荷按照原begin生成的完整字段读取；道路载荷由独立typed分支读取。
TerrainTravelParams params(const Value &v, const StableId &path) {
  fields(v,
         {"terrain", "plan_id", "map_id", "route_index", "points",
          "destination", "origin", "mode", "budget", "travel_credit",
          "payer_wallet_id", "money_per_tick", "fatigue_per_tick", "paused",
          "encounter_id", "met_actions"},
         path);
  // 冷边界先按原领域顺序核基本类型，禁止宽泛类型错误吞掉原信用/游标/路遇错误。
  for (const auto *key :
       {"map_id", "plan_id", "origin", "mode", "payer_wallet_id"}) {
    const auto *x = v.find(key);
    if (!x || !x->as<StableId>())
      bad("InvalidTerrainJourney", path + "/" + key);
  }
  for (const auto *key : {"budget", "money_per_tick", "fatigue_per_tick"}) {
    const auto *x = v.find(key);
    if (!x || !x->as<Integer>() ||
        *x->as<Integer>() < (StableId{key} == "budget" ? 1 : 0))
      bad("InvalidTerrainJourney", path + "/" + key);
  }
  if (!v.find("destination") || !v.find("paused") ||
      !v.find("paused")->as<bool>() || !v.find("points") ||
      !v.find("points")->as<Array>() || v.find("points")->as<Array>()->empty())
    bad("InvalidTerrainJourney", path);
  const auto *route = v.find("route_index");
  if (!route || !route->as<Integer>() || *route->as<Integer>() < 1)
    bad("TerrainJourneySuffixMismatch", path + "/route_index");
  const auto *credit = v.find("travel_credit");
  if (!credit || !credit->as<Integer>() || *credit->as<Integer>() < 0 ||
      *credit->as<Integer>() > terrain::MAX_COST)
    bad("InvalidTerrainCredit", path + "/travel_credit");
  if (const auto *encounter = v.find("encounter_id"))
    if (!encounter->as<StableId>())
      bad("InvalidEncounterReference", path + "/encounter_id");
  if (const auto *met = v.find("met_actions")) {
    if (!met->as<Array>() || met->as<Array>()->size() > 4096)
      bad("InvalidMetJourneys", path + "/met_actions");
    for (const auto &entry : *met->as<Array>())
      if (!entry.as<StableId>() ||
          entry.as<StableId>()->rfind("action:", 0) != 0)
        bad("InvalidMetJourneys", path + "/met_actions");
  }
  if (!boolean(v, "terrain", path))
    bad("UnsupportedNativeTravelKind", path);
  TerrainTravelParams p;
  p.plan_id = text(v, "plan_id", path);
  p.map_id = text(v, "map_id", path);
  p.route_index = integer(v, "route_index", path);
  p.points = points(field(v, "points", path), path + "/points");
  p.destination =
      destination(field(v, "destination", path), path + "/destination");
  p.origin = text(v, "origin", path);
  p.mode = text(v, "mode", path);
  p.budget = integer(v, "budget", path);
  p.travel_credit = integer(v, "travel_credit", path);
  p.payer_wallet_id = text(v, "payer_wallet_id", path);
  p.money_per_tick = integer(v, "money_per_tick", path);
  p.fatigue_per_tick = integer(v, "fatigue_per_tick", path);
  p.paused = boolean(v, "paused", path);
  p.has_encounter_id = v.find("encounter_id") != nullptr;
  p.has_met_actions = v.find("met_actions") != nullptr;
  if (p.has_encounter_id)
    p.encounter_id = text(v, "encounter_id", path);
  if (p.has_met_actions)
    p.met_actions =
        strings(field(v, "met_actions", path), path + "/met_actions");
  p.field_order = field_order(v);
  return p;
}
// 作者道路只接受旧必填字段和已登记可选键，缺失控制键保持缺失而不补写。
RoadTravelParams road_params(const Value &v, const StableId &path) {
  fields(v,
         {"destination", "via", "path", "edge_index", "returning",
          "payer_wallet_id", "terrain", "paused", "reverse_first",
          "encounter_id", "met_actions"},
         path);
  const auto *edges = v.find("path");
  const auto *index = v.find("edge_index");
  const auto *payer = v.find("payer_wallet_id");
  const auto *returning = v.find("returning");
  if (!edges || !edges->as<Array>() || edges->as<Array>()->empty() || !index ||
      !index->as<Integer>() || *index->as<Integer>() < 0 ||
      *index->as<Integer>() >=
          static_cast<Integer>(edges->as<Array>()->size()) ||
      !payer || !payer->as<StableId>() || !returning || !returning->as<bool>())
    bad("InvalidTravelAction", path);
  for (const auto *key : {"paused", "reverse_first"})
    if (const auto *control = v.find(key))
      if (!control->as<bool>())
        bad("InvalidJourneyControl", path + "/" + key);
  for (const auto &edge : *edges->as<Array>())
    if (!edge.as<StableId>())
      bad("InvalidTravelPath", path + "/path");
  if (const auto *terrain = v.find("terrain"))
    if (!terrain->as<bool>() || *terrain->as<bool>())
      bad("InvalidTravelAction", path + "/terrain");
  if (const auto *encounter = v.find("encounter_id"))
    if (!encounter->as<StableId>())
      bad("InvalidEncounterReference", path + "/encounter_id");
  if (const auto *met = v.find("met_actions")) {
    if (!met->as<Array>() || met->as<Array>()->size() > 4096)
      bad("InvalidMetJourneys", path + "/met_actions");
    for (const auto &entry : *met->as<Array>())
      if (!entry.as<StableId>() ||
          entry.as<StableId>()->rfind("action:", 0) != 0)
        bad("InvalidMetJourneys", path + "/met_actions");
  }
  RoadTravelParams p;
  if (const auto *original = v.find("destination"))
    p.destination = destination(*original, path + "/destination");
  if (const auto *original = v.find("via"))
    p.via = strings(*original, path + "/via");
  p.path = strings(*edges, path + "/path");
  p.edge_index = *index->as<Integer>();
  p.returning = *returning->as<bool>();
  p.payer_wallet_id = *payer->as<StableId>();
  p.has_terrain = v.find("terrain") != nullptr;
  p.has_paused = v.find("paused") != nullptr;
  p.has_reverse_first = v.find("reverse_first") != nullptr;
  p.has_encounter_id = v.find("encounter_id") != nullptr;
  p.has_met_actions = v.find("met_actions") != nullptr;
  if (p.has_paused)
    p.paused = boolean(v, "paused", path);
  if (p.has_reverse_first)
    p.reverse_first = boolean(v, "reverse_first", path);
  if (p.has_encounter_id)
    p.encounter_id = text(v, "encounter_id", path);
  if (p.has_met_actions)
    p.met_actions =
        strings(field(v, "met_actions", path), path + "/met_actions");
  p.field_order = field_order(v);
  return p;
}
// 原导出字段与条件只在这些emit中列一次；原JSON和有界shape调用同一路径。
Status emit_point(ProjectionWriter &w, std::string_view name, PointI p) {
  const std::array<Integer, 2> xy{p.x, p.y};
  return w.sequence(name, xy,
                    [](auto &a, Integer n) { return a.field("", n); });
}
Status emit_destination(ProjectionWriter &w, std::string_view name,
                        const TravelDestination &d) {
  if (const auto *id = std::get_if<StableId>(&d))
    return w.field(name, *id);
  return emit_point(w, name, std::get<PointI>(d));
}
Status emit_ids(ProjectionWriter &w, std::string_view name,
                const std::vector<StableId> &ids) {
  return w.sequence(name, ids,
                    [](auto &a, const auto &id) { return a.field("", id); });
}
Status emit_points(ProjectionWriter &w, std::string_view name,
                   const std::vector<PointI> &points) {
  return w.sequence(name, points,
                    [](auto &a, PointI p) { return emit_point(a, "", p); });
}
Status emit_terrain_params(ProjectionWriter &sink, std::string_view name,
                           const TerrainTravelParams &p) {
  return sink.object(name, p.field_order, [&](auto &w) {
    w.field("terrain", true);
    w.field("plan_id", p.plan_id);
    w.field("map_id", p.map_id);
    w.field("route_index", p.route_index);
    emit_points(w, "points", p.points);
    emit_destination(w, "destination", p.destination);
    w.field("origin", p.origin);
    w.field("mode", p.mode);
    w.field("budget", p.budget);
    w.field("travel_credit", p.travel_credit);
    w.field("payer_wallet_id", p.payer_wallet_id);
    w.field("money_per_tick", p.money_per_tick);
    w.field("fatigue_per_tick", p.fatigue_per_tick);
    w.field("paused", p.paused);
    if (p.has_encounter_id)
      w.field("encounter_id", p.encounter_id);
    if (p.has_met_actions)
      emit_ids(w, "met_actions", p.met_actions);
    return w.status();
  });
}
Status emit_road_params(ProjectionWriter &sink, std::string_view name,
                        const RoadTravelParams &p) {
  return sink.object(name, p.field_order, [&](auto &w) {
    if (p.destination)
      emit_destination(w, "destination", *p.destination);
    if (p.via)
      emit_ids(w, "via", *p.via);
    emit_ids(w, "path", p.path);
    w.field("edge_index", p.edge_index);
    w.field("returning", p.returning);
    if (p.has_paused)
      w.field("paused", p.paused);
    if (p.has_reverse_first)
      w.field("reverse_first", p.reverse_first);
    w.field("payer_wallet_id", p.payer_wallet_id);
    if (p.has_terrain)
      w.field("terrain", false);
    if (p.has_encounter_id)
      w.field("encounter_id", p.encounter_id);
    if (p.has_met_actions)
      emit_ids(w, "met_actions", p.met_actions);
    return w.status();
  });
}

// 检查点点位按int64编码，不能经临时浮点几何。
Status visit_point(CodecVisitor &c, PointI &p) {
  c.field("x", p.x);
  c.field("y", p.y);
  return c.status();
}
// variant标签显式编码，非法标签拒绝而不默认城市。
Status visit_destination(CodecVisitor &c, TravelDestination &d) {
  Integer kind = std::holds_alternative<StableId>(d) ? 0 : 1;
  c.field("kind", kind);
  if (!c.status().ok)
    return c.status();
  if (kind != 0 && kind != 1)
    return c.reject("InvalidTravelDestination");
  if (c.reading())
    d = kind == 0 ? TravelDestination{StableId{}} : TravelDestination{PointI{}};
  if (kind == 0)
    c.field("settlement", std::get<StableId>(d));
  else
    c.object("point", [&](CodecVisitor &child) {
      return visit_point(child, std::get<PointI>(d));
    });
  return c.status();
}
// checkpoint按完整领域schema逐字段，不能跳过未知字段。
Status visit_params(CodecVisitor &c, TerrainTravelParams &p) {
  c.field("plan_id", p.plan_id);
  c.field("map_id", p.map_id);
  c.field("route_index", p.route_index);
  c.sequence("points", p.points, visit_point);
  c.object("destination", [&](CodecVisitor &child) {
    return visit_destination(child, p.destination);
  });
  c.field("origin", p.origin);
  c.field("mode", p.mode);
  c.field("budget", p.budget);
  c.field("travel_credit", p.travel_credit);
  c.field("payer_wallet_id", p.payer_wallet_id);
  c.field("money_per_tick", p.money_per_tick);
  c.field("fatigue_per_tick", p.fatigue_per_tick);
  c.field("paused", p.paused);
  c.field("encounter_id", p.encounter_id);
  c.field("has_encounter_id", p.has_encounter_id);
  c.field("has_met_actions", p.has_met_actions);
  c.sequence(
      "met_actions", p.met_actions,
      [](CodecVisitor &child, StableId &id) { return child.field("id", id); });
  c.sequence(
      "field_order", p.field_order,
      [](CodecVisitor &child, StableId &id) { return child.field("key", id); });
  return c.status();
}
// 道路检查点记录完整有限载荷及存在性，不把缺失键恢复成默认存在。
Status visit_road_params(CodecVisitor &c, RoadTravelParams &p) {
  c.optional("destination", p.destination, visit_destination);
  c.optional("via", p.via, [](CodecVisitor &child, std::vector<StableId> &via) {
    return child.sequence("ids", via, [](CodecVisitor &entry, StableId &id) {
      return entry.field("id", id);
    });
  });
  c.sequence("path", p.path, [](CodecVisitor &child, StableId &id) {
    return child.field("id", id);
  });
  c.field("edge_index", p.edge_index);
  c.field("returning", p.returning);
  c.field("paused", p.paused);
  c.field("reverse_first", p.reverse_first);
  c.field("payer_wallet_id", p.payer_wallet_id);
  c.field("has_terrain", p.has_terrain);
  c.field("has_paused", p.has_paused);
  c.field("has_reverse_first", p.has_reverse_first);
  c.field("has_encounter_id", p.has_encounter_id);
  c.field("has_met_actions", p.has_met_actions);
  c.field("encounter_id", p.encounter_id);
  c.sequence(
      "met_actions", p.met_actions,
      [](CodecVisitor &child, StableId &id) { return child.field("id", id); });
  c.sequence("field_order", p.field_order,
             [](CodecVisitor &child, StableId &key) {
               return child.field("key", key);
             });
  return c.status();
}
// ordered表序列保持真实插入序；解码先到局部表后整体替换，重复ID明确拒绝。
template <class T, class Fn>
Status table(CodecVisitor &c, const StableId &name, OrderedTable<T> &values,
             Fn visit) {
  // 留存仅走原拥有图；普通编码继续原字段序列，不误用专用留存访客。
  (void)0;
  using Row = typename OrderedTable<T>::Row;
  auto emit = [&](CodecVisitor &child, Row &row) {
    child.field("id", row.first);
    return child.object(
        "value", [&](CodecVisitor &value) { return visit(value, row.second); });
  };
  if (!c.reading())
    return c.sequence(name, values.rows(), emit);
  std::vector<Row> rows;
  c.sequence(name, rows, emit);
  if (c.status().ok && !values.adopt_checkpoint_rows(std::move(rows)))
    return c.reject("DuplicateTravelCheckpointId", name);
  return c.status();
}
} // namespace

// 每域只消费其action/fact kind；未知旅程字段拒绝，其他域载荷留其owner读取。
Outcome<TravelState> decode_travel_seed(const BoundaryValue &value) {
  try {
    TravelState result;
    for (const auto &row :
         typed<Dict>(field(value, "actions", "/state"), "/actions")) {
      auto kind = text(row.second, "kind", "/actions/" + row.first);
      const auto &p = field(row.second, "params", "/actions/" + row.first);
      if (kind == "travel") {
        const auto *terrain = p.find("terrain");
        if (terrain && terrain->as<bool>() && *terrain->as<bool>())
          result.actions.insert(row.first,
                                params(p, "/actions/" + row.first + "/params"));
        else
          result.road_actions.insert(
              row.first, road_params(p, "/actions/" + row.first + "/params"));
      } else if (kind == "rescue") {
        fields(p, {"origin"}, "/rescue");
        result.rescues.insert(row.first, {text(p, "origin", "/rescue")});
      }
    }
    for (const auto &row :
         typed<Dict>(field(value, "facts", "/state"), "/facts")) {
      const auto &v = row.second;
      auto kind = text(v, "kind", "/facts/" + row.first);
      if (kind != "terrain_journey")
        continue;
      StableId path = "/facts/" + row.first;
      fields(v,
             {"kind", "actor_id", "action_id", "tick", "map_id", "origin",
              "destination", "mode", "budget", "starting_load", "points"},
             path);
      TerrainJourneyFact f;
      f.id = row.first;
      f.actor_id = text(v, "actor_id", path);
      f.action_id = text(v, "action_id", path);
      f.tick = integer(v, "tick", path);
      f.map_id = text(v, "map_id", path);
      f.origin = point(field(v, "origin", path), path + "/origin");
      f.destination =
          destination(field(v, "destination", path), path + "/destination");
      f.mode = text(v, "mode", path);
      f.budget = integer(v, "budget", path);
      f.starting_load = integer(v, "starting_load", path);
      f.points = points(field(v, "points", path), path + "/points");
      f.field_order = field_order(v);
      result.journey_facts.insert(row.first, std::move(f));
    }
    for (const auto &row :
         typed<Dict>(field(value, "itineraries", "/state"), "/itineraries")) {
      const auto *index = row.second.find("index");
      const auto *ready = row.second.find("ready_tick");
      const auto *travelling = row.second.find("travelling");
      if (!index || !index->as<Integer>() || *index->as<Integer>() < 0 ||
          !ready || !ready->as<Integer>() || *ready->as<Integer>() < 0 ||
          *ready->as<Integer>() > 2147483647 || !travelling ||
          !travelling->as<bool>())
        bad("InvalidItinerary", "/itineraries/" + row.first);
      fields(row.second, {"index", "ready_tick", "travelling"},
             "/itineraries/" + row.first);
      result.itineraries.insert(
          row.first, {integer(row.second, "index", "/itineraries"),
                      integer(row.second, "ready_tick", "/itineraries"),
                      boolean(row.second, "travelling", "/itineraries"),
                      field_order(row.second)});
    }
    return {true, std::move(result), {}};
  } catch (const DecodeFailure &f) {
    return {false, {}, f.error};
  } catch (...) {
    return {false,
            {},
            {{"NativeTravelDecodeFailure", "/state"},
             ErrorKind::internal,
             {},
             "travel",
             {},
             {}}};
  }
}
namespace {
Status emit_journey_fact(ProjectionWriter &sink, std::string_view name,
                         const TerrainJourneyFact &f) {
  return sink.object(name, f.field_order, [&](auto &c) {
    c.field("kind", StableId{"terrain_journey"});
    c.field("actor_id", f.actor_id);
    c.field("action_id", f.action_id);
    c.field("tick", f.tick);
    c.field("map_id", f.map_id);
    emit_point(c, "origin", f.origin);
    emit_destination(c, "destination", f.destination);
    c.field("mode", f.mode);
    c.field("budget", f.budget);
    c.field("starting_load", f.starting_load);
    emit_points(c, "points", f.points);
    return c.status();
  });
}
Status emit_itinerary(ProjectionWriter &sink, std::string_view name,
                      const TravelItinerary &i) {
  return sink.object(name, i.field_order, [&](auto &c) {
    c.field("index", i.index);
    c.field("ready_tick", i.ready_tick);
    c.field("travelling", i.travelling);
    return c.status();
  });
}
} // namespace
// 中央可直接向已有facts/action_params字典发射，跨域冲突保留在同sink。
Status emit_travel_action_params_projection(ProjectionWriter &w,
                                            const TravelState &state) {
  for (const auto &row : state.actions.rows())
    emit_terrain_params(w, row.first, row.second);
  for (const auto &row : state.road_actions.rows())
    emit_road_params(w, row.first, row.second);
  for (const auto &row : state.rescues.rows())
    w.object(row.first,
             [&](auto &c) { return c.field("origin", row.second.origin); });
  return w.status();
}
Status emit_travel_facts_projection(ProjectionWriter &w,
                                    const TravelState &state) {
  for (const auto &row : state.journey_facts.rows())
    emit_journey_fact(w, row.first, row.second);
  return w.status();
}
Status emit_travel_itineraries_projection(ProjectionWriter &w,
                                          const TravelState &state) {
  for (const auto &row : state.itineraries.rows())
    emit_itinerary(w, row.first, row.second);
  return w.status();
}
Outcome<bool> emit_travel_action_params_record_projection(
    ProjectionWriter &w, std::string_view name, const TravelState &state,
    const StableId &id) {
  const auto *terrain = state.actions.find(id);
  const auto *road = state.road_actions.find(id);
  const auto *rescue = state.rescues.find(id);
  const int count =
      int(terrain != nullptr) + int(road != nullptr) + int(rescue != nullptr);
  if (count > 1)
    return {
        false, {}, common_failure("DuplicateTravelActionPayload", id).error};
  if (!count)
    return {true, false, {}};
  Status checked = terrain ? emit_terrain_params(w, name, *terrain)
                   : road  ? emit_road_params(w, name, *road)
                           : w.object(name, [&](auto &c) {
                              return c.field("origin", rescue->origin);
                             });
  return checked.ok ? Outcome<bool>{true, true, {}}
                    : Outcome<bool>{false, {}, checked.error};
}
Outcome<bool> emit_travel_fact_record_projection(ProjectionWriter &w,
                                                 std::string_view name,
                                                 const TravelState &state,
                                                 const StableId &id) {
  const auto *row = state.journey_facts.find(id);
  if (!row)
    return {true, false, {}};
  auto checked = emit_journey_fact(w, name, *row);
  return checked.ok ? Outcome<bool>{true, true, {}}
                    : Outcome<bool>{false, {}, checked.error};
}
Outcome<bool> emit_travel_itinerary_record_projection(ProjectionWriter &w,
                                                      std::string_view name,
                                                      const TravelState &state,
                                                      const StableId &id) {
  const auto *row = state.itineraries.find(id);
  if (!row)
    return {true, false, {}};
  auto checked = emit_itinerary(w, name, *row);
  return checked.ok ? Outcome<bool>{true, true, {}}
                    : Outcome<bool>{false, {}, checked.error};
}
Status emit_travel_state_projection(ProjectionWriter &sink,
                                    std::string_view name,
                                    const TravelState &state) {
  return sink.object(name, [&](auto &w) {
    w.object("action_params", [&](auto &c) {
      return emit_travel_action_params_projection(c, state);
    });
    w.object("facts",
             [&](auto &c) { return emit_travel_facts_projection(c, state); });
    w.object("itineraries", [&](auto &c) {
      return emit_travel_itineraries_projection(c, state);
    });
    return w.status();
  });
}
Outcome<BoundaryValue> export_travel_state(const TravelState &state) {
  Value output;
  BoundaryProjectionWriter sink(output);
  auto status = emit_travel_state_projection(sink, "", state);
  if (status.ok)
    status = sink.finish();
  if (!status.ok)
    return {false, {}, status.error};
  return {true, std::move(output), {}};
}
// 检查点读写同一领域schema，不重新实现世界表或业务校验。
Status visit_travel_checkpoint(CodecVisitor &c, TravelState &state) {
  table(c, "travel_actions", state.actions, visit_params);
  table(c, "road_actions", state.road_actions, visit_road_params);
  table(c, "rescues", state.rescues, [](CodecVisitor &child, RescueParams &p) {
    return child.field("origin", p.origin);
  });
  table(c, "journey_facts", state.journey_facts,
        [](CodecVisitor &child, TerrainJourneyFact &f) {
          child.field("id", f.id);
          child.field("actor_id", f.actor_id);
          child.field("action_id", f.action_id);
          child.field("tick", f.tick);
          child.field("map_id", f.map_id);
          child.object("origin", [&](CodecVisitor &inner) {
            return visit_point(inner, f.origin);
          });
          child.object("destination", [&](CodecVisitor &inner) {
            return visit_destination(inner, f.destination);
          });
          child.field("mode", f.mode);
          child.field("budget", f.budget);
          child.field("starting_load", f.starting_load);
          child.sequence("points", f.points, visit_point);
          child.sequence("field_order", f.field_order,
                         [](CodecVisitor &inner, StableId &key) {
                           return inner.field("key", key);
                         });
          return child.status();
        });
  table(c, "itineraries", state.itineraries,
        [](CodecVisitor &child, TravelItinerary &i) {
          child.field("index", i.index);
          child.field("ready_tick", i.ready_tick);
          child.field("travelling", i.travelling);
          child.sequence("field_order", i.field_order,
                         [](CodecVisitor &inner, StableId &key) {
                           return inner.field("key", key);
                         });
          return child.status();
        });
  return c.status();
}
} // namespace hanmo::resident
