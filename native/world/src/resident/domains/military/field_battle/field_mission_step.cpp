#include "field_event_mission_transition.hpp"
#include "resident/model/common_model.hpp"
#include "field_mission_step.hpp"
#include "field_army_exit_selection.hpp"
#include "field_integer_geometry.hpp"
#include "field_mission_transition.hpp"
#include "field_navigation_source.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <algorithm>
#include <limits>
#include <optional>
namespace hanmo::resident {
namespace {
Status hold(FieldBattleSession &session, FieldFormation &f,
            const GeneratedBattleField &field, const FieldBattleTime &at,
            Integer facing, FieldMissionTransitionReason reason,
            CheckpointMemoryCharge &owner) {
  const auto previous = std::get_if<FieldHold>(&f.mission);
  const bool continuous_hold = previous && f.hold_since &&
                               previous->point_dm.x == f.position_dm.x &&
                               previous->point_dm.y == f.position_dm.y &&
                               previous->facing_mdeg == facing;
  auto changed =
      apply_field_rule_mission_transition(session, f, field, reason, at, owner);
  if (!changed.ok)
    return changed;
  f.facing_mdeg = facing;
  f.state = FieldFormationState::Holding;
  // Reaching the same Hold point on each five-second step is not a new Hold.
  // Resetting here forever prevents the original two-minute recovery rule.
  if (!continuous_hold)
    f.hold_since = at;
  f.ai_pursuit = false;
  f.navigation_target_dm.reset();
  f.navigation_observation_sequence.reset();
  f.navigation_replanned_at.reset();
  f.followup_mission.reset();
  return {true, {}};
}
bool present(const FieldFormation &f) {
  return !f.fit.empty() && f.state != FieldFormationState::Withdrawn &&
         f.state != FieldFormationState::Destroyed &&
         f.state != FieldFormationState::Deploying;
}
__int128_t elapsed(const FieldBattleTime &a, const FieldBattleTime &b,
                   Integer window) {
  return (__int128_t(a.tick) - b.tick) * window + a.t - b.t;
}
} // namespace
Outcome<FieldMissionStepRead> apply_field_mission_step_in_candidate(
    const FieldBattleWorldState &world, FieldBattleSession &session, const GeneratedBattleField &field,
    const FieldBattleWindowSlice &step, CheckpointMemoryCharge &owner,
    const std::function<Status()> &before_actual_motion) {
  auto fail = [](const char *code) -> Outcome<FieldMissionStepRead> {
    return {false, {}, common_failure(code).error};
  };
  if (session.state != FieldBattleState::Running ||
      session.field_hash != field.field_hash || step.steps != 1 ||
      session.now.tick != step.from.tick || session.now.t != step.from.t)
    return fail("FieldMissionStepSourceRequired");
  FieldMissionStepRead out;


  const auto temporary = CheckpointMemoryCharge(0);
  std::vector<const FieldFormation *> held_at_origin;
  std::vector<std::pair<StableId, Integer>> arrived;
  // Original rule changes have no position side effect. Complete all origin
  // transitions before recording any actual movement at the closing boundary.
  for (auto &[id, f] : session.formations) {
    if (f.side < 0 || f.side > 1)
      return fail("FieldFormationSideInvalid");
    if (!present(f))
      continue;
    if (f.pending_player_entry_order)
      continue;
    if (f.wave_mission) {
      bool first = false;
      for (const auto &[other_id, other] : session.formations)
        if (other.side == f.side && other.wave == FieldCouncilWave::First &&
            other.has_engaged)
          first = true;
      if (first) {
        auto changed = apply_field_rule_mission_transition(
            session, f, field, FieldMissionTransitionReason::WaveReleased,
            step.from, owner);
        if (!changed.ok)
          return {false, {}, changed.error};
        f.wave_mission.reset();
        f.hold_since.reset();
        f.navigation_target_dm.reset();
        f.navigation_observation_sequence.reset();
        f.navigation_replanned_at.reset();
      }
    }
    const bool routing = f.state == FieldFormationState::Routing;
    const bool withdrawing = std::holds_alternative<FieldWithdraw>(f.mission);
    if (routing || (f.state == FieldFormationState::Engaged && !withdrawing))
      continue;
    bool confused = false;
    for (const auto &status : f.statuses)
      if (status.kind == FieldStatusKind::Confused && status.until &&
          elapsed(*status.until, session.now, field.parameters.window_s) > 0)
        confused = true;
    if (confused)
      continue;
    const auto *engage = std::get_if<FieldEngage>(&f.mission);
    const auto *support = std::get_if<FieldSupport>(&f.mission);
    if (!engage && !support)
      continue;
    const auto &target_id =
        engage ? engage->target_formation_id : support->ally_formation_id;
    auto found = session.formations.find(target_id);
    if (found == session.formations.end())
      return fail("FieldMissionTargetRequired");
    std::optional<FieldMissionTransitionReason> reason;
    if (!present(found->second))
      reason = FieldMissionTransitionReason::TargetUnavailable;
    else if (engage && f.ai_pursuit) {
      bool any = false;
      __int128_t nearest = 0;
      for (auto cell : session.sides[f.side].deploy_zone) {
        if (cell >= field.cells.size())
          return fail("FieldDeploymentCellInvalid");
        auto d = field_distance_squared(f.position_dm,
                                        field_cell_center_dm(field, cell));
        if (!any || d < nearest) {
          any = true;
          nearest = d;
        }
      }
      if (!any)
        return fail("FieldDeploymentSourceRequired");
      if (nearest > __int128_t(15000) * 15000)
        reason = FieldMissionTransitionReason::PursuitLimit;
    }
    if (reason) {
      auto changed =
          hold(session, f, field, step.from, f.facing_mdeg, *reason, owner);
      if (!changed.ok)
        return {false, {}, changed.error};
      held_at_origin.push_back(&f);
    }
  }

  // 原WaveReleased等from时刻采用全部完成后才锁军出口；尚无任何本步to事件。
  if (!before_actual_motion) return fail("FieldArmyExitChoiceOwnerRequired");
  auto locked = before_actual_motion();
  if (!locked.ok) return {false, {}, locked.error};
  for (auto &[id, f] : session.formations) {
    if (!present(f))
      continue;
    if (f.side < 0 || f.side > 1)
      return fail("FieldMissionSideInvalid");
    if (std::find(held_at_origin.begin(), held_at_origin.end(), &f) !=
        held_at_origin.end())
      continue;
    const bool routing = f.state == FieldFormationState::Routing;
    if (f.pending_player_entry_order && !routing)
      continue;
    const bool withdrawing = std::holds_alternative<FieldWithdraw>(f.mission);
    if (f.state == FieldFormationState::Engaged && !routing && !withdrawing)
      continue;
    // 混乱是原地应战，不让旧任务继续搬动位置；有效期由真局部时刻判断。
    bool confused = false;
    for (const auto &status : f.statuses)
      if (status.kind == FieldStatusKind::Confused && status.until &&
          elapsed(*status.until, session.now, field.parameters.window_s) > 0)
        confused = true;
    if (confused && !routing)
      continue;
    if (!routing && f.state == FieldFormationState::Holding && f.hold_since)
      if (const auto *stay = std::get_if<FieldHold>(&f.mission);
          stay && stay->point_dm.x == f.position_dm.x &&
          stay->point_dm.y == f.position_dm.y &&
          stay->facing_mdeg == f.facing_mdeg) {
        if (!f.stopped_since)
          f.stopped_since = step.from;
        continue;
      }
    std::optional<PointI> target;
    bool exit = false;
    Integer finish_facing = f.facing_mdeg;
    if (withdrawing && !routing) {
      auto locked = read_field_locked_army_exit_target(world, session, f, field,
                                                       step.from, session.next_event_sequence - 1);
      if (!locked.ok) return {false, {}, locked.error};
      target = locked.value;
      exit = true;
    } else if (routing) {
      __int128_t nearest = 0;
      std::size_t nearest_index = std::numeric_limits<std::size_t>::max();
      for (const auto index : session.sides[f.side].exits) {
        if (index >= field.cells.size())
          return fail("FieldExitCellInvalid");
        const auto point = field_cell_center_dm(field, index);
        const auto distance = field_distance_squared(f.position_dm, point);
        if (!target || distance < nearest ||
            (distance == nearest && index < nearest_index)) {
          target = point;
          nearest = distance;
          nearest_index = index;
        }
      }
      if (!target)
        return fail("FieldActualExitRequired");
      exit = true;
    } else if (const auto *move = std::get_if<FieldMoveTo>(&f.mission)) {
      target = move->point_dm;
      finish_facing = move->facing_mdeg;
    } else if (const auto *stay = std::get_if<FieldHold>(&f.mission)) {
      target = stay->point_dm;
      finish_facing = stay->facing_mdeg;
    } else {
      const auto *engage = std::get_if<FieldEngage>(&f.mission);
      const auto *support = std::get_if<FieldSupport>(&f.mission);
      const StableId *target_id = engage    ? &engage->target_formation_id
                                  : support ? &support->ally_formation_id
                                            : nullptr;
      if (!target_id)
        return fail("FieldMissionKindInvalid");
      const auto found = session.formations.find(*target_id);
      if (found == session.formations.end())
        return fail("FieldMissionTargetRequired");
      if (!present(found->second))
        return fail("FieldMissionOriginTransitionRequired");
      if ((engage && found->second.side == f.side) ||
          (support && found->second.side != f.side))
        return fail("FieldMissionTargetSideInvalid");
      if (support &&
          field_distance_squared(f.position_dm, found->second.position_dm) <=
              __int128_t(1500) * 1500) {
        f.state = FieldFormationState::Holding;
        f.hold_since.reset();
        continue;
      }
      target = found->second.position_dm;
    }
    if (!f.navigation_target_dm ||
        (withdrawing && !routing &&
         (f.navigation_target_dm->x != target->x || f.navigation_target_dm->y != target->y)) ||
        !f.navigation_replanned_at ||
        elapsed(session.now, *f.navigation_replanned_at,
                field.parameters.window_s) >= 60) {
      auto observed = observe_field_navigation_target_in_candidate(
          world, session, f, field, step, owner);
      if (!observed.ok)
        return {false, {}, observed.error};
    }
    const auto from = f.position_dm;
    if (!routing) {
      f.state = FieldFormationState::Moving;
      f.hold_since.reset();
    }
    auto moved = move_field_formation_in_candidate(
        session, field, id, *f.navigation_target_dm, step, owner);
    if (!moved.ok)
      return {false, {}, moved.error};
    if (from.x != f.position_dm.x || from.y != f.position_dm.y) {
      f.facing_mdeg = field_direction_mdeg(
          {f.position_dm.x - from.x, f.position_dm.y - from.y});
      f.stopped_since.reset();
      f.hold_since.reset();
    } else if (!f.stopped_since)
      f.stopped_since = step.from;
    if (moved.value.blocked) {
      f.navigation_replanned_at.reset();
      continue;
    }
    if (!moved.value.reached)
      continue;
    if (exit) {
      out.reached_exits.push_back(id);
      continue;
    }
    if (std::holds_alternative<FieldMoveTo>(f.mission) ||
        std::holds_alternative<FieldHold>(f.mission))
      arrived.emplace_back(id, finish_facing);
  }
  // Origin-time rule changes for all formations precede endpoint arrivals.
  // Do not append a later formation's step.from event after step.to.
  for (const auto &[id, finish_facing] : arrived) {
    auto &f = session.formations.at(id);
    if (f.flank_assignment_index) {
      auto changed = apply_field_rule_mission_transition(
          session, f, field, FieldMissionTransitionReason::FollowupAdopted,
          step.to, owner);
      if (!changed.ok)
        return {false, {}, changed.error};
      f.facing_mdeg = finish_facing;
      if (std::holds_alternative<FieldHold>(f.mission)) {
        f.state = FieldFormationState::Holding;
        f.hold_since = step.to;
      } else {
        f.state = FieldFormationState::Moving;
        f.hold_since.reset();
      }
      f.navigation_target_dm.reset();
      f.navigation_observation_sequence.reset();
      f.navigation_replanned_at.reset();
    } else {
      auto changed = hold(session, f, field, step.to, finish_facing,
                          FieldMissionTransitionReason::Arrived, owner);
      if (!changed.ok)
        return {false, {}, changed.error};
    }
  }
  return {true, std::move(out), {}};
}
} // namespace hanmo::resident
