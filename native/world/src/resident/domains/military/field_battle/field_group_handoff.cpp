#include "field_group_handoff.hpp"
#include "field_binding_source.hpp"
#include "field_boundary_source.hpp"
#include "field_reinforcement_queue.hpp"
#include "field_segment_settlement.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/travel/movement_boundary_model.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/domains/travel/field_battle_exit_placement.hpp"
#include <set>
#include <algorithm>
#include <limits>
namespace hanmo::resident {
// 多编队从各自不可变exit及原S1释放行读取；无terminal、漏腿、重复成员均拒绝。
Outcome<std::vector<FieldTerminalReturnLegRead>> read_field_terminal_return_legs(
    const FieldGroupHandoffReadContext &x, const SourceRef &terminal_segment,
    const StableId &army_id, const GroupContactMemberReleaseRead &releases,
    const FieldBattleExitPlacementRead &placements, ExactArithmeticBudget budget) {
  auto fail = [&](const char *code)
      -> Outcome<std::vector<FieldTerminalReturnLegRead>> {
    return {false, {}, common_failure(code, army_id).error};
  };
  if (!x.clock.bound_to(x.common, x.military, x.field) ||
      !releases.bound_to(x.common, x.movement, x.groups) ||
      !placements.bound_to(x.common, x.movement))
    return fail("FieldReturnOriginalOwnersRequired");
  auto terminal = read_field_segment_settlement_source(
      x.common, x.military, x.field, terminal_segment, &x.clock);
  if (!terminal.ok)
    return {false, {}, terminal.error};
  if (!terminal.value.terminal || !terminal.value.session ||
      !terminal.value.settlement || !terminal.value.session->result ||
      terminal.value.session->state != FieldBattleState::Ended)
    return fail("FieldReturnActualTerminalRequired");
  const auto &session = *terminal.value.session;
  std::vector<FieldTerminalReturnLegRead> result;
  std::set<StableId> actors, actual_exits;
  bool original_army = false;
  for (const auto &[id, formation] : session.formations) {
    if (formation.army_id != army_id)
      continue;
    original_army = true;
    if (formation.state != FieldFormationState::Withdrawn)
      continue;
    auto departure = read_field_actual_departure_source(
        x.common, x.field, session.battle_id, id);
    if (!departure.ok)
      return {false, {}, departure.error};
    const FieldBattleExitRecord *exit = nullptr;
    for (const auto &[key, row] : x.field.exits)
      if (row.battle_id == session.battle_id && row.formation_id == id) {
        if (exit || key != row.fact_id)
          return fail("FieldReturnExitSourceAmbiguous");
        exit = &row;
      }
    if (!exit)
      return fail("FieldReturnActualExitRequired");
    const SourceRef source{"military", "field_battle_exit", exit->fact_id};
    auto actual = read_field_actual_exit_application(
        x.common, x.military, x.field, x.travel, x.catalog, source, x.clock,
        budget, x.exact_contacts, x.diplomacy, x.territory);
    if (!actual.ok)
      return {false, {}, actual.error};
    if (actual.value != exit || exit->army_id != army_id)
      return fail("FieldReturnExitSourceMismatch");
    auto before = compare_world_moments(exit->applied_at,
                                        terminal.value.settlement->applied_at, budget);
    if (!before.ok)
      return {false, {}, before.error};
    if (before.value > 0)
      return fail("FieldReturnExitAfterTerminal");
    // 原五类是完整成员ID而非匿名数量；departure reader已要求每名fit/wounded
    // 都在原actor binding且存活者进入surviving_actor_ids。这里显式防守该前提，
    // 不能把未来扩展的无名活兵误当全损，也不制造空成员release或空transport。
    if (departure.value.surviving_actor_ids.empty()) {
      if (!formation.fit.empty() || !formation.wounded.empty())
        return fail("FieldReturnLivingRosterWithoutActorBinding");
      if (!exit->named_actor_ids.empty())
        return fail("FieldReturnActualMembersMismatch");
      for (const auto &row : x.groups.contact_member_releases.rows())
        if (same_source_ref(row.second.actual_release_source, source))
          return fail("FieldReturnUnexpectedEmptyMemberRelease");
      actual_exits.insert(exit->fact_id);
      continue;
    }
    const GroupContactMemberReleaseSource *release = nullptr;
    for (const auto &row : x.groups.contact_member_releases.rows())
      if (same_source_ref(row.second.actual_release_source, source)) {
        if (release || row.first != row.second.id)
          return fail("FieldReturnMemberReleaseAmbiguous");
        release = &row.second;
      }
    if (!release || release->named_participants != exit->named_actor_ids)
      return fail("FieldReturnActualMemberHandoffRequired");
    auto checked = releases.validate_actual_group_member_release(*release);
    if (!checked.ok)
      return {false, {}, checked.error};
    std::optional<MovementAnchor> anchor;
    for (const auto &actor : exit->named_actor_ids) {
      if (!actors.insert(actor).second)
        return fail("FieldReturnMemberInMultipleLegs");
      auto placement = placements.read_field_exit_placement({source, 1}, actor);
      if (!placement.ok)
        return {false, {}, placement.error};
      if (anchor) {
        auto same = same_spatial_anchor(*anchor, placement.value.actual_anchor, budget);
        if (!same.ok)
          return {false, {}, same.error};
        if (!same.value)
          return fail("FieldReturnFormationPlacementMismatch");
      } else {
        anchor = placement.value.actual_anchor;
      }
    }
    if (!anchor)
      return fail("FieldReturnActualMembersRequired");
    actual_exits.insert(exit->fact_id);
    result.push_back({exit, &formation, release, std::move(*anchor)});
  }
  if (!original_army)
    return fail("FieldReturnArmyOriginalParticipationRequired");
  // 不能让另一份真实退出正文因编队键/军团键错误而落在枚举外。
  for (const auto &[id, exit] : x.field.exits)
    if (exit.battle_id == session.battle_id && exit.army_id == army_id &&
        !actual_exits.contains(id))
      return fail("FieldReturnExitReverseCoverageRequired");
  return {true, std::move(result), {}};
}
Outcome<FieldSingleGroupExitSourceRead>
read_field_single_group_exit_source(const FieldGroupHandoffReadContext &x,
                                    const FieldBattleExitRecord &exit,
                                    ExactArithmeticBudget budget) {
  auto fail = [&]() -> Outcome<FieldSingleGroupExitSourceRead> {
    return {false,
            {},
            common_failure("FieldSingleGroupExitSourceMismatch", exit.fact_id)
                .error};
  };
  const auto saved = x.field.exits.find(exit.fact_id);
  if (saved == x.field.exits.end() || &saved->second != &exit ||
      !x.exact_contacts || &x.exact_contacts->common != &x.common ||
      &x.exact_contacts->military != &x.military ||
      &x.exact_contacts->field != &x.field ||
      &x.exact_contacts->travel != &x.travel ||
      &x.exact_contacts->movement != &x.movement ||
      &x.exact_contacts->groups != &x.groups ||
      &x.exact_contacts->carriers != &x.carriers ||
      &x.exact_contacts->clock != &x.clock)
    return fail();
  auto binding = read_field_original_battle_binding(
      x.common, x.field, exit.battle_id, exit.army_id);
  if (!binding.ok)
    return {false, {}, binding.error};
  const auto *bound =
      std::get_if<FieldBoundaryContactBinding>(&binding.value->source);
  if (!bound)
    return fail();
  const auto boundary = x.field.boundary_contacts.find(bound->contact_id);
  if (boundary == x.field.boundary_contacts.end() ||
      boundary->second.battle_id != exit.battle_id ||
      boundary->second.army_id != exit.army_id ||
      !boundary->second.reinforcement_queue)
    return fail();
  const auto *origin =
      std::get_if<FieldExactBoundaryOrigin>(&boundary->second.motion);
  if (!origin)
    return fail();
  auto actual = read_field_boundary_original_source(
      *x.exact_contacts, x.catalog, origin->actual_contact, budget);
  if (!actual.ok)
    return {false, {}, actual.error};
  const auto &source = actual.value;
  if (!source.contact || !source.journey || !source.opening || !source.closed ||
      !source.army || source.army->id != exit.army_id)
    return fail();
  const auto session = x.field.sessions.find(exit.battle_id);
  if (session == x.field.sessions.end())
    return fail();
  const auto formation = session->second.formations.find(exit.formation_id);
  if (formation == session->second.formations.end())
    return fail();
  auto entry = read_field_reinforcement_admission_identity(
      x.common, x.field, session->second, formation->second);
  if (!entry.ok)
    return {false, {}, entry.error};
  if (std::none_of(boundary->second.reinforcement_queue->admitted.begin(),
                   boundary->second.reinforcement_queue->admitted.end(),
                   [&](const auto &row) { return &row == entry.value; }))
    return fail();
  for (const auto &actor : exit.named_actor_ids) {
    if (std::count(source.opening->named_participants.begin(),
                   source.opening->named_participants.end(), actor) != 1 ||
        std::count_if(binding.value->members.begin(),
                      binding.value->members.end(), [&](const auto &row) {
                        return row.actor_id == actor &&
                               row.formation_id == exit.formation_id;
                      }) != 1)
      return fail();
  }
  const std::array<SourceRef, 6> ordered{
      {{"travel", "group_movement_opening", source.opening->id},
       origin->actual_contact,
       {"travel", "movement_unit_close", source.closed->id},
       {"military", "field_battle_contact", boundary->first},
       entry.value->application_source,
       {"military", "field_battle_exit", exit.fact_id}}};
  std::optional<std::size_t> previous;
  for (const auto &ref : ordered) {
    auto position = read_actual_common_fact_position(x.common, ref);
    if (!position.ok)
      return {false, {}, position.error};
    if (previous && position.value <= *previous)
      return fail();
    previous = position.value;
  }
  auto after_contact = compare_world_moments(source.contact->occurred_at,
                                             entry.value->applied_at, budget);
  auto after_entry =
      compare_world_moments(entry.value->applied_at, exit.applied_at, budget);
  if (!after_contact.ok)
    return {false, {}, after_contact.error};
  if (!after_entry.ok)
    return {false, {}, after_entry.error};
  if (after_contact.value > 0 || after_entry.value > 0)
    return fail();
  return {true,
          {source.contact, source.journey, source.opening, source.closed},
          {}};
}
Outcome<PreparedFieldGroupMemberHandoff>
prepare_field_group_member_handoff(const FieldGroupHandoffReadContext &x,
                                   const SourceRef &source,
                                   ExactArithmeticBudget budget) {
  auto fail =
      [&](const char *code) -> Outcome<PreparedFieldGroupMemberHandoff> {
    return {false, {}, common_failure(code, source.id).error};
  };
  auto original = read_field_actual_exit_application(
      x.common, x.military, x.field, x.travel, x.catalog, source, x.clock,
      budget, x.exact_contacts, x.diplomacy, x.territory);
  if (!original.ok)
    return {false, {}, original.error};
  const auto &exit = *original.value;
  if (exit.named_actor_ids.empty())
    return fail("FieldGroupActualMembersRequired");
  const auto capture = x.common.actor_location_capture_identity();
  if (!capture || x.common.actor_location_execution_drafts().size() >=
                      std::size_t(std::numeric_limits<Integer>::max()))
    return fail("FieldGroupHandoffCaptureRequired");
  const auto binding = x.carriers.bindings.find("army:" + exit.army_id);
  const auto direct =
      binding ? std::get_if<DirectGroupMovement>(&binding->execution_owner)
              : nullptr;
  const auto journey =
      direct ? x.carriers.journeys.find(direct->group_movement_id) : nullptr;
  const auto carrier =
      journey ? std::get_if<ArmyCarrierRef>(&journey->carrier) : nullptr;
  if (!journey || !carrier || carrier->army_id != exit.army_id ||
      journey->state != GroupJourneyLifecycle::paused ||
      !journey->stop_reason || x.carriers.terminals.find(journey->journey_id))
    return fail("FieldGroupContactPausedJourneyRequired");
  const auto prefix = x.movement.prefixes.find(journey->movement_prefix_ref);
  const auto closed =
      prefix ? std::get_if<MovementClosed>(&prefix->lifecycle) : nullptr;
  const auto close =
      prefix ? x.movement.unit_closes.find(prefix->unit_ref + ":close")
             : nullptr;
  const auto execution =
      prefix ? std::get_if<GroupMovementExecution>(&prefix->execution)
             : nullptr;
  if (!execution || execution->journey_id != journey->journey_id ||
      spatial_carrier_key(execution->carrier) !=
          spatial_carrier_key(journey->carrier) ||
      !closed || !close || close->kind != MovementCloseKind::contact ||
      close->prefix_id != prefix->id || close->unit_ref != prefix->unit_ref ||
      !same_source_ref(closed->original_result_ref, close->actual_cause) ||
      !same_source_ref(*journey->stop_reason, close->actual_cause) ||
      close->actual_cause.domain != "travel" ||
      (close->actual_cause.kind != "movement_contact" &&
       close->actual_cause.kind != "movement_boundary_contact"))
    return fail("FieldGroupOriginalContactCloseRequired");
  if (close->actual_cause.kind == "movement_boundary_contact") {
    auto actual_entry = read_field_single_group_exit_source(x, exit, budget);
    if (!actual_entry.ok)
      return {false, {}, actual_entry.error};
    if (actual_entry.value.journey != journey ||
        actual_entry.value.closed != close)
      return fail("FieldGroupSingleOriginalJourneyMismatch");
  } else {
    const auto contact = x.movement.contacts.find(close->actual_cause.id);
    if (!contact || (contact->first_prefix_id != prefix->id &&
                     contact->second_prefix_id != prefix->id))
      return fail("FieldGroupOriginalContactRequired");
    const auto military_binding = x.field.army_bindings.find(exit.army_id);
    if (military_binding == x.field.army_bindings.end() || military_binding->second.battle_id != exit.battle_id)
      return fail("FieldGroupOriginalBattleBindingRequired");
    StableId contact_id;
    std::visit([&](const auto &v) { contact_id = v.contact_id; },
               military_binding->second.source);
    const SourceRef field_contact{"military", "field_battle_contact",
                                  contact_id};
    const MovementContactInstallationSource *installation = nullptr;
    for (const auto &row : x.movement.contact_installations.rows()) {
      if (row.second.contact_ref != contact->id ||
          !same_source_ref(row.second.actual_business_source, field_contact))
        continue;
      if (installation || row.first != row.second.id ||
          row.second.business_source_schema != 1)
        return fail("FieldGroupContactInstallationAmbiguous");
      installation = &row.second;
    }
    if (!installation)
      return fail("FieldGroupActualFieldInstallationRequired");
    auto close_position = read_actual_common_fact_position(
        x.common, SourceRef{"travel", "movement_unit_close", close->id});
    if (!close_position.ok)
      return {false, {}, close_position.error};
    auto contact_position =
        read_actual_common_fact_position(x.common, close->actual_cause);
    auto field_position =
        read_actual_common_fact_position(x.common, field_contact);
    auto installed_position = read_actual_common_fact_position(
        x.common,
        SourceRef{"travel", "movement_contact_installation", installation->id});
    auto exit_position = read_actual_common_fact_position(x.common, source);
    if (!contact_position.ok)
      return {false, {}, contact_position.error};
    if (!field_position.ok)
      return {false, {}, field_position.error};
    if (!installed_position.ok)
      return {false, {}, installed_position.error};
    if (!exit_position.ok)
      return {false, {}, exit_position.error};
    if (close_position.value >= exit_position.value ||
        contact_position.value >= field_position.value ||
        field_position.value >= installed_position.value ||
        installed_position.value >= exit_position.value)
      return fail("FieldGroupOriginalSourceOrderMismatch");
    auto same_install = compare_world_moments(installation->occurred_at,
                                              contact->occurred_at, budget);
    auto close_time =
        compare_world_moments(close->occurred_at, contact->occurred_at, budget);
    auto before_exit =
        compare_world_moments(contact->occurred_at, exit.applied_at, budget);
    if (!same_install.ok)
      return {false, {}, same_install.error};
    if (!close_time.ok)
      return {false, {}, close_time.error};
    if (!before_exit.ok)
      return {false, {}, before_exit.error};
    if (same_install.value != 0 || close_time.value != 0 ||
        before_exit.value > 0)
      return fail("FieldGroupOriginalMomentMismatch");
  }
  const auto revision = read_group_composition_revision_before(x.common,x.groups,journey->journey_id);
  if(!revision.ok)return {false,{},revision.error};
  const GroupMovementOpeningSource *opening = nullptr;
  for (const auto &row : x.groups.openings.rows())
    if (row.second.journey_id == journey->journey_id) {
      const auto army = std::get_if<ArmyCarrierRef>(&row.second.carrier_origin);
      if (opening || row.first != row.second.id || !army ||
          army->army_id != exit.army_id || !row.second.military_order ||
          revision.value != journey->composition_revision)
        return fail("FieldGroupOriginalOpeningMismatch");
      opening = &row.second;
    }
  if (!opening)
    return fail("FieldGroupOriginalOpeningRequired");
  for (const auto &actor : exit.named_actor_ids) {
    if (std::count(opening->named_participants.begin(),
                   opening->named_participants.end(), actor) != 1)
      return fail("FieldGroupOriginalMemberRequired");
    for (const auto &row : x.groups.contact_member_releases.rows())
      if (row.second.journey_id == journey->journey_id &&
          std::find(row.second.named_participants.begin(),
                    row.second.named_participants.end(),
                    actor) != row.second.named_participants.end())
        return fail("FieldGroupMemberAlreadyReleased");
    const auto member = x.movement.spatial_bindings.find("actor:" + actor);
    const auto carried =
        member ? std::get_if<CarriedMovementWriter>(&member->writer) : nullptr;
    const auto army =
        carried ? std::get_if<ArmyCarrierRef>(&carried->carrier_ref) : nullptr;
    if (!army || army->army_id != exit.army_id ||
        member->subject_kind != "actor" || member->subject_id != actor ||
        !same_source_ref(
            member->binding_source,
            SourceRef{"travel", "group_movement_opening", opening->id}))
      return fail("FieldGroupOriginalCarriedMemberRequired");
  }
  PreparedFieldGroupMemberHandoff result;
  result.common_ = &x.common;
  result.exit_ = &exit;
  result.journey_ = journey;
  result.opening_ = opening;
  result.original_close_ = close;
  result.capture_ = capture;
  result.first_sequence_ =
      Integer(x.common.actor_location_execution_drafts().size()) + 1;
  return {true, std::move(result), {}};
}
} // namespace hanmo::resident

