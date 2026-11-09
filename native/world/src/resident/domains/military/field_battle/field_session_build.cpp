#include "resident/domains/military/military_model.hpp"
// 真实 ContactHit 的有理时刻与道路弧长直接进入连续会话；不从旧模板推导战场。
#include "field_author_data.hpp"
#include "field_session_build.hpp"
#include "resident/domains/military/military_contact.hpp"
#include "resident/model/common_rng.hpp"
#include "resident/model/common_string_hash.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
// 本次两军来源只在原 MilitaryState 查询，不能用调用方构造的 ArmyRecord 替代。
const ArmyRecord *army(const MilitaryState &state, const StableId &id) {
  const ArmyRecord *found = nullptr;
  for (const auto &row : state.armies)
    if (row.id == id) {
      if (found)
        return nullptr;
      found = &row;
    }
  return found;
}
// 两条有向 road 的 canonical 进度用于追击判别；不按演员 ID 给军事阵营排侧。
Integer projected_start(const RoadTravelMotion &motion, const TravelRoad &edge,
                        const TravelRoad &basis) {
  return edge.origin == basis.origin ? motion.start
                                     : edge.duration_ticks * 100 - motion.start;
}
Integer direction(const RoadTravelMotion &motion, const TravelRoad &edge,
                  const TravelRoad &basis) {
  const Integer delta =
      (motion.end - motion.start) * (edge.origin == basis.origin ? 1 : -1);
  return delta > 0 ? 1 : delta < 0 ? -1 : 0;
}
} // namespace
Outcome<GeneratedBattleField>
regenerate_field_road_battle(const TravelCatalog &travel,
                             const FieldBattleCatalog &catalog,
                             const FieldRoadContact &source) {
  const auto &parameters = catalog.parameters;
  const auto &version = catalog.identity.content_version;
  if (!source.contact.road || source.contact.time_den <= 0 ||
      source.contact.time_num < 0 ||
      source.contact.time_num > source.contact.time_den)
    return {false, {}, common_failure("FieldContactSourceRequired").error};
  const auto &first = source.contact.road->first;
  const auto *edge = travel.roads.find(first.trace.edge_id);
  if (!edge || edge->duration_ticks <= 0)
    return {
        false,
        {},
        common_failure("InvalidFieldBattleRoute", first.trace.edge_id).error};
  auto selected = read_field_road_terrain(travel, catalog, edge->id);
  if (!selected.ok)
    return {false, {}, selected.error};
  auto other = read_field_road_terrain(
      travel, catalog, source.contact.road->second.trace.edge_id);
  if (!other.ok)
    return {false, {}, other.error};
  if (selected.value != other.value)
    return {
        false, {}, common_failure("TravelRouteMapMismatch", edge->id).error};
  const auto &terrain = *selected.value;
  auto geometry =
      read_field_road_geometry(travel, catalog, terrain.metadata, edge->id);
  if (!geometry.ok)
    return {false, {}, geometry.error};
  using Wide = __int128_t;
  const Wide progress =
      Wide(first.trace.start) * source.contact.time_den +
      Wide(first.trace.end - first.trace.start) * source.contact.time_num;
  const Integer at = static_cast<Integer>(
      Wide(field_road_length_dm(geometry.value)) * progress /
      (Wide(source.contact.time_den) * edge->duration_ticks * 100));
  auto generated =
      generate_battle_field(terrain, geometry.value, at, parameters, version);
  if (!generated.ok)
    return {false, {}, generated.error};
  return generated;
}
Outcome<FieldSessionPreparation> prepare_field_road_session(
    const CommonStore &common, const MilitaryState &military,
    const MilitaryCatalog &catalog, const TravelState &travel_state,
    const TravelCatalog &travel, const FieldBattleCatalog &field_catalog,
    const FieldRoadContact &source, const StableId &battle_id,
    const StableId &contact_id, const StableId &player, Integer battle_seed,
    const MilitaryFormationSourceRead *exact, const WorldMoment *actual_now) {
  if (source.source_tick != common.tick() || !source.contact.road ||
      battle_id.empty() || contact_id.empty())
    return {false,
            {},
            common_failure("FieldContactSourceRequired", contact_id).error};
  const auto stepped = validate_field_road_contact_after_step(
      common, military, catalog, travel_state, travel, source);
  if (!stepped.ok)
    return {false, {}, stepped.error};
  const auto &first = source.contact.road->first,
             &second = source.contact.road->second;
  const auto *a = army(military, source.army_ids[0]),
             *b = army(military, source.army_ids[1]);
  if (!a || !b || a == b || a->commander_actor_id != first.actor_id ||
      b->commander_actor_id != second.actor_id ||
      !military_armies_hostile(military, catalog, *a, *b))
    return {false,
            {},
            common_failure("FieldContactArmyMismatch", contact_id).error};
  const auto *edge = travel.roads.find(first.trace.edge_id),
             *other = travel.roads.find(second.trace.edge_id);
  if (!edge || !other || edge->duration_ticks <= 0 ||
      other->duration_ticks <= 0)
    return {
        false, {}, common_failure("InvalidFieldBattleRoute", contact_id).error};
  // 与原纯交点叶再核一次两条保存轨迹；不能用独立填写的分子、端点冒接触。
  const auto actual =
      original_road_contact_intersection(travel, first.trace, second.trace);
  if (!actual || actual->a != source.contact.a ||
      actual->b != source.contact.b ||
      actual->time_num != source.contact.time_num ||
      actual->time_den != source.contact.time_den || !actual->road ||
      actual->road->first.anchor.progress != first.anchor.progress ||
      actual->road->second.anchor.progress != second.anchor.progress)
    return {false,
            {},
            common_failure("FieldContactSourceMismatch", contact_id).error};
  const auto time = field_contact_battle_time(
      source.source_tick, source.contact, field_catalog.parameters);
  if (!time.ok)
    return {false, {}, time.error};
  auto generated = regenerate_field_road_battle(travel, field_catalog, source);
  if (!generated.ok)
    return {false, {}, generated.error};
  using Wide = __int128_t;
  const auto da = direction(first.trace, *edge, *edge),
             db = direction(second.trace, *other, *edge);
  Integer side_a = da < 0 ? 1 : 0;
  if (da == 0)
    side_a = db > 0 ? 1 : 0;
  if (da != 0 && da == db) {
    const Wide pa = Wide(projected_start(first.trace, *edge, *edge)) *
                    other->duration_ticks;
    const Wide pb = Wide(projected_start(second.trace, *other, *edge)) *
                    edge->duration_ticks;
    side_a = (da > 0 ? pa < pb : pa > pb) ? 0 : 1;
  }
  // 逆作者方向追及时 side 0
  // 仍是追者，实际来路却在折线终点；不能把逻辑侧号当几何端号。
  const bool reverse_side_ends = da < 0 && da == db;
  auto fa = actual_now
                ? read_field_army_formations(common, military, catalog, a->id,
                                             side_a, player, *actual_now, exact)
                : read_field_army_formations(common, military, catalog, a->id,
                                             side_a, player, exact);
  if (!fa.ok)
    return {false, {}, fa.error};
  auto fb =
      actual_now
          ? read_field_army_formations(common, military, catalog, b->id,
                                       1 - side_a, player, *actual_now, exact)
          : read_field_army_formations(common, military, catalog, b->id,
                                       1 - side_a, player, exact);
  if (!fb.ok)
    return {false, {}, fb.error};
  if (fa.value.size() > static_cast<std::size_t>(
                            field_catalog.parameters.max_formations_per_side) ||
      fb.value.size() > static_cast<std::size_t>(
                            field_catalog.parameters.max_formations_per_side))
    return {false,
            {},
            common_failure("InvalidFieldFormationCount", battle_id).error};
  FieldSessionPreparation out;
  out.field = std::move(generated.value);
  auto &session = out.session;
  session.battle_id = battle_id;
  session.created_tick = source.source_tick + 1;
  session.now = time.value;
  session.anchor = out.field.anchor;
  session.field_hash = out.field.field_hash;
  for (const auto &[actual_army, side] :
       {std::pair{a, side_a}, std::pair{b, 1 - side_a}}) {
    auto &record = session.sides[side];
    record.side = side;
    record.faction_id = actual_army->organization_id;
    record.commander_actor_id = actual_army->commander_actor_id;
    if (record.commander_actor_id == player)
      record.controller = FieldPlayerController{player};
    const auto end = reverse_side_ends ? 1 - side : side;
    record.deploy_zone = out.field.sides[end].deploy_cells;
    record.exits = out.field.sides[end].exit_cells;
    session.ai[side].side = side;
    session.ai[side].last_eval = session.now;
  }
  for (auto *formations : {&fa.value, &fb.value})
    for (auto &formation : *formations) {
      formation.joined_at = session.now;
      const auto id = formation.formation_id;
      if (!session.formations.emplace(id, std::move(formation)).second)
        return {false, {}, common_failure("FieldFormationRepeated", id).error};
    }
  const auto id_hash = godot_string_hash_utf8(battle_id),
             source_hash = godot_string_hash_utf8(contact_id);
  if (!id_hash.ok || !source_hash.ok)
    return {false, {}, id_hash.ok ? source_hash.error : id_hash.error};
  const Integer seed = battle_seed ^ id_hash.value ^ source_hash.value;
  session.rng_seed = std::to_string(seed);
  session.rng_state = std::to_string(godot_pcg32_seed_state(seed));
  return {true, std::move(out), {}};
}
} // namespace hanmo::resident
