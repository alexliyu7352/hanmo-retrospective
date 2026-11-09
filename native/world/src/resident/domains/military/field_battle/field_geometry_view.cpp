// 真实 Session 与其原 Contact 是唯一来源；查询不改当前位置、时钟、RNG 或知情。
#include "field_geometry_view.hpp"
#include "field_contact_origin.hpp"
#include "resident/storage/common_store.hpp"

namespace hanmo::resident {
static Outcome<FieldBattleGeometryView> read_geometry_projection(
    const CommonStore &common, const TravelCatalog &travel,
    const FieldBattleCatalog &catalog, const FieldBattleWorldState &world,
    const StableId &battle_id, const StableId &observer_actor_id,
    const StableId &known_field_hash,
    const FieldBattleAbilityReadContext *abilities) {
  if (!common.actor(observer_actor_id))
    return {false, {}, common_failure("UnknownActor", observer_actor_id).error};
  const auto found = world.sessions.find(battle_id);
  if (found == world.sessions.end())
    return {false, {}, common_failure("FieldBattleNotFound", battle_id).error};
  const FieldBattleContactRecord *contact = nullptr;
  for (const auto &[id, row] : world.contacts)
    if (row.battle_id == battle_id) {
      if (contact)
        return {false,
                {},
                common_failure("InvalidFieldBattleState", battle_id).error};
      const auto *head = common.state().facts.find(id);
      if (row.contact_id != id || !head || head->domain != "military" ||
          head->kind != "field_battle_contact")
        return {
            false, {}, common_failure("FieldContactSourceRequired", id).error};
      contact = &row;
    }
  const auto *session_head = common.state().facts.find(battle_id);
  if (!session_head || session_head->domain != "military" ||
      session_head->kind != "field_battle_session")
    return {false,
            {},
            common_failure("FieldContactSourceRequired", battle_id).error};
  if (!contact)
    return {false,
            {},
            common_failure("FieldContactSourceRequired", battle_id).error};
  auto generated = regenerate_field_contact_battle(
      travel, catalog, contact->source,
      abilities ? abilities->exact_contacts : nullptr);
  if (!generated.ok)
    return {false, {}, generated.error};
  const auto &session = found->second;
  if (generated.value.field_hash != session.field_hash)
    return {false,
            {},
            common_failure("InvalidFieldBattleState", battle_id + "/field_hash")
                .error};
  FieldBattleGeometryView result;
  result.battle_id = session.battle_id;
  result.observer_actor_id = observer_actor_id;
  result.field_hash = session.field_hash;
  result.revision = common.revision();
  result.segment = session.segment;
  result.now = session.now;
  result.state = session.state;
  result.anchor = session.anchor;
  result.length_m = generated.value.parameters.length_m;
  result.width_m = generated.value.parameters.width_m;
  result.cell_m = generated.value.parameters.cell_m;
  result.sides = session.sides;
  // 本方 Council 已含分级后的敌情，不附对方完整军议或未获知计划。
  for (const auto &[army_id, binding] : world.army_bindings) {
    if (binding.battle_id != battle_id)
      continue;
    for (const auto &member : binding.members)
      if (member.actor_id == observer_actor_id) {
        const auto f = session.formations.find(member.formation_id);
        if (f == session.formations.end() || result.observer_side)
          return {false,
                  {},
                  common_failure("FieldBattleBindingInvalid", observer_actor_id)
                      .error};
        result.observer_side = f->second.side;
        result.council = session.councils[f->second.side];
      }
  }

  if (known_field_hash != session.field_hash)
    result.cells = std::move(generated.value.cells);
  return {true, std::move(result), {}};
}
Outcome<FieldBattleGeometryView> read_field_battle_geometry(
    const CommonStore &common, const MilitaryState &military,
    const TravelCatalog &travel, const FieldBattleCatalog &catalog,
    const FieldBattleWorldState &world, const StableId &battle,
    const StableId &observer, const StableId &known_hash,
    const FieldBattleAbilityReadContext *abilities) {
  auto valid = validate_field_battle_world(common, military, travel, catalog,
                                           world, abilities);
  if (!valid.ok)
    return {false, {}, valid.error};
  return read_geometry_projection(common, travel, catalog, world, battle,
                                  observer, known_hash, abilities);
}
Outcome<FieldBattleGeometryView> read_field_battle_candidate_geometry(
    const CommonStore &common, const MilitaryState &,
    const TravelCatalog &travel, const FieldBattleCatalog &catalog,
    const FieldBattleWorldState &world, const StableId &battle,
    const StableId &observer, const StableId &known_hash,
    const FieldBattleAbilityReadContext *abilities) {
  return read_geometry_projection(common, travel, catalog, world, battle,
                                  observer, known_hash, abilities);
}
} // namespace hanmo::resident
