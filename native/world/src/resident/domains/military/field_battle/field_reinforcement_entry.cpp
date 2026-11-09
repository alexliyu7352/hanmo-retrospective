#include "field_event_legacy_kinds.hpp"
#include "field_reinforcement_entry.hpp"
#include "field_ability_sources.hpp"
#include "field_binding_source.hpp"
#include "field_command_source.hpp"
#include "field_reinforcement_geometry.hpp"
#include "field_segment_settlement.hpp"
#include "resident/domains/military/battle_world.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {
namespace {
using namespace movement_detail;


bool contains(const FieldFormation &formation, const StableId &actor) {
  if (formation.commander_actor_id == actor)
    return true;
  for (const auto *group : {&formation.fit, &formation.wounded, &formation.dead,
                            &formation.captured, &formation.deserted})
    if (std::find(group->begin(), group->end(), actor) != group->end())
      return true;
  return false;
}
} // namespace
static Outcome<SourceRef> admit_next_field_reinforcement(
    CommonStore &common, FieldBattleWorldState &world,
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    const FieldOriginalRosterReadContext &history,
    const FieldBattleAbilityReadContext &abilities,
    const GeneratedBattleField &field, const SourceRef &contact,
    CheckpointMemoryCharge &owner, ExactArithmeticBudget b, bool player_led) {
  return guard<SourceRef>([&] {
    if (&common != &x.common || &world != &x.field ||
        abilities.exact_contacts != &x ||
        abilities.original_rosters != &history ||
        abilities.actual_clock != &x.clock)
      fail("FieldReinforcementSameCandidateRequired", contact.id);
    auto prepared = take(prepare_next_field_reinforcement_in_candidate(
        x, catalog, history, contact, b));
    if (!player_led &&
        !std::holds_alternative<FieldAiController>(
            prepared.session->sides[*prepared.boundary->side].controller))
      fail("FieldReinforcementAiSideRequired", contact.id);
    if (prepared.session->field_hash != field.field_hash ||
        prepared.session->next_event_sequence <= 0 ||
        prepared.session->next_event_sequence ==
            std::numeric_limits<Integer>::max())
      fail("FieldReinforcementActualFieldRequired", contact.id);
    const auto *origin =
        std::get_if<FieldExactBoundaryOrigin>(&prepared.boundary->motion);
    if (!origin)
      fail("FieldReinforcementActualBoundaryRequired", contact.id);
    const auto original = take(read_field_boundary_original_source(
        x, catalog, origin->actual_contact, b));
    if (!original.army || !original.incoming_facing_mdeg)
      fail("FieldReinforcementIncomingDirectionRequired", contact.id);
    // Side AI does not turn an arriving player-led Army into an automatic
    // task. Its queued command-authority boundary is handled separately.
    const bool actual_player_led = original.army->commander_actor_id ==
                                   common.state().metadata.player_actor_id;
    if (actual_player_led != player_led)
      fail("FieldReinforcementControllerSelectionMismatch", contact.id);
    // A non-task payload is inert only under the explicit pending marker.
    // No Hold point or MoveTo target is chosen on the player's behalf.
    FieldMission mission = FieldHold{};
    if (player_led) {
      BattleUnit projected;
      projected.unit_id = prepared.formation.formation_id;
      projected.army_id = prepared.formation.army_id;
      projected.commander_actor_id = prepared.formation.commander_actor_id;
      projected.side = prepared.formation.side;
      const auto authority = take(military_battle_control(
          common, x.military, x.military_catalog, {projected},
          original.army->commander_actor_id));
      if (authority.scope != "army" || authority.army_id != original.army->id ||
          authority.side != *prepared.boundary->side)
        fail("FieldReinforcementPlayerOriginalAuthorityRequired", contact.id);
    } else {
      const auto rally = take(read_field_reinforcement_rally(
          *prepared.session, field, *prepared.boundary->side,
          prepared.formation.category));
      const bool commander = prepared.formation.commander_actor_id ==
                             original.army->commander_actor_id;
      mission = FieldMoveTo{commander ? rally.commander_dm : rally.rally_dm,
                            *original.incoming_facing_mdeg};
    }
    auto &session = world.sessions.at(prepared.boundary->battle_id);
    auto &boundary = world.boundary_contacts.at(contact.id);
    auto &queue = *boundary.reinforcement_queue;
    const bool first = queue.admitted.empty();
    const auto active = world.army_bindings.find(*boundary.army_id);
    if (first
            ? active != world.army_bindings.end()
            : active == world.army_bindings.end() || !boundary.entered_binding)
      fail("FieldReinforcementActiveBindingMismatch", contact.id);
    if (!first) {
      const auto *prior = take(read_field_original_battle_binding(
          common, world, session.battle_id, *boundary.army_id));
      const auto &current = active->second;
      const auto *origin_binding =
          std::get_if<FieldBoundaryContactBinding>(&current.source);
      if (prior != &*boundary.entered_binding || !origin_binding ||
          origin_binding->contact_id != contact.id ||
          current.army_id != prior->army_id ||
          current.battle_id != prior->battle_id ||
          current.action_id != prior->action_id ||
          current.commander_actor_id != prior->commander_actor_id ||
          current.members.size() != prior->members.size())
        fail("FieldReinforcementActiveBindingMismatch", contact.id);
      for (std::size_t n = 0; n < current.members.size(); ++n)
        if (current.members[n].actor_id != prior->members[n].actor_id ||
            current.members[n].formation_id != prior->members[n].formation_id)
          fail("FieldReinforcementActiveBindingMismatch", contact.id);
    }
    const auto id = take(common.next_id("field_entry"));
    const SourceRef source{"military", "field_battle_entered", id};

    // Transfer the actual prepared field payload's reservation to the outer
    // candidate before moving it; the temporary lease remains alive meanwhile.
    (void)0;


     // owning map key
    (void)0;
    (owner).grow(0);
    FieldBattleRoadBinding binding;
    if (first) {


      (owner).grow(0);
      binding.army_id = *boundary.army_id;
      binding.battle_id = session.battle_id;
      binding.commander_actor_id = original.army->commander_actor_id;
      binding.source = FieldBoundaryContactBinding{contact.id};
    } else {


      (owner).grow(0);
      binding = *boundary.entered_binding;
    }
    std::size_t added = 0;
    for (const auto &actor : boundary.participant_actor_ids)
      if (contains(prepared.formation, actor))
        ++added;
    if (!added)
      fail("FieldReinforcementOriginalMembersRequired", source.id);


    for (const auto &actor : boundary.participant_actor_ids)
      if (contains(prepared.formation, actor)) {


      }
    (owner).grow(0);
    binding.members.reserve(binding.members.size() + added);
    for (const auto &actor : boundary.participant_actor_ids)
      if (contains(prepared.formation, actor)) {
        for (const auto &member : binding.members)
          if (member.actor_id == actor)
            fail("FieldReinforcementMemberAlreadyEntered", actor);
        binding.members.push_back({actor, prepared.formation.formation_id});
      }


    (owner).grow(0);
    boundary.entered_binding = binding;
    world.army_bindings.insert_or_assign(*boundary.army_id, std::move(binding));
    FieldReinforcementAdmission admission;
    admission.application_source = source;
    admission.formation_id = prepared.formation.formation_id;
    admission.at = session.now;
    admission.applied_at = prepared.actual_at;
    admission.event_sequence = session.next_event_sequence;
    admission.position_dm = boundary.entry_point_dm;
    admission.facing_mdeg = *original.incoming_facing_mdeg;
    admission.initial_control = prepared.formation.control;
    admission.initial_mission = mission;
    admission.ai_mission = !player_led;
    admission.pending_player_entry_order = player_led;
    std::vector<FieldReinforcementAdmission> admissions;
    admissions.reserve(queue.admitted.size() + 1);
    for (auto &old : queue.admitted)
      admissions.push_back(std::move(old));
    admissions.push_back(std::move(admission));
    queue.admitted = std::move(admissions);
    auto &formation = prepared.formation;
    formation.joined_at = session.now;
    formation.position_dm = boundary.entry_point_dm;
    formation.facing_mdeg = *original.incoming_facing_mdeg;
    formation.state = FieldFormationState::Moving;
    formation.mission = mission;
    formation.pending_player_entry_order = player_led;
    const auto formation_id = formation.formation_id;
    session.formations.emplace(formation_id, std::move(formation));
    std::vector<FieldBattleEvent> events;
    events.reserve(session.events.size() + 1);
    for (auto &old : session.events)
      events.push_back(std::move(old));
    FieldBattleEvent event;
    event.sequence = session.next_event_sequence++;
    event.at = session.now;
    event.kind = field_event_kind::Entered;
    event.formation_id = formation_id;
    event.source_id = source.id;
    event.side = *boundary.side;
    events.push_back(std::move(event));
    session.events = std::move(events);
    if (first) {
      boundary.entry_application_source = source;
      boundary.entered_at = session.now;
      boundary.entered_actual_at = prepared.actual_at;
      boundary.entered_observed_tick = prepared.actual_at.half_day_tick;
    }
    boundary.status = FieldBoundaryStatus::Entered;
    take(common.append_fact({source.id, source.kind, source.domain}));
    if (!player_led)
      take(validate_field_reinforcement_ai_control_source(
          common, world, session, session.formations.at(formation_id),
          abilities.original_life));
    take(initialize_field_reinforcement_ability_in_candidate(
        common, abilities, world, session, session.formations.at(formation_id),
        owner));
    if (queue.admitted.size() == queue.formation_ids.size())
      session.pending_entries.erase(
          std::remove_if(session.pending_entries.begin(),
                         session.pending_entries.end(),
                         [&](const auto &entry) {
                           return entry.contact_id == contact.id;
                         }),
          session.pending_entries.end());
    take(read_field_reinforcement_admission_identity(
        common, world, session, session.formations.at(formation_id)));
    return source;
  });
}
Outcome<SourceRef> admit_next_ai_field_reinforcement_in_candidate(
    CommonStore &common, FieldBattleWorldState &world,
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    const FieldOriginalRosterReadContext &history,
    const FieldBattleAbilityReadContext &abilities,
    const GeneratedBattleField &field, const SourceRef &contact,
    CheckpointMemoryCharge &owner, ExactArithmeticBudget b) {
  return admit_next_field_reinforcement(common, world, x, catalog, history,
                                        abilities, field, contact, owner, b,
                                        false);
}
Outcome<SourceRef> admit_next_player_field_reinforcement_in_candidate(
    CommonStore &common, FieldBattleWorldState &world,
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    const FieldOriginalRosterReadContext &history,
    const FieldBattleAbilityReadContext &abilities,
    const GeneratedBattleField &field, const SourceRef &contact,
    CheckpointMemoryCharge &owner, ExactArithmeticBudget b) {
  return admit_next_field_reinforcement(common, world, x, catalog, history,
                                        abilities, field, contact, owner, b,
                                        true);
}
} // namespace hanmo::resident
