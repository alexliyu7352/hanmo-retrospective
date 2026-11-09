#include "field_member_loss_group_apply.hpp"
#include "field_member_loss_group_source.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include "resident/domains/actor_foundation/life_condition.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
namespace hanmo::resident {
Outcome<FieldGroupMemberLossApplication>
apply_field_dead_group_member_in_candidate(
    CommonStore &common, MovementExactState &movement,
    GroupMovementExecutionState &groups, const FieldGroupHandoffReadContext &x,
    const SourceRef &casualty, const StableId &actor,
    const LifeConditionApplication &life, CheckpointMemoryCharge &owner,
    ExactArithmeticBudget budget) {
  auto fail =
      [&](const char *code) -> Outcome<FieldGroupMemberLossApplication> {
    return {false, {}, common_failure(code, actor).error};
  };
  if (&common != &x.common || &movement != &x.movement ||
      &groups != &x.groups || !x.original_body || !x.original_deaths ||
      !x.original_life || &x.original_body->common != &common ||
      x.original_body->military != &x.military ||
      x.original_body->injury_field != &x.field ||
      x.original_body->deaths != x.original_deaths ||
      x.original_body->life_sources != x.original_life ||
      &x.original_life->military != &x.military ||
      x.original_life->field_battles != &x.field ||
      !x.original_life->field_application_clock ||
      !x.original_life->field_application_clock->bound_to(common, x.military,
                                                          x.field) ||
      !common.actor_location_capture_identity() || life.already_applied)
    return fail("FieldDeadGroupActualLifeCandidateRequired");
  auto origin = read_field_member_loss_group_origin(x, casualty, actor, budget);
  if (!origin.ok)
    return {false, {}, origin.error};
  const auto &o = origin.value;
  if (o.actor_change->status != "dead" ||
      o.journey->state != GroupJourneyLifecycle::paused)
    return fail("FieldDeadGroupOriginalPausedMemberRequired");
  auto now = x.clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  auto same = compare_world_moments(now.value, o.segment.settlement->applied_at,
                                    budget);
  if (!same.ok)
    return {false, {}, same.error};
  if (same.value)
    return fail("FieldDeadGroupActualMomentRequired");
  auto death = read_verified_death4_record(
      common, *x.original_deaths, *x.original_life, life.death_ref, now.value,
      MilitaryCasualtyHistoryOrder::actual_common_prefix);
  if (!death.ok)
    return {false, {}, death.error};
  const auto *cause =
      death.value->precise
          ? std::get_if<MilitaryDeathCause4>(&death.value->precise->cause)
          : nullptr;
  if (!cause || death.value->actor_id != actor || cause->actor_id != actor ||
      cause->casualty_fact_id != casualty.id ||
      cause->battle_id != o.segment.session->battle_id)
    return fail("FieldDeadGroupOriginalDeathCauseMismatch");
  if (life.actual_writes.size() != 1 ||
      life.actual_writes.front().receipt_ref.result_id !=
          life.receipt_ref.result_id)
    return fail("FieldDeadGroupActualLifeWriteRequired");
  const auto &written = life.actual_writes.front();
  auto receipt = read_actual_actor_condition_write(
      common, x.original_body->state, written);
  if (!receipt.ok)
    return {false, {}, receipt.error};
  if (receipt.value->actor_id != actor || !receipt.value->life_transition_ref ||
      *receipt.value->life_transition_ref != life.death_ref.id)
    return fail("FieldDeadGroupActualLifeReceiptMismatch");
  auto valid = validate_life_termination_condition(
      common, x.original_body->state, *x.original_deaths,
      x.original_body->catalog, *x.original_life, *receipt.value, now.value,
      MilitaryCasualtyHistoryOrder::actual_common_prefix);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto &conditions = common.actor_condition_execution_drafts();
  const auto condition_count = conditions.size();
  const auto &death_write =
      conditions[std::size_t(written.capture_sequence - 1)];
  if (!death_write.hp_written || !death_write.alive_written ||
      !death_write.available_written || death_write.after.hp != 0 ||
      death_write.after.alive || death_write.after.available ||
      death_write.fatigue_written || death_write.injury_written)
    return fail("FieldDeadGroupActualLifeWriteSetMismatch");

  for (std::size_t i = std::size_t(written.capture_sequence);
       i < conditions.size(); ++i)
    if (conditions[i].actor_id == actor)
      return fail("FieldDeadGroupBodyAlreadyAdvanced");
  for (const auto &r : groups.member_loss_handoffs.rows())
    if (r.second.journey_id == o.journey->journey_id &&
        r.second.actor_id == actor)
      return fail("FieldDeadGroupMemberAlreadyLost");
  for (const auto &r : groups.contact_member_releases.rows())
    if (r.second.journey_id == o.journey->journey_id &&
        std::find(r.second.named_participants.begin(),
                  r.second.named_participants.end(),
                  actor) != r.second.named_participants.end())
      return fail("FieldDeadGroupMemberAlreadyReleased");
  const SpatialWriterBinding *previous = nullptr;
  const StableId *key = nullptr;
  for (const auto &r : movement.spatial_bindings.rows())
    if (r.second.subject_kind == "actor" && r.second.subject_id == actor) {
      if (previous || r.first.size() != 6 + actor.size() ||
          r.first.compare(0, 6, "actor:") ||
          r.first.compare(6, actor.size(), actor))
        return fail("FieldDeadGroupOriginalBindingAmbiguous");
      previous = &r.second;
      key = &r.first;
    }
  const auto *carried =
      previous ? std::get_if<CarriedMovementWriter>(&previous->writer)
               : nullptr;
  const auto *army =
      carried ? std::get_if<ArmyCarrierRef>(&carried->carrier_ref) : nullptr;
  if (!army || army->army_id != o.actor_change->army_id ||
      previous->binding_source.domain != "travel" ||
      previous->binding_source.kind != "group_movement_opening" ||
      previous->binding_source.id != o.opening->id)
    return fail("FieldDeadGroupOriginalCarriedMemberRequired");
  const auto sequence = common.actor_location_execution_drafts().size();
  if (sequence >= std::size_t(std::numeric_limits<Integer>::max()) ||
      common.state().next_instance_id <= 0 ||
      common.state().next_instance_id == std::numeric_limits<Integer>::max())
    return fail("FieldDeadGroupCaptureExhausted");
  char digits[32];
  auto serial = std::to_chars(digits, digits + sizeof(digits),
                              common.state().next_instance_id);
  if (serial.ec != std::errc{})
    return fail("FieldDeadGroupIdExhausted");
  ProducedStableIdBuildView id_build{
      "group_member_loss_handoff",
      std::string_view(digits, std::size_t(serial.ptr - digits))};


  // Existing original anchor copy plan, including fractional path/witness
  // strings. Projection and Stable writer share this same proven world anchor;
  // no Field-local point or current commander location enters this operation.
  (void)0;

  (owner).grow(0);
  auto location = project_movement_location(o.closed->actual_anchor, budget);
  if (!location.ok)
    return {false, {}, location.error};
  auto id = common.next_id("group_member_loss_handoff");
  if (!id.ok)
    return {false, {}, id.error};
  ActorMutation mutation;
  mutation.actor_id = checkpoint_fresh_string_copy(actor);
  mutation.location = std::move(location.value);
  auto changed = common.apply_actor(mutation);
  if (!changed.ok)
    return {false, {}, changed.error};
  const auto &drafts = common.actor_location_execution_drafts();
  if (drafts.size() != sequence + 1 ||
      drafts.back().sequence != Integer(sequence) + 1 ||
      drafts.back().actor_id != actor || !drafts.back().before ||
      !drafts.back().write_identity ||
      drafts.back().kind != ActorLocationExecutionDraft::Kind::mutation ||
      common.actor_condition_execution_drafts().size() != condition_count)
    return fail("FieldDeadGroupActualLocationWriteMismatch");
  auto copy = [](const SourceRef &s) {
    return SourceRef{checkpoint_fresh_string_copy(s.domain),
                     checkpoint_fresh_string_copy(s.kind),
                     checkpoint_fresh_string_copy(s.id)};
  };
  GroupMemberLossHandoffSource row;
  row.id = checkpoint_fresh_string_copy(id.value);
  row.journey_id = checkpoint_fresh_string_copy(o.journey->journey_id);
  row.actor_id = checkpoint_fresh_string_copy(actor);
  row.original_contact = copy(*o.original_contact);
  row.actual_casualty = copy(casualty);
  row.casualty_ordinal = o.casualty_ordinal;
  row.occurred_at = o.segment.settlement->applied_at;
  row.actual_body = GroupMemberDeathApplication{
      {checkpoint_fresh_string_copy(life.death_ref.domain),
       checkpoint_fresh_string_copy(life.death_ref.kind),
       checkpoint_fresh_string_copy(life.death_ref.id)}};
  SpatialWriterBinding next;
  next.subject_kind = checkpoint_fresh_text_copy("actor");
  next.subject_id = checkpoint_fresh_string_copy(actor);
  next.writer = StableMovementWriter{o.closed->actual_anchor};
  next.binding_source = {
      checkpoint_fresh_text_copy("travel"),
      checkpoint_fresh_text_copy("group_member_loss_handoff"),
      checkpoint_fresh_string_copy(id.value)};
  movement.spatial_bindings.set(checkpoint_fresh_string_copy(*key),
                                std::move(next));
  groups.member_loss_handoffs.set(checkpoint_fresh_string_copy(id.value),
                                  std::move(row));
  auto appended = common.append_fact(
      {checkpoint_fresh_string_copy(id.value),
       checkpoint_fresh_text_copy("group_member_loss_handoff"),
       checkpoint_fresh_text_copy("travel")});
  if (!appended.ok)
    return {false, {}, appended.error};
  return {true,
          {{checkpoint_fresh_text_copy("travel"),
            checkpoint_fresh_text_copy("group_member_loss_handoff"),
            std::move(id.value)},
           common.actor_location_capture_identity(),
           drafts.back().write_identity,
           Integer(sequence) + 1},
          {}};
}
} // namespace hanmo::resident
