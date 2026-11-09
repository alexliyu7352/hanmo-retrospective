#include "field_event_registry.hpp"
#include "field_event_legacy_kinds.hpp"
#include "field_command_source.hpp"
// 实际 Session 只读投影；知情裁剪发生于复制 DTO 前，不输出另一份名册或随机流。
#include "field_deployment.hpp"
#include "field_geometry_view.hpp"
#include "field_view.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/views/common_presentation.hpp"
#include <algorithm>
#include <limits>
#include <set>
#include <type_traits>
namespace hanmo::resident {
namespace {
// 枚举来自已验证有限记录，未知值明确拒绝，不退回第一种状态。
template <class T, std::size_t N>
const char *label(T value, const char *const (&names)[N]) {
  auto index = static_cast<std::size_t>(value);
  return index < N ? names[index] : nullptr;
}
constexpr const char *terrain_names[]{
    "Plain", "Field",  "Forest", "Rough",   "Steep", "Stream", "Water",
    "Road",  "Bridge", "Ford",   "Village", "Wall",  "Gate"};
constexpr const char *formation_names[]{"Deploying", "Moving",  "Holding",
                                        "Engaged",   "Routing", "Withdrawn",
                                        "Destroyed"};
constexpr const char *category_names[]{"Infantry", "Archer", "Cavalry"};
constexpr const char *trait_names[]{"cavalry_master", "infantry_master",
                                    "archery_master", "inspiring",
                                    "staff_work",     "valiant"};
constexpr const char *tactic_names[]{"charge", "volley", "rally", "guard"};
constexpr const char *status_names[]{"confused", "suppressed", "guarding",
                                     "charging", "hidden"};
constexpr const char *order_names[]{"in_transit", "adopted",
                                    "delivered",  "not_responding",
                                    "superseded", "undeliverable"};
constexpr const char *stratagem_names[]{"ambush", "feint_strength", "confuse"};
bool before(const FieldBattleTime &a, const FieldBattleTime &b) {
  return a.tick < b.tick || (a.tick == b.tick && a.t < b.t);
}
// 原五种任务仅公开对应目标；不把同一任意字典作为规则输入。
FieldMissionView mission_view(const FieldMission &mission) {
  return std::visit(
      [](const auto &source) {
        FieldMissionView out;
        using T = std::decay_t<decltype(source)>;
        if constexpr (std::is_same_v<T, FieldMoveTo> ||
                      std::is_same_v<T, FieldHold>) {
          out.kind = std::is_same_v<T, FieldMoveTo> ? "MoveTo" : "Hold";
          out.point_dm = source.point_dm;
          out.facing_mdeg = source.facing_mdeg;
        } else if constexpr (std::is_same_v<T, FieldEngage>) {
          out.kind = "Engage";
          out.target_formation_id = source.target_formation_id;
        } else if constexpr (std::is_same_v<T, FieldSupport>) {
          out.kind = "Support";
          out.target_formation_id = source.ally_formation_id;
        } else {
          out.kind = "Withdraw";
          out.exit_side = source.exit_side;
        }
        return out;
      },
      mission);
}
// 所有情报已由本侧军议 producer 分级；不读对侧原 Council 偷补缺项。
FieldCouncilView council_view(const FieldCouncilRecord &source) {
  FieldCouncilView out;
  out.participants = source.participants;
  out.intel = source.intel;
  out.adopted_proposal_id = source.adopted_proposal_id;
  out.closed = source.closed;
  for (const auto &p : source.proposals)
    out.proposals.push_back(
        {p.proposal_id, p.speaker_actor_id, p.stance, p.plan, p.text_key});
  for (const auto &r : source.requests)
    out.requests.push_back({r.actor_id, r.value, r.status, r.reason});
  return out;
}
// hidden 直到真实 producer 移除才现身；读口绝不替它结束效果或扣计策点。
bool hidden(const FieldFormation &formation) {
  return std::any_of(
      formation.statuses.begin(), formation.statuses.end(),
      [](const auto &s) { return s.kind == FieldStatusKind::Hidden; });
}
} // namespace
static Outcome<FieldBattleView> read_field_view_projection(
    const CommonStore &common, const MilitaryState &military,
    const TravelCatalog &travel, const FieldBattleCatalog &catalog,
    const FieldBattleWorldState &world, const StableId &observer,
    const FieldBattleRead &request,
    const FieldBattleAbilityReadContext &abilities, bool local_candidate) {
  if (abilities.phase)
    return {false,
            {},
            common_failure("FieldViewRequiresCommittedWorld", request.battle_id)
                .error};
  const auto &common_catalog = abilities.common;
  const auto &military_catalog = abilities.military;
  const MilitaryTacticLearningCatalog unselected_catalog;
  const MilitaryTacticLearningState unselected_state;
  const auto &selected_learning =
      military_catalog.battle_catalog.tactic_learning;
  if (selected_learning && selected_learning->enabled && !abilities.learning)
    return {
        false,
        {},
        common_failure("FieldAbilityLearningStateRequired", request.battle_id)
            .error};
  const auto &learning_catalog =
      selected_learning ? *selected_learning : unselected_catalog;
  const auto &learning =
      abilities.learning ? *abilities.learning : unselected_state;
  auto geometry =
      local_candidate
          ? read_field_battle_candidate_geometry(
                common, military, travel, catalog, world, request.battle_id,
                observer, request.known_field_hash, &abilities)
          : read_field_battle_geometry(common, military, travel, catalog, world,
                                       request.battle_id, observer,
                                       request.known_field_hash, &abilities);
  if (!geometry.ok)
    return {false, {}, geometry.error};
  const auto &g = geometry.value;
  const auto &session = world.sessions.at(request.battle_id);
  if (request.after_event_sequence < 0 ||
      request.after_event_sequence >= session.next_event_sequence)
    return {false,
            {},
            common_failure("FieldEventCursorInvalid", request.battle_id).error};
  CommonPresentation presentation(common.state(), common_catalog);
  FieldBattleView out;
  out.identity = {session.battle_id, observer,    session.segment,
                  common.revision(), session.now, session.state};
  out.field.anchor = session.anchor;
  out.field.field_hash = session.field_hash;
  out.field.length_m = g.length_m;
  out.field.width_m = g.width_m;
  out.field.cell_m = g.cell_m;
  if (g.cells) {
    out.field.cells.emplace();
    out.field.cells->reserve(g.cells->size());
    for (const auto &cell : *g.cells) {
      const auto *name = label(cell.terrain, terrain_names);
      if (!name)
        return {false, {}, common_failure("InvalidFieldBattleTag").error};
      out.field.cells->push_back({name, cell.height_dm});
    }
  }
  for (const auto &side : session.sides) {
    auto name = presentation.display_name(side.commander_actor_id);
    if (!name.ok)
      return {false, {}, name.error};
    out.sides.push_back(
        {side.side, std::move(name.value), side.faction_id,
         std::holds_alternative<FieldPlayerController>(side.controller)
             ? "player"
             : "ai",
         g.observer_side == side.side, side.deploy_zone, side.exits});
  }
  if (g.observer_side && (session.state == FieldBattleState::Council ||
                          session.state == FieldBattleState::Ended))
    out.council = council_view(session.councils[*g.observer_side]);
  std::set<StableId> visible;
  for (const auto &[id, f] : session.formations) {
    const bool own = g.observer_side == f.side;
    // 军议时敌情只从本侧 IntelReport 给出，不能同时泄漏完整敌军行覆盖侦察等级。
    if ((!own && session.state == FieldBattleState::Council) ||
        (!own && hidden(f)))
      continue;
    if (f.ranks <= 0 ||
        f.fit.size() > std::size_t(std::numeric_limits<Integer>::max() / 15))
      return {
          false, {}, common_failure("InvalidFieldFormationCount", id).error};
    auto name = presentation.display_name(f.commander_actor_id);
    if (!name.ok)
      return {false, {}, name.error};
    const auto *state = label(f.state, formation_names),
               *category = label(f.category, category_names);
    if (!state || !category)
      return {false, {}, common_failure("InvalidFieldBattleTag", id).error};
    FieldFormationView row;
    row.formation_id = id;
    row.army_id = f.army_id;
    row.display_name = name.value + "部";
    row.category = category;
    row.troop_type_id = f.troop_type_id;
    row.side = f.side;
    row.position_dm = f.position_dm;
    row.facing_mdeg = f.facing_mdeg;
    row.width_dm = field_formation_width_dm(f);
    row.depth_dm = f.ranks * 15;
    row.state = state;
    const auto *autonomous = std::get_if<FieldAutonomousControl>(&f.control);
    row.control = !autonomous                 ? "direct"
                  : autonomous->dispatched_at ? "dispatched"
                                              : "prebattle";
    row.mission = mission_view(f.mission);
    row.fit = Integer(f.fit.size());
    row.wounded = Integer(f.wounded.size());
    row.dead = Integer(f.dead.size());
    row.captured = Integer(f.captured.size());
    row.deserted = Integer(f.deserted.size());
    row.morale = f.morale;
    row.commander = {f.commander_actor_id, std::move(name.value),
                     f.commander_lost_at ? "Lost" : "Active"};
    row.deputy_actor_id = f.deputy_actor_id;
    row.wave = f.wave;
    if (f.ability) {
      row.tactic_points = f.ability->points;
      for (auto t : f.ability->traits) {
        const auto *n = label(t, trait_names);
        if (!n)
          return {false, {}, common_failure("InvalidFieldBattleTag", id).error};
        row.traits.emplace_back(n);
      }
      for (const auto &t : f.ability->tactics) {
        const auto *n = label(t.source.kind, tactic_names);
        if (!n)
          return {false, {}, common_failure("InvalidFieldBattleTag", id).error};
        row.tactics.push_back(
            {t.source.tactic_id, n, t.source.point_cost, t.ready_at});
      }
    } else {
      if (session.started_at)
        return {false,
                {},
                common_failure("FieldAbilityStateRequired", id,
                               ErrorKind::unsupported_capability)
                    .error};
      // 开战前仅读真实可用来源；点数尚未补满，读口不代替 Start 冻结能力。
      auto observed_abilities = read_field_formation_abilities(
          common, abilities, military_catalog.battle_catalog, learning_catalog,
          learning, f, session.sides[f.side].commander_actor_id, common.tick());
      if (!observed_abilities.ok)
        return {false, {}, observed_abilities.error};
      for (auto t : observed_abilities.value.traits) {
        const auto *n = label(t, trait_names);
        if (!n)
          return {false, {}, common_failure("InvalidFieldBattleTag", id).error};
        row.traits.emplace_back(n);
      }
      for (const auto &t : observed_abilities.value.tactics) {
        const auto *n = label(t.kind, tactic_names);
        if (!n)
          return {false, {}, common_failure("InvalidFieldBattleTag", id).error};
        row.tactics.push_back({t.tactic_id, n, t.point_cost, {}});
      }
    }
    for (const auto &status : f.statuses) {
      const auto *n = label(status.kind, status_names);
      if (!n)
        return {false, {}, common_failure("InvalidFieldBattleTag", id).error};
      row.statuses.push_back({n, status.until});
    }
    visible.insert(id);
    out.formations.push_back(std::move(row));
  }
  if (g.observer_side)
    for (const auto &[id, order] : session.orders) {
      const auto recipient = session.formations.find(order.target_formation_id);
      if (recipient == session.formations.end())
        return {
            false, {}, common_failure("FieldOrderSourceRequired", id).error};
      if (recipient->second.side != *g.observer_side)
        continue;
      FieldOrderView row;
      row.order_id = id;
      row.recipient = order.target_formation_id;
      row.issued = order.issued;
      if (const auto *mission = std::get_if<FieldMission>(&order.payload))
        row.mission = mission_view(*mission);
      else
        row.tactic_id = std::get<FieldTacticIntent>(order.payload).tactic_id;
      bool personally_delivered = false;
      if (order.status == FieldOrderStatus::Delivered) {
        auto original = read_field_original_control_before_order_resolution(
            common, world, session, order);
        if (!original.ok)
          return {false, {}, original.error};
        personally_delivered = original.value.direct &&
                               original.value.controller_actor_id == observer;
      }
      const bool reported =
          personally_delivered ||
          (order.report_due && !before(session.now, *order.report_due));
      const auto *known = label(order.status, order_names);
      if (!known)
        return {false, {}, common_failure("InvalidFieldBattleTag", id).error};
      row.status = reported                         ? known
                   : before(session.now, order.due) ? "in_transit"
                                                    : "unconfirmed";
      if (!reported)
        row.eta_range = std::array<FieldBattleTime, 2>{order.due, order.due};
      out.orders.push_back(std::move(row));
    }
  for (const auto &event : session.events) {
    // 登记只选择原公开/私有/内部类别，不生成观察者的知情事实。
    const auto *definition = find_field_event_kind(event.kind);
    if (!definition) return {false,{},common_failure("InvalidFieldBattleTag").error};
    if (definition->exposure == FieldEventExposure::internal) continue;
    if (event.sequence <= request.after_event_sequence)
      continue;
    const bool private_side_event = definition->exposure == FieldEventExposure::private_side;
    if (private_side_event && event.side && g.observer_side != event.side)
      continue;
    if ((event.formation_id && !visible.contains(*event.formation_id)) ||
        (event.target_formation_id &&
         !visible.contains(*event.target_formation_id)))
      continue;
    const auto *kind = definition->public_name;
    if (!kind)
      return {false, {}, common_failure("InvalidFieldBattleTag").error};
    FieldBattleEventView row;
    row.sequence = event.sequence;
    row.at = event.at;
    row.kind = kind;
    row.formation_id = event.formation_id;
    row.target_formation_id = event.target_formation_id;
    row.count = event.count;
    row.success = event.success;
    if (event.tactic) {
      const auto *n = label(*event.tactic, tactic_names);
      if (!n)
        return {false, {}, common_failure("InvalidFieldBattleTag").error};
      row.tactic = n;
    }
    if (event.stratagem) {
      const auto *n = label(*event.stratagem, stratagem_names);
      if (!n)
        return {false, {}, common_failure("InvalidFieldBattleTag").error};
      row.stratagem = n;
    }
    out.events.push_back(std::move(row));
  }
  out.result = session.result;
  return {true, std::move(out), {}};
}
Outcome<FieldBattleView>
read_field_battle_view(const CommonStore &common, const MilitaryState &military,
                       const TravelCatalog &travel,
                       const FieldBattleCatalog &catalog,
                       const FieldBattleWorldState &world,
                       const StableId &observer, const FieldBattleRead &request,
                       const FieldBattleAbilityReadContext &abilities) {
  return read_field_view_projection(common, military, travel, catalog, world,
                                    observer, request, abilities, false);
}
Outcome<FieldBattleView> read_field_battle_candidate_view(
    const CommonStore &common, const MilitaryState &military,
    const TravelCatalog &travel, const FieldBattleCatalog &catalog,
    const FieldBattleWorldState &world, const StableId &observer,
    const FieldBattleRead &request,
    const FieldBattleAbilityReadContext &abilities) {
  return read_field_view_projection(common, military, travel, catalog, world,
                                    observer, request, abilities, true);
}
} // namespace hanmo::resident
