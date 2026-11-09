#include "resident/domains/military/military_model.hpp"
// 沿原道路动作与有理相遇叶建立军团候选；只读，不变动军团、世界时钟或名册。
#include "field_contact.hpp"
#include "resident/domains/military/battle_world.hpp"
#include "resident/domains/military/military_contact.hpp"
#include "resident/domains/travel/movement_contact_source_equal.hpp"
#include "resident/domains/travel/road_motion.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
// 无 fit 部队或全部失去士气的军团不作为新的战斗候选。
bool has_available_formation(const MilitaryState &military,
                             const ArmyRecord &army) {
  for (const auto &id : army.unit_ids)
    for (const auto &unit : military.military_units)
      if (unit.id == id && unit.army_id == army.id && !unit.fit.empty() &&
          unit.morale > 0)
        return true;
  return false;
}
// 既有遭遇已经标记的原行动对不重复创建接触。
bool already_met(const RoadTravelMotion &a, const RoadTravelMotion &b) {
  return std::find(a.params.met_actions.begin(), a.params.met_actions.end(),
                   b.action_id) != a.params.met_actions.end() ||
         std::find(b.params.met_actions.begin(), b.params.met_actions.end(),
                   a.action_id) != b.params.met_actions.end();
}
} // namespace
Outcome<std::vector<FieldRoadContact>> probe_field_road_contacts(
    const CommonRead &common, const MilitaryState &military,
    const MilitaryCatalog &catalog, const TravelState &travel,
    const TravelCatalog &geography, const TravelServices &services,
    const std::vector<StableId> &held_armies,
    const std::vector<StableId> &held_actors) {
  struct Eligible {
    const ArmyRecord *army;
    RoadTravelMotion motion;
  };
  std::vector<Eligible> eligible;
  std::set<StableId> commanders;
  for (const auto &army : military.armies) {
    if (army.status != "active" || services.army_embarked(army.id) ||
        std::find(held_armies.begin(), held_armies.end(), army.id) !=
            held_armies.end() ||
        military_actor_in_battle(military, army.commander_actor_id) ||
        !has_available_formation(military, army))
      continue;
    const auto *actor = common.actor(army.commander_actor_id);
    if (!actor || !actor->alive || !actor->available ||
        !std::holds_alternative<OnRouteLocation>(actor->location))
      continue;
    const auto *action = common.action(actor->action_id);
    if (!action || action->kind != "travel" || action->actor_id != actor->id)
      continue;
    const auto *params = travel.road_actions.find(action->id);
    if (!params || !params->encounter_id.empty())
      continue;
    const auto active = services.active_army(actor->id);
    if (!active.ok)
      return {false, {}, active.error};
    if (!active.value ||
        active.value->commander_actor_id != army.commander_actor_id ||
        active.value->wallet_id != army.wallet_id ||
        active.value->inventory_id != army.inventory_id)
      return {
          false, {}, common_failure("FieldArmySourceMismatch", army.id).error};
    if (!commanders.insert(actor->id).second)
      return {false,
              {},
              common_failure("FieldArmyCommanderRepeated", actor->id).error};
    auto trace = original_road_travel_motion(common, geography, services,
                                             *action, *params, held_actors);
    if (!trace.ok)
      return {false, {}, trace.error};
    if (trace.value)
      eligible.push_back({&army, std::move(*trace.value)});
  }
  std::sort(eligible.begin(), eligible.end(), [](const auto &a, const auto &b) {
    return a.army->id < b.army->id;
  });
  std::vector<FieldRoadContact> candidates;
  for (std::size_t a = 0; a < eligible.size(); ++a)
    for (std::size_t b = a + 1; b < eligible.size(); ++b) {
      const auto &first = eligible[a], &second = eligible[b];
      if (!military_armies_hostile(military, catalog, *first.army,
                                   *second.army) ||
          already_met(first.motion, second.motion))
        continue;
      auto contact = original_road_contact_intersection(geography, first.motion,
                                                        second.motion);
      if (!contact)
        continue;
      const bool same = contact->a == first.motion.actor_id;
      candidates.push_back({common.tick(),
                            {same ? first.army->id : second.army->id,
                             same ? second.army->id : first.army->id},
                            std::move(*contact)});
    }
  std::sort(candidates.begin(), candidates.end(),
            [](const auto &a, const auto &b) {
              return contact_before(a.contact, b.contact);
            });
  std::vector<FieldRoadContact> result;
  std::set<StableId> used;
  for (auto &candidate : candidates) {
    if (used.contains(candidate.army_ids[0]) ||
        used.contains(candidate.army_ids[1]))
      continue;
    used.insert(candidate.army_ids[0]);
    used.insert(candidate.army_ids[1]);
    result.push_back(std::move(candidate));
  }
  return {true, std::move(result), {}};
}
Status validate_field_road_contact_after_step(const CommonRead &common,
                                              const MilitaryState &military,
                                              const MilitaryCatalog &catalog,
                                              const TravelState &travel,
                                              const TravelCatalog &geography,
                                              const FieldRoadContact &source) {
  if (source.source_tick != common.tick() || !source.contact.road ||
      source.army_ids[0] == source.army_ids[1])
    return common_failure("FieldContactSourceRequired");
  std::array<const ArmyRecord *, 2> armies{};
  for (std::size_t side = 0; side < armies.size(); ++side)
    for (const auto &army : military.armies)
      if (army.id == source.army_ids[side]) {
        if (armies[side])
          return common_failure("FieldContactArmyMismatch", army.id);
        armies[side] = &army;
      }
  if (!armies[0] || !armies[1] ||
      !military_armies_hostile(military, catalog, *armies[0], *armies[1]))
    return common_failure("FieldContactArmyMismatch");
  const std::array<const RoadContactEndpoint *, 2> endpoints{
      &source.contact.road->first, &source.contact.road->second};
  const auto actual = original_road_contact_intersection(
      geography, endpoints[0]->trace, endpoints[1]->trace);
  if (!actual || !movement_contact_detail::same(*actual, source.contact))
    return common_failure("FieldContactSourceMismatch");
  for (std::size_t side = 0; side < armies.size(); ++side) {
    const auto &army = *armies[side];
    const auto &endpoint = *endpoints[side];
    const auto &trace = endpoint.trace;
    const auto *actor = common.actor(endpoint.actor_id);
    const auto *action = common.action(trace.action_id);
    const auto *params = travel.road_actions.find(trace.action_id);
    if (army.status != "active" || !has_available_formation(military, army) ||
        army.commander_actor_id != endpoint.actor_id || !actor ||
        !actor->alive || !actor->available || !action ||
        actor->action_id != action->id || action->actor_id != actor->id ||
        action->kind != "travel" || !params ||
        military_actor_in_battle(military, actor->id))
      return common_failure("FieldContactArmyUnavailable", army.id);
    if (army.last_supply_tick != common.tick() + 1)
      return common_failure("MilitaryContactHalfDayRequired", army.id);
    auto expected_params = trace.params;
    Location expected = OnRouteLocation{trace.edge_id, trace.origin, trace.end};
    if (trace.moving) {
      const auto finished =
          finish_road_leg(geography, common.tick(), expected_params, expected);
      if (!finished.ok)
        return {false, {}, finished.error};
    }
    // 原步可正好到城门或转下一边；再由真正接触截断器恢复相遇锚，不假定仍在旧边。
    bool same_location = false;
    if (const auto *road = std::get_if<OnRouteLocation>(&expected)) {
      const auto *current = std::get_if<OnRouteLocation>(&actor->location);
      same_location = current && movement_contact_detail::same(*current, *road);
    } else if (const auto *settlement =
                   std::get_if<SettlementLocation>(&expected)) {
      const auto *current = std::get_if<SettlementLocation>(&actor->location);
      same_location = current &&
                      current->settlement_id == settlement->settlement_id &&
                      current->place_id == settlement->place_id;
    }
    if (!same_location ||
        !movement_contact_detail::same(*params, expected_params))
      return common_failure("FieldTravelStepRequired", trace.action_id);
  }
  return common_success();
}
} // namespace hanmo::resident
