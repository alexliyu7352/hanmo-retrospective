#include "field_event_legacy_kinds.hpp"
#include "field_event_mission_transition.hpp"
#include "field_mission_transition.hpp"
#include "field_command_source.hpp"
#include "field_deployment.hpp"
#include "field_flank.hpp"
#include "field_generation.hpp"
#include "field_integer_geometry.hpp"
#include "field_reinforcement_queue.hpp"
#include "field_validation.hpp"
#include "field_view_model.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <algorithm>
#include <limits>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
bool same_ref(const std::optional<SourceRef> &a,
              const std::optional<SourceRef> &b) {
  return bool(a) == bool(b) && (!a || same_source_ref(*a, *b));
}
bool present(const FieldFormation &f) {
  return !f.fit.empty() && f.state != FieldFormationState::Withdrawn &&
         f.state != FieldFormationState::Destroyed &&
         f.state != FieldFormationState::Deploying;
}
bool point(PointI a, PointI b) { return a.x == b.x && a.y == b.y; }
const FieldBattleEvent *event(const FieldBattleSession &s, Integer sequence) {
  const FieldBattleEvent *out = nullptr;
  for (const auto &e : s.events)
    if (e.sequence == sequence) {
      if (out)
        return nullptr;
      out = &e;
    }
  return out;
}
bool unavailable_before(const FieldBattleSession &s,
                        const FieldFormation &target, Integer sequence) {
  Integer remaining = target.fit_start;
  if (remaining <= 0)
    return false;
  for (const auto &e : s.events) {
    if (e.sequence >= sequence)
      break;
    if (e.formation_id != target.formation_id)
      continue;
    if (e.kind == field_event_kind::Withdrawn)
      return true;
    if (e.kind == field_event_kind::Casualties) {
      if (!e.count || *e.count < 0 || *e.count > remaining)
        return false;
      remaining -= *e.count;
    } else if (e.kind == field_event_kind::Rout) {
      if (!e.count || *e.count < 0)
        return false;
      // All-capture includes wounded too; original roster validation verifies
      // the exact allocation. Its fit contribution is bounded by remaining.
      remaining -= std::min(remaining, *e.count);
    }
  }
  return remaining == 0;
}
Status invalid(const StableId &id) {
  return common_failure("FieldMissionTransitionSourceInvalid", id);
}
} // namespace
Status apply_field_rule_mission_transition(
    FieldBattleSession &s, FieldFormation &f, const GeneratedBattleField &field,
    FieldMissionTransitionReason reason, const FieldBattleTime &at,
    CheckpointMemoryCharge &owner, std::optional<Integer> trigger) {
  if (s.state != FieldBattleState::Running ||
      s.field_hash != field.field_hash ||
      s.formations.find(f.formation_id) == s.formations.end() ||
      &s.formations.at(f.formation_id) != &f || f.side < 0 || f.side > 1 ||
      s.next_event_sequence <= 0 ||
      s.next_event_sequence == std::numeric_limits<Integer>::max())
    return invalid(f.formation_id);
  const __int128_t elapsed =
      (__int128_t(at.tick) - s.now.tick) * field.parameters.window_s + at.t -
      s.now.t;
  const bool endpoint = reason == FieldMissionTransitionReason::Arrived ||
                        reason == FieldMissionTransitionReason::Rallied ||
                        reason == FieldMissionTransitionReason::FollowupAdopted;
  if (elapsed != (endpoint ? field.parameters.step_s : 0))
    return invalid(f.formation_id);
  // A repeated same-point Hold is not a new result and allocates nothing.
  // Do not accumulate a persistent ownership charge on every idle step.
  if (reason == FieldMissionTransitionReason::Arrived && !f.ai_pursuit &&
      !f.followup_mission && !trigger)
    if (const auto *stay = std::get_if<FieldHold>(&f.mission);
        stay && point(stay->point_dm, f.position_dm) && f.hold_since &&
        stay->facing_mdeg == f.facing_mdeg)
      return common_success();
  std::optional<FieldFlankFollowupRead> flank;
  if (reason == FieldMissionTransitionReason::FollowupAdopted) {
    if (!f.flank_assignment_index || f.followup_mission || trigger)
      return invalid(f.formation_id);
    auto actual = read_field_flank_followup(s, field, f.formation_id,
                                            *f.flank_assignment_index);
    if (!actual.ok)
      return {false, {}, actual.error};
    if (!same_field_battle_mission(f.mission, actual.value.assignment->mission))
      return invalid(f.formation_id);
    flank = actual.value;
  }


  auto mission_text = [&](const FieldMission &m) {


  };
  // Temporary result, retained from/to, and the installed mission are all
  // charged before any potentially allocating string/variant copy.
  for (Integer i = 0; i < 4; ++i) {
    mission_text(f.mission);
    if (f.wave_mission)
      mission_text(*f.wave_mission);
    if (f.followup_mission)
      mission_text(*f.followup_mission);
  }


  (owner).grow(0);
  FieldMission to = FieldHold{f.position_dm, f.facing_mdeg};
  std::optional<StableId> target;
  if (f.pending_player_entry_order &&
      reason != FieldMissionTransitionReason::Rallied)
    return invalid(f.formation_id);
  switch (reason) {
  case FieldMissionTransitionReason::Arrived: {
    const auto *move = std::get_if<FieldMoveTo>(&f.mission);
    const auto *hold = std::get_if<FieldHold>(&f.mission);
    if ((!move && !hold) ||
        !point(move ? move->point_dm : hold->point_dm, f.position_dm) ||
        f.followup_mission || f.flank_assignment_index || trigger)
      return invalid(f.formation_id);
    to = FieldHold{f.position_dm, move ? move->facing_mdeg : hold->facing_mdeg};
    break;
  }
  case FieldMissionTransitionReason::TargetUnavailable: {
    const auto *e = std::get_if<FieldEngage>(&f.mission);
    const auto *a = std::get_if<FieldSupport>(&f.mission);
    if ((!e && !a) || trigger)
      return invalid(f.formation_id);
    target = e ? e->target_formation_id : a->ally_formation_id;
    auto found = s.formations.find(*target);
    if (found == s.formations.end() || present(found->second) ||
        !unavailable_before(s, found->second, s.next_event_sequence))
      return invalid(f.formation_id);
    break;
  }
  case FieldMissionTransitionReason::PursuitLimit: {
    if (!f.ai_pursuit || !std::holds_alternative<FieldEngage>(f.mission) ||
        trigger || s.sides[f.side].deploy_zone.empty())
      return invalid(f.formation_id);
    for (auto cell : s.sides[f.side].deploy_zone) {
      if (cell >= field.cells.size() ||
          field_distance_squared(f.position_dm,
                                 field_cell_center_dm(field, cell)) <=
              __int128_t(15000) * 15000)
        return invalid(f.formation_id);
    }
    break;
  }
  case FieldMissionTransitionReason::Rallied: {
    const auto *e = trigger ? event(s, *trigger) : nullptr;
    if (!e || e->kind != field_event_kind::Rallied ||
        e->formation_id != f.formation_id || e->side != f.side ||
        f.morale < 30 || f.state != FieldFormationState::Routing ||
        std::tie(e->at.tick, e->at.t) != std::tie(at.tick, at.t))
      return invalid(f.formation_id);
    break;
  }
  case FieldMissionTransitionReason::WaveReleased: {
    if (!f.wave_mission || !f.wave || *f.wave != FieldCouncilWave::Second ||
        trigger)
      return invalid(f.formation_id);
    bool ready = false;
    for (const auto &[id, other] : s.formations)
      if (other.side == f.side && other.wave == FieldCouncilWave::First &&
          other.has_engaged)
        ready = true;
    if (!ready)
      return invalid(f.formation_id);
    to = *f.wave_mission;
    break;
  }
  case FieldMissionTransitionReason::FollowupAdopted:
    if (!flank)
      return invalid(f.formation_id);
    if (flank->target) {
      target = flank->target->formation_id;
      to = FieldEngage{*target};
    } else
      to = FieldHold{flank->position_dm, flank->facing_mdeg};
    break;
  default:
    return invalid(f.formation_id);
  }
  const bool next_pursuit =
      reason == FieldMissionTransitionReason::WaveReleased ||
              (reason == FieldMissionTransitionReason::FollowupAdopted &&
               std::holds_alternative<FieldEngage>(to))
          ? f.ai_pursuit
          : false;

  FieldBattleEvent e;
  e.sequence = s.next_event_sequence++;
  e.kind = field_event_kind::MissionTransition;
  e.at = at;
  e.formation_id = f.formation_id;
  e.side = f.side;
  FieldMissionTransition t;
  t.reason = reason;
  t.from = f.mission;
  t.to = to;
  t.from_ai_pursuit = f.ai_pursuit;
  t.to_ai_pursuit = next_pursuit;
  t.mission_revision = f.mission_revision;
  t.previous_sequence = f.mission_transition_sequence;
  t.original_command_result = f.mission_command_result;
  t.original_ai_source_id = f.mission_ai_source_id;
  t.original_order_id = f.adopted_order_id;
  t.at_position_dm = f.position_dm;
  t.target_formation_id = target;
  t.trigger_event_sequence = trigger;
  if (flank)
    t.original_flank_assignment_index = f.flank_assignment_index;
  field_event_mission_transition(e) = std::move(t);
  f.mission_transition_sequence = e.sequence;
  s.events.push_back(std::move(e));
  f.mission = std::move(to);
  // Original forced recovery supplies its own real rule mission, not a
  // fabricated player choice. Other automatic task transitions are forbidden
  // while the entry is awaiting the player's first task.
  if (reason == FieldMissionTransitionReason::Rallied)
    f.pending_player_entry_order = false;
  if (reason != FieldMissionTransitionReason::WaveReleased)
    f.flank_assignment_index.reset();
  f.ai_pursuit = next_pursuit;
  return {true, {}};
}
Outcome<FieldMissionOriginRead>
read_field_current_mission_origin(const FieldBattleSession &s,
                                  const FieldFormation &f) {
  auto fail = [&]() -> Outcome<FieldMissionOriginRead> {
    return {false, {}, invalid(f.formation_id).error};
  };
  std::optional<Integer> latest;
  for (const auto &e : s.events) {
    if (e.kind != field_event_kind::MissionTransition ||
        e.formation_id != f.formation_id || !field_event_mission_transition(e))
      continue;
    const auto &t = *field_event_mission_transition(e);
    const bool same_revision =
        f.pending_direct
            ? t.mission_revision < f.pending_direct->accepted_revision
            : t.mission_revision == f.mission_revision;
    if (same_revision &&
        same_ref(t.original_command_result, f.mission_command_result) &&
        t.original_ai_source_id == f.mission_ai_source_id &&
        t.original_order_id == f.adopted_order_id)
      latest = e.sequence;
  }
  if (latest != f.mission_transition_sequence)
    return fail();
  const FieldMission *value = &f.mission;
  bool pursuit = f.ai_pursuit;
  auto seq = f.mission_transition_sequence;
  Integer upper = s.next_event_sequence;
  while (seq) {
    if (*seq <= 0 || *seq >= upper)
      return fail();
    const auto *e = event(s, *seq);
    if (!e || e->kind != field_event_kind::MissionTransition ||
        !field_event_mission_transition(*e) || e->formation_id != f.formation_id ||
        e->side != f.side)
      return fail();
    const auto &t = *field_event_mission_transition(*e);
    if (!same_field_battle_mission(t.to, *value) ||
        t.to_ai_pursuit != pursuit ||
        ((!f.pending_direct && t.mission_revision != f.mission_revision) ||
         (f.pending_direct &&
          t.mission_revision >= f.pending_direct->accepted_revision)) ||
        !same_ref(t.original_command_result, f.mission_command_result) ||
        t.original_ai_source_id != f.mission_ai_source_id ||
        t.original_order_id != f.adopted_order_id)
      return fail();
    value = &t.from;
    pursuit = t.from_ai_pursuit;
    upper = *seq;
    seq = t.previous_sequence;
  }
  return {true, {value, pursuit}, {}};
}
Status validate_field_mission_transition_events(
    const CommonStore &common, const FieldBattleWorldState &world,
    const FieldBattleSession &s, const GeneratedBattleField &field) {
  if (!s.started_at) {
    for (const auto &[id, f] : s.formations)
      if (f.mission_transition_sequence)
        return invalid(id);
    for (const auto &e : s.events)
      if (field_event_mission_transition(e) ||
          e.kind == field_event_kind::MissionTransition)
        return invalid(s.battle_id);
    return common_success();
  }
  const auto initial_mission = [&](const FieldFormation &f,
                                   const FieldMission &observed,
                                   bool release) -> Status {
    if (f.deployment_command_result) {
      auto source = read_field_original_deployment_application(common, s, f);
      if (!source.ok)
        return {false, {}, source.error};
      if (release && !source.value->wave_mission)
        return invalid(f.formation_id);
      const auto &expected = release ? *source.value->wave_mission
                                     : source.value->immediate_mission;
      return same_field_battle_mission(expected, observed)
                 ? common_success()
                 : invalid(f.formation_id);
    }
    bool reinforcement = false;
    for (const auto &[id, boundary] : world.boundary_contacts)
      if (boundary.battle_id == s.battle_id && boundary.reinforcement_queue)
        for (const auto &entry : boundary.reinforcement_queue->admitted)
          if (entry.formation_id == f.formation_id)
            reinforcement = true;
    if (reinforcement) {
      if (release)
        return invalid(f.formation_id);
      auto entry =
          read_field_reinforcement_admission_identity(common, world, s, f);
      if (!entry.ok)
        return {false, {}, entry.error};
      return same_field_battle_mission(entry.value->initial_mission, observed)
                 ? common_success()
                 : invalid(f.formation_id);
    }
    const auto &c = s.councils[f.side];
    const FieldCouncilPlan *plan = nullptr;
    for (const auto &proposal : c.proposals)
      if (c.adopted_proposal_id == proposal.proposal_id) {
        if (plan)
          return invalid(f.formation_id);
        plan = &proposal.plan;
      }
    if (!c.closed || !plan)
      return invalid(f.formation_id);
    const FieldCouncilAssignment *assignment = nullptr;
    for (const auto &a : plan->assignments)
      if (a.formation_id == f.formation_id) {
        if (assignment)
          return invalid(f.formation_id);
        assignment = &a;
      }
    if (!assignment)
      return invalid(f.formation_id);
    if (release || assignment->wave != FieldCouncilWave::Second)
      return (!release || assignment->wave == FieldCouncilWave::Second) &&
                     same_field_battle_mission(assignment->mission, observed)
                 ? common_success()
                 : invalid(f.formation_id);
    const FieldBattleContactRecord *contact = nullptr;
    for (const auto &[id, v] : world.contacts)
      if (v.battle_id == s.battle_id) {
        if (contact)
          return invalid(f.formation_id);
        contact = &v;
      }
    if (!contact)
      return invalid(f.formation_id);
    std::set<StableId> original_ids;
    for (const auto &binding : contact->original_bindings)
      for (const auto &member : binding.members)
        original_ids.insert(member.formation_id);
    if (!original_ids.contains(f.formation_id))
      return invalid(f.formation_id);
    FieldAiStrategy strategy;
    switch (plan->layout) {
    case FieldCouncilLayout::Hold:
      strategy = FieldAiStrategy::HoldGround;
      break;
    case FieldCouncilLayout::Line:
    case FieldCouncilLayout::Ambush:
      strategy = FieldAiStrategy::LineReserve;
      break;
    case FieldCouncilLayout::Flank:
      strategy = FieldAiStrategy::Flanking;
      break;
    default:
      return invalid(f.formation_id);
    }
    auto layout = prepare_field_original_deployment_layout(
        s, field, f.side, strategy, original_ids);
    if (!layout.ok)
      return {false, {}, layout.error};
    for (const auto &placed : layout.value.placements)
      if (placed.formation_id == f.formation_id)
        return same_field_battle_mission(
                   FieldHold{placed.point_dm, placed.facing_mdeg}, observed)
                   ? common_success()
                   : invalid(f.formation_id);
    return invalid(f.formation_id);
  };
  std::set<Integer> used_previous;
  for (const auto &e : s.events) {
    if (e.kind != field_event_kind::MissionTransition) {
      if (field_event_mission_transition(e))
        return invalid(s.battle_id);
      continue;
    }
    if (!field_event_mission_transition(e) || !e.formation_id || !e.side || e.source_id ||
        e.target_formation_id || e.count || e.tactic || e.stratagem ||
        e.success)
      return invalid(s.battle_id);
    const auto fit = s.formations.find(*e.formation_id);
    if (fit == s.formations.end() || fit->second.side != *e.side)
      return invalid(s.battle_id);
    const auto &t = *field_event_mission_transition(e);
    if (t.mission_revision < 0 ||
        (bool(t.original_command_result) + bool(t.original_ai_source_id) +
             bool(t.original_order_id) >
         1) ||
        (t.reason != FieldMissionTransitionReason::WaveReleased &&
         t.reason != FieldMissionTransitionReason::Rallied &&
         t.reason != FieldMissionTransitionReason::Arrived &&
         same_field_battle_mission(t.from, t.to) &&
         t.from_ai_pursuit == t.to_ai_pursuit))
      return invalid(*e.formation_id);
    if (t.previous_sequence) {
      const auto *p = event(s, *t.previous_sequence);
      if (!p || p->sequence >= e.sequence || !field_event_mission_transition(*p) ||
          p->formation_id != e.formation_id || p->side != e.side ||
          std::tie(p->at.tick, p->at.t) > std::tie(e.at.tick, e.at.t) ||
          !used_previous.insert(*t.previous_sequence).second)
        return invalid(*e.formation_id);
      const auto &before = *field_event_mission_transition(*p);
      if (!same_field_battle_mission(before.to, t.from) ||
          before.to_ai_pursuit != t.from_ai_pursuit ||
          before.mission_revision != t.mission_revision ||
          !same_ref(before.original_command_result,
                    t.original_command_result) ||
          before.original_ai_source_id != t.original_ai_source_id ||
          before.original_order_id != t.original_order_id)
        return invalid(*e.formation_id);
    }
    if (!t.previous_sequence && !t.original_command_result &&
        !t.original_ai_source_id && !t.original_order_id) {
      auto valid = initial_mission(fit->second, t.from, false);
      if (!valid.ok)
        return valid;
    }
    const auto *to = std::get_if<FieldHold>(&t.to);
    if (bool(t.original_flank_assignment_index) !=
        (t.reason == FieldMissionTransitionReason::FollowupAdopted))
      return invalid(*e.formation_id);
    if (t.reason != FieldMissionTransitionReason::WaveReleased &&
        t.reason != FieldMissionTransitionReason::FollowupAdopted &&
        (!to || !point(to->point_dm, t.at_position_dm) || t.to_ai_pursuit))
      return invalid(*e.formation_id);
    switch (t.reason) {
    case FieldMissionTransitionReason::Arrived: {
      const auto *move = std::get_if<FieldMoveTo>(&t.from);
      const auto *hold = std::get_if<FieldHold>(&t.from);
      if ((!move && !hold) || t.target_formation_id ||
          t.trigger_event_sequence ||
          !point(move ? move->point_dm : hold->point_dm, t.at_position_dm) ||
          to->facing_mdeg != (move ? move->facing_mdeg : hold->facing_mdeg))
        return invalid(*e.formation_id);
      break;
    }
    case FieldMissionTransitionReason::TargetUnavailable: {
      const auto *engage = std::get_if<FieldEngage>(&t.from);
      const auto *support = std::get_if<FieldSupport>(&t.from);
      if ((!engage && !support) || !t.target_formation_id ||
          t.trigger_event_sequence ||
          *t.target_formation_id != (engage ? engage->target_formation_id
                                            : support->ally_formation_id))
        return invalid(*e.formation_id);
      auto target = s.formations.find(*t.target_formation_id);
      if (target == s.formations.end() ||
          !unavailable_before(s, target->second, e.sequence))
        return invalid(*e.formation_id);
      break;
    }
    case FieldMissionTransitionReason::PursuitLimit:
      if (!t.from_ai_pursuit || !std::holds_alternative<FieldEngage>(t.from) ||
          t.target_formation_id || t.trigger_event_sequence ||
          s.sides[*e.side].deploy_zone.empty())
        return invalid(*e.formation_id);
      for (auto cell : s.sides[*e.side].deploy_zone)
        if (cell >= field.cells.size() ||
            field_distance_squared(t.at_position_dm,
                                   field_cell_center_dm(field, cell)) <=
                __int128_t(15000) * 15000)
          return invalid(*e.formation_id);
      break;
    case FieldMissionTransitionReason::Rallied: {
      const auto *r = t.trigger_event_sequence
                          ? event(s, *t.trigger_event_sequence)
                          : nullptr;
      if (!r || r->kind != field_event_kind::Rallied ||
          r->sequence >= e.sequence || r->formation_id != e.formation_id ||
          r->side != e.side || t.target_formation_id ||
          std::tie(r->at.tick, r->at.t) != std::tie(e.at.tick, e.at.t))
        return invalid(*e.formation_id);
      break;
    }
    case FieldMissionTransitionReason::WaveReleased: {
      if (t.original_command_result || t.original_ai_source_id ||
          t.original_order_id || t.target_formation_id ||
          t.trigger_event_sequence ||
          !std::holds_alternative<FieldHold>(t.from) || t.from_ai_pursuit ||
          t.to_ai_pursuit)
        return invalid(*e.formation_id);
      auto actual_assignment = initial_mission(fit->second, t.to, true);
      if (!actual_assignment.ok)
        return actual_assignment;
      bool contact = false;
      for (const auto &prior : s.events) {
        if (prior.sequence >= e.sequence ||
            prior.kind != field_event_kind::Contact ||
            prior.side != e.side || !prior.formation_id)
          continue;
        auto first = s.formations.find(*prior.formation_id);
        if (first != s.formations.end() &&
            first->second.wave == FieldCouncilWave::First)
          contact = true;
      }
      if (!contact)
        return invalid(*e.formation_id);
      break;
    }
    case FieldMissionTransitionReason::FollowupAdopted: {
      const auto move = std::get_if<FieldMoveTo>(&t.from);
      const auto engage = std::get_if<FieldEngage>(&t.to);
      if (!move || !point(move->point_dm, t.at_position_dm) ||
          t.trigger_event_sequence || !t.original_flank_assignment_index ||
          *t.original_flank_assignment_index < 0 ||
          bool(engage) != bool(t.target_formation_id) || (!engage && !to) ||
          (engage && engage->target_formation_id != *t.target_formation_id) ||
          (to && (!point(to->point_dm, t.at_position_dm) ||
                  to->facing_mdeg != move->facing_mdeg || t.to_ai_pursuit)))
        return invalid(*e.formation_id);
      break;
    }
    default:
      return invalid(*e.formation_id);
    }
  }
  for (const auto &[id, f] : s.formations) {
    auto origin = read_field_current_mission_origin(s, f);
    if (!origin.ok)
      return {false, {}, origin.error};
    if (!f.mission_command_result && !f.mission_ai_source_id &&
        !f.adopted_order_id) {
      auto valid = initial_mission(f, *origin.value.mission, false);
      if (!valid.ok)
        return valid;
    }
  }
  return {true, {}};
}
} // namespace hanmo::resident
