#include "field_member_loss_application.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include "resident/domains/actor_foundation/military_availability.hpp"
#include "resident/domains/military/battle_world.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_store.hpp"
#include <limits>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code) {
  throw Failure{common_failure(code, "field_member_loss_application").error};
}
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
void take(Status value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
}
template <class T, class F> Outcome<T> guard(F fn) {
  try {
    return {true, fn(), {}};
  } catch (Failure &f) {
    return {false, {}, std::move(f.error)};
  } catch (const std::bad_alloc &) {
    return {false,
            {},
            common_failure("FieldMemberLossResourceExhausted", {},
                           ErrorKind::internal)
                .error};
  }
}
} // namespace
struct FieldCapturedMemberApplication::Impl {
  CheckpointCopyLease payload_memory;
  const CommonStore *common = nullptr;
  const AuthoredActorFoundationState *foundation = nullptr;
  const MilitaryState *military = nullptr;
  const FieldBattleWorldState *field = nullptr;
  SourceRef casualty, capacity_source;
  StableId actor_id;
  WorldMoment occurred_at;
  Integer ordinal = 0;
  BoundActorAvailabilityWrite availability;
  std::shared_ptr<const ActorLocationCaptureIdentity> location_capture;
  std::shared_ptr<const ActorLocationWriteIdentity> location_write;
  Integer location_sequence = 0;
  std::optional<ActorConditionSavepointBoundary> boundary;
};
Outcome<FieldCapturedMemberApplicationRead>
read_actual_field_captured_member_application(
    const CommonStore &common, const AuthoredActorFoundationState &foundation,
    const CharacterConditionSources &body, const MilitaryState &military,
    const FieldBattleWorldState &field,
    const FieldSegmentApplicationRead &clock,
    const FieldCapturedMemberApplication &application) {
  return guard<FieldCapturedMemberApplicationRead>([&] {
    if (!application.evidence_)
      fail("FieldCapturedMemberApplicationRequired");
    const auto &e = *application.evidence_;
    if (e.common != &common || e.foundation != &foundation ||
        e.military != &military || e.field != &field ||
        &body.common != &common || &body.state != &foundation ||
        body.military != &military || body.injury_field != &field ||
        !body.injury_field_clock ||
        !body.injury_field_clock->bound_to(common, military, field) ||
        !clock.bound_to(common, military, field) || !e.boundary)
      fail("FieldCapturedMemberSameCandidateRequired");
    take(common.validate_actor_condition_savepoint_boundary(*e.boundary));
    if (e.availability.capture_identity !=
            common.actor_condition_capture_identity() ||
        !e.availability.capture_identity || !e.location_capture ||
        e.location_capture != common.actor_location_capture_identity() ||
        e.availability.capture_sequence < 1 || e.location_sequence < 1 ||
        !e.location_write)
      fail("FieldCapturedMemberCaptureRequired");
    const auto original = take(read_field_segment_captivity_source(
        common, military, field, e.casualty, e.actor_id, clock));
    const auto now = take(clock.read_current_moment());
    if (take(compare_world_moments(
            take(body.injury_field_clock->read_current_moment()), now)) ||
        take(compare_world_moments(body.actual_now, now)))
      fail("FieldCapturedMemberConditionClockMismatch");
    if (original.source_ordinal != e.ordinal ||
        take(compare_world_moments(original.source.settlement->applied_at,
                                   e.occurred_at)) ||
        take(compare_world_moments(now, e.occurred_at)))
      fail("FieldCapturedMemberActualMomentMismatch");
    const auto transition =
        take(read_verified_military_availability_transition_provenance(
            body, e.capacity_source, 2));
    if (transition->transition_id != e.availability.transition_id ||
        transition->actor_id != e.actor_id || transition->to)
      fail("FieldCapturedMemberAvailabilityMismatch");
    const auto cause =
        take(read_original_availability_cause_position(body, *transition));
    if (!cause.source || cause.schema != 1 || cause.ordinal != e.ordinal ||
        cause.source->domain != e.casualty.domain ||
        cause.source->kind != e.casualty.kind ||
        cause.source->id != e.casualty.id)
      fail("FieldCapturedMemberAvailabilityCauseMismatch");
    const auto &conditions = common.actor_condition_execution_drafts();
    const auto &locations = common.actor_location_execution_drafts();
    if (std::size_t(e.availability.capture_sequence) > conditions.size() ||
        std::size_t(e.location_sequence) > locations.size())
      fail("FieldCapturedMemberCaptureMissing");
    const auto &condition =
        conditions[std::size_t(e.availability.capture_sequence - 1)];
    const auto &location = locations[std::size_t(e.location_sequence - 1)];
    const auto *army = std::get_if<InArmyLocation>(&location.after);
    if (condition.sequence != e.availability.capture_sequence ||
        condition.kind != ActorConditionExecutionDraft::Kind::mutation ||
        condition.actor_id != e.actor_id || !condition.before ||
        !condition.available_written || condition.after.available ||
        condition.before->available != transition->from ||
        condition.hp_written || condition.fatigue_written ||
        condition.injury_written || condition.alive_written ||
        condition.before->hp != condition.after.hp ||
        condition.before->fatigue != condition.after.fatigue ||
        condition.before->injury != condition.after.injury ||
        !condition.before->alive || !condition.after.alive ||
        condition.next_instance_before != condition.next_instance_after ||
        location.sequence != e.location_sequence ||
        location.kind != ActorLocationExecutionDraft::Kind::mutation ||
        location.actor_id != e.actor_id || !location.before ||
        location.write_identity != e.location_write || !army ||
        army->army_id != original.actor_change->captor_army_id)
      fail("FieldCapturedMemberActualWriteMismatch");
    // A later write of this same actor requires its own handoff; do not use an
    // old successful capture as permission to replace the current owner.
    for (std::size_t i = std::size_t(e.location_sequence); i < locations.size();
         ++i)
      if (locations[i].actor_id == e.actor_id)
        fail("FieldCapturedMemberPlacementAlreadyAdvanced");
    for (std::size_t i = std::size_t(e.availability.capture_sequence);
         i < conditions.size(); ++i)
      if (conditions[i].actor_id == e.actor_id)
        fail("FieldCapturedMemberBodyAlreadyAdvanced");
    return FieldCapturedMemberApplicationRead{
        original,  &e.availability, transition,        &condition,
        &location, &e.casualty,     &e.capacity_source};
  });
}
Outcome<FieldCapturedMemberApplication>
apply_field_captured_member_in_candidate(
    CommonStore &common, AuthoredActorFoundationState &foundation,
    const CharacterConditionSources &body, const MilitaryState &military,
    const FieldBattleWorldState &field, const SourceRef &casualty,
    const StableId &actor_id, const FieldSegmentApplicationRead &clock,
    const ReadStamp &stamp, const ReadStampAuthority &stamps,
    const DomainReadVersionSource &versions) {
  return guard<FieldCapturedMemberApplication>([&] {
    if (&body.common != &common || &body.state != &foundation ||
        body.military != &military || body.injury_field != &field ||
        !body.injury_field_clock ||
        !body.injury_field_clock->bound_to(common, military, field))
      fail("FieldCapturedMemberSameCandidateRequired");
    auto original = take(read_field_segment_captivity_source(
        common, military, field, casualty, actor_id, clock));
    auto permit = take(prepare_field_military_availability(
        common, foundation, body, casualty, original.source_ordinal, stamp,
        stamps, versions));
    if (!permit || !common.actor_location_capture_identity())
      fail("FieldCapturedMemberCaptureRequired");
    const auto location_capture = common.actor_location_capture_identity();
    const auto location_offset =
        common.actor_location_execution_drafts().size();
    const auto condition_offset =
        common.actor_condition_execution_drafts().size();
    const auto boundary = take(common.actor_condition_savepoint_boundary());
    if (location_offset >= std::size_t(std::numeric_limits<Integer>::max()) ||
        condition_offset >= std::size_t(std::numeric_limits<Integer>::max()))
      fail("FieldCapturedMemberCaptureSequenceExhausted");
    take(apply_original_field_captured_actor_change(common, military, field,
                                                    casualty, actor_id, clock));
    auto bound = take(
        bind_field_military_availability(common, foundation, body, *permit));
    const auto &locations = common.actor_location_execution_drafts();
    if (!bound.transition ||
        bound.capture_sequence != Integer(condition_offset + 1) ||
        common.actor_condition_execution_drafts().size() !=
            condition_offset + 1 ||
        locations.size() != location_offset + 1 ||
        !locations[location_offset].write_identity)
      fail("FieldCapturedMemberSingleOriginalWriteRequired");


    auto memory = CheckpointCopyLease::reserve();
    auto evidence =
        checkpoint_make_shared<FieldCapturedMemberApplication::Impl>();
    evidence->payload_memory = std::move(memory);
    evidence->common = &common;
    evidence->foundation = &foundation;
    evidence->military = &military;
    evidence->field = &field;
    evidence->casualty = {checkpoint_fresh_string_copy(casualty.domain),
                          checkpoint_fresh_string_copy(casualty.kind),
                          checkpoint_fresh_string_copy(casualty.id)};
    evidence->capacity_source = {
        checkpoint_fresh_text_copy("actor_foundation"),
        checkpoint_fresh_text_copy("capacity_transition"),
        checkpoint_fresh_string_copy(bound.transition->transition_id)};
    evidence->actor_id = checkpoint_fresh_string_copy(actor_id);
    evidence->occurred_at = original.source.settlement->applied_at;
    evidence->ordinal = original.source_ordinal;
    evidence->availability = {
        checkpoint_fresh_string_copy(bound.transition->transition_id),
        bound.capture_identity, bound.capture_sequence};
    evidence->location_capture = location_capture;
    evidence->location_sequence = Integer(location_offset + 1);
    evidence->location_write = locations[location_offset].write_identity;
    evidence->boundary = boundary;
    FieldCapturedMemberApplication out;
    out.evidence_ = std::move(evidence);
    take(read_actual_field_captured_member_application(
        common, foundation, body, military, field, clock, out));
    return out;
  });
}
} // namespace hanmo::resident
