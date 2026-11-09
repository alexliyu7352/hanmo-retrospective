#include "field_event_legacy_kinds.hpp"
#include "field_event_mission_transition.hpp"
#include "field_event_navigation_observation.hpp"
#include "field_event_motion_application.hpp"
#include "field_event_mission_application.hpp"
#include "field_event_contact_phase_from.hpp"
#include "field_navigation_source.hpp"
#include "field_army_exit_selection.hpp"
#include "field_roster_origin.hpp"
#include "resident/domains/military/military_model.hpp"
#include "field_ability_sources.hpp"
#include "field_binding_source.hpp"
#include "field_command_source.hpp"
#include "field_contacts.hpp"
#include "field_deployment.hpp"
#include "field_flank.hpp"
#include "field_generation.hpp"
#include "field_initial_placement_source.hpp"
#include "field_integer_geometry.hpp"
#include "field_reinforcement_geometry.hpp"
#include "field_reinforcement_model.hpp"
#include "field_validation.hpp"
#include "resident/kernel/command_types.hpp"
#include "resident/kernel/common_command_sources.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <algorithm>
#include <limits>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
bool same(PointI a, PointI b) { return a.x == b.x && a.y == b.y; }
Status invalid(const StableId &id) {
  return common_failure("FieldNavigationOriginalSourceMismatch", id);
}
Status step_source(const FieldBattleSession &s, const FieldFormation &f,
                   const GeneratedBattleField &field,
                   const FieldBattleWindowSlice &step) {
  if (s.state != FieldBattleState::Running ||
      s.field_hash != field.field_hash || step.steps != 1 ||
      !s.formations.contains(f.formation_id) ||
      &s.formations.at(f.formation_id) != &f ||
      std::tie(s.now.tick, s.now.t) != std::tie(step.from.tick, step.from.t) ||
      f.side < 0 || f.side > 1 || s.next_event_sequence <= 0 ||
      s.next_event_sequence == std::numeric_limits<Integer>::max())
    return invalid(f.formation_id);
  auto to = add_field_battle_seconds(step.from, field.parameters.step_s,
                                     field.parameters);
  if (!to.ok)
    return {false, {}, to.error};
  return std::tie(to.value.tick, to.value.t) ==
                 std::tie(step.to.tick, step.to.t)
             ? common_success()
             : invalid(f.formation_id);
}

} // namespace
Status observe_field_navigation_target_in_candidate(
    const FieldBattleWorldState &world, FieldBattleSession &s, FieldFormation &f,
    const GeneratedBattleField &field, const FieldBattleWindowSlice &step,
    CheckpointMemoryCharge &owner) {
  auto valid = step_source(s, f, field, step);
  if (!valid.ok)
    return valid;


  (owner).grow(0);
  FieldNavigationObservation observed;
  observed.evaluated_at = step.from;
  observed.mission = f.mission;
  observed.mission_revision = f.mission_revision;
  observed.mission_transition_sequence = f.mission_transition_sequence;
  observed.original_command_result = f.mission_command_result;
  observed.original_ai_source_id = f.mission_ai_source_id;
  observed.original_order_id = f.adopted_order_id;
  observed.from_dm = f.position_dm;
  observed.routing = f.state == FieldFormationState::Routing;
  observed.ai_pursuit = f.ai_pursuit;
  if (!observed.routing && std::holds_alternative<FieldWithdraw>(f.mission)) {
    auto locked = read_field_locked_army_exit_target(world, s, f, field,
                                                     step.from, s.next_event_sequence - 1);
    if (!locked.ok) return {false, {}, locked.error};
    observed.target_dm = locked.value;
  } else if (observed.routing) {
    std::optional<std::size_t> closest;
    __int128_t distance = 0;
    for (auto cell : s.sides[f.side].exits) {
      if (cell >= field.cells.size())
        return invalid(f.formation_id);
      auto point = field_cell_center_dm(field, cell);
      auto d = field_distance_squared(f.position_dm, point);
      if (!closest || d < distance || (d == distance && cell < *closest)) {
        closest = cell;
        distance = d;
        observed.target_dm = point;
      }
    }
    if (!closest)
      return invalid(f.formation_id);
  } else if (const auto *m = std::get_if<FieldMoveTo>(&f.mission))
    observed.target_dm = m->point_dm;
  else if (const auto *m = std::get_if<FieldHold>(&f.mission))
    observed.target_dm = m->point_dm;
  else {
    const auto *e = std::get_if<FieldEngage>(&f.mission);
    const auto *a = std::get_if<FieldSupport>(&f.mission);
    if (!e && !a)
      return invalid(f.formation_id);
    const auto &id = e ? e->target_formation_id : a->ally_formation_id;
    auto target = s.formations.find(id);
    if (target == s.formations.end() || target->second.fit.empty() ||
        target->second.state == FieldFormationState::Withdrawn ||
        target->second.state == FieldFormationState::Destroyed ||
        (e && target->second.side == f.side) ||
        (a && target->second.side != f.side))
      return invalid(f.formation_id);
    observed.target_formation_id = id;
    observed.target_dm = target->second.position_dm;
    observed.target_motion_sequence = target->second.last_motion_sequence;
  }
  FieldBattleEvent event;
  event.sequence = s.next_event_sequence++;
  event.at = step.to;
  event.kind = field_event_kind::NavigationObserved;
  event.formation_id = f.formation_id;
  event.side = f.side;
  f.navigation_target_dm = observed.target_dm;
  f.navigation_replanned_at = step.from;
  f.navigation_observation_sequence = event.sequence;
  field_event_navigation_observation(event) = std::move(observed);
  s.events.push_back(std::move(event));
  return common_success();
}
Status record_field_original_motion_application(
    FieldBattleSession &s, FieldFormation &f, const GeneratedBattleField &field,
    const FieldBattleWindowSlice &step, const FieldNavigationStep &actual,
    CheckpointMemoryCharge &owner) {
  auto valid = step_source(s, f, field, step);
  if (!valid.ok)
    return valid;
  if (!f.navigation_observation_sequence || !f.navigation_target_dm ||
      !same(actual.to_dm, f.position_dm))
    return invalid(f.formation_id);
  const FieldBattleEvent *source = nullptr;
  for (const auto &e : s.events)
    if (e.sequence == *f.navigation_observation_sequence) {
      if (source)
        return invalid(f.formation_id);
      source = &e;
    }
  if (!source || source->kind != field_event_kind::NavigationObserved ||
      source->formation_id != f.formation_id ||
      !field_event_navigation_observation(*source) ||
      !same(field_event_navigation_observation(*source)->target_dm, *f.navigation_target_dm))
    return invalid(f.formation_id);


  (owner).grow(0);
  FieldBattleEvent event;
  event.sequence = s.next_event_sequence++;
  event.at = step.to;
  event.kind = field_event_kind::MotionApplied;
  event.formation_id = f.formation_id;
  event.side = f.side;
  field_event_motion_application(event) =
      FieldMotionApplication{step.from,
                             *f.navigation_observation_sequence,
                             f.last_motion_sequence,
                             actual.from_dm,
                             actual.to_dm,
                             actual.reached,
                             actual.blocked};
  f.last_motion_sequence = event.sequence;
  s.events.push_back(std::move(event));
  return common_success();
}

