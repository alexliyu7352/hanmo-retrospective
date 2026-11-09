#include "field_binding_source.hpp"
#include "field_army_exit_ready.hpp"
#include "field_ability_sources.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include "field_boundary_source.hpp"
#include "field_exit_placement.hpp"
#include "field_reinforcement_queue.hpp"
#include "field_segment_settlement.hpp"
#include "field_winner_continuation.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/travel/movement_boundary_model.hpp"
#include "resident/domains/travel/movement_boundary_continuation_source.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
namespace hanmo::resident {
Outcome<CheckpointVector<FieldActorEngagementRead>>
OriginalFieldBattleExitPlacementRead::read_field_actor_engagement_history(
    const StableId &actor) const {
  const auto &x = context_;
  auto fail = [&](const char *code)
      -> Outcome<CheckpointVector<FieldActorEngagementRead>> {
    return {false, {}, common_failure(code, actor).error};
  };
  if (!x.common.actor(actor) ||
      !x.clock.bound_to(x.common, x.military, x.field))
    return fail("FieldEngagementOriginalOwnerRequired");
  auto now = x.clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  CheckpointVector<FieldActorEngagementRead> result;
  auto actual_release = [&](const StableId &journey, const SourceRef &contact,
                            const GroupContactMemberReleaseSource *exit,
                            const StableId &battle, const StableId &army)
      -> Outcome<FieldActorEngagementRelease> {
    const GroupMemberLossHandoffSource *loss = nullptr;
    for (const auto &row : x.groups.member_loss_handoffs.rows()) {
      if (row.second.journey_id != journey || row.second.actor_id != actor ||
          !same_source_ref(row.second.original_contact, contact))
        continue;
      if (loss || exit)
        return {
            false,
            {},
            common_failure("FieldEngagementLossReleaseAmbiguous", actor).error};
      auto valid = validate_actual_group_member_loss(row.second);
      if (!valid.ok)
        return {false, {}, valid.error};
      if (!same_source_ref(row.second.original_contact, contact))
        return {
            false,
            {},
            common_failure("FieldEngagementLossContactMismatch", actor).error};
      loss = &row.second;
    }
    if (loss)
      return {true, FieldActorEngagementRelease{loss}, {}};
    if (exit)
      return {true, FieldActorEngagementRelease{exit}, {}};
    // 只以本军本场真实ready/cancel/finite_return新opening关闭仍held的参战成员；
    // 原先有序退出或损失成员在上方保留自己的旧终点，不造第二条member release。
    const GroupMovementOpeningSource *returned = nullptr;
    const GroupMovementControlSource *returned_stable = nullptr;
    for (const auto &[id, plan] : x.field.army_exit_plans) {
      if (plan.battle_id != battle || plan.army_id != army || !plan.actual_continuation) continue;
      const auto *exact = x.exact_contacts;
      if (returned || returned_stable || !exact || !x.original_abilities || !x.original_body ||
          &exact->common != &x.common || &exact->military != &x.military ||
          &exact->field != &x.field || &exact->movement != &x.movement ||
          &exact->carriers != &x.carriers || &exact->groups != &x.groups ||
          &exact->travel != &x.travel || &exact->clock != &x.clock ||
          x.original_abilities->exact_contacts != exact ||
          x.original_abilities->conditions != x.original_body ||
          &x.original_body->common != &x.common || x.original_body->military != &x.military ||
          x.original_body->injury_field != &x.field)
        return {false, {}, common_failure("FieldEngagementFiniteReturnOriginalOwnerRequired", actor).error};
      auto adopted = read_field_army_exit_continuation_source(
          *exact, x.catalog, *x.original_abilities, *plan.actual_continuation, budget_);
      if (!adopted.ok) return {false, {}, adopted.error};
      const auto &full = adopted.value;
      if (full.ready.plan != &plan ||
          full.ready.original_journey != x.carriers.journeys.find(journey) ||
          !same_source_ref(full.ready.original_contact, contact) ||
          std::count(full.ready.held_named_participants.begin(),
                     full.ready.held_named_participants.end(), actor) != 1)
        return {false, {}, common_failure("FieldEngagementFiniteReturnHeldMemberRequired", actor).error};
      if (full.already_at_destination) {
        if (full.opening || full.journey || !full.cancellation ||
            full.cancellation != x.groups.controls.find(plan.actual_continuation->id))
          return {false, {}, common_failure("FieldEngagementStableReturnOriginalRequired", actor).error};
        returned_stable = full.cancellation;
      } else {
        if (!full.opening || full.opening != x.groups.openings.find(plan.actual_continuation->id) ||
            std::count(full.opening->named_participants.begin(), full.opening->named_participants.end(), actor) != 1)
          return {false, {}, common_failure("FieldEngagementFiniteReturnHeldMemberRequired", actor).error};
        returned = full.opening;
      }
    }
    // 单prefix赢家只借真实边界续程；与初始两军continuation互斥，不伪造endpoint。
    if (contact.domain == "travel" && contact.kind == "movement_boundary_contact") {
      const MovementBoundaryContinuationSource *boundary_winner = nullptr;
      for (const auto &item : x.movement.boundary_continuations.rows()) {
        const auto &row = item.second;
        if (!same_source_ref(row.boundary_contact, contact)) continue;
        if (boundary_winner || returned || returned_stable || !x.exact_contacts || !x.diplomacy)
          return {false, {}, common_failure("FieldEngagementBoundaryWinnerOriginalRequired", actor).error};
        const SourceRef actual{"travel", "movement_boundary_continuation", row.id};
        auto verified = read_movement_boundary_continuation_source(
            *x.exact_contacts, *x.diplomacy, x.catalog, actual, budget_);
        if (!verified.ok) return {false, {}, verified.error};
        const auto *unit = verified.value.new_unit;
        const auto *group = unit ? std::get_if<GroupMovementExecution>(&unit->execution) : nullptr;
        if (verified.value.continuation != &row || !group || group->journey_id != journey ||
            !std::holds_alternative<ArmyCarrierRef>(group->carrier) ||
            !verified.value.new_basis ||
            std::count(verified.value.new_basis->actual_participants.begin(),
                       verified.value.new_basis->actual_participants.end(), actor) != 1)
          return {false, {}, common_failure("FieldEngagementBoundaryWinnerMemberRequired", actor).error};
        boundary_winner = &row;
      }
      if (boundary_winner)
        return {true, FieldActorEngagementRelease{boundary_winner}, {}};
    }
    const MovementContactContinuationSource *winner = nullptr;
    for (const auto &item : x.movement.contact_continuations.rows()) {
      const auto &row = item.second;
      if (contact.domain != "travel" || contact.kind != "movement_contact" ||
          row.contact_ref != contact.id ||
          row.actual_resolution_source.domain != "military" ||
          row.actual_resolution_source.kind != "field_battle_segment")
        continue;
      const auto unit = x.movement.units.find(row.new_unit_ref);
      const auto group =
          unit ? std::get_if<GroupMovementExecution>(&unit->execution)
               : nullptr;
      const auto army =
          group ? std::get_if<ArmyCarrierRef>(&group->carrier) : nullptr;
      if (!group || group->journey_id != journey)
        continue;
      if (winner || returned || returned_stable || !army || !x.exact_contacts)
        return {false,
                {},
                common_failure("FieldEngagementWinnerOriginalRequired", actor)
                    .error};
      const SourceRef actual{"travel", "movement_contact_continuation", row.id};
      auto terminal = read_field_segment_settlement_source(
          x.common, x.military, x.field, row.actual_resolution_source,
          &x.clock);
      if (!terminal.ok)
        return {false, {}, terminal.error};
      auto verified = read_field_winner_continuation(
          *x.exact_contacts, x.catalog, terminal.value.session->battle_id,
          army->army_id, &actual, budget_);
      if (!verified.ok)
        return {false, {}, verified.error};
      const auto effort = unit->effort_basis_refs.size() == 1
                              ? x.movement.exertion_bases.find(
                                    unit->effort_basis_refs.front().id)
                              : nullptr;
      if (!effort || std::count(effort->actual_participants.begin(),
                                effort->actual_participants.end(), actor) != 1)
        return {
            false,
            {},
            common_failure("FieldEngagementWinnerMemberRequired", actor).error};
      winner = &row;
    }
    const MovementArrivalSource *arrival = nullptr;
    for (const auto &item : x.movement.arrivals.rows()) {
      const auto &row = item.second;
      const auto *anchor =
          std::get_if<OriginalMovementAnchor>(&row.actual_anchor);
      const auto *group = std::get_if<GroupMovementExecution>(&row.execution);
      const auto *army =
          group ? std::get_if<ArmyCarrierRef>(&group->carrier) : nullptr;
      const auto *close = x.movement.unit_closes.find(row.unit_ref + ":close");
      if (!anchor || anchor->actual_source.domain != "military" ||
          anchor->actual_source.kind != "field_battle_segment" || !group ||
          group->journey_id != journey || !close ||
          !same_source_ref(close->actual_cause, contact))
        continue;
      if (winner || arrival || returned || returned_stable || !army || !x.exact_contacts)
        return {false,
                {},
                common_failure("FieldEngagementWinnerArrivalAmbiguous", actor)
                    .error};
      auto terminal = read_field_segment_settlement_source(
          x.common, x.military, x.field, anchor->actual_source, &x.clock);
      if (!terminal.ok)
        return {false, {}, terminal.error};
      const SourceRef actual{"travel", "movement_arrival", row.id};
      auto verified = read_field_winner_continuation(
          *x.exact_contacts, x.catalog, terminal.value.session->battle_id,
          army->army_id, &actual, budget_);
      if (!verified.ok)
        return {false, {}, verified.error};
      const auto &members =
          verified.value.original.endpoints[verified.value.endpoint]
              .opening->named_participants;
      if (std::count(members.begin(), members.end(), actor) != 1)
        return {
            false,
            {},
            common_failure("FieldEngagementWinnerMemberRequired", actor).error};
      // 此 actor 无本次 contact 的 loss/exit（已在上面返回），仍属原 carrier。
      arrival = &row;
    }
    if (returned_stable)
      return {true, FieldActorEngagementRelease{returned_stable}, {}};
    if (returned)
      return {true, FieldActorEngagementRelease{returned}, {}};
    if (arrival)
      return {true, FieldActorEngagementRelease{arrival}, {}};
    if (winner)
      return {true, FieldActorEngagementRelease{winner}, {}};
    return {true, FieldActorEngagementRelease{std::monostate{}}, {}};
  };

  for (const auto &installed : x.movement.contact_installations.rows()) {
    const auto &entry = installed.second;
    const auto &business = entry.actual_business_source;
    if (business.domain != "military" ||
        business.kind != "field_battle_contact")
      continue;
    if (installed.first != entry.id || entry.business_source_schema != 1)
      return fail("FieldEngagementInstallationShapeMismatch");
    const auto contact = x.movement.contacts.find(entry.contact_ref);
    if (!contact)
      return fail("FieldEngagementActualContactRequired");
    const StableId *battle = nullptr;
    if (auto hit = x.field.contacts.find(business.id);
        hit != x.field.contacts.end())
      battle = &hit->second.battle_id;
    if (auto hit = x.field.boundary_contacts.find(business.id);
        hit != x.field.boundary_contacts.end()) {
      if (battle)
        return fail("FieldEngagementContactAmbiguous");
      battle = &hit->second.battle_id;
    }
    if (!battle)
      return fail("FieldEngagementFieldContactRequired");
    const std::array<SourceRef, 3> heads{
        {{"travel", "movement_contact", contact->id},
         business,
         {"travel", "movement_contact_installation", entry.id}}};
    std::optional<std::size_t> previous;
    for (const auto &head : heads) {
      auto position = read_actual_common_fact_position(x.common, head);
      if (!position.ok)
        return {false, {}, position.error};
      if (previous && position.value <= *previous)
        return fail("FieldEngagementSourceOrderMismatch");
      previous = position.value;
    }
    auto same =
        compare_world_moments(contact->occurred_at, entry.occurred_at, budget_);
    auto future = compare_world_moments(entry.occurred_at, now.value, budget_);
    if (!same.ok)
      return {false, {}, same.error};
    if (!future.ok)
      return {false, {}, future.error};
    if (same.value != 0 || future.value > 0)
      return fail("FieldEngagementActualMomentMismatch");
    for (const auto *prefix_id :
         {&contact->first_prefix_id, &contact->second_prefix_id}) {
      const auto prefix = x.movement.prefixes.find(*prefix_id);
      const auto execution =
          prefix ? std::get_if<GroupMovementExecution>(&prefix->execution)
                 : nullptr;
      const auto army = execution
                            ? std::get_if<ArmyCarrierRef>(&execution->carrier)
                            : nullptr;
      if (!army)
        continue;
      auto binding = read_field_original_battle_binding(x.common, x.field,
                                                        *battle, army->army_id);
      if (!binding.ok)
        return {false, {}, binding.error};
      const StableId *binding_contact = nullptr;
      std::visit(
          [&](const auto &value) { binding_contact = &value.contact_id; },
          binding.value->source);
      // A boundary contact may meet an already engaged opponent; only the
      // actual entrant's original binding belongs to this installation.
      if (!binding_contact || *binding_contact != business.id)
        continue;
      if (std::none_of(
              binding.value->members.begin(), binding.value->members.end(),
              [&](const auto &member) { return member.actor_id == actor; }))
        continue;
      const auto journey = x.carriers.journeys.find(execution->journey_id);
      if (!journey || spatial_carrier_key(journey->carrier) !=
                          spatial_carrier_key(execution->carrier))
        return fail("FieldEngagementOriginalJourneyRequired");
      const GroupMovementOpeningSource *opening = nullptr;
      for (const auto &row : x.groups.openings.rows())
        if (row.second.journey_id == journey->journey_id) {
          if (opening || row.first != row.second.id)
            return fail("FieldEngagementOpeningAmbiguous");
          opening = &row.second;
        }
      const auto original_army =
          opening ? std::get_if<ArmyCarrierRef>(&opening->carrier_origin)
                  : nullptr;
      if (!original_army || original_army->army_id != army->army_id ||
          !opening->military_order ||
          std::count(opening->named_participants.begin(),
                     opening->named_participants.end(), actor) != 1)
        return fail("FieldEngagementOpeningMemberRequired");
      auto opening_head = read_actual_common_fact_position(
          x.common, {"travel", "group_movement_opening", opening->id});
      auto contact_head = read_actual_common_fact_position(x.common, heads[0]);
      if (!opening_head.ok)
        return {false, {}, opening_head.error};
      if (!contact_head.ok)
        return {false, {}, contact_head.error};
      if (opening_head.value >= contact_head.value)
        return fail("FieldEngagementOpeningOrderMismatch");
      const MovementUnitCloseSource *closed = nullptr;
      for (const auto &row : x.movement.unit_closes.rows()) {
        const auto owner =
            std::get_if<GroupMovementExecution>(&row.second.execution);
        if (!owner || owner->journey_id != journey->journey_id ||
            !same_source_ref(row.second.actual_cause, heads[0]))
          continue;
        if (closed || row.first != row.second.id ||
            row.second.prefix_id != *prefix_id ||
            row.second.kind != MovementCloseKind::contact)
          return fail("FieldEngagementCloseAmbiguous");
        closed = &row.second;
      }
      if (!closed)
        return fail("FieldEngagementActualCloseRequired");
      auto close_at = compare_world_moments(closed->occurred_at,
                                            entry.occurred_at, budget_);
      auto close_head = read_actual_common_fact_position(
          x.common, {"travel", "movement_unit_close", closed->id});
      if (!close_at.ok)
        return {false, {}, close_at.error};
      if (!close_head.ok)
        return {false, {}, close_head.error};
      if (close_at.value != 0 || close_head.value >= *previous)
        return fail("FieldEngagementCloseOrderMismatch");
      const GroupContactMemberReleaseSource *release = nullptr;
      for (const auto &row : x.groups.contact_member_releases.rows()) {
        if (row.second.journey_id != journey->journey_id ||
            !same_source_ref(row.second.original_contact, heads[0]) ||
            std::find(row.second.named_participants.begin(),
                      row.second.named_participants.end(),
                      actor) == row.second.named_participants.end())
          continue;
        if (release)
          return fail("FieldEngagementReleaseAmbiguous");
        auto valid = validate_actual_group_member_release(row.second);
        if (!valid.ok)
          return {false, {}, valid.error};
        if (!same_source_ref(row.second.original_contact, heads[0]))
          return fail("FieldEngagementReleaseContactMismatch");
        release = &row.second;
      }
      for (const auto &old : result)
        if (*old.battle_id == *battle && *old.journey_id == journey->journey_id)
          return fail("FieldEngagementDuplicateOriginalEntry");
      auto adopted = actual_release(journey->journey_id, heads[0], release, *battle, army->army_id);
      if (!adopted.ok)
        return {false, {}, adopted.error};
      result.push_back({battle, &journey->journey_id,
                        FieldActorDoubleContactEntry{&entry, contact},
                        std::move(adopted.value)});
    }
  }
  // A stopped single prefix is only a queue hold. Return an engagement for
  // this actor only after its own formation's real Entered application.
  for (const auto &[id, boundary] : x.field.boundary_contacts) {
    const auto *single =
        std::get_if<FieldExactBoundaryOrigin>(&boundary.motion);
    if (!single || !boundary.entered_binding)
      continue;
    const auto &binding = *boundary.entered_binding;
    const StableId *formation_id = nullptr;
    for (const auto &member : binding.members)
      if (member.actor_id == actor) {
        if (formation_id)
          return fail("FieldEngagementSingleMemberAmbiguous");
        formation_id = &member.formation_id;
      }
    if (!formation_id)
      continue; // Actual queue member, but not admitted, is not in battle.
    if (!x.exact_contacts || &x.exact_contacts->common != &x.common ||
        &x.exact_contacts->military != &x.military ||
        &x.exact_contacts->field != &x.field ||
        &x.exact_contacts->movement != &x.movement ||
        &x.exact_contacts->carriers != &x.carriers ||
        &x.exact_contacts->groups != &x.groups ||
        &x.exact_contacts->clock != &x.clock)
      return fail("FieldEngagementSingleOriginalOwnersRequired");
    auto original = read_field_boundary_original_source(
        *x.exact_contacts, x.catalog, single->actual_contact, budget_);
    if (!original.ok)
      return {false, {}, original.error};
    const auto &source = original.value;
    if (!source.contact || !source.journey || !source.opening ||
        !source.closed || !source.army || source.army->id != boundary.army_id ||
        id != boundary.contact_id || binding.army_id != boundary.army_id ||
        binding.battle_id != boundary.battle_id ||
        std::count(source.opening->named_participants.begin(),
                   source.opening->named_participants.end(), actor) != 1)
      return fail("FieldEngagementSingleOriginalMemberRequired");
    const auto session = x.field.sessions.find(boundary.battle_id);
    if (session == x.field.sessions.end())
      return fail("FieldEngagementSingleSessionRequired");
    const auto formation = session->second.formations.find(*formation_id);
    if (formation == session->second.formations.end())
      return fail("FieldEngagementSingleAdmissionRequired");
    auto admission = read_field_reinforcement_admission_identity(
        x.common, x.field, session->second, formation->second);
    if (!admission.ok)
      return {false, {}, admission.error};
    const auto &entered = *admission.value;
    if (!boundary.reinforcement_queue ||
        std::none_of(boundary.reinforcement_queue->admitted.begin(),
                     boundary.reinforcement_queue->admitted.end(),
                     [&](const auto &row) { return &row == admission.value; }))
      return fail("FieldEngagementSingleForeignAdmission");
    const SourceRef field_source{"military", "field_battle_contact", id};
    const std::array<SourceRef, 5> ordered{
        {{"travel", "group_movement_opening", source.opening->id},
         single->actual_contact,
         {"travel", "movement_unit_close", source.closed->id},
         field_source,
         entered.application_source}};
    std::optional<std::size_t> position;
    for (const auto &ref : ordered) {
      auto head = read_actual_common_fact_position(x.common, ref);
      if (!head.ok)
        return {false, {}, head.error};
      if (position && head.value <= *position)
        return fail("FieldEngagementSingleSourceOrderMismatch");
      position = head.value;
    }
    auto after_contact = compare_world_moments(source.contact->occurred_at,
                                               entered.applied_at, budget_);
    auto not_future =
        compare_world_moments(entered.applied_at, now.value, budget_);
    if (!after_contact.ok)
      return {false, {}, after_contact.error};
    if (!not_future.ok)
      return {false, {}, not_future.error};
    if (after_contact.value > 0 || not_future.value > 0)
      return fail("FieldEngagementSingleActualMomentMismatch");
    const GroupContactMemberReleaseSource *release = nullptr;
    for (const auto &row : x.groups.contact_member_releases.rows()) {
      if (row.second.journey_id != source.journey->journey_id ||
          !same_source_ref(row.second.original_contact,
                           single->actual_contact) ||
          std::find(row.second.named_participants.begin(),
                    row.second.named_participants.end(),
                    actor) == row.second.named_participants.end())
        continue;
      if (release ||
          !same_source_ref(row.second.original_contact, single->actual_contact))
        return fail("FieldEngagementSingleReleaseAmbiguous");
      auto checked = validate_actual_group_member_release(row.second);
      if (!checked.ok)
        return {false, {}, checked.error};
      auto after_entry = compare_world_moments(entered.applied_at,
                                               row.second.occurred_at, budget_);
      if (!after_entry.ok)
        return {false, {}, after_entry.error};
      if (after_entry.value > 0)
        return fail("FieldEngagementSingleReleaseBeforeEntry");
      release = &row.second;
    }
    for (const auto &old : result)
      if (*old.battle_id == boundary.battle_id &&
          *old.journey_id == source.journey->journey_id)
        return fail("FieldEngagementDuplicateOriginalEntry");
    const auto *field_head = x.common.state().facts.find(id);
    const auto *entry_head =
        x.common.state().facts.find(entered.application_source.id);
    if (!field_head || !entry_head)
      return fail("FieldEngagementSingleOriginalHeadsRequired");
    auto adopted = actual_release(source.journey->journey_id,
                                  single->actual_contact, release,
                                  boundary.battle_id, *boundary.army_id);
    if (!adopted.ok)
      return {false, {}, adopted.error};
    result.push_back(
        {&boundary.battle_id, &source.journey->journey_id,
         FieldActorSingleBoundaryEntry{source.contact, field_head, entry_head,
                                       &entered.applied_at},
         std::move(adopted.value)});
  }
  // Reverse coverage: a true military entry cannot disappear just because its
  // S1 installation/release row was omitted from the checkpoint.
  auto covered = [&](const StableId &contact_id,
                     const FieldBattleRoadBinding &binding) {
    const bool member =
        std::any_of(binding.members.begin(), binding.members.end(),
                    [&](const auto &value) { return value.actor_id == actor; });
    if (!member)
      return true;
    return std::count_if(result.begin(), result.end(), [&](const auto &value) {
             const bool contact_matches = std::visit(
                 [&](const auto &entry) {
                   using T = std::decay_t<decltype(entry)>;
                   if constexpr (std::is_same_v<T,
                                                FieldActorDoubleContactEntry>)
                     return entry.installation && entry.contact &&
                            entry.installation->actual_business_source.id ==
                                contact_id;
                   else
                     return entry.contact && entry.field_contact_head &&
                            entry.entered_head && entry.entered_at &&
                            entry.field_contact_head->id == contact_id;
                 },
                 value.entry);
             return contact_matches && *value.battle_id == binding.battle_id;
           }) == 1;
  };
  for (const auto &[id, contact] : x.field.contacts)
    for (const auto &binding : contact.original_bindings) {
      if (binding.members.empty() || !covered(id, binding))
        return fail("FieldEngagementInstallationCoverageMissing");
    }
  for (const auto &[id, contact] : x.field.boundary_contacts)
    if (contact.entered_binding && !covered(id, *contact.entered_binding))
      return fail("FieldEngagementInstallationCoverageMissing");
  for (const auto &loss : x.groups.member_loss_handoffs.rows())
    if (loss.second.actor_id == actor) {
      const auto count =
          std::count_if(result.begin(), result.end(), [&](const auto &entry) {
            const auto *selected =
                std::get_if<const GroupMemberLossHandoffSource *>(
                    &entry.release);
            return selected && *selected == &loss.second;
          });
      if (count != 1)
        return fail("FieldEngagementLossCoverageRequired");
    }
  return {true, std::move(result), {}};
}
bool OriginalFieldBattleExitPlacementRead::bound_to(
    const CommonStore &common,
    const FieldBattleWorldState &field) const noexcept {
  return &common == &context_.common && &field == &context_.field;
}
Outcome<FieldActorEngagementRead>
OriginalFieldBattleExitPlacementRead::read_field_active_member_engagement(
    const StableId &battle, const StableId &actor) const {
  auto all = validate_group_contact_member_releases(
      context_.common, context_.movement, context_.groups, this);
  if (!all.ok)
    return {false, {}, all.error};
  auto history = read_field_actor_engagement_history(actor);
  if (!history.ok)
    return {false, {}, history.error};
  const FieldActorEngagementRead *selected = nullptr;
  for (const auto &entry : history.value)
    if (entry.battle_id && *entry.battle_id == battle) {
      if (selected)
        return {false,
                {},
                common_failure("FieldActiveMemberEntryAmbiguous", actor).error};
      selected = &entry;
    }
  if (!selected)
    return {false,
            {},
            common_failure("FieldActiveMemberEntryRequired", actor).error};
  return {true, *selected, {}};
}
} // namespace hanmo::resident