#include "resident/domains/travel/field_battle_exit_placement.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <charconv>
namespace hanmo::resident {
Outcome<SourceRef> adopt_field_group_member_handoff_in_candidate(
    CommonStore &common, MovementExactState &movement,
    GroupMovementExecutionState &groups, const FieldGroupHandoffReadContext &x,
    PreparedFieldGroupMemberHandoff &&prepared,
    const std::vector<BoundSpatialLocationWrite> &writes,
    const FieldBattleExitPlacementRead &placements,
    CheckpointMemoryCharge &owner, ExactArithmeticBudget budget) {
  auto fail = [&](const char *code) -> Outcome<SourceRef> {
    return {false, {}, common_failure(code).error};
  };
  if (&common != &x.common || &movement != &x.movement ||
      &groups != &x.groups || prepared.common_ != &common || !prepared.exit_ ||
      !prepared.journey_ || !prepared.opening_ || !prepared.original_close_ ||
      prepared.capture_ != common.actor_location_capture_identity() ||
      !placements.bound_to(common, movement))
    return fail("FieldGroupHandoffPermitMismatch");
  const auto &exit = *prepared.exit_;
  const auto &journey = *prepared.journey_;
  if (x.field.exits.find(exit.fact_id) == x.field.exits.end() ||
      &x.field.exits.at(exit.fact_id) != prepared.exit_ ||
      x.carriers.journeys.find(journey.journey_id) != prepared.journey_ ||
      groups.openings.find(prepared.opening_->id) != prepared.opening_ ||
      movement.unit_closes.find(prepared.original_close_->id) !=
          prepared.original_close_ ||
      journey.state != GroupJourneyLifecycle::paused)
    return fail("FieldGroupHandoffOriginalChanged");
  const SourceRef source{"military", "field_battle_exit", exit.fact_id};
  auto now = x.clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  auto same_m = compare_world_moments(now.value, exit.applied_at, budget);
  if (!same_m.ok)
    return {false, {}, same_m.error};
  if (same_m.value != 0)
    return fail("FieldGroupHandoffActualMomentMismatch");
  for (const auto &actor : exit.named_actor_ids) {
    const BoundSpatialLocationWrite *write = nullptr;
    for (const auto &candidate : writes) {
      if (!same_source_ref(candidate.actual_source, source) ||
          candidate.actor_id != actor)
        continue;
      if (write || candidate.source_schema != 1 ||
          candidate.capture_sequence < prepared.first_sequence_)
        return fail("FieldGroupHandoffActualWriteAmbiguous");
      write = &candidate;
    }
    if (!write)
      return fail("FieldGroupHandoffActualWriteRequired");
    auto actual = read_actual_actor_spatial_write(
        common, movement, *write, &x.carriers, nullptr, budget, nullptr,
        nullptr, nullptr, &placements);
    if (!actual.ok)
      return {false, {}, actual.error};
    auto same = compare_world_moments(actual.value.occurred_at, exit.applied_at,
                                      budget);
    if (!same.ok)
      return {false, {}, same.error};
    if (same.value != 0 || actual.value.actor_id != actor)
      return fail("FieldGroupHandoffActualWriteMismatch");
    const auto binding = movement.spatial_bindings.find("actor:" + actor);
    if (!binding ||
        !std::holds_alternative<StableMovementWriter>(binding->writer) ||
        !same_source_ref(binding->binding_source, source))
      return fail("FieldGroupHandoffActualStableWriterRequired");
    for (const auto &row : groups.contact_member_releases.rows())
      if (row.second.journey_id == journey.journey_id &&
          std::find(row.second.named_participants.begin(),
                    row.second.named_participants.end(),
                    actor) != row.second.named_participants.end())
        return fail("FieldGroupMemberAlreadyReleased");
  }
  for (const auto &candidate : writes)
    if (same_source_ref(candidate.actual_source, source) &&
        std::count(exit.named_actor_ids.begin(), exit.named_actor_ids.end(),
                   candidate.actor_id) != 1)
      return fail("FieldGroupHandoffUnexpectedActualWrite");
  if (common.state().next_instance_id <= 0 ||
      common.state().next_instance_id == std::numeric_limits<Integer>::max())
    return fail("FieldGroupReleaseIdExhausted");
  char digits[32];
  auto serial = std::to_chars(digits, digits + sizeof(digits),
                              common.state().next_instance_id);
  if (serial.ec != std::errc{})
    return fail("FieldGroupReleaseIdExhausted");
  ProducedStableIdBuildView id_build{
      "group_contact_member_release",
      std::string_view(digits, std::size_t(serial.ptr - digits))};


  (owner).grow(0);
  auto id = common.next_id("group_contact_member_release");
  if (!id.ok)
    return {false, {}, id.error};
  GroupContactMemberReleaseSource row{id.value,
                                      journey.journey_id,
                                      prepared.original_close_->actual_cause,
                                      source,
                                      1,
                                      exit.applied_at,
                                      exit.named_actor_ids};
  groups.contact_member_releases.set(id.value, std::move(row));
  auto appended =
      common.append_fact({id.value, "group_contact_member_release", "travel"});
  if (!appended.ok)
    return {false, {}, appended.error};
  // Neither the original opening nor the remaining members' writers are
  // touched. A retry must go through the enclosing original command result, not
  // reuse this permit.
  prepared.common_ = nullptr;
  prepared.exit_ = nullptr;
  prepared.capture_.reset();
  return {true,
          {"travel", "group_contact_member_release", std::move(id.value)},
          {}};
}
} // namespace hanmo::resident