Status validate_field_navigation_sources(
    const CommonStore &common, const MilitaryState &military,
    const FieldBattleWorldState &world, const FieldBattleSession &session,
    const GeneratedBattleField &field,
    const FieldBattleAbilityReadContext &sources) {
  // 冷读先验唯一选择头及其真实cut；完整路线/权限由同World choice表验证继续反证。
  for (const auto &[id, plan] : world.army_exit_plans)
    if (plan.battle_id == session.battle_id) {
      auto original = validate_field_army_exit_player_command_source(common, world, plan);
      if (!original.ok) return original;
    }
  std::map<StableId, FieldMotionPrefixFormation> prefix;
  std::map<StableId, FieldInitialPlacementRead> initial;
  std::map<StableId, Integer> facing;
  std::map<StableId, FieldMission> missions;
  std::map<StableId, std::optional<Integer>> flank_pending;
  std::map<StableId, FieldFormationState> states;
  std::map<StableId, bool> holding, pursuits, wave_pending, ever_engaged,
      entry_order_pending;
  std::map<StableId, std::optional<FieldBattleTime>> hold_since, stopped_since;
  std::map<StableId, std::set<std::pair<Integer, Integer>>> used_volley_pulses;
  std::map<StableId, std::optional<Integer>> last_motion;
  std::map<Integer, const FieldBattleEvent *> observations;
  std::map<StableId, Integer> latest_observation;
  std::set<Integer> consumed_observations;
  std::set<StableId> awaiting_first_contact, pending_admissions;
  std::map<StableId, const FieldBattleEvent *> last_movement;
  for (const auto &[id, f] : session.formations) {
    auto original = read_field_original_initial_placement(
        common, military, world, session, f, field, sources);
    if (!original.ok)
      return {false, {}, original.error};
    if (f.side < 0 || f.side > 1 || f.fit_start <= 0)
      return invalid(id);
    initial.emplace(id, original.value);
    if (original.value.entry)
      pending_admissions.insert(id);
    std::optional<Integer> flank_index;
    if (original.value.council && !original.value.deployment) {
      const auto &c = original.value.council->council;
      if (!c.adopted_proposal_id)
        return invalid(id);
      const FieldCouncilProposal *adopted = nullptr;
      for (const auto &proposal : c.proposals)
        if (proposal.proposal_id == *c.adopted_proposal_id) {
          if (adopted)
            return invalid(id);
          adopted = &proposal;
        }
      if (!adopted)
        return invalid(id);
      for (std::size_t i = 0; i < adopted->plan.assignments.size(); ++i) {
        const auto &a = adopted->plan.assignments[i];
        if (a.formation_id == id && a.flank_followup) {
          if (flank_index)
            return invalid(id);
          flank_index = Integer(i);
        }
      }
    }
    flank_pending.emplace(id, flank_index);
    if (original.value.immediate_mission)
      missions.emplace(id, *original.value.immediate_mission);
    else if (original.value.original_second_wave_hold)
      missions.emplace(id,
                       FieldMission{*original.value.original_second_wave_hold});
    else
      return invalid(id);
    states.emplace(id, std::holds_alternative<FieldHold>(missions.at(id))
                           ? FieldFormationState::Holding
                           : FieldFormationState::Moving);
    entry_order_pending.emplace(
        id, original.value.entry &&
                original.value.entry->pending_player_entry_order);
    holding.emplace(id, false);
    hold_since.emplace(id, std::nullopt);
    stopped_since.emplace(id, std::nullopt);
    pursuits.emplace(id, false);
    wave_pending.emplace(id, original.value.wave_mission != nullptr);
    ever_engaged.emplace(id, false);
    facing.emplace(id, original.value.facing_mdeg);
    prefix.emplace(
        id, FieldMotionPrefixFormation{original.value.position_dm,
                                       original.value.entry ? 0 : f.fit_start,
                                       false, false});
    last_motion.emplace(id, std::nullopt);
  }

  if (!session.started_at)
    return invalid(session.battle_id);
  FieldBattleTime phase_from = *session.started_at;
  bool in_motion = false;
  auto next_formation = session.formations.begin();
  auto same_time = [](const FieldBattleTime &a, const FieldBattleTime &b) {
    return a.tick == b.tick && a.t == b.t;
  };
  auto requires_motion = [&](const StableId &id) -> Outcome<bool> {
    const auto &p = prefix.at(id);
    const auto &mission = missions.at(id);
    if (p.fit <= 0 || p.withdrawn)
      return {true, false, {}};
    const bool withdrawing = std::holds_alternative<FieldWithdraw>(mission);
    if (!p.routing && states.at(id) == FieldFormationState::Engaged &&
        !withdrawing)
      return {true, false, {}};
    if (!p.routing)
      for (const auto &[key, row] : session.stratagems)
        if (row.kind == FieldStratagemKind::Confuse &&
            row.target_formation_id == id && row.success.value_or(false) &&
            row.resolved_at &&
            std::tie(row.resolved_at->tick, row.resolved_at->t) <=
                std::tie(phase_from.tick, phase_from.t)) {
          auto until =
              add_field_battle_seconds(*row.resolved_at, 600, field.parameters);
          if (!until.ok)
            return {false, false, until.error};
          if (std::tie(phase_from.tick, phase_from.t) <
              std::tie(until.value.tick, until.value.t))
            return {true, false, {}};
        }
    if (p.routing)
      return {true, true, {}};
    if (entry_order_pending.at(id))
      return {true, false, {}};
    if (states.at(id) == FieldFormationState::Holding && holding.at(id))
      if (const auto *h = std::get_if<FieldHold>(&mission);
          h && same(h->point_dm, p.position_dm) &&
          h->facing_mdeg == facing.at(id)) {
        if (!stopped_since.at(id))
          stopped_since.at(id) = phase_from;
        return {true, false, {}};
      }

    if (const auto *engage = std::get_if<FieldEngage>(&mission)) {
      auto target = prefix.find(engage->target_formation_id);
      if (target == prefix.end() || target->second.fit <= 0 ||
          target->second.withdrawn)
        return {false, false,
                common_failure("FieldTargetUnavailableTransitionMissing", id)
                    .error};
      if (pursuits.at(id)) {
        bool have = false;
        __int128_t nearest = 0;
        const auto side = session.formations.at(id).side;
        for (auto cell : session.sides[side].deploy_zone) {
          if (cell >= field.cells.size())
            return {false, false, invalid(id).error};
          auto d = field_distance_squared(p.position_dm,
                                          field_cell_center_dm(field, cell));
          if (!have || d < nearest) {
            have = true;
            nearest = d;
          }
        }
        if (!have)
          return {false, false, invalid(id).error};
        if (nearest > __int128_t(15000) * 15000)
          return {
              false, false,
              common_failure("FieldPursuitLimitTransitionMissing", id).error};
      }
    }
    if (const auto *support = std::get_if<FieldSupport>(&mission)) {
      auto target = prefix.find(support->ally_formation_id);
      if (target == prefix.end() || target->second.fit <= 0 ||
          target->second.withdrawn)
        return {false, false, invalid(id).error};
      if (field_distance_squared(p.position_dm, target->second.position_dm) <=
          __int128_t(1500) * 1500) {
        states.at(id) = FieldFormationState::Holding;
        holding.at(id) = false;
        hold_since.at(id).reset();
        return {true, false, {}};
      }
    }
    return {true, true, {}};
  };
  auto drain_skipped = [&](const StableId *target) -> Status {
    if (!in_motion) {
      for (const auto &[id, f] : session.formations) {
        if (!wave_pending.at(id) || prefix.at(id).fit <= 0 ||
            prefix.at(id).withdrawn)
          continue;
        for (const auto &[other_id, other] : session.formations)
          if (other.side == f.side && other.wave == FieldCouncilWave::First &&
              ever_engaged.at(other_id))
            return common_failure("FieldWaveReleaseTransitionMissing", id);
      }
      in_motion = true;
      next_formation = session.formations.begin();
    }
    while (next_formation != session.formations.end() &&
           (!target || next_formation->first < *target)) {
      auto required = requires_motion(next_formation->first);
      if (!required.ok)
        return {false, {}, required.error};
      if (required.value)
        return common_failure("FieldRequiredMotionApplicationMissing",
                              next_formation->first);
      ++next_formation;
    }
    if (target) {
      if (next_formation == session.formations.end() ||
          next_formation->first != *target)
        return invalid(*target);
      auto required = requires_motion(*target);
      if (!required.ok)
        return {false, {}, required.error};
      if (!required.value)
        return common_failure("FieldUnexpectedMotionApplication", *target);
    }
    return common_success();
  };
  Integer sequence = 0;
  for (const auto &event : session.events) {
    if (event.sequence <= sequence)
      return invalid(session.battle_id);
    sequence = event.sequence;
    if ((event.formation_id &&
         pending_admissions.contains(*event.formation_id) &&
         event.kind != field_event_kind::Entered) ||
        (event.target_formation_id &&
         pending_admissions.contains(*event.target_formation_id)))
      return common_failure("FieldQueuedFormationInLocalEvent",
                            session.battle_id);
    if (event.kind == field_event_kind::Entered) {
      if (!event.formation_id ||
          !pending_admissions.contains(*event.formation_id) || in_motion ||
          !same_time(event.at, phase_from))
        return invalid(session.battle_id);
      const auto &id = *event.formation_id;
      const auto *entry = initial.at(id).entry;
      if (!entry || entry->event_sequence != event.sequence ||
          event.source_id != entry->application_source.id ||
          !same_time(event.at, entry->at) || prefix.at(id).fit != 0)
        return invalid(id);
      bool original_queue = false;
      for (const auto &[contact_id, boundary] : world.boundary_contacts) {
        if (!boundary.reinforcement_queue)
          continue;
        const auto &admitted = boundary.reinforcement_queue->admitted;
        for (std::size_t n = 0; n < admitted.size(); ++n) {
          if (&admitted[n] != entry)
            continue;
          if (original_queue)
            return invalid(id);
          original_queue = true;
          if (n) {
            const auto &previous_id = admitted[n - 1].formation_id;
            if (pending_admissions.contains(previous_id) ||
                !prefix.contains(previous_id))
              return invalid(id);
            const auto &previous = prefix.at(previous_id);
            if (previous.fit > 0 && !previous.withdrawn) {
              const auto &body = session.formations.at(previous_id);
              if (body.ranks <= 0)
                return invalid(previous_id);
              const auto radius = field_formation_radius_dm(body, previous.fit);
              if (field_distance_squared(previous.position_dm,
                                         entry->position_dm) <=
                  __int128_t(radius) * radius)
                return common_failure(
                    "FieldReinforcementPreviousCircleStillAtEntry", id);
            }
          }
        }
      }
      if (!original_queue)
        return invalid(id);
      if (entry->ai_mission) {
        const auto &arriving = session.formations.at(id);
        auto authority = validate_field_reinforcement_ai_control_source(
            common, world, session, arriving, sources.original_life);
        if (!authority.ok)
          return authority;
        FieldMotionPrefixRead original_prefix{event.at, prefix};
        auto rally = read_field_reinforcement_rally_at_prefix(
            session, field, arriving.side, arriving.category, original_prefix);
        if (!rally.ok)
          return {false, {}, rally.error};
        auto binding = read_field_original_battle_binding(
            common, world, session.battle_id, arriving.army_id);
        if (!binding.ok)
          return {false, {}, binding.error};
        const bool commander =
            arriving.commander_actor_id == binding.value->commander_actor_id;
        const auto expected =
            commander ? rally.value.commander_dm : rally.value.rally_dm;
        const auto *mission = std::get_if<FieldMoveTo>(&entry->initial_mission);
        if (!mission || !same(mission->point_dm, expected) ||
            mission->facing_mdeg != entry->facing_mdeg)
          return common_failure("FieldReinforcementOriginalRallyMismatch", id);
      }
      prefix.at(id).fit = session.formations.at(id).fit_start;
      states.at(id) = FieldFormationState::Moving;
      pending_admissions.erase(id);
    }
    if (!awaiting_first_contact.empty() &&
        event.kind != field_event_kind::Contact)
      return common_failure("FieldFirstContactApplicationMissing",
                            session.battle_id);
    if (event.kind == field_event_kind::Contact) {
      if (!event.formation_id || !same_time(event.at, phase_from) ||
          !awaiting_first_contact.erase(*event.formation_id))
        return common_failure("FieldUnexpectedFirstContactApplication",
                              session.battle_id);
    }
    const bool observation =
        event.kind == field_event_kind::NavigationObserved;
    const bool motion = event.kind == field_event_kind::MotionApplied;
    if (bool(field_event_navigation_observation(event)) != observation ||
        bool(field_event_motion_application(event)) != motion)
      return invalid(session.battle_id);

    if (bool(field_event_contact_phase_from(event)) !=
        (event.kind == field_event_kind::ContactPhaseApplied))
      return invalid(session.battle_id);
    if (field_event_mission_application(event)) {
      if (!event.formation_id || !missions.contains(*event.formation_id) ||
          !same_time(event.at, phase_from) || in_motion)
        return invalid(session.battle_id);
      const auto &id = *event.formation_id;
      const auto &a = *field_event_mission_application(event);
      if (entry_order_pending.at(id)) {
        const SourceRef *player_source =
            a.command_result ? &*a.command_result : nullptr;
        const StableId *ai_source = a.ai_source_id ? &*a.ai_source_id : nullptr;
        if (a.order_id) {
          const auto order = session.orders.find(*a.order_id);
          if (player_source || ai_source || order == session.orders.end())
            return common_failure("FieldReinforcementTaskSourceRequired", id);
          if (order->second.ai_issued) {
            for (const auto &[key, fact] : world.council_facts)
              if (fact.kind == FieldCouncilFactKind::AiOrder &&
                  fact.battle_id == session.battle_id &&
                  fact.order_id == a.order_id) {
                if (ai_source)
                  return invalid(id);
                ai_source = &fact.fact_id;
              }
          } else {
            if (order->second.issuer_actor_id !=
                    common.state().metadata.player_actor_id ||
                !order->second.command_result)
              return common_failure(
                  "FieldReinforcementPlayerTaskSourceRequired", id);
            player_source = &*order->second.command_result;
          }
        }
        if (bool(player_source) == bool(ai_source))
          return common_failure("FieldReinforcementTaskSourceRequired", id);
        if (ai_source) {
          auto source = read_field_ai_command_source(common, world, *ai_source,
                                                     sources.original_life);
          if (!source.ok)
            return {false, {}, source.error};
          if (source.value->formation_id != id ||
              source.value->side != session.formations.at(id).side ||
              std::tie(source.value->at.tick, source.value->at.t) >
                  std::tie(event.at.tick, event.at.t))
            return invalid(id);
        } else {
          auto command = read_common_command_source(common, *player_source);
          if (!command.ok)
            return {false, {}, command.error};
          if (command.value.history->actor_id !=
                  common.state().metadata.player_actor_id ||
              (command.value.result->kind != "field_battle_order" &&
               command.value.result->kind != "field_battle_direct" &&
               command.value.result->kind != "field_battle_dispatch"))
            return common_failure("FieldReinforcementPlayerTaskSourceRequired",
                                  id);
        }
      }
      missions.at(id) = a.mission;
      entry_order_pending.at(id) = false;
      flank_pending.at(id).reset();
      pursuits.at(id) = a.ai_pursuit;
      wave_pending.at(id) = false;
      bool dispatch = false;
      if (a.command_result) {
        auto source = read_common_command_source(common, *a.command_result);
        if (!source.ok)
          return {false, {}, source.error};
        dispatch = source.value.result->kind == "field_battle_dispatch";
      }
      holding.at(id) = false;
      hold_since.at(id).reset();
      if (dispatch)
        stopped_since.at(id).reset();
      if (!prefix.at(id).routing &&
          states.at(id) != FieldFormationState::Engaged) {
        states.at(id) = std::holds_alternative<FieldHold>(a.mission)
                            ? FieldFormationState::Holding
                            : FieldFormationState::Moving;
        holding.at(id) =
            !dispatch && std::holds_alternative<FieldHold>(a.mission);
        if (holding.at(id))
          hold_since.at(id) = event.at;
      }
    }
    if (field_event_mission_transition(event) &&
        (field_event_mission_transition(event)->reason ==
             FieldMissionTransitionReason::Arrived ||
         field_event_mission_transition(event)->reason ==
             FieldMissionTransitionReason::FollowupAdopted)) {
      auto complete = drain_skipped(nullptr);
      if (!complete.ok)
        return complete;
    }
    if (event.kind == field_event_kind::ContactPhaseApplied) {
      if (!field_event_contact_phase_from(event) ||
          !same_time(*field_event_contact_phase_from(event), phase_from) ||
          event.formation_id || event.target_formation_id || event.source_id ||
          event.side || event.count || event.tactic || event.stratagem ||
          event.success || field_event_mission_transition(event) ||
          field_event_mission_application(event) || field_event_navigation_observation(event) ||
          field_event_motion_application(event))
        return invalid(session.battle_id);
      auto end = add_field_battle_seconds(phase_from, field.parameters.step_s,
                                          field.parameters);
      if (!end.ok)
        return {false, {}, end.error};
      if (!same_time(end.value, event.at))
        return invalid(session.battle_id);
      auto complete = drain_skipped(nullptr);
      if (!complete.ok)
        return complete;
      std::map<StableId, Integer> volleys;
      for (const auto &[id, f] : session.formations)
        if (f.ability)
          for (const auto &a : f.ability->active)
            if (a.kind == FieldTacticKind::Volley &&
                std::tie(a.started.tick, a.started.t) <=
                    std::tie(phase_from.tick, phase_from.t)) {
              const auto used = used_volley_pulses.find(a.activation_id);
              const auto count =
                  used == used_volley_pulses.end() ? 0 : used->second.size();
              if (count > 3)
                return invalid(a.activation_id);
              volleys.emplace(a.activation_id, Integer(3 - count));
            }
      FieldMotionPrefixRead movement{phase_from, prefix};
      FieldAttackPrefixRead view{movement, facing, volleys};
      auto contact =
          read_field_attack_contacts_at_prefix(session, field, &view);
      if (!contact.ok)
        return {false, {}, contact.error};
      std::set<StableId> engaged(contact.value.melee_formations.begin(),
                                 contact.value.melee_formations.end());
      for (const auto &a : contact.value.attacks)
        engaged.insert(a.attacker_id);
      for (const auto &[id, p] : prefix) {
        if (p.fit <= 0 || p.withdrawn)
          continue;
        if (engaged.contains(id)) {
          if (!ever_engaged.at(id))
            awaiting_first_contact.insert(id);
          ever_engaged.at(id) = true;
          if (!p.routing &&
              !std::holds_alternative<FieldWithdraw>(missions.at(id)))
            states.at(id) = FieldFormationState::Engaged;
          holding.at(id) = false;
          hold_since.at(id).reset();
        } else if (states.at(id) == FieldFormationState::Engaged) {
          const bool hold = !entry_order_pending.at(id) &&
                            std::holds_alternative<FieldHold>(missions.at(id));
          states.at(id) =
              hold ? FieldFormationState::Holding : FieldFormationState::Moving;
          holding.at(id) = hold;
          if (hold)
            hold_since.at(id) = event.at;
          else
            hold_since.at(id).reset();
        }
      }
      phase_from = event.at;
      in_motion = false;
    }
    if (event.kind == field_event_kind::VolleyImpact) {
      if (!event.source_id)
        return invalid(session.battle_id);
      used_volley_pulses[*event.source_id].emplace(event.at.tick, event.at.t);
    }
    if (event.kind == field_event_kind::MissionTransition) {
      if (!event.formation_id || !prefix.contains(*event.formation_id) ||
          !field_event_mission_transition(event) ||
          !same(field_event_mission_transition(event)->at_position_dm,
                prefix.at(*event.formation_id).position_dm))
        return invalid(session.battle_id);
      const auto &t = *field_event_mission_transition(event);
      const auto &id = *event.formation_id;
      if (entry_order_pending.at(id)) {
        if (t.reason != FieldMissionTransitionReason::Rallied)
          return invalid(id);
        entry_order_pending.at(id) = false;
      }
      if (!same_field_battle_mission(missions.at(id), t.from))
        return invalid(id);
      if (t.reason == FieldMissionTransitionReason::Arrived &&
          flank_pending.at(id))
        return invalid(id);
      if (t.reason == FieldMissionTransitionReason::FollowupAdopted) {
        if (!flank_pending.at(id) ||
            t.original_flank_assignment_index != flank_pending.at(id))
          return invalid(id);
        std::map<StableId, Integer> volleys;
        for (const auto &[key, f] : session.formations)
          if (f.ability)
            for (const auto &a : f.ability->active)
              if (a.kind == FieldTacticKind::Volley &&
                  std::tie(a.started.tick, a.started.t) <=
                      std::tie(phase_from.tick, phase_from.t)) {
                const auto used = used_volley_pulses.find(a.activation_id);
                const auto count =
                    used == used_volley_pulses.end() ? 0 : used->second.size();
                if (count > 3)
                  return invalid(a.activation_id);
                volleys.emplace(a.activation_id, Integer(3 - count));
              }
        FieldMotionPrefixRead movement{phase_from, prefix};
        FieldAttackPrefixRead view{movement, facing, volleys};
        auto actual = read_field_flank_followup(session, field, id,
                                                *flank_pending.at(id), &view);
        if (!actual.ok)
          return {false, {}, actual.error};
        if (!same_field_battle_mission(t.from,
                                       actual.value.assignment->mission))
          return invalid(id);
        if (actual.value.target) {
          const auto engage = std::get_if<FieldEngage>(&t.to);
          if (!engage ||
              engage->target_formation_id !=
                  actual.value.target->formation_id ||
              t.to_ai_pursuit != t.from_ai_pursuit)
            return invalid(id);
        } else {
          const auto hold = std::get_if<FieldHold>(&t.to);
          if (!hold || !same(hold->point_dm, actual.value.position_dm) ||
              hold->facing_mdeg != actual.value.facing_mdeg || t.to_ai_pursuit)
            return invalid(id);
        }
        facing.at(id) = actual.value.facing_mdeg;
        flank_pending.at(id).reset();
      }
      if (t.reason != FieldMissionTransitionReason::WaveReleased)
        flank_pending.at(id).reset();
      missions.at(id) = t.to;
      pursuits.at(id) = t.to_ai_pursuit;
      if (t.reason == FieldMissionTransitionReason::WaveReleased ||
          t.reason == FieldMissionTransitionReason::Rallied)
        wave_pending.at(id) = false;
      if (t.reason == FieldMissionTransitionReason::WaveReleased ||
          (t.reason == FieldMissionTransitionReason::FollowupAdopted &&
           std::holds_alternative<FieldEngage>(t.to))) {
        states.at(id) = FieldFormationState::Moving;
        holding.at(id) = false;
        hold_since.at(id).reset();
      } else {
        states.at(id) = FieldFormationState::Holding;
        holding.at(id) = true;
        hold_since.at(id) = event.at;
      }

      if (t.reason != FieldMissionTransitionReason::WaveReleased &&
          !(t.reason == FieldMissionTransitionReason::FollowupAdopted &&
            std::holds_alternative<FieldEngage>(t.to))) {
        const auto *hold = std::get_if<FieldHold>(&t.to);
        if (!hold)
          return invalid(*event.formation_id);
        facing.at(*event.formation_id) = hold->facing_mdeg;
      }
    }
    if (event.kind == field_event_kind::Ended) {
      for (auto &[id, p] : prefix)
        if (p.routing && !p.withdrawn)
          p.fit = 0;
    }
    if (!observation && !motion &&
        event.kind != field_event_kind::Casualties &&
        event.kind != field_event_kind::Rout &&
        event.kind != field_event_kind::Rallied &&
        event.kind != field_event_kind::Withdrawn)
      continue;
    if (!event.formation_id || !prefix.contains(*event.formation_id))
      return invalid(session.battle_id);
    const auto &id = *event.formation_id;
    auto &p = prefix.at(id);
    const auto &f = session.formations.at(id);
    if (event.side != f.side)
      return invalid(id);
    if (observation || motion) {
      const auto actual_from = observation
                                   ? field_event_navigation_observation(event)->evaluated_at
                                   : field_event_motion_application(event)->evaluated_at;
      if (!same_time(actual_from, phase_from))
        return invalid(id);
      auto expected = drain_skipped(&id);
      if (!expected.ok)
        return expected;

      if (event.target_formation_id || event.source_id || event.count ||
          event.tactic || event.stratagem || event.success ||
          field_event_mission_transition(event) || p.withdrawn || p.fit <= 0)
        return invalid(id);
      const auto at = observation ? field_event_navigation_observation(event)->evaluated_at
                                  : field_event_motion_application(event)->evaluated_at;
      auto end = add_field_battle_seconds(at, field.parameters.step_s,
                                          field.parameters);
      if (!end.ok)
        return {false, {}, end.error};
      if (std::tie(end.value.tick, end.value.t) !=
              std::tie(event.at.tick, event.at.t) ||
          std::tie(event.at.tick, event.at.t) >
              std::tie(session.now.tick, session.now.t))
        return invalid(id);
    }
    if (observation) {
      const auto &n = *field_event_navigation_observation(event);
      if (!same(n.from_dm, p.position_dm) || n.routing != p.routing)
        return invalid(id);
      auto origin = validate_field_navigation_mission_source(
          common, world, session, f, event, initial.at(id),
          sources.original_life);
      if (!origin.ok)
        return origin;
      PointI expected;
      bool target = false;
      if (!n.routing && std::holds_alternative<FieldWithdraw>(n.mission)) {
        auto locked = read_field_locked_army_exit_target(world, session, f, field,
                                                         n.evaluated_at, event.sequence - 1);
        if (!locked.ok) return {false, {}, locked.error};
        expected = locked.value;
      } else if (n.routing) {
        std::optional<std::size_t> closest;
        __int128_t distance = 0;
        for (auto cell : session.sides[f.side].exits) {
          if (cell >= field.cells.size())
            return invalid(id);
          auto point = field_cell_center_dm(field, cell);
          auto d = field_distance_squared(p.position_dm, point);
          if (!closest || d < distance || (d == distance && cell < *closest)) {
            closest = cell;
            distance = d;
            expected = point;
          }
        }
        if (!closest)
          return invalid(id);
      } else if (const auto *m = std::get_if<FieldMoveTo>(&n.mission))
        expected = m->point_dm;
      else if (const auto *m = std::get_if<FieldHold>(&n.mission))
        expected = m->point_dm;
      else {
        const auto *e = std::get_if<FieldEngage>(&n.mission);
        const auto *a = std::get_if<FieldSupport>(&n.mission);
        if (!e && !a)
          return invalid(id);
        const auto &other = e ? e->target_formation_id : a->ally_formation_id;
        if (!prefix.contains(other) || n.target_formation_id != other ||
            n.target_motion_sequence != last_motion.at(other))
          return invalid(id);
        const auto &q = prefix.at(other);
        const auto &of = session.formations.at(other);
        if (q.withdrawn || q.fit <= 0 || (e && of.side == f.side) ||
            (a && of.side != f.side))
          return invalid(id);
        expected = q.position_dm;
        target = true;
      }
      if (!same(expected, n.target_dm) ||
          (!target && (n.target_formation_id || n.target_motion_sequence)))
        return invalid(id);
      auto previous = latest_observation.find(id);
      if (previous != latest_observation.end()) {
        const auto &prior =
            *field_event_navigation_observation(*observations.at(previous->second));
        if (!consumed_observations.contains(previous->second))
          return invalid(id);
        const auto same_ref = [](const auto &a, const auto &b) {
          return bool(a) == bool(b) && (!a || same_source_ref(*a, *b));
        };
        const bool unchanged =
            prior.mission_revision == n.mission_revision &&
            prior.mission_transition_sequence ==
                n.mission_transition_sequence &&
            same_ref(prior.original_command_result,
                     n.original_command_result) &&
            prior.original_ai_source_id == n.original_ai_source_id &&
            prior.original_order_id == n.original_order_id &&
            prior.routing == n.routing;
        const __int128_t age =
            (__int128_t(n.evaluated_at.tick) - prior.evaluated_at.tick) *
                field.parameters.window_s +
            n.evaluated_at.t - prior.evaluated_at.t;
        const auto last = last_movement.find(id);
        const bool blocked = last != last_movement.end() &&
                             field_event_motion_application(*last->second)->blocked;
        if (age < 0 || (unchanged && !blocked && age < 60))
          return invalid(id);
      }
      observations.emplace(event.sequence, &event);
      latest_observation[id] = event.sequence;
    } else if (motion) {
      const auto &m = *field_event_motion_application(event);
      auto nav = observations.find(m.navigation_sequence);
      if (nav == observations.end() || nav->second->formation_id != id ||
          !same(m.from_dm, p.position_dm) ||
          m.previous_motion_sequence != last_motion.at(id))
        return invalid(id);
      const auto &n = *field_event_navigation_observation(*nav->second);
      if (!latest_observation.contains(id) ||
          latest_observation.at(id) != m.navigation_sequence)
        return invalid(id);
      if (!consumed_observations.contains(m.navigation_sequence) &&
          std::tie(n.evaluated_at.tick, n.evaluated_at.t) !=
              std::tie(m.evaluated_at.tick, m.evaluated_at.t))
        return invalid(id);
      auto previous_move = last_movement.find(id);
      if (previous_move != last_movement.end()) {
        if (field_event_motion_application(*previous_move->second)->blocked &&
            field_event_motion_application(*previous_move->second)->navigation_sequence ==
                m.navigation_sequence)
          return common_failure("FieldBlockedNavigationReplanRequired", id);
        const auto &before =
            field_event_motion_application(*previous_move->second)->evaluated_at;
        if (std::tie(before.tick, before.t) >=
            std::tie(m.evaluated_at.tick, m.evaluated_at.t))
          return invalid(id);
      }
      const __int128_t age =
          (__int128_t(m.evaluated_at.tick) - n.evaluated_at.tick) *
              field.parameters.window_s +
          m.evaluated_at.t - n.evaluated_at.t;
      if (age < 0 || age >= 60)
        return invalid(id);
      consumed_observations.insert(m.navigation_sequence);
      if (std::tie(n.evaluated_at.tick, n.evaluated_at.t) >
          std::tie(m.evaluated_at.tick, m.evaluated_at.t))
        return invalid(id);
      FieldBattleWindowSlice step;
      step.from = m.evaluated_at;
      step.to = event.at;
      step.steps = 1;
      // The original mission source above, not today's Formation mission,
      // determines the finite Gate-terminal exception at this historical step.
      p.withdrawing = std::holds_alternative<FieldWithdraw>(n.mission);
      FieldMotionPrefixRead view{m.evaluated_at, prefix};
      auto actual = read_field_navigation_step_at_prefix(
          session, field, id, n.target_dm, step, &view);
      if (!actual.ok)
        return {false, {}, actual.error};
      if (!same(actual.value.from_dm, m.from_dm) ||
          !same(actual.value.to_dm, m.to_dm) ||
          actual.value.reached != m.reached ||
          actual.value.blocked != m.blocked)
        return invalid(id);
      if (!same(m.from_dm, m.to_dm))
        facing.at(id) = field_direction_mdeg(
            {m.to_dm.x - m.from_dm.x, m.to_dm.y - m.from_dm.y});
      p.position_dm = m.to_dm;
      if (!p.routing) {
        states.at(id) = FieldFormationState::Moving;
        holding.at(id) = false;
        hold_since.at(id).reset();
      }
      if (!same(m.from_dm, m.to_dm)) {
        holding.at(id) = false;
        hold_since.at(id).reset();
        stopped_since.at(id).reset();
      } else if (!stopped_since.at(id))
        stopped_since.at(id) = m.evaluated_at;
      ++next_formation;
      last_motion.at(id) = event.sequence;
      last_movement[id] = &event;
    } else if (event.kind == field_event_kind::Casualties) {
      if (!event.count || *event.count <= 0 || *event.count > p.fit)
        return invalid(id);
      p.fit -= *event.count;

    } else if (event.kind == field_event_kind::Rout) {
      if (!event.count || *event.count < 0 || p.routing)
        return invalid(id);
      p.routing = true;
      states.at(id) = FieldFormationState::Routing;
      holding.at(id) = false;
      hold_since.at(id).reset();
      p.fit -= std::min(p.fit, *event.count);
      if (!p.fit)
        states.at(id) = FieldFormationState::Destroyed;
    } else if (event.kind == field_event_kind::Rallied)
      p.routing = false;
    else if (event.kind == field_event_kind::Withdrawn) {
      p.withdrawn = true;
      states.at(id) = FieldFormationState::Withdrawn;
      holding.at(id) = false;
      hold_since.at(id).reset();
      if (p.routing) {
        if (!sources.original_rosters)
          return common_failure("FieldOriginalRosterSourcesRequired", id);
        auto remaining = read_field_local_roster_at_event_prefix(
            common, military, sources.military, world, session, f,
            *sources.original_rosters, event.sequence);
        if (!remaining.ok) return {false, {}, remaining.error};
        p.fit = Integer(remaining.value.fit.size());
      }
    }
  }
  if (!pending_admissions.empty() || !awaiting_first_contact.empty() ||
      in_motion || !same_time(phase_from, session.now))
    return common_failure("FieldOriginalContactPhaseCoverageRequired",
                          session.battle_id);
  if (consumed_observations.size() != observations.size())
    return invalid(session.battle_id);
  for (const auto &[id, f] : session.formations) {
    const auto &p = prefix.at(id);
    const auto same_optional_time = [&](const auto &a, const auto &b) {
      return bool(a) == bool(b) && (!a || same_time(*a, *b));
    };
    if (!same_optional_time(hold_since.at(id), f.hold_since) ||
        !same_optional_time(stopped_since.at(id), f.stopped_since))
      return common_failure("FieldOriginalStationaryTimeMismatch", id);
    if (entry_order_pending.at(id) != f.pending_player_entry_order ||
        states.at(id) != f.state ||
        !same_field_battle_mission(missions.at(id), f.mission) ||
        pursuits.at(id) != f.ai_pursuit ||
        flank_pending.at(id) != f.flank_assignment_index ||
        wave_pending.at(id) != bool(f.wave_mission) ||
        holding.at(id) != bool(f.hold_since))
      return common_failure("FieldOriginalMotionStateMismatch", id);
    if (ever_engaged.at(id) != f.has_engaged ||
        facing.at(id) != f.facing_mdeg || !same(p.position_dm, f.position_dm) ||
        p.fit != Integer(f.fit.size()) ||
        last_motion.at(id) != f.last_motion_sequence)
      return invalid(id);
    if (bool(f.navigation_target_dm) != bool(f.navigation_observation_sequence))
      return invalid(id);
    if (!f.navigation_target_dm) {
      if (f.navigation_replanned_at)
        return invalid(id);
      continue;
    }
    auto it = observations.find(*f.navigation_observation_sequence);
    if (it == observations.end() || it->second->formation_id != id ||
        !same(*f.navigation_target_dm,
              field_event_navigation_observation(*it->second)->target_dm))
      return invalid(id);
    const auto &n = *field_event_navigation_observation(*it->second);
    if (n.mission_transition_sequence != f.mission_transition_sequence ||
        n.original_ai_source_id != f.mission_ai_source_id ||
        n.original_order_id != f.adopted_order_id ||
        bool(n.original_command_result) != bool(f.mission_command_result) ||
        (n.original_command_result &&
         !same_source_ref(*n.original_command_result,
                          *f.mission_command_result)) ||
        !same_field_battle_mission(n.mission, f.mission) ||
        n.ai_pursuit != f.ai_pursuit)
      return invalid(id);
    if (f.navigation_replanned_at) {
      if (std::tie(f.navigation_replanned_at->tick,
                   f.navigation_replanned_at->t) !=
          std::tie(n.evaluated_at.tick, n.evaluated_at.t))
        return invalid(id);
    } else {
      auto move = last_movement.find(id);
      if (move == last_movement.end() ||
          !field_event_motion_application(*move->second)->blocked ||
          field_event_motion_application(*move->second)->navigation_sequence !=
              *f.navigation_observation_sequence)
        return invalid(id);
    }
  }
  return common_success();
}
} // namespace hanmo::resident
