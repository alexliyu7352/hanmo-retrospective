#include "field_battle/field_segment_settlement.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "military_combat_conditions_source.hpp"
#include "military_command_shock_source.hpp"
#include "military_field_terrain.hpp"
#include "military_field_terrain_source.hpp"
#include "military_recovery_release.hpp"
#include "military_rout_history.hpp"
#include "military_rout_status.hpp"
#include "military_siege_engineering_access.hpp"
#include "military_supply_attrition_battle.hpp"
// 原MilitaryCombat/Assets结算边界：真实军团授权、逐轮收据、俘虏看管及真人后果不另设世界。
#include "battle_world.hpp"
#include "military_actor_condition_write.hpp"
#include "military_battle_lifecycle.hpp"
#include "military_commander.hpp"
#include "military_commander_history.hpp"
#include "military_conditions.hpp"
#include "military_equal.hpp"
#include "military_field_command_source.hpp"
#include "military_integration_assets.hpp"
#include "military_roster_casualties.hpp"
#include "field_battle/field_journey_termination.hpp"
#include "field_battle/field_validation.hpp"
#include "military_round_rng.hpp"
#include "resident/model/common_operations.hpp"
#include "resident/model/common_rng.hpp"
#include "resident/model/common_string_hash.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <set>
namespace hanmo::resident {
namespace {
template <class T> T *find(std::vector<T> &rows, const StableId &id) {
  for (auto &r : rows)
    if (r.id == id)
      return &r;
  return nullptr;
}
template <class T>
const T *find(const std::vector<T> &rows, const StableId &id) {
  for (auto &r : rows)
    if (r.id == id)
      return &r;
  return nullptr;
}
template <class T>
const T *pair(const std::vector<std::pair<StableId, T>> &rows,
              const StableId &id) {
  for (auto &r : rows)
    if (r.first == id)
      return &r.second;
  return nullptr;
}
bool has(const std::vector<StableId> &a, const StableId &id) {
  return std::find(a.begin(), a.end(), id) != a.end();
}
bool unique(const std::vector<StableId> &a) {
  for (auto &id : a)
    if (id.empty())
      return false;
  return std::set<StableId>(a.begin(), a.end()).size() == a.size();
}
std::vector<StableId> sorted(std::vector<StableId> a) {
  std::sort(a.begin(), a.end());
  return a;
}
std::vector<StableId> unit_ids(const MilitaryBattleRecord &b) {
  std::vector<StableId> ids;
  for (auto &u : b.units)
    ids.push_back(u.unit_id);
  return sorted(ids);
}
template <class T>
std::vector<StableId> keys(const std::vector<std::pair<StableId, T>> &a) {
  std::vector<StableId> out;
  for (auto &p : a)
    out.push_back(p.first);
  return sorted(out);
}
const BattleUnit *unit(const MilitaryBattleRecord &b, const StableId &id) {
  for (auto &u : b.units)
    if (u.unit_id == id)
      return &u;
  return nullptr;
}
Status fail(const StableId &code, const StableId &path = {}) {
  return common_failure(code, path);
}
template <class T>
Outcome<T> error(const StableId &code, const StableId &path = {}) {
  return {false, {}, fail(code, path).error};
}
// 原MilitaryCombat失败保留battle_id及path，其他Assets/行旅失败不借用此形状。
Status combat_fail(const StableId &code, const StableId &path = {}) {
  auto out = common_failure(code, path);
  out.error.battle_id = path;
  return out;
}
template <class T>
Outcome<T> combat_error(const StableId &code, const StableId &path = {}) {
  return {false, {}, combat_fail(code, path).error};
}
// 接敌来源只接受原野/城内位置，比较完整位置值而非屏幕距离。
bool same_location(const Location &a, const Location &b) {
  if (a.index() != b.index())
    return false;
  if (auto x = std::get_if<SettlementLocation>(&a)) {
    auto &y = std::get<SettlementLocation>(b);
    return x->settlement_id == y.settlement_id && x->place_id == y.place_id;
  }
  if (auto x = std::get_if<WildernessLocation>(&a)) {
    auto &y = std::get<WildernessLocation>(b);
    return x->map_id == y.map_id && x->origin == y.origin &&
           x->point.x == y.point.x && x->point.y == y.point.y;
  }
  return false;
}
bool result_equal(const std::optional<MilitaryBattleResult> &a,
                  const std::optional<MilitaryBattleResult> &b) {
  return a.has_value() == b.has_value() &&
         (!a || (a->winner_side == b->winner_side && a->reason == b->reason));
}
bool roster_equal(const MilitaryUnitRecord &a, const BattleUnit &b) {
  return a.fit == b.fit && a.wounded == b.wounded && a.dead == b.dead &&
         a.captured == b.captured && a.deserted == b.deserted;
}
StableId group(const BattleUnit &u, const StableId &p) {
  if (has(u.fit, p))
    return "fit";
  if (has(u.wounded, p))
    return "wounded";
  if (has(u.dead, p))
    return "dead";
  if (has(u.captured, p))
    return "captured";
  if (has(u.deserted, p))
    return "deserted";
  return {};
}
// 军级指挥必须由当前公职和本军组织共同授权；否则真实部将仅控制自己的一部。
Status authority(const CommonRead &c, const MilitaryState &s,
                 const MilitaryCatalog &cat, MilitaryBattleRecord &b,
                 const StableId &controller) {
  b.controller_actor_id = controller;
  b.controller_side = 0;
  b.command_scope = "unit";
  b.command_army_id.clear();
  b.command_unit_ids.clear();
  b.authority_office_id.clear();
  if (controller.empty())
    return common_success();
  auto actor = c.actor(controller);
  if (!actor || !actor->alive || !actor->available)
    return combat_fail("MilitaryCommanderUnavailable");
  for (auto &id : unit_ids(b)) {
    auto u = unit(b, id);
    auto army = find(s.armies, u->army_id);
    auto campaign = army ? find(s.campaigns, army->order_id) : nullptr;
    if (!army || !campaign)
      return combat_fail("MilitaryArmyFormationSourceMissing", id);
    if (army->commander_actor_id != controller)
      continue;
    if (const auto *field = current_military_field_army_assignment(
            s, cat, army->id, controller)) {
      b.controller_side = u->side;
      b.command_scope = "army";
      b.command_army_id = army->id;
      b.authority_office_id = field->office_id;
      for (const auto &owned : unit_ids(b))
        if (unit(b, owned)->army_id == army->id)
          b.command_unit_ids.push_back(owned);
      return common_success();
    }
    auto office = find(s.offices, campaign->office_id);
    auto definition = find(cat.offices, campaign->office_id);
    if (office && definition && office->enabled &&
        office->holder_actor_id == controller &&
        definition->command_scope == "army" &&
        definition->organization_id == army->organization_id) {
      bool valid = true;
      for (auto &h : s.strongholds) {
        auto hold = find(cat.strongholds, h.id);
        if (hold && hold->garrison_id == definition->garrison_id &&
            h.owner_organization_id != definition->organization_id)
          valid = false;
      }
      if (!valid)
        continue;
      b.controller_side = u->side;
      b.command_scope = "army";
      b.command_army_id = army->id;
      b.authority_office_id = campaign->office_id;
      for (auto &owned : unit_ids(b))
        if (unit(b, owned)->army_id == army->id)
          b.command_unit_ids.push_back(owned);
      return common_success();
    }
  }
  for (auto &id : unit_ids(b))
    if (unit(b, id)->commander_actor_id == controller) {
      b.controller_side = unit(b, id)->side;
      b.command_unit_ids = {id};
      return common_success();
    }
  return combat_fail("MilitaryCommanderNotPresent");
}
bool same_authority(const MilitaryBattleRecord &a,
                    const MilitaryBattleRecord &b) {
  return a.controller_actor_id == b.controller_actor_id &&
         a.controller_side == b.controller_side &&
         a.command_scope == b.command_scope &&
         a.command_army_id == b.command_army_id &&
         a.command_unit_ids == b.command_unit_ids &&
         a.authority_office_id == b.authority_office_id;
}
StableId source_army(const MilitaryState &s, const MilitaryContactFact &f,
                     const StableId &id) {
  for (auto &p : f.army_order_ids) {
    auto army = find(s.armies, p.first);
    auto fact = army ? find(s.action_facts, army->formation_source) : nullptr;
    auto terms = fact ? std::get_if<ArmyFormationTerms>(&fact->terms) : nullptr;
    if (terms && has(military_army_units_at(s, army->id, f.tick, f.id), id))
      return army->id;
  }
  return {};
}
// 永久接敌来源和实际编组逐项对应，历史战场不要求归营后的单位永远留在旧军团。
Status contact_valid(const CommonRead &c, const MilitaryState &s,
                     const MilitaryCatalog &cat, const MilitaryContactFact &f,
                     bool live) {
  if (f.tick < 0 || f.tick > c.tick() + 1)
    return combat_fail("MilitaryContactSourceRequired");
  if (!unique(f.unit_ids) || f.unit_ids.size() < 2 ||
      !unique(f.objective_unit_ids))
    return combat_fail("InvalidMilitaryContact");
  auto board = find(cat.battle_catalog.battlefields, f.battlefield_id);
  auto goal = find(cat.battle_catalog.objectives, f.objective_id);
  if (!board || !goal)
    return combat_fail("UnknownMilitaryDefinition");
  if (board->objective_id != f.objective_id)
    return combat_fail("MilitaryBattlefieldObjectiveMismatch");
  if (live && f.battle_id)
    return combat_fail("MilitaryContactAlreadyUsed", *f.battle_id);
  std::vector<StableId> armies;
  std::set<Integer> sides;
  for (auto &id : f.unit_ids) {
    auto u = find(s.military_units, id);
    if (!u || !find(cat.battle_catalog.troop_types, u->troop_type_id))
      return combat_fail("UnknownMilitaryUnit", id);
    if (!c.actor(u->commander_actor_id) || !unique(u->member_ids))
      return combat_fail("MilitaryActualRosterMissing", id);
    auto army_id = live ? u->army_id : source_army(s, f, id);
    auto army = find(s.armies, army_id);
    auto side = pair(f.side_by_army, army_id);
    if (!army || !side || *side < 0 || *side > 1)
      return combat_fail("MilitaryContactArmyMismatch", id);
    if (!has(armies, army_id))
      armies.push_back(army_id);
    sides.insert(*side);
    auto campaign = find(s.campaigns, army->order_id);
    auto form = find(s.action_facts, army->formation_source);
    auto terms = form ? std::get_if<ArmyFormationTerms>(&form->terms) : nullptr;
    auto order = pair(f.army_order_ids, army_id);
    if (!campaign || !order || *order != campaign->id ||
        campaign->army_id != army_id || !form || form->command != "army_form" ||
        form->phase != "completed" || !form->success || !form->result ||
        form->result->army_id != army_id || !terms ||
        !has(military_army_units_at(s, army->id, f.tick, f.id), id))
      return combat_fail("MilitaryArmyFormationSourceMissing", id);
    if (live) {
      if (army->status != "active" ||
          !has({"mustered", "marching", "operating", "returning"},
               campaign->status) ||
          !has(army->unit_ids, id))
        return combat_fail("MilitaryArmyUnavailable", id);
      auto p = c.actor(army->commander_actor_id);
      auto location = pair(f.army_locations, army_id);
      if (!p || !location || !same_location(p->location, *location))
        return combat_fail("MilitaryContactPositionChanged", army_id);
    }
  }
  if (sorted(armies) != keys(f.side_by_army) ||
      sorted(armies) != keys(f.army_order_ids) ||
      sorted(armies) != keys(f.army_locations) || sides.size() != 2)
    return combat_fail("MilitaryContactSidesMismatch");
  for (auto &p : f.army_locations)
    if (!std::holds_alternative<SettlementLocation>(p.second) &&
        !std::holds_alternative<WildernessLocation>(p.second))
      return combat_fail("MilitaryContactLocationRequired", p.first);
  for (auto &a : armies)
    for (auto &b : armies)
      if (*pair(f.side_by_army, a) != *pair(f.side_by_army, b) &&
          find(s.armies, a)->organization_id ==
              find(s.armies, b)->organization_id)
        return combat_fail("MilitaryContactOrganizationConflict");
  if (f.siege_id || f.node_id) {
    auto siege = f.siege_id ? find(s.sieges, *f.siege_id) : nullptr;
    if (!siege || !f.node_id)
      return combat_fail("MilitarySiegeSourceMissing");
    if (live) {
      auto pending = pair(siege->pending_nodes, *f.node_id);
      if (!pending || pending->source_id != f.id ||
          pending->battlefield_id != f.battlefield_id ||
          sorted(pending->unit_ids) != sorted(f.unit_ids))
        return combat_fail("MilitarySiegeSourceMismatch");
    }
  }
  return validate_military_field_contact_source(s, cat, f);
}
// 按兵种可通行入口做增广匹配，不把第一个单位固定到后续唯一可用格。
// 本轮只推进会话保存的PCG state，旧seed只校验，不重新播种或触及世界battle流。
Outcome<Integer> random_draw(StableId &seed, StableId &state, Integer low,
                             Integer high) {
  return military_round_random_draw(seed, state, low, high);
}
// 先核全部损失不可逆与俘虏来源，再写真实单位及一份永久回执；外围world候选负责失败回滚。
Outcome<MilitaryCasualtyFact>
settle_casualties(CommonWrite &c, MilitaryState &s, const MilitaryCatalog &cat,
                  const MilitaryBattleFact &fact,
                  const MilitaryBattleRecord &battle,
                  const std::vector<std::pair<StableId, StableId>> &captors) {
  // 旧来源、幂等与将领读义原样留在原入口；通用叶不接受一个假 BattleRecord。
  for (auto &f : s.casualty_facts)
    if (f.battle_id == fact.id)
      return {true, f, {}};
  if (!unique(fact.unit_ids) || !unique(fact.army_ids) ||
      unit_ids(battle) != sorted(fact.unit_ids))
    return error<MilitaryCasualtyFact>("MilitaryBattleRosterMismatch");
  auto original = find(s.battles, fact.session_id);
  if (!original)
    return error<MilitaryCasualtyFact>("MilitaryCommanderRoundSourceRequired",
                                       fact.id);
  auto officers = military_commander_actor_changes(c, s, *original, battle);
  if (!officers.ok)
    return {false, {}, officers.error};
  std::vector<OriginalMilitaryRosterFinalView> finals;
  finals.reserve(fact.unit_ids.size());
  for (const auto &id : fact.unit_ids) {
    const auto &u = *unit(battle, id);
    finals.push_back({u.unit_id, u.army_id, u.commander_actor_id,
                      u.troop_type_id, u.member_ids, u.fit, u.wounded, u.dead,
                      u.captured, u.deserted, u.morale});
  }
  return apply_original_military_roster_casualties(
      c, s, fact.id, fact.tick, fact.army_ids, finals,
      std::move(officers.value), captors,
      military_rout_rule_in_use(battle, cat.battle_catalog));
}
Outcome<MilitaryBattleStep> settle(CommonWrite &c, MilitaryState &s,
                                   const MilitaryCatalog &cat,
                                   MilitaryBattleRecord &b, Integer tick,
                                   Integer start_tick) {
  bool final = b.phase == "result";
  StableId id = b.id;
  if (!final) {
    auto allocated = c.next_id("battle_round");
    if (!allocated.ok)
      return {false, {}, allocated.error};
    id = allocated.value;
  }
  if (find(s.battle_facts, id))
    return combat_error<MilitaryBattleStep>("MilitaryResultAlreadyApplied",
                                            b.id);
  std::vector<std::pair<StableId, StableId>> captors;
  for (auto &e : b.last_events)
    if (e.kind == "rout" && e.captured_ids && e.captor_army_id)
      for (auto &person : *e.captured_ids) {
        auto u = find(s.military_units, e.unit_id);
        if (u && !has(u->captured, person))
          captors.emplace_back(person, *e.captor_army_id);
      }
  MilitaryBattleFact f;
  f.id = id;
  f.tick = tick;
  f.start_tick = start_tick;
  f.session_id = b.id;
  f.round = final ? b.round : b.round - 1;
  f.army_ids = b.army_ids;
  f.unit_ids = unit_ids(b);
  f.result = b.result;
  f.source_id = final ? b.source_id : id;
  f.contact_source_id = b.source_id;
  f.winner_side = b.result ? b.result->winner_side : -1;
  f.final = final;
  auto appended = c.append_fact({id, "military_battle", "military"});
  if (!appended.ok)
    return {false, {}, appended.error};
  s.battle_facts.push_back(f);
  auto receipt = settle_casualties(c, s, cat, f, b, captors);
  if (!receipt.ok)
    return {false, {}, receipt.error};
  b.round_receipt_ids.push_back(receipt.value.id);
  if (final) {
    b.applied = true;
    b.completed_tick = tick;
    b.settlement_receipt_id = receipt.value.id;
  }
  MilitaryBattleStep result;
  result.round_fact_ids = {id};
  result.casualty_source_ids = {receipt.value.id};
  result.actor_changes = std::move(receipt.value.actor_changes);
  if (final)
    result.completed_battle_ids = {b.id};
  return {true, std::move(result), {}};
}
} // namespace
// 原authority是唯一裁定实现；只将本次参与单位作为只读控制范围输入。
Outcome<MilitaryBattleControl> military_battle_control(
    const CommonRead &c, const MilitaryState &s, const MilitaryCatalog &cat,
    const std::vector<BattleUnit> &units, const StableId &actor) {
  MilitaryBattleRecord projection;
  projection.units = units;
  auto checked = authority(c, s, cat, projection, actor);
  if (!checked.ok)
    return {false, {}, checked.error};
  return {true,
          {projection.controller_actor_id, projection.command_scope,
           projection.command_army_id, projection.authority_office_id,
           projection.controller_side, projection.command_unit_ids},
          {}};
}
// 请求仅由原军级权源决定可响应部队；本人军旅身份及原战场权限逐项复核。
Outcome<MilitaryRequestAuthority>
military_request_authority(const CommonRead &c, const MilitaryState &s,
                           const MilitaryCatalog &cat,
                           const MilitaryAccess &access, const StableId &actor,
                           const MilitaryBattleRequest &request) {
  auto battle = find(s.battles, request.battle_id);
  if (!battle)
    return combat_error<MilitaryRequestAuthority>("UnknownMilitaryBattle");
  if (actor != access.player_actor_id || actor != battle->controller_actor_id)
    return combat_error<MilitaryRequestAuthority>(
        "MilitaryCommanderUnauthorized", battle->id);
  auto player_scope = *battle;
  auto checked = authority(c, s, cat, player_scope, actor);
  if (!checked.ok)
    return {false, {}, checked.error};
  if (!same_authority(*battle, player_scope))
    return combat_error<MilitaryRequestAuthority>("MilitaryAuthorityChanged",
                                                  battle->id);
  if (player_scope.command_scope != "unit")
    return combat_error<MilitaryRequestAuthority>(
        "MilitaryRequestRequiresSubordinate", battle->id);
  auto own = unit(*battle, request.unit_id);
  auto actual = find(s.military_units, request.unit_id);
  if (!own || !actual || own->commander_actor_id != actor ||
      !has(player_scope.command_unit_ids, request.unit_id) ||
      actual->army_id != own->army_id || actual->commander_actor_id != actor)
    return combat_error<MilitaryRequestAuthority>("MilitaryUnitUnauthorized",
                                                  request.unit_id);
  auto army = find(s.armies, own->army_id);
  auto person = c.actor(actor);
  auto attached =
      person ? std::get_if<InArmyLocation>(&person->location) : nullptr;
  if (!army || army->status != "active" || army->commander_actor_id == actor ||
      army->formation_source.empty() || !attached ||
      attached->army_id != army->id ||
      std::count(army->member_actor_ids.begin(), army->member_actor_ids.end(),
                 actor) != 1 ||
      has(army->prisoner_ids, actor))
    return combat_error<MilitaryRequestAuthority>(
        "MilitaryRequestInvalidAuthority", request.unit_id);
  auto commander_scope = *battle;
  checked = authority(c, s, cat, commander_scope, army->commander_actor_id);
  if (!checked.ok)
    return {false, {}, checked.error};
  if (commander_scope.command_scope != "army" ||
      commander_scope.command_army_id != army->id ||
      commander_scope.authority_office_id.empty())
    return combat_error<MilitaryRequestAuthority>(
        "MilitaryRequestInvalidAuthority", army->id);
  MilitaryRequestAuthority result;
  result.commander_actor_id = army->commander_actor_id;
  result.army_id = army->id;
  result.office_id = commander_scope.authority_office_id;
  result.formation_source = army->formation_source;
  if (const auto *field = current_military_field_army_assignment(
          s, cat, army->id, army->commander_actor_id))
    result.appointment_source = field->id;
  else {
    auto office = find(s.offices, result.office_id);
    if (!office || office->appointment_id.empty())
      return combat_error<MilitaryRequestAuthority>(
          "MilitaryRequestInvalidAuthority", army->id);
    result.appointment_source = office->appointment_id;
  }
  for (const auto &id : commander_scope.command_unit_ids)
    if (!has(battle->command_unit_ids, id) && pair(battle->ai_plans, id))
      result.ai_unit_ids.push_back(id);
  return {true, std::move(result), {}};
}
// 占用只来自尚未关闭的真实会战；统帅通过本军成员集合一并暂停宏观旅行。
bool military_actor_in_battle(const MilitaryState &s, const StableId &actor) {
  for (auto &b : s.battles)
    if (b.phase != "closed") {
      if (has(b.participant_actor_ids, actor))
        return true;
      for (auto &id : b.army_ids) {
        auto army = find(s.armies, id);
        if (army && has(army->member_actor_ids, actor))
          return true;
      }
    }
  return false;
}
std::vector<StableId> military_held_actors(const MilitaryState &s) {
  std::vector<StableId> held;
  for (auto &b : s.battles)
    if (b.phase != "closed")
      for (auto &id : b.army_ids) {
        auto army = find(s.armies, id);
        if (army)
          for (auto &actor : army->member_actor_ids)
            if (!has(held, actor))
              held.push_back(actor);
      }
  return held;
}
Outcome<MilitaryDamageSources>
military_damage_sources(const MilitaryState &s, const MilitaryCatalog &c,
                        const MilitaryBattleRecord &b) {
  auto source = find(s.contact_facts, b.source_id);
  if (!source)
    return combat_error<MilitaryDamageSources>("MilitaryContactSourceRequired");
  if (!source->siege_id && !source->node_id)
    return {true, {}, {}};
  if (!source->siege_id || !source->node_id)
    return error<MilitaryDamageSources>("InvalidSiegeFortificationSource",
                                        b.source_id);
  auto siege = find(s.sieges, *source->siege_id);
  if (!siege || siege->phase != "active")
    return error<MilitaryDamageSources>("SiegeFortificationNotActive",
                                        *source->siege_id);
  auto topology = find(c.siege_topologies, siege->topology_id);
  if (!topology)
    return error<MilitaryDamageSources>("UnknownSiegeFortificationTopology",
                                        siege->id);
  auto node = find(topology->nodes, *source->node_id);
  if (!node || node->battlefield_id != source->battlefield_id)
    return error<MilitaryDamageSources>("SiegeFortificationBattlefieldMismatch",
                                        *source->node_id);
  auto pending = pair(siege->pending_nodes, *source->node_id);
  if (!pending || pending->source_id != source->id ||
      pending->node_id != *source->node_id ||
      pending->battlefield_id != source->battlefield_id)
    return error<MilitaryDamageSources>("SiegeFortificationPendingMismatch",
                                        *source->node_id);
  if (sorted(pending->unit_ids) != sorted(source->unit_ids))
    return error<MilitaryDamageSources>("SiegeFortificationUnitMismatch",
                                        *source->node_id);
  bool walls = false;
  for (auto &n : topology->nodes)
    if (n.kind == "wall") {
      auto hp = pair(siege->node_hp, n.id);
      if (!hp || *hp < 0 || *hp > n.max_hp)
        return error<MilitaryDamageSources>(
            "InvalidSiegeFortificationDurability", n.id);
      bool adjacent = n.id == *source->node_id;
      for (auto &e : topology->edges)
        if ((e.from == n.id && e.to == *source->node_id) ||
            (e.to == n.id && e.from == *source->node_id))
          adjacent = true;
      if (*hp > 0 && adjacent)
        walls = true;
    }
  MilitaryDamageSources result;
  for (auto &id : sorted(source->unit_ids)) {
    auto actual = find(s.military_units, id);
    auto node_id = pair(siege->unit_nodes, id);
    if (!actual || !node_id || *node_id != *source->node_id)
      return error<MilitaryDamageSources>("SiegeFortificationUnitMismatch", id);
    auto side = pair(source->side_by_army, actual->army_id),
         siege_side = pair(siege->armies, actual->army_id);
    if (!side || !siege_side || *side != *siege_side || *side < 0 || *side > 1)
      return error<MilitaryDamageSources>("SiegeFortificationSideMismatch", id);
    if (walls && *side == 1)
      result.emplace_back(id, topology->rules.wall_defense_bp);
  }
  return {true, std::move(result), {}};
}
Outcome<StableId>
create_military_battle(CommonWrite &c, MilitaryState &s,
                       const MilitaryCatalog &cat, const MilitaryAccess &access,
                       const StableId &source_id,
                       const std::optional<StableId> &controller) {
  auto lifecycle_binding =
      validate_military_battle_lifecycle_write_binding(c, s, access);
  if (!lifecycle_binding.ok)
    return {false, {}, lifecycle_binding.error};
  if (access.rng_stream && !access.rng_stream("battle"))
    return {false,
            {},
            common_failure("UnknownRandomStream", "battle", ErrorKind::internal)
                .error};
  auto source = find(s.contact_facts, source_id);
  if (!source)
    return combat_error<StableId>("MilitaryContactSourceRequired");
  auto checked = contact_valid(c, s, cat, *source, true);
  if (!checked.ok)
    return {false, {}, checked.error};
  MilitaryBattleRecord b;
  b.id = "pending";
  b.source_id = source_id;
  const auto *selected_board =
      find(cat.battle_catalog.battlefields, source->battlefield_id);
  b.rout_retreat_enabled = cat.rout_retreat_policy && selected_board &&
                           selected_board->rout_retreat && !source->siege_id &&
                           !source->node_id;
  b.battlefield_id = source->battlefield_id;
  b.objective_id = source->objective_id;
  b.objective_unit_ids = source->objective_unit_ids;
  b.created_tick = b.last_tick = std::max(c.tick(), source->tick);
  for (auto &id : sorted(source->unit_ids)) {
    auto actual = find(s.military_units, id);
    auto army = find(s.armies, actual->army_id);
    if (access.army_embarked && access.army_embarked(army->id))
      return combat_error<StableId>("NavalDisbandRequired", army->id);
    if (actual->fit.empty() || actual->morale <= 0)
      return combat_error<StableId>("MilitaryUnitUnavailable", id);
    if (!actual->mobilizing_action_id.empty() ||
        (actual->training_action_id && !actual->training_action_id->empty()))
      return combat_error<StableId>("MilitaryUnitReserved", id);
    if (!has(b.army_ids, army->id))
      b.army_ids.push_back(army->id);
    BattleUnit u;
    u.unit_id = id;
    u.army_id = army->id;
    u.commander_actor_id = actual->commander_actor_id;
    u.troop_type_id = actual->troop_type_id;
    u.side = *pair(source->side_by_army, army->id);
    u.morale = actual->morale;
    u.tactic_points =
        find(cat.battle_catalog.troop_types, u.troop_type_id)->tactic_capacity;
    u.member_ids = actual->member_ids;
    u.fit = actual->fit;
    u.wounded = actual->wounded;
    u.dead = actual->dead;
    u.captured = actual->captured;
    u.deserted = actual->deserted;
    b.units.push_back(u);
  }
  auto requested = controller.value_or(access.player_actor_id);
  if (requested != access.player_actor_id && !requested.empty())
    return combat_error<StableId>("MilitaryControllerMustBePlayer");
  checked = authority(c, s, cat, b, requested);
  if (!checked.ok) {
    if (controller)
      return {false, {}, checked.error};
    checked = authority(c, s, cat, b, {});
  }
  for (auto &u : b.units) {
    std::vector<StableId> people{u.commander_actor_id};
    people.insert(people.end(), u.fit.begin(), u.fit.end());
    people.insert(people.end(), u.wounded.begin(), u.wounded.end());
    for (auto &id : people)
      if (c.actor(id) && !has(b.participant_actor_ids, id))
        b.participant_actor_ids.push_back(id);
  }
  if (!b.controller_actor_id.empty() &&
      !has(b.participant_actor_ids, b.controller_actor_id))
    b.participant_actor_ids.push_back(b.controller_actor_id);
  b.participant_actor_ids = sorted(b.participant_actor_ids);
  b.army_ids = sorted(b.army_ids);
  for (auto &existing : s.battles) {
    if (existing.phase == "closed")
      continue;
    if (existing.source_id == source_id)
      return combat_error<StableId>("MilitaryContactAlreadyUsed", existing.id);
    for (auto &u : b.units)
      if (unit(existing, u.unit_id))
        return combat_error<StableId>("MilitaryUnitAlreadyEngaged", u.unit_id);
    for (auto &id : b.participant_actor_ids)
      if (has(existing.participant_actor_ids, id))
        return combat_error<StableId>("MilitaryActorAlreadyEngaged", id);
  }
  for (auto &id : b.participant_actor_ids) {
    auto actor = c.actor(id);
    if (!actor || !actor->alive || !actor->available)
      return combat_error<StableId>("MilitaryCommanderUnavailable", id);
    if (access.in_open_battle && access.in_open_battle(id))
      return combat_error<StableId>("MilitaryActorAlreadyEngaged", id);
    if (!actor->action_id.empty()) {
      auto action = c.action(actor->action_id);
      if (!action || action->kind != "travel" ||
          (action->actor_id != id && !has(action->participants, id)))
        return combat_error<StableId>("MilitaryActorBusy", id);
    }
  }
  auto board = find(cat.battle_catalog.battlefields, b.battlefield_id);
  checked = military_hex::validate(*board);
  if (!checked.ok)
    return {false, {}, checked.error};
  checked = place_military_battle_at_original_spawns(b, cat.battle_catalog);
  if (!checked.ok)
    return {false, {}, checked.error};
  if (cat.command_shock_policy && b.rout_retreat_enabled) {
    auto leaders = prepare_military_command_shock_entry(c, s, cat, access, b);
    if (!leaders.ok)
      return {false, {}, leaders.error};
    b.command_shock = std::move(leaders.value);
  }
  if (cat.combat_condition_policy && b.rout_retreat_enabled) {
    auto condition =
        prepare_military_combat_condition_entry(c, s, cat, access, b);
    if (!condition.ok)
      return {false, {}, condition.error};
    b.combat_conditions = std::move(condition.value);
  }
  if (cat.commanded_damage_policy && b.rout_retreat_enabled) {
    if (!access.prepare_commanded_damage)
      return {false,
              {},
              common_failure("MilitaryCommandedSkillReadRequired", source_id)
                  .error};
    auto prepared = access.prepare_commanded_damage(b);
    if (!prepared.ok)
      return {false, {}, prepared.error};
    b.commanded_damage = std::move(prepared.value);
  }
  if (cat.command_resources_policy && b.rout_retreat_enabled) {
    if (!access.prepare_command_resources)
      return {false,
              {},
              common_failure("MilitaryTacticsReadRequired", source_id).error};
    auto prepared = access.prepare_command_resources(b);
    if (!prepared.ok)
      return {false, {}, prepared.error};
    b.command_resources = std::move(prepared.value);
    for (auto &u : b.units)
      u.tactic_points = 40;
  }
  if (cat.battle_catalog.tactic_learning && b.rout_retreat_enabled) {
    if (!access.prepare_learned_tactics)
      return {false,
              {},
              common_failure("MilitaryTacticKnowledgeReadRequired", source_id)
                  .error};
    auto prepared = access.prepare_learned_tactics(b);
    if (!prepared.ok)
      return {false, {}, prepared.error};
    b.learned_tactics = std::move(prepared.value);
  }
  checked = validate_military_battle(b, cat.battle_catalog);
  if (!checked.ok)
    return {false, {}, checked.error};
  auto sources = military_damage_sources(s, cat, b);
  if (!sources.ok)
    return {false, {}, sources.error};
  auto ai = make_military_ai(b, cat.battle_catalog, sources.value);
  if (!ai.ok)
    return {false, {}, ai.error};
  auto id = c.next_id("battle");
  if (!id.ok)
    return {false, {}, id.error};
  b.id = id.value;
  auto hash = godot_string_hash_utf8(b.id),
       source_hash = godot_string_hash_utf8(source_id);
  if (!hash.ok || !source_hash.ok)
    return {false, {}, hash.ok ? source_hash.error : hash.error};
  Integer seed = access.battle_seed ^ hash.value ^ source_hash.value;
  b.rng_seed = std::to_string(seed);
  b.rng_state = std::to_string(godot_pcg32_seed_state(seed));
  b.ai_plans = std::move(ai.value);
  s.battles.push_back(b);
  source->battle_id = b.id;
  if (!source->field_order.empty())
    source->field_order.push_back("battle_id");
  auto published =
      publish_military_battle_lifecycle_start_in_candidate(c, s, access, b.id);
  if (!published.ok)
    return {false, {}, published.error};
  if (military_rout_rule_in_use(b, cat.battle_catalog)) {
    auto recorded =
        record_military_rout_entry(c, s, cat, *find(s.battles, b.id));
    if (!recorded.ok)
      return {false, {}, recorded.error};
  }
  return {true, b.id, {}};
}
Outcome<MilitaryQuote>
preview_battle_command(const CommonRead &c, const MilitaryState &s,
                       const MilitaryCatalog &cat, const MilitaryAccess &access,
                       const StableId &actor, const MilitaryCommand &command) {
  auto plan = std::get_if<MilitaryBattlePlan>(&command);
  auto close = std::get_if<MilitaryBattleClose>(&command);
  if (!plan && !close)
    return combat_error<MilitaryQuote>("UnknownMilitaryCommand");
  auto b = find(s.battles, plan ? plan->battle_id : close->battle_id);
  if (!b)
    return combat_error<MilitaryQuote>("UnknownMilitaryBattle");
  if (b->controller_actor_id != actor || actor != access.player_actor_id)
    return combat_error<MilitaryQuote>("MilitaryCommanderUnauthorized", b->id);
  MilitaryQuote q;
  q.battle_id = b->id;
  if (close) {
    if (b->phase != "result" || !b->applied || !find(s.battle_facts, b->id))
      return combat_error<MilitaryQuote>("MilitaryResultNotSettled", b->id);
    return {true, q, {}};
  }
  if (b->phase != "planning")
    return combat_error<MilitaryQuote>("MilitaryBattleNotPlanning", b->id);
  if (b->plan_submitted)
    return combat_error<MilitaryQuote>("MilitaryPlanAlreadySubmitted", b->id);
  if (!plan->orders_valid)
    return combat_error<MilitaryQuote>("InvalidMilitaryOrders", b->id);
  auto candidate = *b;
  auto checked = authority(c, s, cat, candidate, actor);
  if (!checked.ok)
    return {false, {}, checked.error};
  if (!same_authority(*b, candidate))
    return combat_error<MilitaryQuote>("MilitaryAuthorityChanged", b->id);
  auto orders =
      validate_military_orders(*b, cat.battle_catalog, actor, plan->orders);
  if (!orders.ok) {
    orders.error.battle_id = b->id;
    orders.error.battle_id_after_path = true;
    return {false, {}, orders.error};
  }
  q.battle_orders = std::move(orders.value);
  return {true, std::move(q), {}};
}
Outcome<MilitaryEffect> apply_battle_command(CommonWrite &c, MilitaryState &s,
                                             const MilitaryCatalog &cat,
                                             const MilitaryAccess &access,
                                             const StableId &actor,
                                             const MilitaryCommand &command) {
  auto lifecycle_binding =
      validate_military_battle_lifecycle_write_binding(c, s, access);
  if (!lifecycle_binding.ok)
    return {false, {}, lifecycle_binding.error};
  auto q = preview_battle_command(c, s, cat, access, actor, command);
  if (!q.ok)
    return {false, {}, q.error};
  auto b = find(s.battles, q.value.battle_id);
  if (std::holds_alternative<MilitaryBattleClose>(command)) {
    auto released =
        record_military_battle_release(c, s, cat, access, *b, actor);
    if (!released.ok)
      return {false, {}, released.error};
    b->phase = "closed";
    auto published =
        publish_military_battle_lifecycle_close_in_candidate(c, s, access, *b);
    if (!published.ok)
      return {false, {}, published.error};
  } else {
    b->pending_orders = q.value.battle_orders;
    b->plan_submitted = true;
  }
  MilitaryEffect e;
  e.battle_id = b->id;
  e.plan_submitted = b->plan_submitted;
  return {true, e, {}};
}
// 永久轮次链以source、名单、日期和最终战果闭合，拒绝重放终局或伪造已落账标记。
Status validate_military_battle_world(const CommonRead &c,
                                      const MilitaryState &s,
                                      const MilitaryCatalog &cat,
                                      const MilitaryAccess &access,
                                      bool supplied) {
  std::set<StableId> occupied, sources;
  for (auto &b : s.battles) {
    if (!unique(b.army_ids) || !unique(b.participant_actor_ids) ||
        !unique(b.round_receipt_ids) ||
        !has({"planning", "result", "closed"}, b.phase))
      return combat_fail("InvalidMilitarySession", b.id);
    if (b.created_tick < 0 || b.created_tick > c.tick() ||
        b.last_tick < b.created_tick || b.last_tick > c.tick() ||
        b.completed_tick < -1 || b.completed_tick > c.tick())
      return combat_fail("InvalidMilitarySessionTime", b.id);
    auto copy = b;
    if (copy.phase == "closed")
      copy.phase = "result";
    auto checked = validate_military_battle(copy, cat.battle_catalog, true);
    if (!checked.ok) {
      checked.error.battle_id = b.id;
      checked.error.battle_id_after_path = true;
      return checked;
    }
    auto source = find(s.contact_facts, b.source_id);
    if (!source)
      return combat_fail("MilitaryContactSourceRequired");
    checked = contact_valid(c, s, cat, *source, false);
    if (!checked.ok)
      return checked;
    if (!source->battle_id || *source->battle_id != b.id ||
        sorted(source->unit_ids) != unit_ids(b) ||
        keys(source->side_by_army) != b.army_ids)
      return combat_fail("MilitaryContactMismatch", b.id);
    if (!sources.insert(b.source_id).second)
      return combat_fail("DuplicateMilitarySource", b.id);
    std::vector<StableId> people;
    for (auto &u : b.units) {
      auto side = pair(source->side_by_army, u.army_id);
      if (u.army_id != source_army(s, *source, u.unit_id) || !side ||
          u.side != *side)
        return combat_fail("MilitaryContactSidesMismatch", b.id);
      for (auto &person : u.member_ids)
        if (c.actor(person) && !has(people, person))
          people.push_back(person);
      if (!has(people, u.commander_actor_id))
        people.push_back(u.commander_actor_id);
      if (!has(b.participant_actor_ids, u.commander_actor_id))
        return combat_fail("MilitaryParticipantMissing", b.id);
    }
    for (auto &person : b.participant_actor_ids)
      if (!has(people, person) && person != b.controller_actor_id)
        return combat_fail("MilitaryParticipantMismatch", b.id);
    if (!b.controller_actor_id.empty() &&
        !has(b.participant_actor_ids, b.controller_actor_id))
      return combat_fail("MilitaryParticipantMissing", b.id);
    Integer previous = b.created_tick;
    std::set<StableId> receipts;
    for (std::size_t i = 0; i < b.round_receipt_ids.size(); ++i) {
      auto r = find(s.casualty_facts, b.round_receipt_ids[i]);
      auto fact = r ? find(s.battle_facts, r->battle_id) : nullptr;
      if (!r || !fact || !receipts.insert(r->battle_id).second ||
          fact->phase != "completed" || fact->session_id != b.id)
        return combat_fail("MilitarySettlementSourceMissing", b.id);
      std::vector<StableId> roster;
      for (auto &u : r->units)
        roster.push_back(u.unit_id);
      if (sorted(roster) != unit_ids(b))
        return combat_fail("MilitarySettlementSourceMissing", b.id);
      bool final = i == b.round_receipt_ids.size() - 1 && b.phase != "planning";
      if (fact->final != final || fact->round != Integer(i) + 1)
        return combat_fail("MilitarySettlementRoundMismatch", b.id);
      if (fact->army_ids != b.army_ids || fact->unit_ids != unit_ids(b) ||
          fact->contact_source_id != b.source_id ||
          fact->source_id != (final ? b.source_id : fact->id) ||
          (final && fact->id != b.id))
        return combat_fail("MilitarySettlementSourceMismatch", b.id);
      if (!result_equal(fact->result,
                        final ? b.result
                              : std::optional<MilitaryBattleResult>{}) ||
          fact->winner_side != (final && b.result ? b.result->winner_side : -1))
        return combat_fail("MilitarySettlementResultMismatch", b.id);
      if (fact->tick < b.created_tick || fact->tick > c.tick() ||
          fact->start_tick < b.created_tick || fact->start_tick > fact->tick ||
          r->tick != fact->tick)
        return combat_fail("MilitarySettlementTimeMismatch", b.id);
      bool terminated = final && b.result &&
                        has({"authority_lost", "responsible_actor_lost",
                             "truce", "cancelled", "timeout"},
                            b.result->reason);
      if (terminated) {
        if (fact->winner_side != -1 || fact->start_tick != fact->tick ||
            (fact->tick != previous && fact->tick != previous + 1) ||
            b.last_tick != previous)
          return combat_fail("MilitarySettlementTimeMismatch", b.id);
      } else if (fact->start_tick != previous || fact->tick != previous + 1 ||
                 (i == b.round_receipt_ids.size() - 1 &&
                  b.last_tick != fact->tick))
        return combat_fail("MilitarySettlementTimeMismatch", b.id);
      previous = fact->tick;
    }
    if (b.phase == "planning") {
      if (b.applied || b.completed_tick != -1 ||
          !b.settlement_receipt_id.empty() ||
          b.round != Integer(b.round_receipt_ids.size()) + 1 ||
          b.last_tick != c.tick() ||
          b.last_tick - b.created_tick != Integer(b.round_receipt_ids.size()))
        return combat_fail("InvalidMilitarySettlement", b.id);
      copy = b;
      checked = authority(c, s, cat, copy, b.controller_actor_id);
      if (!checked.ok || !same_authority(b, copy))
        return combat_fail("MilitaryAuthorityChanged", b.id);
      for (auto &u : b.units) {
        auto actual = find(s.military_units, u.unit_id);
        auto army = find(s.armies, u.army_id);
        if (!actual || !army || actual->army_id != u.army_id ||
            army->status != "active" ||
            actual->commander_actor_id != u.commander_actor_id ||
            actual->member_ids != u.member_ids ||
            actual->troop_type_id != u.troop_type_id)
          return combat_fail("MilitaryActualUnitChanged", u.unit_id);
        if (!supplied && !roster_equal(*actual, u))
          return combat_fail("MilitaryActualRosterChanged", u.unit_id);
        if (supplied) {
          const MilitarySupplyFact *food = nullptr;
          for (auto &f : s.supply_facts)
            if (f.army_id == u.army_id && f.tick == c.tick() + 1) {
              if (food)
                return combat_fail("MilitarySupplyRequired", b.id);
              food = &f;
            }
          if (!food || army->last_supply_tick != c.tick() + 1)
            return combat_fail("MilitarySupplyRequired", b.id);
          auto supplied_unit = u;
          supplied_unit.morale =
              food->shortage > 0 && !u.fit.empty()
                  ? (cat.rout_retreat_policy && u.morale == 0
                         ? 0
                         : std::max<Integer>(1, u.morale - 8))
                  : u.morale;
          if (cat.supply_attrition_policy) {
            auto applied = apply_military_supply_attrition_to_battle(
                s, cat, supplied_unit, food->id, c.tick() + 1);
            if (!applied.ok)
              return applied;
          }
          if (!roster_equal(*actual, supplied_unit))
            return combat_fail("MilitaryActualRosterChanged", u.unit_id);
          if (actual->morale != supplied_unit.morale)
            return combat_fail("MilitarySupplyMoraleMismatch", u.unit_id);
        } else if (actual->morale != u.morale)
          return combat_fail("MilitaryActualMoraleChanged", u.unit_id);
        auto actor = c.actor(army->commander_actor_id);
        auto location = pair(source->army_locations, u.army_id);
        if (!actor || !location || !same_location(actor->location, *location))
          return combat_fail("MilitaryContactPositionChanged", b.id);
      }
    } else {
      if (!b.applied || b.completed_tick < 0 || b.round_receipt_ids.empty() ||
          b.round != Integer(b.round_receipt_ids.size()) ||
          b.settlement_receipt_id != b.round_receipt_ids.back())
        return combat_fail("MilitaryResultNotSettled", b.id);
      auto f = find(s.battle_facts, b.id);
      auto receipt = find(s.casualty_facts, b.settlement_receipt_id);
      if (!f || !f->final || f->source_id != b.source_id ||
          !result_equal(f->result, b.result) || f->tick != b.completed_tick ||
          !receipt || receipt->battle_id != b.id)
        return combat_fail("MilitaryResultSourceMismatch", b.id);
      for (auto &u : b.units) {
        const MilitaryRosterReceipt *r = nullptr;
        for (auto &row : receipt->units)
          if (row.unit_id == u.unit_id)
            r = &row;
        if (!r || r->fit != u.fit || r->wounded != u.wounded ||
            r->dead != u.dead || r->captured != u.captured ||
            r->deserted != u.deserted)
          return combat_fail("MilitaryResultRosterMismatch", u.unit_id);
      }
    }
    checked = validate_military_commander_history(c, s, cat, access, b);
    if (!checked.ok)
      return checked;
    if ((!b.plan_submitted && !b.pending_orders.empty()) ||
        (b.phase != "planning" && b.plan_submitted))
      return combat_fail("MilitaryUnexpectedPendingOrders", b.id);
    if (b.plan_submitted) {
      auto q = validate_military_orders(
          b, cat.battle_catalog, b.controller_actor_id, b.pending_orders);
      if (!q.ok)
        return {false, {}, q.error};
    }
    if (b.phase != "closed") {
      for (auto &u : b.units)
        if (!occupied.insert(u.unit_id).second)
          return combat_fail("MilitaryUnitAlreadyEngaged", u.unit_id);
      for (auto &person : b.participant_actor_ids)
        if (!occupied.insert(person).second)
          return combat_fail("MilitaryActorAlreadyEngaged", person);
    }
  }
  return common_success();
}
Status preflight_military_battles(const CommonRead &c, const MilitaryState &s,
                                  const MilitaryCatalog &cat,
                                  const MilitaryAccess &access, Integer tick,
                                  bool supplied) {
  if (tick != c.tick() + 1)
    return combat_fail("MilitaryInvalidStep");
  auto r = validate_military_battle_world(c, s, cat, access, supplied);
  if (!r.ok)
    return r;
  for (auto &b : s.battles) {
    if (b.phase != "planning" || b.created_tick > c.tick() ||
        b.last_tick == tick)
      continue;
    if (b.last_tick != c.tick())
      return combat_fail("MilitarySkippedStep", b.id);
    bool needed = false;
    for (auto &id : b.command_unit_ids) {
      auto u = unit(b, id);
      if (!u->fit.empty() && !u->withdrawn && u->morale > 0 &&
          military_commander_loss(*u) != 2)
        needed = true;
    }
    if (needed && !b.plan_submitted)
      return combat_fail("MilitaryPlanRequired", b.id);
  }
  return common_success();
}
Outcome<MilitaryBattleStep> step_military_battles(CommonWrite &c,
                                                  MilitaryState &s,
                                                  const MilitaryCatalog &cat,
                                                  const MilitaryAccess &access,
                                                  Integer tick) {
  auto lifecycle_binding =
      validate_military_battle_lifecycle_write_binding(c, s, access);
  if (!lifecycle_binding.ok)
    return {false, {}, lifecycle_binding.error};
  auto r = preflight_military_battles(c, s, cat, access, tick, true);
  if (!r.ok)
    return {false, {}, r.error};
  MilitaryBattleStep result;
  std::vector<StableId> ids;
  for (auto &b : s.battles)
    ids.push_back(b.id);
  for (auto &id : sorted(ids)) {
    auto b = find(s.battles, id);
    if (b->phase != "planning" || b->created_tick > c.tick() ||
        b->last_tick == tick)
      continue;
    // 原预检仍先行；本轮围城真实到期先沿原terminate产回执，不能再借已终态城墙结算一轮。
    if (military_siege_timeout_in_scope(s, cat, *b)) {
      if (!access.siege_engineering)
        return combat_error<MilitaryBattleStep>(
            "MilitarySiegeEngineeringAccessRequired", id);
      auto release = access.siege_engineering->timeout_release(*b);
      if (!release.ok)
        return {false, {}, release.error};
      if (release.value) {
        if (release.value->tick != tick)
          return combat_error<MilitaryBattleStep>(
              "MilitarySiegeTimeoutPhaseMismatch", id);
        auto ended =
            terminate_military_battle(c, s, cat, access, id, "timeout", tick);
        if (!ended.ok)
          return ended;
        result.actor_changes.insert(result.actor_changes.end(),
                                    ended.value.actor_changes.begin(),
                                    ended.value.actor_changes.end());
        result.casualty_source_ids.insert(
            result.casualty_source_ids.end(),
            ended.value.casualty_source_ids.begin(),
            ended.value.casualty_source_ids.end());
        result.round_fact_ids.insert(result.round_fact_ids.end(),
                                     ended.value.round_fact_ids.begin(),
                                     ended.value.round_fact_ids.end());
        result.completed_battle_ids.insert(
            result.completed_battle_ids.end(),
            ended.value.completed_battle_ids.begin(),
            ended.value.completed_battle_ids.end());
        continue;
      }
    }
    auto current = *b;
    std::vector<MilitaryBattleEvent> supply_interruptions;
    if (cat.supply_attrition_policy) {
      auto prepared =
          prepare_military_supply_attrition_battle(s, cat, current, tick);
      if (!prepared.ok)
        return {false, {}, prepared.error};
      current = std::move(prepared.value.battle);
      supply_interruptions = std::move(prepared.value.interrupted_orders);
    } else
      for (auto &u : current.units)
        u.morale = find(s.military_units, u.unit_id)->morale;
    auto sources = military_damage_sources(s, cat, current);
    if (!sources.ok)
      return {false, {}, sources.error};
    auto resolved = resolve_military_round(
        current, cat.battle_catalog, current.controller_actor_id,
        current.plan_submitted ? current.pending_orders : MilitaryOrders{},
        random_draw, sources.value);
    if (!resolved.ok) {
      resolved.error.battle_id = id;
      resolved.error.battle_id_after_path = true;
      return {false, {}, resolved.error};
    }
    auto next = std::move(resolved.value);
    auto lifecycle = validate_military_battle_resolved_lifecycle(
        *b, next, access.military_battle_lifecycle_schema);
    if (!lifecycle.ok)
      return {false, {}, lifecycle.error};
    next.last_events.insert(next.last_events.begin(),
                            supply_interruptions.begin(),
                            supply_interruptions.end());
    const auto resolved_frame = next;
    next.last_tick = tick;
    next.plan_submitted = false;
    next.pending_orders.clear();
    auto settled = settle(c, s, cat, next, tick, b->last_tick);
    if (!settled.ok)
      return {false, {}, settled.error};
    if (military_rout_rule_in_use(current, cat.battle_catalog)) {
      auto recorded = record_military_rout_round(
          c, s, cat, current, resolved_frame,
          settled.value.round_fact_ids.front(), next.round_receipt_ids.back(),
          sources.value);
      if (!recorded.ok)
        return {false, {}, recorded.error};
    }
    result.actor_changes.insert(result.actor_changes.end(),
                                settled.value.actor_changes.begin(),
                                settled.value.actor_changes.end());
    result.casualty_source_ids.insert(result.casualty_source_ids.end(),
                                      settled.value.casualty_source_ids.begin(),
                                      settled.value.casualty_source_ids.end());
    result.round_fact_ids.insert(result.round_fact_ids.end(),
                                 settled.value.round_fact_ids.begin(),
                                 settled.value.round_fact_ids.end());
    result.completed_battle_ids.insert(
        result.completed_battle_ids.end(),
        settled.value.completed_battle_ids.begin(),
        settled.value.completed_battle_ids.end());
    if (next.phase == "result") {
      bool controller_dead = false;
      for (const auto &change : settled.value.actor_changes)
        if (change.actor_id == next.controller_actor_id &&
            change.status == "dead") {
          const auto *field =
              find(cat.battle_catalog.battlefields, next.battlefield_id);
          controller_dead = field && field->commander_losses;
        }
      if (next.controller_actor_id.empty() || controller_dead)
        next.phase = "closed";
    }
    *b = std::move(next);
    if (b->phase == "closed") {
      auto published = publish_military_battle_lifecycle_close_in_candidate(
          c, s, access, *b);
      if (!published.ok)
        return {false, {}, published.error};
    }
  }
  return {true, std::move(result), {}};
}
Outcome<MilitaryBattleStep>
terminate_military_battle(CommonWrite &c, MilitaryState &s,
                          const MilitaryCatalog &cat,
                          const MilitaryAccess &access, const StableId &id,
                          const StableId &reason, Integer tick) {
  if (!has({"authority_lost", "responsible_actor_lost", "truce", "cancelled",
            "timeout"},
           reason))
    return combat_error<MilitaryBattleStep>("UnknownMilitaryTermination");
  auto b = find(s.battles, id);
  if (!b)
    return combat_error<MilitaryBattleStep>("UnknownMilitaryBattle", id);
  if (b->phase == "closed")
    return {true, {}, {}};
  auto lifecycle_binding =
      validate_military_battle_lifecycle_write_binding(c, s, access);
  if (!lifecycle_binding.ok)
    return {false, {}, lifecycle_binding.error};
  if (b->phase == "result") {
    if (!b->applied || !find(s.casualty_facts, b->settlement_receipt_id))
      return combat_error<MilitaryBattleStep>("MilitaryResultNotSettled", id);
    b->phase = "closed";
    auto published =
        publish_military_battle_lifecycle_close_in_candidate(c, s, access, *b);
    if (!published.ok)
      return {false, {}, published.error};
    MilitaryBattleStep r;
    r.completed_battle_ids = {id};
    return {true, r, {}};
  }
  if (tick < b->last_tick || (tick != c.tick() && tick != c.tick() + 1))
    return combat_error<MilitaryBattleStep>("MilitaryTerminationBoundary", id);
  auto next = *b;
  if (cat.supply_attrition_policy && tick > b->last_tick) {
    auto prepared =
        prepare_military_supply_attrition_battle(s, cat, next, tick);
    if (!prepared.ok)
      return {false, {}, prepared.error};
    next = std::move(prepared.value.battle);
  }
  const auto original_frame = next;
  next.phase = "result";
  next.result = MilitaryBattleResult{-1, reason, {}};
  next.ai_plans.clear();
  next.plan_submitted = false;
  next.pending_orders.clear();
  for (auto &u : next.units)
    u.morale = find(s.military_units, u.unit_id)->morale;
  const auto resolved_frame = next;
  auto result = settle(c, s, cat, next, tick, tick);
  if (!result.ok)
    return result;
  if (military_rout_rule_in_use(original_frame, cat.battle_catalog)) {
    auto recorded =
        record_military_rout_round(c, s, cat, original_frame, resolved_frame,
                                   result.value.round_fact_ids.front(),
                                   next.round_receipt_ids.back(), {}, reason);
    if (!recorded.ok)
      return {false, {}, recorded.error};
  }
  next.phase = "closed";
  auto lifecycle = validate_military_battle_resolved_lifecycle(
      *b, next, access.military_battle_lifecycle_schema);
  if (!lifecycle.ok)
    return {false, {}, lifecycle.error};
  *b = std::move(next);
  auto published =
      publish_military_battle_lifecycle_close_in_candidate(c, s, access, *b);
  if (!published.ok)
    return {false, {}, published.error};
  return result;
}
// Field 原根只在明确选择的同一实际 Common/Military 上借用；缺根不回落旧六角会战。
static Outcome<const FieldBattleActorOutcomeContext *>
field_actor_outcomes(const CommonRead &common, const MilitaryState &military,
                     const MilitaryAccess &access) {
  const auto *actual = dynamic_cast<const CommonStore *>(&common);
  const auto *context = access.field_battle_actor_outcomes.get();
  if (!access.actual_catalog_identity) {
    if (context)
      return {false, {}, fail("FieldActorOutcomeCatalogIdentityRequired").error};
    // 不持锁目录的独立旧 Legacy 调用只保原分支；已有任何原 Field 头就不能
    // 借空 context 冒充未选。完整 World 的 service 必须连未选情形也传真目录。
    if (actual) for (const auto &[id, head] : actual->state().facts.rows())
      if (head.kind.starts_with("field_battle_"))
        return {false, {}, fail("FieldActorOutcomeCatalogIdentityRequired", id).error};
    return {true, nullptr, {}};
  }
  auto selected = selected_field_battle_version(*access.actual_catalog_identity);
  if (!selected.ok) return {false, {}, selected.error};
  if (bool(selected.value) != bool(context))
    return {false, {}, fail("FieldActorOutcomeSelectedRootRequired").error};
  if (context && (!actual || &context->common != actual || &context->military != &military ||
                  context->field.schema != 1))
    return {false, {}, fail("FieldActorOutcomeSameWorldRequired").error};
  return {true, context, {}};
}
// 实时退出必须落在实际会战显式策略的本次伤亡上；仅载入其它新战场不能改变旧人物提交。
static Outcome<std::optional<StableId>>
field_exit_source(const CommonRead &c, const MilitaryState &s,
                  const MilitaryCatalog &cat, const MilitaryAccess &access,
                  const MilitaryActorChange &change) {
  auto field_context = field_actor_outcomes(c, s, access);
  if (!field_context.ok) return {false, {}, field_context.error};
  std::optional<StableId> source;
  for (const auto &receipt : s.casualty_facts) {
    if (receipt.tick < c.tick() || receipt.tick - c.tick() > 1)
      continue;
    if (field_context.value) {
      const auto &context = *field_context.value;
      const auto *parent = context.common.state().facts.find(receipt.battle_id);
      if (parent && parent->domain == "military" && parent->kind == "field_battle_segment") {
        for (const auto &row : receipt.actor_changes)
          if (row.actor_id == change.actor_id && row.status == change.status &&
              row.army_id == change.army_id && row.captor_army_id == change.captor_army_id &&
              row.garrison_id == change.garrison_id) {
            const auto *actor = c.actor(change.actor_id);
            if (source || !actor || !access.end_journey_with_source)
              return {false, {}, fail("FieldJourneyOriginalWriterRequired", receipt.id).error};
            auto actual = preview_field_segment_journey_termination(
                context.common, s, context.field, receipt.id, actor->action_id, actor->id);
            if (!actual.ok) return {false, {}, actual.error};
            if (!actual.value)
              return {false, {}, fail("FieldJourneyOriginalActionRequired", receipt.id).error};
            source = receipt.id;
          }
        continue;
      }
    }
    const auto *fact = find(s.battle_facts, receipt.battle_id);
    const auto *battle = fact ? find(s.battles, fact->session_id) : nullptr;
    const auto *field =
        battle ? find(cat.battle_catalog.battlefields, battle->battlefield_id)
               : nullptr;
    if (!field || !field->commander_losses)
      continue;
    for (const auto &row : receipt.actor_changes)
      if (row.actor_id == change.actor_id && row.status == change.status &&
          row.army_id == change.army_id &&
          row.captor_army_id == change.captor_army_id &&
          row.garrison_id == change.garrison_id) {
        if (source || fact->tick != receipt.tick ||
            fact->phase != "completed" ||
            !has(battle->round_receipt_ids, receipt.id) ||
            !access.fact_exists || !access.fact_exists(receipt.id) ||
            !access.fact_exists(fact->id))
          return {false,
                  {},
                  fail("MilitaryJourneyTerminationSourceRequired", receipt.id)
                      .error};
        source = receipt.id;
      }
  }
  return {true, std::move(source), {}};
}
Status
apply_military_actor_changes(CommonWrite &c, const MilitaryState &s,
                             const MilitaryCatalog &cat,
                             const MilitaryAccess &access,
                             const std::vector<MilitaryActorChange> &changes) {
  auto field_context = field_actor_outcomes(c, s, access);
  if (!field_context.ok) return {false, {}, field_context.error};
  for (auto &change : changes) {
    auto p = c.actor(change.actor_id);
    if (!p)
      return fail("UnknownActor", change.actor_id);
    auto actor = *p;
    Location actual = actor.location;
    if (auto in = std::get_if<InArmyLocation>(&actual)) {
      auto army = find(s.armies, in->army_id);
      auto commander = army ? c.actor(army->commander_actor_id) : nullptr;
      if (!commander)
        return fail("DanglingArmyLocation");
      actual = commander->location;
    }
    ActorMutation m;
    m.actor_id = actor.id;
    if (has({"dead", "captured", "deserted"}, change.status)) {
      if (!actor.action_id.empty()) {
        auto action = c.action(actor.action_id);
        if (action && action->kind == "travel" &&
            action->actor_id == actor.id) {
          auto source = field_exit_source(c, s, cat, access, change);
          if (!source.ok)
            return {false, {}, source.error};
          if (source.value) {
            if (!access.end_journey && !access.end_journey_with_source)
              return fail("MilitaryJourneyEndServiceRequired", action->id);
            auto ended =
                access.end_journey_with_source
                    ? access.end_journey_with_source(
                          action->id,
                          {MilitaryTravelEndingSource::Kind::field_casualties,
                           *source.value})
                    : access.end_journey(action->id);
            if (!ended.ok)
              return ended;
            m.action_id = "";
          }
        } else if (action && action->kind == "travel" &&
                   action->actor_id != actor.id) {
          auto amended = *action;
          amended.participants.erase(std::remove(amended.participants.begin(),
                                                 amended.participants.end(),
                                                 actor.id),
                                     amended.participants.end());
          ActionMutation a;
          a.action = amended;
          a.participants = amended.participants;
          auto r = c.apply_action(a);
          if (!r.ok)
            return r;
          m.action_id = "";
        }
      }
      m.location = actual;
    }
    if (change.status == "dead") {
      m.alive = false;
      m.available = false;
      m.hp = 0;
    } else if (change.status == "deserted")
      m.available = false;
    else if (change.status == "wounded") {
      if (!access.actor_injury)
        return fail("MilitaryActorInjuryRequired");
      m.injury = std::max<Integer>(1, access.actor_injury(actor.id));
      m.hp = std::min<Integer>(actor.hp, 70);
    } else if (change.status == "captured") {
      m.available = false;
      if (change.garrison_id) {
        auto camp = find(cat.garrisons, *change.garrison_id);
        if (!camp)
          return fail("UnknownGarrison");
        auto place = access.common_catalog
                         ? access.common_catalog->places.find(camp->place_id)
                         : nullptr;
        if (!place)
          return fail("UnknownPlace", camp->place_id);
        m.location = SettlementLocation{place->settlement_id, camp->place_id};
      } else
        m.location = InArmyLocation{change.captor_army_id};
    }
    auto r = c.apply_actor(m);
    if (!r.ok)
      return r;
  }
  return common_success();
}
// 原Land批次只在完整人物写完后绑定；未来Naval仍由其真正头落成后的独立出口调用。
Status apply_military_actor_changes_from_sources(
    CommonWrite &c, const MilitaryState &s, const MilitaryCatalog &cat,
    const MilitaryAccess &access,
    const std::vector<MilitaryActorChange> &changes,
    const std::vector<StableId> &sources) {
  if (access.actor_conditions) {
    auto matched =
        validate_military_actor_condition_changes(s, changes, sources);
    if (!matched.ok)
      return matched;
  }
  auto applied = apply_military_actor_changes(c, s, cat, access, changes);
  if (!applied.ok)
    return applied;
  return finish_military_actor_condition_sources(c, s, access, sources);
}
// 半日旅行已计费后只截断真实行程，静止军团没有伪行动；整步证明先核，再保存保留前缀。
Outcome<StableId>
install_military_contact(CommonWrite &c, MilitaryState &s,
                         const MilitaryCatalog &cat,
                         const MilitaryAccess &access, TravelState &travel,
                         const MilitaryContactHit &hit,
                         const std::vector<TerrainExecution> &executions) {
  if (hit.source_tick != c.tick() || hit.contact.a.empty() ||
      hit.contact.b.empty() || hit.contact.a == hit.contact.b)
    return error<StableId>("InvalidMilitaryContactMotion");
  auto first = find(s.armies, hit.first_army_id),
       second = find(s.armies, hit.second_army_id);
  if (!first || !second)
    return error<StableId>("MilitaryContactArmyMismatch");
  auto same_point = [](PointI a, PointI b) { return a.x == b.x && a.y == b.y; };
  auto same_points = [&](const std::vector<PointI> &a,
                         const std::vector<PointI> &b) {
    return a.size() == b.size() &&
           std::equal(a.begin(), a.end(), b.begin(), same_point);
  };
  auto army_for = [&](const StableId &actor) {
    return first->commander_actor_id == actor ? first : second;
  };
  std::array<const TravelMotion *, 2> motions{&hit.first_motion,
                                              &hit.second_motion};
  for (auto motion : motions) {
    auto army = army_for(motion->actor_id);
    auto actor = c.actor(motion->actor_id);
    bool fit = false;
    for (auto &id : army->unit_ids) {
      auto u = find(s.military_units, id);
      if (u && u->army_id == army->id && !u->fit.empty() && u->morale > 0)
        fit = true;
    }
    if (army->commander_actor_id != motion->actor_id ||
        army->status != "active" || !actor || !actor->alive ||
        !actor->available || military_actor_in_battle(s, actor->id) || !fit)
      return error<StableId>("MilitaryContactArmyUnavailable");
    if (army->last_supply_tick != c.tick() + 1)
      return error<StableId>("MilitaryContactHalfDayRequired");
    if (!validate_terrain_trace(*motion).ok)
      return error<StableId>("InvalidMilitaryContactMotion");
    if (motion->stationary) {
      if (!actor->action_id.empty() || !motion->stationary_location ||
          !same_location(actor->location, *motion->stationary_location))
        return error<StableId>("MilitaryStationarySourceChanged");
      continue;
    }
    auto action = c.action(motion->action_id);
    auto params = travel.actions.find(motion->action_id);
    auto fact = travel.journey_facts.find(motion->params.plan_id);
    if (!action || actor->action_id != motion->action_id ||
        action->kind != "travel" || action->actor_id != actor->id || !params)
      return error<StableId>("MilitaryTravelSourceChanged");
    auto &p = motion->params;
    if (!fact || fact->actor_id != actor->id || fact->action_id != action->id ||
        fact->map_id != motion->map_id || fact->mode != p.mode ||
        fact->budget != p.budget || p.route_index < 1 ||
        p.route_index >= Integer(fact->points.size()))
      return error<StableId>("MilitaryTravelPlanRequired");
    std::vector<PointI> suffix(fact->points.begin() + p.route_index,
                               fact->points.end());
    if (!same_points(suffix, p.points))
      return error<StableId>("MilitaryTravelPlanRequired");
    if (std::tie(p.plan_id, p.map_id, p.origin, p.mode, p.budget,
                 p.payer_wallet_id, p.money_per_tick, p.fatigue_per_tick,
                 p.paused, p.encounter_id, p.met_actions) !=
        std::tie(params->plan_id, params->map_id, params->origin, params->mode,
                 params->budget, params->payer_wallet_id,
                 params->money_per_tick, params->fatigue_per_tick,
                 params->paused, params->encounter_id, params->met_actions))
      return error<StableId>("MilitaryTravelSourceChanged");
    PointI actual;
    if (auto wild = std::get_if<WildernessLocation>(&actor->location))
      actual = wild->point;
    else if (auto city = std::get_if<SettlementLocation>(&actor->location)) {
      auto settlement =
          access.travel_catalog
              ? access.travel_catalog->settlements.find(city->settlement_id)
              : nullptr;
      if (!settlement)
        return error<StableId>("MilitaryTravelStepRequired");
      actual = settlement->map_xy;
    } else
      return error<StableId>("MilitaryTravelStepRequired");
    if (!same_point(actual, motion->end))
      return error<StableId>("MilitaryTravelStepRequired");
    Integer reached = motion->moving ? motion->reached_points : 0;
    if (reached < 0 || reached > Integer(p.points.size()))
      return error<StableId>("MilitaryTravelStepRequired");
    std::vector<PointI> remaining(p.points.begin() + reached, p.points.end());
    if (!same_points(params->points, remaining) ||
        params->route_index != p.route_index + reached ||
        params->travel_credit !=
            (motion->moving ? motion->end_credit : p.travel_credit))
      return error<StableId>("MilitaryTravelStepRequired");
  }
  ContactEntrances entrances;
  if (access.travel_catalog)
    for (auto &settlement : access.travel_catalog->settlements.rows())
      entrances[access.travel_catalog->world_map_id].push_back(
          settlement.second.map_xy);
  auto actual =
      terrain_intersection(hit.first_motion, hit.second_motion, entrances);
  if (!actual || actual->a != hit.contact.a || actual->b != hit.contact.b ||
      actual->time_num != hit.contact.time_num ||
      actual->time_den != hit.contact.time_den ||
      !same_point(actual->first.anchor.point, hit.contact.first.anchor.point) ||
      !same_point(actual->second.anchor.point,
                  hit.contact.second.anchor.point) ||
      !same_points(actual->first.trace.params.points,
                   hit.contact.first.trace.params.points) ||
      !same_points(actual->second.trace.params.points,
                   hit.contact.second.trace.params.points) ||
      actual->first.trace.params.route_index !=
          hit.contact.first.trace.params.route_index ||
      actual->second.trace.params.route_index !=
          hit.contact.second.trace.params.route_index)
    return error<StableId>("MilitaryContactGeometryMismatch");
  if (access.travel_catalog && access.travel_services) {
    for (auto endpoint : {&hit.contact.first, &hit.contact.second}) {
      auto map = access.travel_catalog->maps.find(endpoint->anchor.map_id);
      if (!map ||
          !terrain::point_valid_prepared(
              *map, endpoint->anchor.point, terrain::Mode::army,
              access.travel_services->terrain_flags()) ||
          (!endpoint->trace.stationary &&
           (endpoint->trace.params.points.empty() ||
            !terrain::segment_cost_prepared(
                 *map, endpoint->anchor.point,
                 endpoint->trace.params.points.front(), terrain::Mode::army, {})
                 .ok)))
        return error<StableId>("MilitaryContactAlreadyUsed");
    }
  }
  if (hit.first_motion.stationary || hit.second_motion.stationary) {
    auto moving =
        hit.first_motion.stationary ? &hit.second_motion : &hit.first_motion;
    auto still =
        hit.first_motion.stationary ? &hit.first_motion : &hit.second_motion;
    auto moving_army = army_for(moving->actor_id),
         still_army = army_for(still->actor_id);
    for (auto &fact : s.contact_facts)
      if (fact.motion_sources) {
        auto first = pair(*fact.motion_sources, moving_army->id);
        if (first && first->action_id == moving->action_id &&
            !moving->action_id.empty() &&
            pair(*fact.motion_sources, still_army->id))
          return error<StableId>("MilitaryContactAlreadyUsed");
      }
  }
  StableId aggressor;
  for (auto p :
       {std::pair<const ArmyRecord *, const ArmyRecord *>{first, second},
        {second, first}}) {
    auto campaign = find(s.campaigns, p.first->order_id);
    auto definition =
        campaign ? find(cat.orders, campaign->template_id) : nullptr;
    auto target = definition
                      ? find(cat.strongholds, definition->target_stronghold_id)
                      : nullptr;
    auto hold = target ? find(s.strongholds, target->id) : nullptr;
    if (campaign && definition && hold && campaign->army_id == p.first->id &&
        has({"marching", "operating"}, campaign->status) &&
        definition->objective_kind == "occupy" &&
        hold->owner_organization_id == p.second->organization_id) {
      aggressor = p.first->id;
      break;
    }
  }
  if (aggressor.empty())
    return error<StableId>("MilitaryContactNotHostile");
  std::array<const ContactEndpoint *, 2> endpoints{&hit.contact.first,
                                                   &hit.contact.second};
  if (army_for(endpoints[0]->actor_id)->id != aggressor)
    std::swap(endpoints[0], endpoints[1]);
  MilitaryContactFact f;
  f.tick = c.tick() + 1;
  f.time_num = hit.contact.time_num;
  f.time_den = hit.contact.time_den;
  f.motion_sources.emplace();
  std::vector<std::pair<TerrainExecution, TerrainExecution>> retained;
  for (Integer side = 0; side < 2; ++side) {
    auto e = endpoints[side];
    auto army = army_for(e->actor_id);
    auto motion = hit.first_motion.actor_id == e->actor_id ? &hit.first_motion
                                                           : &hit.second_motion;
    const TerrainExecution *executed = nullptr;
    for (auto &x : executions)
      if (x.actor_id == e->actor_id)
        executed = &x;
    bool moved = false;
    if (!motion->stationary && hit.contact.time_num > 0 &&
        !motion->segments.empty()) {
      PointI previous{
          static_cast<Integer>(std::llround(motion->segments.front().from.x)),
          static_cast<Integer>(std::llround(motion->segments.front().from.y))};
      auto plan = travel.journey_facts.find(motion->params.plan_id);
      for (Integer i = motion->params.route_index;
           plan && i < e->trace.params.route_index; ++i) {
        if (!same_point(previous, plan->points[i]))
          moved = true;
        previous = plan->points[i];
      }
      if (!same_point(previous, e->anchor.point))
        moved = true;
    }
    if (cat.march_condition_model && moved && !executed)
      return error<StableId>("MilitaryMarchExecutionRequired");
    if (cat.march_condition_model && executed) {
      if (motion->stationary || motion->segments.empty())
        return error<StableId>("MilitaryMarchExecutionMismatch");
      auto action = c.action(motion->action_id);
      PointI start{
          static_cast<Integer>(std::llround(motion->segments.front().from.x)),
          static_cast<Integer>(std::llround(motion->segments.front().from.y))};
      if (motion->stationary || !action ||
          executed->action_id != motion->action_id ||
          executed->action_start_tick != action->start_tick ||
          executed->source_tick != c.tick() ||
          executed->plan_id != motion->params.plan_id ||
          executed->map_id != motion->map_id ||
          !same_point(executed->from_point, start) ||
          !same_point(executed->to_point, motion->end) ||
          executed->route_index_before != motion->params.route_index ||
          executed->route_index_after !=
              motion->params.route_index + motion->reached_points ||
          executed->travel_credit_before != motion->params.travel_credit ||
          executed->travel_credit_after != motion->end_credit ||
          !same_location(executed->result_location,
                         c.actor(e->actor_id)->location))
        return error<StableId>("MilitaryMarchExecutionMismatch");
    }
    if (!e->trace.stationary) {
      auto params = e->trace.params;
      auto peer = endpoints[1 - side]->trace.action_id;
      if (!peer.empty() && !has(params.met_actions, peer))
        params.met_actions.push_back(peer);
      travel.actions.set(e->trace.action_id, std::move(params));
      ActorMutation m;
      m.actor_id = e->actor_id;
      m.location = e->anchor;
      auto relocated = c.apply_actor(m);
      if (!relocated.ok)
        return {false, {}, relocated.error};
    }
    auto person = c.actor(e->actor_id);
    f.side_by_army.emplace_back(army->id, side);
    f.army_order_ids.emplace_back(army->id, army->order_id);
    f.army_locations.emplace_back(army->id, person->location);
    MilitaryMotionSource source;
    source.actor_id = e->actor_id;
    source.action_id = e->trace.action_id;
    source.plan_id = e->trace.params.plan_id;
    source.stationary = e->trace.stationary;
    source.location = person->location;
    if (cat.march_condition_model && !source.stationary)
      source.route_index = e->trace.params.route_index;
    f.motion_sources->emplace_back(army->id, source);
    for (auto &uid : army->unit_ids) {
      auto u = find(s.military_units, uid);
      if (u && !u->fit.empty())
        f.unit_ids.push_back(uid);
    }
    if (cat.march_condition_model && moved) {
      auto prefix = *executed;
      prefix.to_point = e->anchor.point;
      prefix.route_index_after = e->trace.params.route_index;
      prefix.travel_credit_after = e->trace.params.travel_credit;
      prefix.result_location = person->location;
      retained.emplace_back(prefix, *executed);
    }
  }
  auto campaign = find(s.campaigns, find(s.armies, aggressor)->order_id);
  auto definition = find(cat.orders, campaign->template_id);
  auto target = find(cat.strongholds, definition->target_stronghold_id);
  auto topology =
      target ? find(cat.siege_topologies, target->topology_id) : nullptr;
  if (topology)
    for (auto &n : topology->nodes)
      if (n.id == topology->attacker_entry_id)
        f.battlefield_id = n.battlefield_id;
  if (cat.field_terrain_policy) {
    auto selected = select_military_field_contact_battlefield(
        *cat.field_terrain_policy, f.battlefield_id);
    if (!selected.ok)
      return {false, {}, selected.error};
    f.battlefield_id = selected.value;
  }
  auto field = find(cat.battle_catalog.battlefields, f.battlefield_id);
  if (!field)
    return error<StableId>("MilitaryContactBattlefieldRequired");
  f.objective_id = field->objective_id;
  auto id = c.next_id("contact");
  if (!id.ok)
    return {false, {}, id.error};
  f.id = id.value;
  auto recorded = c.append_fact({f.id, "military_contact", "military"});
  if (!recorded.ok)
    return {false, {}, recorded.error};
  s.contact_facts.push_back(f);
  for (auto &execution : retained) {
    auto marched = apply_military_march(c, s, cat, access, execution.first,
                                        c.tick() + 1, f.id, execution.second);
    if (!marched.ok)
      return {false, {}, marched.error};
  }
  return create_military_battle(c, s, cat, access, f.id);
}
} // namespace hanmo::resident

