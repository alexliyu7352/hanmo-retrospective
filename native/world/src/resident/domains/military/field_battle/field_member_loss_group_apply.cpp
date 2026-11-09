#include "field_member_loss_group_apply.hpp"
#include "field_army_group_origin.hpp"
#include "field_member_loss_group_source.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
namespace hanmo::resident {
Outcome<FieldGroupMemberLossApplication>
adopt_field_captured_group_member_in_candidate(
    CommonStore &common, MovementExactState &movement,
    GroupMovementExecutionState &groups, const FieldGroupHandoffReadContext &x,
    const FieldCapturedMemberApplication &application,
    CheckpointMemoryCharge &owner, ExactArithmeticBudget budget) {
  auto fail =
      [&](const char *code) -> Outcome<FieldGroupMemberLossApplication> {
    return {false, {}, common_failure(code).error};
  };
  if (&common != &x.common || &movement != &x.movement ||
      &groups != &x.groups || !x.original_body)
    return fail("FieldCapturedGroupSameCandidateRequired");
  auto applied = read_actual_field_captured_member_application(
      common, x.original_body->state, *x.original_body, x.military, x.field,
      x.clock, application);
  if (!applied.ok)
    return {false, {}, applied.error};
  const auto &body = applied.value;
  if (!body.actual_casualty || !body.actual_capacity_transition ||
      !body.location_write)
    return fail("FieldCapturedGroupActualBodyRequired");
  const auto &actor = body.original.actor_change->actor_id;
  auto origin = read_field_member_loss_group_origin(x, *body.actual_casualty,
                                                    actor, budget);
  if (!origin.ok)
    return {false, {}, origin.error};
  const auto &o = origin.value;
  if (o.actor_change != body.original.actor_change ||
      o.casualty_ordinal != body.original.source_ordinal ||
      o.journey->state != GroupJourneyLifecycle::paused)
    return fail("FieldCapturedGroupOriginalPausedMemberRequired");
  for (const auto &r : groups.member_loss_handoffs.rows())
    if (r.second.journey_id == o.journey->journey_id &&
        r.second.actor_id == actor)
      return fail("FieldCapturedGroupMemberAlreadyLost");
  for (const auto &r : groups.contact_member_releases.rows())
    if (r.second.journey_id == o.journey->journey_id &&
        std::find(r.second.named_participants.begin(),
                  r.second.named_participants.end(),
                  actor) != r.second.named_participants.end())
      return fail("FieldCapturedGroupMemberAlreadyReleased");
  const SpatialWriterBinding *previous = nullptr;
  const StableId *key = nullptr;
  for (const auto &r : movement.spatial_bindings.rows())
    if (r.second.subject_kind == "actor" && r.second.subject_id == actor) {
      if (previous || r.first.size() != 6 + actor.size() ||
          r.first.compare(0, 6, "actor:") ||
          r.first.compare(6, actor.size(), actor))
        return fail("FieldCapturedGroupOriginalBindingAmbiguous");
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
    return fail("FieldCapturedGroupOriginalCarriedMemberRequired");
  const auto *target = std::get_if<InArmyLocation>(&body.location_write->after);
  if (!target || target->army_id != o.actor_change->captor_army_id ||
      target->army_id == army->army_id || !body.original.captor_binding ||
      body.original.captor_binding->army_id != target->army_id)
    return fail("FieldCapturedGroupActualCaptorRequired");
  auto captor_origin = read_field_original_army_group_origin(
      x, *body.original.captor_binding, budget);
  if (!captor_origin.ok)
    return {false, {}, captor_origin.error};
  auto captor_at = read_actual_common_fact_position(
      common, *captor_origin.value.original_contact);
  auto loss_at =
      read_actual_common_fact_position(common, *body.actual_casualty);
  auto captor_time =
      compare_world_moments(captor_origin.value.closed->occurred_at,
                            o.segment.settlement->applied_at, budget);
  if (!captor_at.ok)
    return {false, {}, captor_at.error};
  if (!loss_at.ok)
    return {false, {}, loss_at.error};
  if (!captor_time.ok)
    return {false, {}, captor_time.error};
  if (captor_at.value >= loss_at.value || captor_time.value > 0)
    return fail("FieldCapturedGroupOriginalCaptorOrderMismatch");
  if (common.state().next_instance_id <= 0 ||
      common.state().next_instance_id == std::numeric_limits<Integer>::max())
    return fail("FieldCapturedGroupIdExhausted");
  char digits[32];
  auto serial = std::to_chars(digits, digits + sizeof(digits),
                              common.state().next_instance_id);
  if (serial.ec != std::errc{})
    return fail("FieldCapturedGroupIdExhausted");
  ProducedStableIdBuildView id_build{
      "group_member_loss_handoff",
      std::string_view(digits, std::size_t(serial.ptr - digits))};

  // Returned ID, table key/body ID, Common header and binding source. Original
  // OrderedTable/Common own their backing allocation; strings are pre-rented.
  (void)0;


  (owner).grow(0);
  auto id = common.next_id("group_member_loss_handoff");
  if (!id.ok)
    return {false, {}, id.error};
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
  row.actual_casualty = copy(*body.actual_casualty);
  row.casualty_ordinal = o.casualty_ordinal;
  row.occurred_at = o.segment.settlement->applied_at;
  row.actual_body =
      GroupMemberCaptivityApplication{copy(*body.actual_capacity_transition)};
  SpatialWriterBinding next;
  next.subject_kind = checkpoint_fresh_text_copy("actor");
  next.subject_id = checkpoint_fresh_string_copy(actor);
  next.writer = CarriedMovementWriter{
      ArmyCarrierRef{checkpoint_fresh_string_copy(target->army_id)}};
  next.binding_source = {
      checkpoint_fresh_text_copy("travel"),
      checkpoint_fresh_text_copy("group_member_loss_handoff"),
      checkpoint_fresh_string_copy(id.value)};
  auto binding_key = checkpoint_fresh_string_copy(*key);
  // No actor mutation here: both after-location and availability are the exact
  // single original captured draft already validated above.
  movement.spatial_bindings.set(std::move(binding_key), std::move(next));
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
           body.location_write->write_identity,
           body.location_write->sequence},
          {}};
}
} // namespace hanmo::resident
