#include "field_member_loss_group_source.hpp"
#include "field_binding_source.hpp"
#include "field_army_exit_rally.hpp"
#include "field_ability_sources.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include "field_boundary_source.hpp"
#include "field_contact_moment.hpp"
#include "field_reinforcement_queue.hpp"
#include "field_settlement_exit_source.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/travel/movement_boundary_model.hpp"
#include "resident/domains/travel/movement_exact.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {
Outcome<FieldMemberLossGroupOrigin> read_field_member_loss_group_origin(
    const FieldGroupHandoffReadContext &x, const SourceRef &casualty,
    const StableId &actor, ExactArithmeticBudget budget) {
  auto fail = [&](const char *code) -> Outcome<FieldMemberLossGroupOrigin> {
    return {false, {}, common_failure(code, actor).error};
  };
  if (!x.exact_contacts || &x.exact_contacts->common != &x.common ||
      &x.exact_contacts->military != &x.military ||
      &x.exact_contacts->field != &x.field ||
      &x.exact_contacts->movement != &x.movement ||
      &x.exact_contacts->groups != &x.groups ||
      &x.exact_contacts->carriers != &x.carriers ||
      &x.exact_contacts->travel != &x.travel ||
      &x.exact_contacts->clock != &x.clock ||
      !x.clock.bound_to(x.common, x.military, x.field))
    return fail("FieldLossGroupOriginalOwnerRequired");
  if (casualty.domain != "military" || casualty.kind != "military_casualties" ||
      casualty.id.empty())
    return fail("FieldLossActualCasualtyRequired");
  const MilitaryCasualtyFact *receipt = nullptr;
  for (const auto &row : x.military.casualty_facts)
    if (row.id == casualty.id) {
      if (receipt)
        return fail("FieldLossActualCasualtyAmbiguous");
      receipt = &row;
    }
  if (!receipt)
    return fail("FieldLossActualCasualtyRequired");
  FieldMemberLossGroupOrigin out;
  const auto *parent_head = x.common.state().facts.find(receipt->battle_id);
  if (parent_head && parent_head->domain == "military" && parent_head->kind == "field_army_exit_step") {
    if (!x.original_abilities || x.original_abilities->exact_contacts != x.exact_contacts ||
        x.original_abilities->conditions != x.original_body || !x.original_body ||
        &x.original_body->common != &x.common || x.original_body->military != &x.military)
      return fail("FieldArmyExitLossOriginalAbilitiesRequired");
    auto actual = read_field_army_exit_ordinary_casualty_source(
        x.common, x.military, x.field, casualty, x.clock);
    if (!actual.ok) return {false, {}, actual.error};
    out.ordinary_loss = actual.value;
    const SourceRef step{"military", "field_army_exit_step", receipt->battle_id};
    auto departed = read_field_army_exit_ordinary_departure_source(
        *x.exact_contacts, x.catalog, *x.original_abilities, step, actor);
    if (!departed.ok) return {false, {}, departed.error};
    if (departed.value.source.step != actual.value.step || !departed.value.member)
      return fail("FieldArmyExitLossOriginalNamedMemberRequired");
    out.session = actual.value.session;
    out.applied_at = &actual.value.step->applied_at;
  } else {
    // 原父源保持资源归属，不再逐字段记账。
    auto parent_memory = CheckpointMemoryCharge(0);
    const SourceRef parent{checkpoint_fresh_text_copy("military"),
        checkpoint_fresh_text_copy("field_battle_segment"), checkpoint_fresh_string_copy(receipt->battle_id)};
    auto segment = read_field_segment_settlement_source(x.common, x.military, x.field, parent, &x.clock);
    if (!segment.ok) return {false, {}, segment.error};
    if (segment.value.casualties != receipt) return fail("FieldLossActualCasualtyParentMismatch");
    out.segment = segment.value;
    out.session = segment.value.session;
    out.applied_at = &segment.value.settlement->applied_at;
  }
  const auto &changes = receipt->actor_changes;
  if (changes.size() >
      static_cast<std::size_t>(std::numeric_limits<Integer>::max()))
    return fail("FieldLossOriginalOrdinalOverflow");
  for (std::size_t i = 0; i < changes.size(); ++i)
    if (changes[i].actor_id == actor) {
      if (out.actor_change ||
          (changes[i].status != "dead" && changes[i].status != "captured" &&
           changes[i].status != "deserted"))
        return fail("FieldLossOriginalNamedOutcomeRequired");
      out.actor_change = &changes[i];
      out.casualty_ordinal = static_cast<Integer>(i);
    }
  if (!out.actor_change)
    return fail("FieldLossOriginalNamedOutcomeRequired");
  auto binding = read_field_original_battle_binding(
      x.common, x.field, out.session->battle_id,
      out.actor_change->army_id);
  if (!binding.ok)
    return {false, {}, binding.error};
  out.battle_binding = binding.value;
  for (const auto &member : binding.value->members)
    if (member.actor_id == actor) {
      if (out.member)
        return fail("FieldLossOriginalMemberAmbiguous");
      out.member = &member;
    }
  if (!out.member)
    return fail("FieldLossOriginalMemberRequired");
  const auto formation =
      out.session->formations.find(out.member->formation_id);
  if (formation == out.session->formations.end())
    return fail("FieldLossOriginalFormationRequired");
  const FactRecord *entered_head = nullptr;
  if (const auto *initial =
          std::get_if<FieldInitialContactBinding>(&binding.value->source)) {
    const MovementContactInstallationSource *installed = nullptr;
    for (const auto &row : x.movement.contact_installations.rows()) {
      const auto &source = row.second.actual_business_source;
      if (source.domain == "military" &&
          source.kind == "field_battle_contact" &&
          source.id == initial->contact_id) {
        if (installed || row.first != row.second.id ||
            row.second.business_source_schema != 1)
          return fail("FieldLossInitialInstallationAmbiguous");
        installed = &row.second;
      }
    }
    if (!installed)
      return fail("FieldLossInitialInstallationRequired");
    auto actual = read_field_initial_contact_actual_moment(
        x.common, x.military, x.field, x.catalog,
        installed->actual_business_source, *x.exact_contacts, budget);
    if (!actual.ok)
      return {false, {}, actual.error};
    out.original_contact = actual.value.original_movement_contact;
    out.entered_at = &installed->occurred_at;
    entered_head = x.common.state().facts.find(installed->id);
    if (!entered_head || entered_head->domain != "travel" ||
        entered_head->kind != "movement_contact_installation" ||
        installed->contact_ref != out.original_contact->id)
      return fail("FieldLossInitialApplicationRequired");
    auto same = compare_world_moments(*out.entered_at,
                                      *actual.value.occurred_at, budget);
    if (!same.ok)
      return {false, {}, same.error};
    if (same.value)
      return fail("FieldLossInitialApplicationMomentMismatch");
  } else if (const auto *entry = std::get_if<FieldBoundaryContactBinding>(
                 &binding.value->source)) {
    const auto boundary = x.field.boundary_contacts.find(entry->contact_id);
    if (boundary == x.field.boundary_contacts.end() ||
        !boundary->second.reinforcement_queue)
      return fail("FieldLossBoundaryOriginalQueueRequired");
    const auto *motion =
        std::get_if<FieldExactBoundaryOrigin>(&boundary->second.motion);
    if (!motion)
      return fail("FieldLossBoundaryOriginalExactSourceRequired");
    auto source = read_field_boundary_original_source(
        *x.exact_contacts, x.catalog, motion->actual_contact, budget);
    if (!source.ok)
      return {false, {}, source.error};
    if (!source.value.army ||
        source.value.army->id != out.actor_change->army_id)
      return fail("FieldLossBoundaryOriginalArmyMismatch");
    auto admission = read_field_reinforcement_admission_identity(
        x.common, x.field, *out.session, formation->second);
    if (!admission.ok)
      return {false, {}, admission.error};
    if (std::none_of(boundary->second.reinforcement_queue->admitted.begin(),
                     boundary->second.reinforcement_queue->admitted.end(),
                     [&](const auto &a) { return &a == admission.value; }))
      return fail("FieldLossBoundaryActualAdmissionRequired");
    out.original_contact = &motion->actual_contact;
    out.entered_at = &admission.value->applied_at;
    entered_head =
        x.common.state().facts.find(admission.value->application_source.id);
    out.journey = source.value.journey;
    out.opening = source.value.opening;
    out.closed = source.value.closed;
  } else
    return fail("FieldLossEntryKindUnsupported");
  if (!out.original_contact || !out.entered_at || !entered_head)
    return fail("FieldLossOriginalEntryRequired");
  // The retained real close, never today's Army/current-prefix/commander
  // position.
  const MovementUnitCloseSource *closed = nullptr;
  for (const auto &row : x.movement.unit_closes.rows()) {
    const auto *execution =
        std::get_if<GroupMovementExecution>(&row.second.execution);
    const auto *army =
        execution ? std::get_if<ArmyCarrierRef>(&execution->carrier) : nullptr;
    if (!army || army->army_id != out.actor_change->army_id ||
        !same_source_ref(row.second.actual_cause, *out.original_contact))
      continue;
    if (closed || row.first != row.second.id ||
        row.second.kind != MovementCloseKind::contact)
      return fail("FieldLossOriginalCloseAmbiguous");
    closed = &row.second;
    const auto *journey = x.carriers.journeys.find(execution->journey_id);
    if (!journey || spatial_carrier_key(journey->carrier) !=
                        spatial_carrier_key(execution->carrier))
      return fail("FieldLossOriginalJourneyRequired");
    if (out.journey && out.journey != journey)
      return fail("FieldLossOriginalJourneyMismatch");
    out.journey = journey;
  }
  if (!closed || (out.closed && out.closed != closed))
    return fail("FieldLossOriginalCloseRequired");
  out.closed = closed;
  const auto *prefix = x.movement.prefixes.find(closed->prefix_id);
  const auto *unit = x.movement.units.find(closed->unit_ref);
  const auto *prefix_group =
      prefix ? std::get_if<GroupMovementExecution>(&prefix->execution)
             : nullptr;
  const auto *unit_group =
      unit ? std::get_if<GroupMovementExecution>(&unit->execution) : nullptr;
  if (!prefix_group || !unit_group ||
      prefix_group->journey_id != out.journey->journey_id ||
      unit_group->journey_id != out.journey->journey_id)
    return fail("FieldLossOriginalClosedPrefixRequired");
  if (const auto *initial =
          std::get_if<FieldInitialContactBinding>(&binding.value->source)) {
    const auto *contact = x.movement.contacts.find(out.original_contact->id);
    if (!contact || initial->source_endpoint < 0 ||
        initial->source_endpoint > 1 ||
        closed->prefix_id != (initial->source_endpoint == 0
                                  ? contact->first_prefix_id
                                  : contact->second_prefix_id))
      return fail("FieldLossOriginalEndpointMismatch");
    auto expected = read_exact_contact_endpoint_anchor(
        x.movement, *contact, closed->prefix_id, budget);
    if (!expected.ok)
      return {false, {}, expected.error};
    auto same =
        same_spatial_anchor(closed->actual_anchor, expected.value, budget);
    if (!same.ok)
      return {false, {}, same.error};
    auto moment = compare_world_moments(closed->occurred_at,
                                        contact->occurred_at, budget);
    if (!moment.ok)
      return {false, {}, moment.error};
    const auto *finalized = std::get_if<MovementUnitFinalized>(&unit->status);
    if (!same.value || moment.value || !finalized ||
        !same_source_ref(finalized->original_close_source,
                         *out.original_contact))
      return fail("FieldLossOriginalClosedProjectionMismatch");
  }
  auto close_at = read_actual_common_fact_position(
      x.common,
      SourceRefBuildView{"travel", "movement_unit_close", closed->id});
  auto entry_at = read_actual_common_fact_position(x.common, *entered_head);
  if (!close_at.ok)
    return {false, {}, close_at.error};
  if (!entry_at.ok)
    return {false, {}, entry_at.error};
  if (close_at.value >= entry_at.value)
    return fail("FieldLossOriginalCloseOrderMismatch");

  const GroupMovementOpeningSource *opening = nullptr;
  for (const auto &row : x.groups.openings.rows())
    if (row.second.journey_id == out.journey->journey_id) {
      if (opening || row.first != row.second.id)
        return fail("FieldLossOriginalOpeningAmbiguous");
      opening = &row.second;
    }
  const auto *army =
      opening ? std::get_if<ArmyCarrierRef>(&opening->carrier_origin) : nullptr;
  if (!army || army->army_id != out.actor_change->army_id ||
      !opening->military_order || (out.opening && out.opening != opening) ||
      std::count(opening->named_participants.begin(),
                 opening->named_participants.end(), actor) != 1)
    return fail("FieldLossOriginalOpeningMemberRequired");
  out.opening = opening;
  auto before = read_actual_common_fact_position(x.common, *entered_head);
  auto after = read_actual_common_fact_position(x.common, casualty);
  if (!before.ok)
    return {false, {}, before.error};
  if (!after.ok)
    return {false, {}, after.error};
  if (before.value >= after.value)
    return fail("FieldLossActualEntryOrderMismatch");
  auto time = compare_world_moments(*out.entered_at,
                                    *out.applied_at, budget);
  if (!time.ok)
    return {false, {}, time.error};
  if (time.value > 0)
    return fail("FieldLossActualEntryMomentMismatch");
  return {true, std::move(out), {}};
}
// 反查唯一真实 exit，复用原道路/城村权源验证，不给终局未离场者猜位置。
Outcome<const FieldBattleExitRecord *>
read_field_member_desertion_exit_source(const FieldGroupHandoffReadContext &x,
                                        const FieldMemberLossGroupOrigin &o,
                                        ExactArithmeticBudget budget) {
  auto fail = [](const char *code) -> Outcome<const FieldBattleExitRecord *> {
    return {false, {}, common_failure(code).error};
  };
  if (!o.actor_change || o.actor_change->status != "deserted" || !o.member ||
      !o.segment.session || !o.segment.settlement)
    return fail("FieldDesertionOriginalOutcomeRequired");
  const auto f = o.segment.session->formations.find(o.member->formation_id);
  if (f == o.segment.session->formations.end() ||
      std::count(f->second.deserted.begin(), f->second.deserted.end(),
                 o.actor_change->actor_id) != 1)
    return fail("FieldDesertionOriginalFormationRequired");
  const FieldBattleExitRecord *exit = nullptr;
  for (const auto &entry : x.field.exits)
    if (entry.second.battle_id == o.segment.session->battle_id &&
        entry.second.formation_id == o.member->formation_id) {
      if (exit || entry.first != entry.second.fact_id)
        return fail("FieldDesertionOriginalExitAmbiguous");
      exit = &entry.second;
    }
  if (!exit)
    return fail("FieldDesertionActualExitRequired");


  const auto memory = CheckpointMemoryCharge(0);
  const SourceRef ref{checkpoint_fresh_text_copy("military"),
                      checkpoint_fresh_text_copy("field_battle_exit"),
                      checkpoint_fresh_string_copy(exit->fact_id)};
  auto verified = read_field_actual_exit_application(
      x.common, x.military, x.field, x.travel, x.catalog, ref, x.clock, budget,
      x.exact_contacts, x.diplomacy, x.territory);
  if (!verified.ok)
    return {false, {}, verified.error};
  auto at = compare_world_moments(exit->applied_at,
                                  o.segment.settlement->applied_at, budget);
  if (!at.ok)
    return {false, {}, at.error};
  if (verified.value != exit || exit->army_id != o.actor_change->army_id ||
      at.value != 0 ||
      std::find(exit->named_actor_ids.begin(), exit->named_actor_ids.end(),
                o.actor_change->actor_id) != exit->named_actor_ids.end())
    return fail("FieldDesertionOriginalExitMismatch");
  return {true, exit, {}};
}
} // namespace hanmo::resident