namespace hanmo::resident {
Status apply_original_field_captured_actor_change(
    CommonStore &common, const MilitaryState &military,
    const FieldBattleWorldState &field, const SourceRef &casualty,
    const StableId &actor_id, const FieldSegmentApplicationRead &clock) {
  auto original = read_field_segment_captivity_source(
      common, military, field, casualty, actor_id, clock);
  if (!original.ok) return {false, {}, original.error};
  auto now = clock.read_current_moment();
  if (!now.ok) return {false, {}, now.error};
  auto actual = compare_world_moments(
      now.value, original.value.source.settlement->applied_at);
  if (!actual.ok) return {false, {}, actual.error};
  if (actual.value || !common.actor_condition_capture_identity() ||
      !common.actor_location_capture_identity())
    return common_failure("FieldCaptureActualWriteBoundaryRequired", actor_id);
  const auto *actor = common.actor(actor_id);
  // Group members carry no independently payable personal travel action.
  // Existing standalone travel cancellation is NOT silently skipped here.
  if (!actor || !actor->alive || !actor->action_id.empty())
    return common_failure("FieldCaptureOriginalMemberRequired", actor_id);
  try {


    const auto memory = CheckpointMemoryCharge(0);
    ActorMutation mutation;
    mutation.actor_id = checkpoint_fresh_string_copy(actor_id);
    mutation.available = false;
    mutation.location = InArmyLocation{checkpoint_fresh_string_copy(
        original.value.actor_change->captor_army_id)};
    // These are exactly the original captured branch's two fields. No second
    // body write, new custody account, clock advance or group release occurs.
    return common.apply_actor(mutation);
  } catch (const std::bad_alloc &) {
    return common_failure("FieldCaptureResourceExhausted", actor_id,
                          ErrorKind::internal);
  }
}
} // namespace hanmo::resident
