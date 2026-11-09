// 文件系统只在内容装载边界读取；热生成及会战推进不访问外部文件。
#include "field_author_data.hpp"
#include "resident/checkpoint/checkpoint_json_decode.hpp"
#include "resident/checkpoint/ordered_value.hpp"
#include "resident/domains/travel/route_geometry.hpp"
#include "resident/domains/travel/travel.hpp"
#include "resident/domains/travel/travel_decode_detail.hpp"
#include "resident/kernel/original_catalog_identity.hpp"
#include <fstream>
#include <limits>
#include <set>
namespace hanmo::resident {
namespace {
// 不吞缺资源错误，也不替换为平地或另一个 map。
template <class T>
Outcome<T> failure(const StableId &code, const StableId &path) {
  Outcome<T> out;
  out.error.original = {code, path};
  return out;
}
// 一次读取原文件，I/O 错误不返回截短字节。
Outcome<CheckpointVector<std::uint8_t>>
read_file(const std::filesystem::path &path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream)
    return failure<CheckpointVector<std::uint8_t>>("FieldTerrainMissing",
                                                   path.string());
  std::error_code error;
  const auto size = std::filesystem::file_size(path, error);
  if (error || size > std::numeric_limits<std::size_t>::max() ||
      size > static_cast<std::uintmax_t>(
                 std::numeric_limits<std::streamsize>::max()))
    return failure<CheckpointVector<std::uint8_t>>("FieldTerrainMissing",
                                                   path.string());
  // 原 CheckpointAllocator 先按真实文件长度取得同 job
  // 分配；不让迭代器无界扩容。
  CheckpointVector<std::uint8_t> bytes(static_cast<std::size_t>(size));
  if (!bytes.empty())
    stream.read(reinterpret_cast<char *>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
  if (stream.bad() ||
      stream.gcount() != static_cast<std::streamsize>(bytes.size()) ||
      stream.peek() != std::char_traits<char>::eof())
    return failure<CheckpointVector<std::uint8_t>>("FieldTerrainMissing",
                                                   path.string());
  return {true, std::move(bytes), {}};
}
// 路径须是本次作者目录内的相对路径；任何上跳段都拒绝，不能先归一化再隐藏它。
bool relative_path(const std::filesystem::path &path) {
  if (path.empty() || path.is_absolute() || path.has_root_name())
    return false;
  for (const auto &part : path)
    if (part == "..")
      return false;
  return true;
}
// 直接比较原身份型的全部字段，既不另造摘要也不把另一规则版本借来使用。
bool same_identity(const CatalogIdentity &a, const CatalogIdentity &b) {
  return !a.content_id.empty() && !a.content_version.empty() &&
         !a.content_digest.empty() && !a.schema_sha256.empty() &&
         same_original_catalog_identity(a, b);
}
} // namespace
DirectoryFieldBattleResources::DirectoryFieldBattleResources(
    CatalogIdentity identity, std::filesystem::path root)
    : identity_(std::move(identity)), root_(std::move(root)) {
}
const CatalogIdentity &
DirectoryFieldBattleResources::identity() const noexcept {
  return identity_;
}
Outcome<CheckpointVector<std::uint8_t>> DirectoryFieldBattleResources::read(
    const StableId &relative_author_path) const {
  const std::filesystem::path path(relative_author_path);
  const auto file = path.filename().string();
  // 资源目录只能是合同中的单个地图目录，四文件白名单不授权其它相对位置。
  if (!root_.is_absolute() || !relative_path(path) ||
      path.parent_path().parent_path() !=
          std::filesystem::path("content/world/terrain") ||
      (file != "terrain.json" && file != "height.u16" &&
       file != "rivers.u8x2" && file != "land.u8x3"))
    return failure<CheckpointVector<std::uint8_t>>("FieldTerrainMissing",
                                                   "fine_terrain/path");
  std::error_code error;
  const auto root = std::filesystem::canonical(root_, error);
  if (error)
    return failure<CheckpointVector<std::uint8_t>>("FieldTerrainMissing",
                                                   "fine_terrain/root");
  const auto resolved = std::filesystem::canonical(root / path, error);
  if (error || !relative_path(resolved.lexically_relative(root)))
    return failure<CheckpointVector<std::uint8_t>>("FieldTerrainMissing",
                                                   relative_author_path);
  return read_file(resolved);
}
Outcome<FieldTerrainData>
load_field_battle_terrain(const BoundaryValue &data, const StableId &map_id,
                          const CatalogIdentity &identity,
                          const FieldBattleResourceRead &resources) {
  if (!same_identity(identity, resources.identity()))
    return failure<FieldTerrainData>("FieldTerrainContentIdentityMismatch",
                                     "field_battle/resources");
  const auto *maps = data.find("world_maps");
  const auto *map = maps ? maps->find(map_id) : nullptr;
  const auto *ref = map ? map->find("fine_terrain") : nullptr;
  if (!ref || !ref->as<StableId>() || ref->as<StableId>()->empty())
    return failure<FieldTerrainData>("FieldTerrainMissing",
                                     "world_maps/" + map_id + "/fine_terrain");
  const auto &authored_directory = *ref->as<StableId>();
  const std::filesystem::path directory(authored_directory);
  const StableId expected_directory = "content/world/terrain/" + map_id;
  // 准入后的目录仍须属于本 map；不把另一个作者图的资源借作此图。
  if (!relative_path(directory) ||
      (authored_directory != expected_directory &&
       authored_directory != expected_directory + "/"))
    return failure<FieldTerrainData>("FieldTerrainMissing",
                                     "fine_terrain/path");
  auto raw = resources.read((directory / "terrain.json").generic_string());
  if (!raw.ok)
    return {false, {}, raw.error};
  auto json = parse_boundary_json(StableId(raw.value.begin(), raw.value.end()));
  if (!json.ok)
    return failure<FieldTerrainData>("FieldTerrainMissing", "terrain.json");
  auto metadata = decode_field_terrain_metadata(json.value);
  if (!metadata.ok)
    return {false, {}, metadata.error};
  if (metadata.value.map_id != map_id)
    return failure<FieldTerrainData>("FieldTerrainMissing",
                                     "terrain.json/map_id");
  auto height = resources.read((directory / "height.u16").generic_string()),
       rivers = resources.read((directory / "rivers.u8x2").generic_string()),
       land = resources.read((directory / "land.u8x3").generic_string());
  if (!height.ok)
    return {false, {}, height.error};
  if (!rivers.ok)
    return {false, {}, rivers.error};
  if (!land.ok)
    return {false, {}, land.error};
  return load_field_terrain_data(std::move(metadata.value), height.value,
                                 rivers.value, land.value);
}
Outcome<FieldBattleCatalog> prepare_field_battle_catalog(
    const BoundaryValue &data, const StableId &scenario_id,
    const CatalogIdentity &identity, const FieldBattleResourceRead &resources) {
  if (!same_identity(identity, resources.identity()))
    return failure<FieldBattleCatalog>("FieldTerrainContentIdentityMismatch",
                                       "field_battle/resources");
  const auto *value = data.find("world_maps");
  const auto *maps = value ? value->as<BoundaryValue::Dict>() : nullptr;
  if (!maps)
    return failure<FieldBattleCatalog>("FieldTerrainMissing", "world_maps");
  const auto *scenarios = data.find("scenarios");
  const auto *scenario = scenarios ? scenarios->find(scenario_id) : nullptr;
  const auto *dynamic = scenario ? scenario->find("dynamic_world") : nullptr;
  const auto *selected = dynamic ? dynamic->find("field_battle") : nullptr;
  if (!selected)
    return failure<FieldBattleCatalog>("FieldBattleProfileRequired",
                                       scenario_id);
  auto parameters = decode_field_battle_parameters(*selected);
  if (!parameters.ok)
    return {false, {}, parameters.error};
  FieldBattleCatalog result;
  const auto *settlement_rows = data.find("settlements");
  const auto *settlements =
      settlement_rows ? settlement_rows->as<BoundaryValue::Dict>() : nullptr;
  if (!settlements)
    return failure<FieldBattleCatalog>("InvalidFieldBattleRoute",
                                       "settlements");
  for (const auto &[id, row] : *settlements) {
    const auto *kind = row.find("kind");
    if (!kind || !kind->as<StableId>() ||
        !result.settlement_kinds.emplace(id, *kind->as<StableId>()).second)
      return failure<FieldBattleCatalog>("InvalidFieldBattleRoute",
                                         "settlements/" + id + "/kind");
  }
  // 原军事目录曾只保职务权限；军议在同作者身份下另保实际职名，不造官职。
  const auto *office_rows = data.find("offices");
  const auto *offices =
      office_rows ? office_rows->as<BoundaryValue::Dict>() : nullptr;
  const auto *texts = data.find("texts");
  if (!offices || !texts || !texts->as<BoundaryValue::Dict>())
    return failure<FieldBattleCatalog>("FieldCouncilOfficeTitleRequired",
                                       "offices/texts");
  for (const auto &[id, row] : *offices) {
    const auto *key_value = row.find("name_key");
    const auto *key = key_value ? key_value->as<StableId>() : nullptr;
    const auto *title_value = key ? texts->find(*key) : nullptr;
    const auto *title = title_value ? title_value->as<StableId>() : nullptr;
    if (!key || !title || title->empty())
      return failure<FieldBattleCatalog>("FieldCouncilOfficeTitleRequired",
                                         "offices/" + id);
    result.office_titles.emplace(checkpoint_decode_boundary_string(id),
                                 checkpoint_decode_boundary_string(*title));
  }
  result.identity = identity;
  result.parameters = std::move(parameters.value);
  for (const auto &[id, row] : *maps) {
    if (!row.find("fine_terrain"))
      continue;
    auto loaded = load_field_battle_terrain(data, id, identity, resources);
    if (!loaded.ok)
      return {false, {}, loaded.error};
    if (!result.maps.emplace(id, std::move(loaded.value)).second)
      return failure<FieldBattleCatalog>("FieldTerrainMissing",
                                         "world_maps/" + id);
  }
  if (result.maps.empty())
    return failure<FieldBattleCatalog>("FieldTerrainMissing",
                                       "world_maps/fine_terrain");
  return {true, std::move(result), {}};
}
Outcome<const FieldTerrainData *>
read_field_road_terrain(const TravelCatalog &travel,
                        const FieldBattleCatalog &catalog,
                        const StableId &route_id) {
  const auto *road = travel.roads.find(route_id);
  if (!road)
    return failure<const FieldTerrainData *>("InvalidFieldBattleRoute",
                                             route_id);
  auto map = original_travel_road_world_map(travel, *road);
  if (!map.ok)
    return {false, {}, map.error};
  const auto found = catalog.maps.find(map.value);
  if (found == catalog.maps.end() || found->second.metadata.map_id != map.value)
    return failure<const FieldTerrainData *>("FieldTerrainMissing", map.value);
  return {true, &found->second, {}};
}
Outcome<FieldRoadGeometry>
read_field_road_geometry(const BoundaryValue &data, const TravelCatalog &travel,
                         const FieldTerrainMetadata &metadata,
                         const StableId &route_id) {
  const auto *road = travel.roads.find(route_id);
  if (!road)
    return failure<FieldRoadGeometry>("InvalidFieldBattleRoute",
                                      "routes/" + route_id);
  auto map = original_travel_road_world_map(travel, *road);
  if (!map.ok)
    return {false, {}, map.error};
  if (map.value != metadata.map_id)
    return failure<FieldRoadGeometry>("TravelRouteMapMismatch", route_id);
  auto line = original_travel_road_polyline(travel, *road);
  if (!line.ok)
    return {false, {}, line.error};
  FieldRoadGeometry result;
  result.route_id = route_id;
  for (auto point : line.value)
    result.points_dm.push_back(field_map_point_dm(metadata, point));
  const auto *settlements = data.find("settlements");
  if (!settlements)
    return failure<FieldRoadGeometry>("InvalidFieldBattleRoute", "settlements");
  for (const auto &entry : travel.settlements.rows()) {
    if (entry.second.world_map_id != metadata.map_id)
      continue;
    const auto *author = settlements->find(entry.first);
    const auto *kind = author ? author->find("kind") : nullptr;
    if (!kind || !kind->as<StableId>())
      return failure<FieldRoadGeometry>("InvalidFieldBattleRoute",
                                        "settlements/" + entry.first + "/kind");
    const auto &k = *kind->as<StableId>();
    if (k == "city" || k == "village")
      result.settlements.push_back(
          {entry.first, field_map_point_dm(metadata, entry.second.map_xy),
           k == "city"});
  }
  return {true, std::move(result), {}};
}
Outcome<FieldRoadGeometry> read_field_road_geometry(
    const TravelCatalog &travel, const FieldBattleCatalog &catalog,
    const FieldTerrainMetadata &metadata, const StableId &route_id) {
  const auto *road = travel.roads.find(route_id);
  if (!road)
    return failure<FieldRoadGeometry>("InvalidFieldBattleRoute",
                                      "routes/" + route_id);
  auto map = original_travel_road_world_map(travel, *road);
  if (!map.ok)
    return {false, {}, map.error};
  if (map.value != metadata.map_id)
    return failure<FieldRoadGeometry>("TravelRouteMapMismatch", route_id);
  auto line = original_travel_road_polyline(travel, *road);
  if (!line.ok)
    return {false, {}, line.error};
  FieldRoadGeometry result;
  result.route_id = route_id;
  for (auto point : line.value)
    result.points_dm.push_back(field_map_point_dm(metadata, point));

  for (const auto &entry : travel.settlements.rows()) {
    if (entry.second.world_map_id != metadata.map_id)
      continue;
    const auto kind = catalog.settlement_kinds.find(entry.first);
    if (kind == catalog.settlement_kinds.end())
      return failure<FieldRoadGeometry>("InvalidFieldBattleRoute",
                                        "settlements/" + entry.first + "/kind");
    const auto &k = kind->second;
    if (k == "city" || k == "village")
      result.settlements.push_back(
          {entry.first, field_map_point_dm(metadata, entry.second.map_xy),
           k == "city"});
  }
  return {true, std::move(result), {}};
}
// 只读作者边界的路与城坐标；此处不替代完整 TravelCatalog 的准入与留存计量。
Outcome<FieldRoadGeometry>
read_field_road_geometry(const BoundaryValue &data,
                         const FieldTerrainMetadata &metadata,
                         const StableId &route_id) {
  using namespace travel_decode_detail;
  try {
    const auto path = StableId("/routes/") + route_id;
    const auto &author =
        field(field(data, "routes", "/data"), route_id, "/routes");
    TravelRoad road;
    road.id = route_id;
    road.origin = text(author, "origin", path);
    road.destination = text(author, "destination", path);
    if (const auto *map = author.find("world_map_id"))
      road.world_map_id = typed<StableId>(*map, path + "/world_map_id");
    if (const auto *shape = author.find("shape"))
      road.shape = points(*shape, path + "/shape");
    std::map<StableId, TravelSettlement> endpoints;
    FieldRoadGeometry result;
    result.route_id = route_id;
    for (const auto &[id, row] :
         typed<Dict>(field(data, "settlements", "/data"), "/settlements")) {
      const auto sp = StableId("/settlements/") + id;
      TravelSettlement settlement{
          id, text(row, "entry_place", sp),
          point(field(row, "map_xy", sp), sp + "/map_xy")};
      if (const auto *map = row.find("world_map_id"))
        settlement.world_map_id = typed<StableId>(*map, sp + "/world_map_id");
      const auto kind = text(row, "kind", sp);
      if (settlement.world_map_id == metadata.map_id &&
          (kind == "city" || kind == "village"))
        result.settlements.push_back(
            {id, field_map_point_dm(metadata, settlement.map_xy),
             kind == "city"});
      if (id == road.origin || id == road.destination)
        endpoints.emplace(id, std::move(settlement));
    }
    if (!endpoints.contains(road.origin) ||
        !endpoints.contains(road.destination))
      return failure<FieldRoadGeometry>("InvalidFieldBattleRoute", path);
    auto map = original_travel_road_world_map(endpoints.at(road.origin), road,
                                              endpoints.at(road.destination));
    if (!map.ok)
      return {false, {}, map.error};
    if (map.value != metadata.map_id)
      return failure<FieldRoadGeometry>("TravelRouteMapMismatch", route_id);
    auto line = original_travel_road_polyline(endpoints.at(road.origin), road,
                                              endpoints.at(road.destination));
    if (!line.ok)
      return {false, {}, line.error};
    for (const auto &point : line.value)
      result.points_dm.push_back(field_map_point_dm(metadata, point));
    return {true, std::move(result), {}};
  } catch (const DecodeFailure &failure) {
    return {false, {}, failure.error};
  }
}
// 全部 §10 参数逐项准入；旧部分作者块必须显式补齐，未知字段也不静默接受。
Outcome<FieldBattleParameters>
decode_field_battle_parameters(const BoundaryValue &selected) {
  FieldBattleParameters out;
  const auto *object = selected.as<BoundaryValue::Dict>();
  if (!object)
    return failure<FieldBattleParameters>("InvalidFieldBattleConfiguration",
                                          "field_battle");
  const auto *version = selected.find("version");
  if (!version || !version->as<StableId>() ||
      *version->as<StableId>() != "field_battle/1")
    return failure<FieldBattleParameters>("UnsupportedFieldBattleVersion",
                                          "field_battle/version");
  out.version = checkpoint_decode_boundary_string(*version->as<StableId>());
  std::set<StableId> allowed{"version", "terrain_speed_bp", "terrain_cover"};
  for (auto entry :
       {std::pair{"window_s", &out.window_s},
        std::pair{"step_s", &out.step_s},
        std::pair{"length_m", &out.length_m},
        std::pair{"width_m", &out.width_m},
        std::pair{"cell_m", &out.cell_m},
        std::pair{"deployment_length_m", &out.deployment_length_m},
        std::pair{"deployment_width_m", &out.deployment_width_m},
        std::pair{"deployment_inset_m", &out.deployment_inset_m},
        std::pair{"deployment_min_percent", &out.deployment_min_percent},
        std::pair{"exit_radius_m", &out.exit_radius_m},
        std::pair{"anchor_shift_step_m", &out.anchor_shift_step_m},
        std::pair{"anchor_shift_limit_m", &out.anchor_shift_limit_m},
        std::pair{"courier_m_per_minute", &out.courier_m_per_minute},
        std::pair{"minimum_order_delay_s", &out.minimum_order_delay_s},
        std::pair{"pulse_s", &out.pulse_s},
        std::pair{"round_equiv_s", &out.round_equiv_s},
        std::pair{"morale_loss_bp", &out.morale_loss_bp},
        std::pair{"commander_exposure_bp", &out.commander_exposure_bp},
        std::pair{"max_segments", &out.max_segments},
        std::pair{"infantry_m_per_minute", &out.infantry_m_per_minute},
        std::pair{"archer_m_per_minute", &out.archer_m_per_minute},
        std::pair{"cavalry_m_per_minute", &out.cavalry_m_per_minute},
        std::pair{"charge_m_per_minute", &out.charge_m_per_minute},
        std::pair{"rout_speed_bp", &out.rout_speed_bp},
        std::pair{"melee_contact_m", &out.melee_contact_m},
        std::pair{"archer_min_range_m", &out.archer_min_range_m},
        std::pair{"archer_max_range_m", &out.archer_max_range_m},
        std::pair{"ai_retreat_fit_percent", &out.ai_retreat_fit_percent},
        std::pair{"ai_retreat_morale", &out.ai_retreat_morale},
        std::pair{"ai_support_morale", &out.ai_support_morale},
        std::pair{"ai_eval_s", &out.ai_eval_s},
        std::pair{"ai_advance_hold_s", &out.ai_advance_hold_s},
        std::pair{"rejoin_distance_m", &out.rejoin_distance_m},
        std::pair{"rejoin_stopped_s", &out.rejoin_stopped_s},
        std::pair{"max_formations_per_side", &out.max_formations_per_side}}) {
    allowed.insert(entry.first);
    const auto *value = selected.find(entry.first);
    if (!value || !value->as<Integer>())
      return failure<FieldBattleParameters>("InvalidFieldBattleConfiguration",
                                            StableId("field_battle/") +
                                                entry.first);
    *entry.second = *value->as<Integer>();
  }
  for (const auto &[key, value] : *object)
    if (!allowed.contains(key))
      return failure<FieldBattleParameters>("InvalidFieldBattleConfiguration",
                                            "field_battle/" + key);
  static constexpr std::array<const char *, 13> terrain_names{
      "Plain", "Field",  "Forest", "Rough",   "Steep", "Stream", "Water",
      "Road",  "Bridge", "Ford",   "Village", "Wall",  "Gate"};
  const auto *speed_value = selected.find("terrain_speed_bp");
  const auto *cover_value = selected.find("terrain_cover");
  const auto *speeds =
      speed_value ? speed_value->as<BoundaryValue::Dict>() : nullptr;
  const auto *covers =
      cover_value ? cover_value->as<BoundaryValue::Dict>() : nullptr;
  if (!speeds || !covers || speeds->size() != terrain_names.size() ||
      covers->size() != terrain_names.size())
    return failure<FieldBattleParameters>("InvalidFieldBattleConfiguration",
                                          "field_battle/terrain");
  for (std::size_t i = 0; i < terrain_names.size(); ++i) {
    const auto *row = speed_value->find(terrain_names[i]);
    const auto *values = row ? row->as<BoundaryValue::Array>() : nullptr;
    const auto *cover = cover_value->find(terrain_names[i]);
    if (!values || values->size() != 3 || !cover || !cover->as<Integer>())
      return failure<FieldBattleParameters>("InvalidFieldBattleConfiguration",
                                            StableId("field_battle/terrain/") +
                                                terrain_names[i]);
    out.terrain_cover[i] = *cover->as<Integer>();
    for (std::size_t category = 0; category < 3; ++category) {
      const auto *value = (*values)[category].as<Integer>();
      if (!value)
        return failure<FieldBattleParameters>(
            "InvalidFieldBattleConfiguration",
            StableId("field_battle/terrain_speed_bp/") + terrain_names[i]);
      out.terrain_speed_bp[i][category] = *value;
    }
  }
  auto valid = validate_field_battle_parameters(out);
  if (!valid.ok)
    return {false, {}, valid.error};
  return {true, std::move(out), {}};
}
} // namespace hanmo::resident
