// 当前战场位置仅沿原军团、接触和编队闭包读取，不写世界或创建缺失记录。
#include "field_world.hpp"
#include "field_active_membership.hpp"
#include "field_binding_source.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace hanmo::resident {
namespace {
// 绑定来源损坏是内部权威失败；不能降为“未在战”而放行世界移动。
template <class T> Outcome<T> invalid(const StableId &id) {
  return {false,
          {},
          common_failure("FieldBattleBindingInvalid", id, ErrorKind::internal)
              .error};
}
// 一军只有一个真实在战绑定；只读确认其出生出处与冻结的统帅锚点。
Status validate_binding(const CommonRead &common, const MilitaryState &,
                        const FieldBattleWorldState &world, const StableId &key,
                        const FieldBattleRoadBinding &binding,
                        const FieldBattleActiveMembershipRead *membership) {
  if (membership) {
    const auto *store = dynamic_cast<const CommonStore *>(&common);
    if (!store || !membership->bound_to(*store, world) ||
        key != binding.army_id)
      return common_failure("FieldActiveMembershipOriginalOwnerRequired", key);
    auto original = read_field_original_battle_binding(*store, world,
                                                       binding.battle_id, key);
    if (!original.ok)
      return {false, {}, original.error};
    if (binding.action_id != original.value->action_id ||
        binding.commander_actor_id != original.value->commander_actor_id ||
        binding.members.size() != original.value->members.size())
      return {false, {}, invalid<int>(key).error};
    std::set<StableId> unique;
    for (const auto &member : binding.members) {
      if (!unique.insert(member.actor_id).second ||
          std::count_if(original.value->members.begin(),
                        original.value->members.end(), [&](const auto &v) {
                          return v.actor_id == member.actor_id &&
                                 v.formation_id == member.formation_id;
                        }) != 1)
        return {false, {}, invalid<int>(member.actor_id).error};
      auto actual = membership->read_field_active_member_engagement(
          binding.battle_id, member.actor_id);
      if (!actual.ok)
        return {false, {}, actual.error};
      if (!has_field_engagement_borrowed_entry(actual.value.entry) ||
          !actual.value.battle_id ||
          *actual.value.battle_id != binding.battle_id)
        return common_failure("FieldActiveMembershipActualEntryRequired",
                              member.actor_id);
      if (has_field_engagement_borrowed_release(actual.value.release))
        continue;
      // Entry and absence of an adopted member release retain the hold even
      // for a dead/captured actor. S1 owns actual location/custody history;
      // neither an old commander OnRoute nor today's InArmy is entry proof.
    }
    return common_success();
  }
  return common_failure("FieldActiveMembershipOriginalOwnerRequired", key);
}

} // namespace
Status append_field_battle_event(FieldBattleSession &session,
                                 FieldBattleEvent event) {
  if (session.next_event_sequence <= 0 ||
      session.next_event_sequence == std::numeric_limits<Integer>::max())
    return common_failure("FieldEventSequenceExhausted", session.battle_id);
  event.sequence = session.next_event_sequence;
  event.at = session.now;
  session.events.push_back(std::move(event));
  ++session.next_event_sequence;
  return common_success();
}
Outcome<std::optional<FieldBattleSpatialView>> read_field_battle_actor_position(
    const CommonRead &common, const MilitaryState &military,
    const FieldBattleWorldState &world, const StableId &actor,
    const FieldBattleActiveMembershipRead *membership) {
  if (world.schema != 1)
    return invalid<std::optional<FieldBattleSpatialView>>(actor);
  if (!common.actor(actor))
    return {false, {}, common_failure("UnknownActor", actor).error};
  std::optional<FieldBattleSpatialView> result;
  for (const auto &[key, binding] : world.army_bindings) {
    auto valid =
        validate_binding(common, military, world, key, binding, membership);
    if (!valid.ok)
      return {false, {}, valid.error};
    for (const auto &member : binding.members)
      if (member.actor_id == actor) {
        if (membership) {
          auto actual = membership->read_field_active_member_engagement(
              binding.battle_id, actor);
          if (!actual.ok)
            return {false, {}, actual.error};
          if (has_field_engagement_borrowed_release(actual.value.release))
            continue;
        }
        if (result)
          return invalid<std::optional<FieldBattleSpatialView>>(actor);
        const auto &session = world.sessions.at(binding.battle_id);
        const auto &formation = session.formations.at(member.formation_id);
        if (formation.state == FieldFormationState::Deploying)
          return {
              false,
              {},
              common_failure("FieldFormationNotDeployed", member.formation_id)
                  .error};
        result =
            FieldBattleSpatialView{binding.battle_id,    key,
                                   member.formation_id,  session.field_hash,
                                   formation.side,       session.now,
                                   formation.position_dm};
        // session战斗终局不回写；已验证领域表的唯一step才是汇合者当前位置。
        // 此处只投影真实Common行，不授新的移动/ready权限；完整几何由领域cold表反证。
        const auto *store = dynamic_cast<const CommonStore *>(&common);
        const FieldArmyExitPlan *plan = nullptr;
        for (const auto &[id, candidate] : world.army_exit_plans)
          if (candidate.battle_id == binding.battle_id && candidate.army_id == key && candidate.terminal_segment) {
            if (plan || !store) return invalid<std::optional<FieldBattleSpatialView>>(actor);
            plan = &candidate;
          }
        if (plan) {
          const FieldArmyExitRallyFormation *origin = nullptr;
          for (const auto &row : plan->rally_formations)
            if (row.formation_id == member.formation_id) {
              if (origin) return invalid<std::optional<FieldBattleSpatialView>>(actor);
              origin = &row;
            }
          if (origin) {
            const bool ordinary = std::count(origin->ordinary_member_ids.begin(), origin->ordinary_member_ids.end(), actor) == 1;
            const bool officer = std::count(origin->retained_officer_ids.begin(), origin->retained_officer_ids.end(), actor) == 1;
            if (ordinary || officer) {
              auto previous = read_actual_common_fact_position(*store, *plan->terminal_segment);
              if (!previous.ok) return {false, {}, previous.error};
              PointI point = origin->initial_position_dm;
              for (const auto &step : plan->steps) {
                auto head = read_actual_common_fact_position(*store,
                    SourceRef{"military", "field_army_exit_step", step.fact_id});
                if (!head.ok) return {false, {}, head.error};
                if (head.value <= previous.value) return invalid<std::optional<FieldBattleSpatialView>>(actor);
                previous = head;
                const FieldArmyExitMotion *motion = nullptr;
                for (const auto &row : step.formations) if (row.formation_id == member.formation_id) {
                  if (motion) return invalid<std::optional<FieldBattleSpatialView>>(actor);
                  motion = &row;
                }
                if (!motion || motion->from_dm.x != point.x || motion->from_dm.y != point.y)
                  return invalid<std::optional<FieldBattleSpatialView>>(actor);
                point = motion->to_dm;
                result->point_dm = point; result->now = step.to;
                // 普通兵从本步实际位置逃散后不随仍在走的将领继续移动。
                if (std::find(motion->deserted_member_ids.begin(), motion->deserted_member_ids.end(), actor) != motion->deserted_member_ids.end()) break;
              }
            }
          }
        }
      }
  }
  return {true, std::move(result), {}};
}
Outcome<std::vector<StableId>> read_field_battle_held_actors(
    const CommonRead &common, const MilitaryState &military,
    const FieldBattleWorldState &world,
    const FieldBattleActiveMembershipRead *membership) {
  if (world.schema != 1)
    return invalid<std::vector<StableId>>("field_battle/version");
  std::vector<StableId> result;
  std::set<StableId> unique;
  for (const auto &[key, binding] : world.army_bindings) {
    auto valid =
        validate_binding(common, military, world, key, binding, membership);
    if (!valid.ok)
      return {false, {}, valid.error};
    // 尚未摆位也是真正在战；hold 只核真实绑定，不借未部署的位置放行。
    for (const auto &member : binding.members) {
      if (membership) {
        auto actual = membership->read_field_active_member_engagement(
            binding.battle_id, member.actor_id);
        if (!actual.ok)
          return {false, {}, actual.error};
        if (has_field_engagement_borrowed_release(actual.value.release))
          continue;
      }
      if (!unique.insert(member.actor_id).second)
        return invalid<std::vector<StableId>>(member.actor_id);
      result.push_back(member.actor_id);
    }
  }
  std::sort(result.begin(), result.end());
  return {true, std::move(result), {}};
}
Outcome<std::vector<StableId>> read_field_battle_held_armies(
    const CommonRead &common, const MilitaryState &military,
    const FieldBattleWorldState &world,
    const FieldBattleActiveMembershipRead *membership) {
  auto actors =
      read_field_battle_held_actors(common, military, world, membership);
  if (!actors.ok)
    return {false, {}, actors.error};
  std::vector<StableId> result;
  for (const auto &[key, binding] : world.army_bindings)
    result.push_back(key);
  return {true, std::move(result), {}};
}
Outcome<FieldWorldAdvanceGate>
read_field_world_advance_gate(const FieldBattleWorldState &world,
                              const StableId &player_actor_id,
                              Integer committed_tick) {
  if (world.schema != 1)
    return invalid<FieldWorldAdvanceGate>("field_battle/schema");
  const FieldBattleSession *first = nullptr;
  for (const auto &[id, session] : world.sessions) {
    if (id != session.battle_id)
      return invalid<FieldWorldAdvanceGate>(id);
    if (session.state == FieldBattleState::Ended)
      continue;
    if (committed_tick < 0 || session.now.tick > committed_tick)
      return invalid<FieldWorldAdvanceGate>(id);
    // 宏观 T→T+1 尚未提交时，绝不提前细算 [T,T+1)。
    // Council/布阵仍是零时决定；Running 在下一真实窗口开放前不挡世界提交。
    if (session.state == FieldBattleState::Running &&
        session.now.tick == committed_tick)
      continue;
    if (session.state != FieldBattleState::Council &&
        session.state != FieldBattleState::Deploying &&
        session.state != FieldBattleState::Running)
      return invalid<FieldWorldAdvanceGate>(id);
    if (!first || std::tie(session.created_tick, id) <
                      std::tie(first->created_tick, first->battle_id))
      first = &session;
  }
  if (!first)
    return {true, {}, {}};
  bool player = std::any_of(
      first->sides.begin(), first->sides.end(), [](const auto &side) {
        return std::holds_alternative<FieldPlayerController>(side.controller);
      });
  // 军议的等待者由实际安装记录决定；后续 metadata 不能使待发言窗口静默消失。
  if (first->state == FieldBattleState::Council)
    for (const auto &council : first->councils)
      if (!council.closed && council.player_actor_id)
        player = true;
  // 原玩家援军可能统帅最后入场；首个真实编队的待令标记也必须挡住 NPC 自动推进。
  // 此处只增加等待，不授予命令权限；原 entry/Common/queue validator 验其来源。
  for (const auto &[id, formation] : first->formations)
    if (formation.pending_player_entry_order)
      player = true;
  // 玩家只领一部时侧统帅仍是 AI，不能据侧 controller 将其当全 NPC 自动推进。
  for (const auto &[army, binding] : world.army_bindings)
    if (binding.battle_id == first->battle_id)
      for (const auto &member : binding.members)
        if (member.actor_id == player_actor_id)
          player = true;
  return {true,
          {player ? FieldWorldAdvanceReason::player_input
                  : FieldWorldAdvanceReason::local_settlement,
           first->battle_id},
          {}};
}
} // namespace hanmo::resident
