#include "native_field_window_driver.hpp"
#include "native_field_army_exit_return.hpp"
#include "exact_complete_world.hpp"
#include "native_complete_world_core.hpp"
#include "native_field_application_read.hpp"
#include "native_field_group_world.hpp"
#include "native_field_world_effects.hpp"
#include "native_field_world_scope.hpp"
#include "resident/domains/military/field_battle/field_ability_sources.hpp"
#include "resident/domains/military/field_battle/field_army_exit_rally.hpp"
#include "resident/domains/military/field_battle/field_army_exit_return.hpp"
#include "resident/domains/military/field_battle/field_author_data.hpp"
#include "resident/domains/military/field_battle/field_contact_origin.hpp"
#include "resident/domains/military/field_battle/field_group_handoff.hpp"
#include "resident/domains/military/field_battle/field_local_step.hpp"
#include "resident/domains/military/field_battle/field_reinforcement_closing.hpp"
#include "resident/domains/military/field_battle/field_segment_settlement.hpp"
#include "world_bundle.hpp"
#include "world_read_stamp.hpp"
#include <algorithm>
#include <limits>
#include <map>
#include <new>
#include <set>
#include <stdexcept>
#include <tuple>

namespace hanmo::resident {
namespace {
// 只搬运原错误；本层没有回滚半个领域或发布局部成功的接口。
struct Failure {
  RuleError error;
};
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
void take(Status value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
}
void require(bool value, const char *code, const StableId &id = {}) {
  if (!value)
    throw Failure{common_failure(code, id).error};
}
// 生成只借本场唯一原接触，不从当前两军位置重新选相遇点。
GeneratedBattleField original_field(WorldBundle &world,
                                    NativeFieldWorldScope &scope,
                                    const StableId &battle) {
  const FieldBattleContactRecord *contact = nullptr;
  for (const auto &[id, row] : world.field_battle->contacts) {
    if (row.battle_id != battle)
      continue;
    require(!contact && id == row.contact_id,
            "FieldWindowOriginalContactAmbiguous", battle);
    contact = &row;
  }
  require(contact && scope.exact_contacts(),
          "FieldWindowOriginalContactRequired", battle);
  auto field = take(regenerate_field_contact_battle(
      *world.catalog->travel, *world.catalog->field_battle, contact->source,
      scope.exact_contacts()));
  require(field.field_hash ==
              world.field_battle->sessions.at(battle).field_hash,
          "FieldWindowOriginalGeometryMismatch", battle);
  return field;
}
// 只把这次真正离场的源交给同捕获 S1；空幸存者仍保留上游真实 Exit，
// 其逃散／死亡／被俘由各自唯一原身体结果采用，不伪造空成员 release。
void handoff_exits(WorldBundle &world, NativeFieldWorldScope &scope,
                   const std::vector<SourceRef> &exits,
                   ExactPhaseWriteContext &writes) {
  require(scope.group_handoffs() && scope.placements(),
          "FieldWindowOriginalHandoffRequired");
  for (const auto &source : exits) {
    const auto found = world.field_battle->exits.find(source.id);
    require(found != world.field_battle->exits.end(),
            "FieldWindowActualExitRequired", source.id);
    if (found->second.named_actor_ids.empty())
      continue;
    take(apply_field_exit_group_handoff_in_candidate(
        world.common, *world.movement_exact, *world.group_movement_executions,
        *scope.group_handoffs(), source, *scope.placements(),
        writes.spatial_location_writes, world.checkpoint_copy_memory));
  }
}
// 同一个真实整数 M 内仅走该场原 STEP_S；从不把本地秒换成全局 WorldMoment。
bool run_window(WorldBundle &world, CompleteExactWorldBindingOwner &owner,
                NativeFieldGroupWorld &group, const StableId &battle,
                ExactPhaseWriteContext &writes) {
  auto &scope = group.field();
  const auto &abilities = scope.abilities();
  const auto *clock =
      owner.reads().original_core->characters.early.field_application_clock;
  require(clock && scope.exact_contacts() && scope.diplomacy() &&
              abilities.actual_clock == clock,
          "FieldWindowOriginalSameClockRequired", battle);
  auto field = original_field(world, scope, battle);
  const auto &parameters = field.parameters;
  take(validate_field_battle_parameters(parameters));
  std::vector<SourceRef> exits;
  std::set<StableId> exit_formations;
  const auto limit = parameters.window_s / parameters.step_s;
  Integer steps = 0;
  auto &session = world.field_battle->sessions.at(battle);
  require(session.state == FieldBattleState::Running &&
              session.started_source && session.started_actual_at,
          "FieldWindowOriginalNpcStartRequired", battle);
  const auto actual = take(clock->read_field_window_closing_moment());
  require(actual.half_day_tick == world.common.tick() &&
              take(compare_fractions(actual.sub_half_day,
                                     CanonicalFraction{})) == 0,
          "FieldWindowPublishedMacroBoundaryRequired", battle);
  const auto original_tick = world.common.tick();
  bool settled = false;
  while (session.state == FieldBattleState::Running &&
         session.now.tick < original_tick) {
    require(++steps <= limit, "FieldWindowStepBoundExceeded", battle);
    const auto next = take(
        add_field_battle_seconds(session.now, parameters.step_s, parameters));
    // 世界宏观已真实完成；原 S1 接触只决定局部 entry，不冒充实际 closing。
    require(abilities.original_rosters, "FieldWindowOriginalReinforcementRosterRequired", battle);
    const auto arrivals = take(admit_due_field_reinforcements_at_closing_in_candidate(
        world.common, *world.field_battle, *scope.exact_contacts(),
        *scope.diplomacy(), *world.catalog->field_battle,
        *abilities.original_rosters, abilities, field, battle,
        world.checkpoint_copy_memory));
    if (arrivals.player_input_required) {
      require(world.common.tick() == original_tick &&
                  take(compare_world_moments(actual, take(clock->read_current_moment()))) == 0,
              "FieldWindowChangedWorldTime", battle);
      return true;
    }
    auto slice = take(read_field_window_at_actual_boundary(
        world.common, world.military, *world.field_battle, battle, *clock, next,
        parameters));
    auto applied = take(apply_field_local_step_in_candidate(
        world.common, world.military, *world.catalog->military,
        *world.catalog->travel, *world.catalog->field_battle,
        *world.field_battle, battle, field, abilities, *clock, slice,
        world.checkpoint_copy_memory));
    for (const auto &formation : applied.departures.exited_formations) {
      require(exit_formations.insert(formation).second,
              "FieldWindowDuplicateDeparture", formation);
      exits.push_back(take(record_field_exit_in_candidate(
          world.common, *world.field_battle, *scope.exact_contacts(),
          *scope.diplomacy(), abilities.territory, *world.catalog->field_battle,
          battle, formation, world.checkpoint_copy_memory)));
    }
    if (!applied.settlement_due)
      continue;
    const auto previous = session.settlements.size();
    auto result = take(settle_field_battle_segment_in_candidate(
        world.common, world.military, *world.field_battle,
        *world.catalog->field_battle, battle, *clock));
    require(!settled && session.settlements.size() == previous + 1 &&
                result.casualty_source_ids.size() == 1 &&
                session.settlements.back().casualty_fact_ids ==
                    result.casualty_source_ids,
            "FieldWindowFreshSettlementRequired", battle);
    const SourceRef source{"military", "field_battle_segment",
                           session.settlements.back().fact_id};
    take(apply_native_field_segment_injuries_in_candidate(world, owner, source,
                                                          writes));
    take(apply_native_field_segment_captivity_in_candidate(world, owner, source,
                                                           writes));
    take(apply_native_field_segment_deaths_in_candidate(world, owner, source,
                                                        writes));
    take(apply_native_field_segment_desertions_in_candidate(world, owner,
                                                            source, writes));
    handoff_exits(world, scope, exits, writes);
    // 真实终局与本段退出/人物后果全部采用后才建立汇合原点；此处不移动或开返程。
    if (session.state == FieldBattleState::Ended)
      take(begin_field_army_exit_rallies_in_candidate(
          world.common, *world.field_battle, *world.catalog->field_battle,
          abilities, source, world.checkpoint_copy_memory));
    settled = true;
    break;
  }
  require(settled &&
              (session.state == FieldBattleState::Ended ||
               (session.window_complete && session.now.tick == original_tick &&
                session.now.t == 0)),
          "FieldWindowSettlementIncomplete", battle);
  require(world.common.tick() == original_tick &&
              take(compare_world_moments(
                  actual, take(clock->read_current_moment()))) == 0,
          "FieldWindowChangedWorldTime", battle);
  return false;
}
// 终局后沿原STEP推进到本真实closing；每个新step的普通兵后果必须先全部采用。
// 返回的ready仅供下一同候选原BW09消费者，调用本函数本身不代表已开返程。
std::vector<SourceRef> run_army_exit_rally_window(
    WorldBundle &world, CompleteExactWorldBindingOwner &owner,
    NativeFieldGroupWorld &group, const StableId &battle,
    ExactPhaseWriteContext &writes) {
  auto &scope = group.field();
  const auto *contacts = scope.exact_contacts();
  const auto &abilities = scope.abilities();
  // 期末读取只借原同 World 的 Field application clock，不向普通时钟添权限。
  const auto *clock =
      owner.reads().original_core->characters.early.field_application_clock;
  require(contacts && clock && abilities.actual_clock == clock &&
              &contacts->clock == clock && abilities.exact_contacts == contacts,
          "FieldRallyOriginalSameWorldRequired", battle);
  const auto actual = take(clock->read_field_window_closing_moment());
  require(take(compare_world_moments(actual,
      take(abilities.actual_clock->read_current_moment()))) == 0 &&
      actual.half_day_tick == world.common.tick(),
      "FieldRallyActualClosingRequired", battle);
  const auto field = original_field(world, scope, battle);
  take(validate_field_battle_parameters(field.parameters));
  const auto ordinary_steps = field.parameters.window_s / field.parameters.step_s;
  require(ordinary_steps < std::numeric_limits<Integer>::max(),
          "FieldRallyStepBoundExceeded", battle);
  const auto per_army = ordinary_steps + 1;
  std::size_t armies = 0;
  for (const auto &[id, plan] : world.field_battle->army_exit_plans)
    if (plan.battle_id == battle && plan.terminal_segment) ++armies;
  require(per_army > 0 && armies <= std::size_t(std::numeric_limits<Integer>::max() / per_army),
          "FieldRallyStepBoundExceeded", battle);
  const auto maximum = Integer(armies) * per_army;
  Integer applied_count = 0;
  for (;;) {
    std::map<StableId, std::pair<std::size_t, bool>> before;
    for (const auto &[id, plan] : world.field_battle->army_exit_plans)
      if (plan.battle_id == battle && plan.terminal_segment)
        before.emplace(id, std::make_pair(plan.steps.size(), bool(plan.ready_source)));
    const bool changed = take(advance_field_army_exit_rally_step_in_candidate(
        world.common, *world.field_battle, *world.catalog->field_battle, abilities,
        battle, world.checkpoint_copy_memory));
    if (!changed) break;
    require(++applied_count <= maximum, "FieldRallyStepBoundExceeded", battle);
    std::size_t actual_changes = 0;
    std::optional<SourceRef> new_step;
    for (const auto &[id, prior] : before) {
      const auto &plan = world.field_battle->army_exit_plans.at(id);
      require(plan.steps.size() == prior.first || plan.steps.size() == prior.first + 1,
              "FieldRallyUnexpectedStepMutation", id);
      if (plan.steps.size() == prior.first + 1) {
        require(!new_step && !prior.second && !plan.ready_source,
                "FieldRallyDuplicateActualStep", id);
        new_step = SourceRef{"military", "field_army_exit_step", plan.steps.back().fact_id};
        ++actual_changes;
      }
      if (bool(plan.ready_source) != prior.second) {
        require(!prior.second && plan.ready_source && plan.steps.size() == prior.first,
                "FieldRallyUnexpectedReadyMutation", id);
        ++actual_changes;
      }
    }
    require(actual_changes == 1, "FieldRallyActualSourceRequired", battle);
    if (new_step)
      take(apply_native_field_army_exit_ordinary_losses_in_candidate(
          world, owner, *new_step, writes));
  }
  std::vector<SourceRef> ready;
  for (const auto &[id, plan] : world.field_battle->army_exit_plans)
    if (plan.battle_id == battle && plan.ready_source && !plan.actual_continuation) {
      require(plan.ready_applied_at &&
          take(compare_world_moments(*plan.ready_applied_at, actual)) <= 0,
          "FieldRallyUnconsumedReadyClosingRequired", id);
      ready.push_back(*plan.ready_source);
    }
  require(actual.half_day_tick == world.common.tick() &&
      take(compare_world_moments(actual,
          take(abilities.actual_clock->read_current_moment()))) == 0,
      "FieldRallyChangedWorldTime", battle);
  return ready;
}
} // namespace

Status
settle_native_field_npc_windows_in_candidate(WorldBundle &world,
                                             ExactPhaseWriteContext &writes) {
  try {
    require(
        world.catalog && world.catalog->field_battle && world.field_battle &&
            world.world_timeline && world.read_stamps &&
            world.active_exact_write_context() == &writes &&
            !writes.actual_advance_span() &&
            world.common.actor_condition_capture_identity() &&
            world.common.actor_location_capture_identity() &&
            world.world_timeline->boundary_state == PeriodBoundaryState::ready,
        "FieldWindowActualPostMacroCandidateRequired");
    const auto at =
        take(read_world_moment(world.common.tick(), *world.world_timeline));
    require(take(compare_fractions(at.sub_half_day, CanonicalFraction{})) == 0,
            "FieldWindowActualIntegerBoundaryRequired");
    std::set<StableId> consumed, rallied;
    for (;;) {
      // 只处理已真实终局的Army计划，不替玩家打仍在进行的战斗。每场本closing
      // 至多一次；将领跨窗的剩余实走留到下一次原宏观推进后，再取其真实closing。
      std::optional<StableId> pending_rally;
      for (const auto &[id, plan] : world.field_battle->army_exit_plans) {
        if (!plan.terminal_segment || plan.actual_continuation || rallied.contains(plan.battle_id))
          continue;
        const auto session = world.field_battle->sessions.find(plan.battle_id);
        require(session != world.field_battle->sessions.end() &&
            session->second.state == FieldBattleState::Ended && session->second.result,
            "FieldRallyActualTerminalRequired", plan.battle_id);
        pending_rally = plan.battle_id;
        break;
      }
      if (pending_rally) {
        require(rallied.insert(*pending_rally).second,
                "FieldRallyRepeatedActualWindow", *pending_rally);
        auto phase = take(world.enter_phase_read_context(
            "field_local_settlement", "military", {at, at}, at));
        auto owner = take(world.complete_exact_time_bindings());
        require(owner && owner->reads().original_core,
                "FieldWindowOriginalCompleteOwnerRequired");
        auto group = take(NativeFieldGroupWorld::create(
            world, *owner->reads().original_core, &world));
        ReadStampWriteBoundary boundary(*world.read_stamps);
        const auto ready = run_army_exit_rally_window(world, *owner, *group,
            *pending_rally, writes);
        for (const auto &source : ready) {
          auto &scope = group->field();
          require(scope.exact_contacts(), "FieldArmyExitActualOriginalSourcesRequired", source.id);
          // 真正无可用主将时保原ready/hold等待，不称已开程或造空consumer成功。
          if (take(read_field_army_exit_return_wait(*scope.exact_contacts(),
              *world.catalog->field_battle, scope.abilities(), source))) continue;
          take(adopt_native_field_army_exit_ready_in_candidate(
              world, *owner, *group, source, writes));
        }
        continue;
      }
      const auto gate = take(read_field_world_advance_gate(
          *world.field_battle, world.common.state().metadata.player_actor_id,
          world.common.tick()));
      // 玩家在场保持原 input gate；NPC 自动 driver 不为其选择命令或任务。
      if (gate.reason == FieldWorldAdvanceReason::allowed ||
          gate.reason == FieldWorldAdvanceReason::player_input)
        return common_success();
      require(consumed.insert(gate.battle_id).second,
              "FieldWindowRepeatedNpcWindow", gate.battle_id);
      auto phase = take(world.enter_phase_read_context(
          "field_local_settlement", "military", {at, at}, at));
      auto owner = take(world.complete_exact_time_bindings());
      require(owner && owner->reads().original_core,
              "FieldWindowOriginalCompleteOwnerRequired");
      auto group = take(NativeFieldGroupWorld::create(
          world, *owner->reads().original_core, &world));
      ReadStampWriteBoundary boundary(*world.read_stamps);
      if (run_window(world, *owner, *group, gate.battle_id, writes))
        return common_success();
    }
  } catch (Failure &failure) {
    return {false, {}, std::move(failure.error)};
  } catch (const std::bad_alloc &) {
    return common_failure("FieldWindowResourceExhausted", {},
                          ErrorKind::internal);
  } catch (const std::length_error &) {
    return common_failure("FieldWindowResourceExhausted", {},
                          ErrorKind::internal);
  }
}
} // namespace hanmo::resident
