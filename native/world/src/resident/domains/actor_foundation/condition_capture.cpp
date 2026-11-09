#include "resident/domains/actor_foundation/model.hpp"
// Same original capture, independent scalar and availability source coverage.
#include "condition_capture.hpp"
#include "resident/storage/common_store.hpp"
#include <array>
namespace hanmo::resident {
Outcome<const ConditionReceiptV2 *>
read_actual_actor_condition_write(const CommonStore &common,
                                  const AuthoredActorFoundationState &state,
                                  const BoundActorConditionWrite &binding) {
  auto failure = [](Status s) -> Outcome<const ConditionReceiptV2 *> {
    return {false, nullptr, s.error};
  };
  const auto capture = common.actor_condition_capture_identity();
  const auto &drafts = common.actor_condition_execution_drafts();
  if (!capture || binding.capture_identity != capture ||
      binding.capture_sequence < 1 ||
      static_cast<std::size_t>(binding.capture_sequence) > drafts.size())
    return failure(common_failure("ConditionCaptureBindingMismatch"));
  const auto &d =
      drafts[static_cast<std::size_t>(binding.capture_sequence - 1)];
  if (d.sequence != binding.capture_sequence ||
      d.kind != ActorConditionExecutionDraft::Kind::mutation || !d.before ||
      d.next_instance_before != d.next_instance_after ||
      !(d.hp_written || d.fatigue_written || d.injury_written ||
        d.alive_written))
    return failure(common_failure("ConditionCaptureBindingMismatch"));
  const auto *receipt =
      state.character_condition_extensions.applied_receipts.find(
          binding.receipt_ref.result_id);
  if (!receipt || receipt->result_id != binding.receipt_ref.result_id ||
      receipt->actor_id != d.actor_id)
    return failure(
        common_failure("ConditionCaptureBindingMismatch", d.actor_id));
  const std::array<Integer, 4> before{d.before->hp, d.before->fatigue,
                                      d.before->injury,
                                      d.before->alive ? 1 : 0},
      after{d.after.hp, d.after.fatigue, d.after.injury, d.after.alive ? 1 : 0};
  const std::array<bool, 4> written{d.hp_written, d.fatigue_written,
                                    d.injury_written, d.alive_written};
  std::array<bool, 4> covered{};
  for (const auto &change : receipt->scalar_changes) {
    const auto i = static_cast<std::size_t>(change.field);
    if (i >= 4 || covered[i] || !written[i] || change.before != before[i] ||
        change.after != after[i])
      return failure(
          common_failure("ConditionCaptureScalarMismatch", receipt->result_id));
    covered[i] = true;
  }
  for (std::size_t i = 0; i < 4; ++i)
    if (written[i] != covered[i] || (!written[i] && before[i] != after[i]))
      return failure(
          common_failure("ConditionCaptureWriteSetMismatch", d.actor_id));
  return {true, receipt, {}};
}
Outcome<const InitialConditionSeed *> read_actual_actor_condition_creation(
    const CommonStore &common, const AuthoredActorFoundationState &state,
    const BoundActorConditionCreation &binding) {
  const auto capture = common.actor_condition_capture_identity();
  const auto &drafts = common.actor_condition_execution_drafts();
  auto fail =
      [&](const StableId &code) -> Outcome<const InitialConditionSeed *> {
    return {false, nullptr,
            common_failure(code, binding.actor_id, ErrorKind::internal).error};
  };
  if (!capture || binding.capture_identity != capture ||
      binding.capture_sequence < 1 ||
      static_cast<std::size_t>(binding.capture_sequence) > drafts.size())
    return fail("ConditionCreationCaptureMismatch");
  const auto &d =
      drafts[static_cast<std::size_t>(binding.capture_sequence - 1)];
  if (d.sequence != binding.capture_sequence ||
      d.kind != ActorConditionExecutionDraft::Kind::creation || d.before ||
      d.actor_id != binding.actor_id || !d.hp_written || !d.fatigue_written ||
      !d.injury_written || !d.alive_written || !d.available_written ||
      d.next_instance_before != d.next_instance_after)
    return fail("ConditionCreationCaptureMismatch");
  const auto *origin =
      state.character_origin_records.origins.find(binding.actor_id);
  const auto *seed = state.actor_start_context.initial_condition_seeds.find(
      binding.initial_seed_ref.seed_key);
  if (!origin || origin->actor_id != binding.actor_id ||
      origin->initial_condition_seed_ref.seed_key !=
          binding.initial_seed_ref.seed_key ||
      !seed || seed->seed_key != binding.initial_seed_ref.seed_key ||
      seed->actor_id != binding.actor_id || d.after.hp != seed->hp ||
      d.after.fatigue != seed->fatigue || d.after.injury != seed->injury ||
      !d.after.alive)
    return fail("ConditionCreationInitialSourceMismatch");
  if (origin->origin_kind == CharacterOriginRecordV2::Kind::born) {
    if (!std::holds_alternative<BirthConditionSeedSource>(seed->source) ||
        d.after.available)
      return fail("BornInitialAvailabilityMismatch");
  } else if (origin->origin_kind == CharacterOriginRecordV2::Kind::created) {
    if (!std::holds_alternative<CreatedConditionSeedSource>(seed->source) ||
        !d.after.available)
      return fail("CreatedInitialAvailabilityMismatch");
  } else
    return {false, nullptr,
            common_failure("ConditionCreationSourceProviderRequired",
                           binding.actor_id, ErrorKind::unsupported_capability)
                .error};
  return {true, seed, {}};
}
Status validate_actor_condition_capture(
    const CommonStore &common, const AuthoredActorFoundationState &state,
    const std::vector<BoundActorConditionWrite> &bindings,
    const std::vector<BoundActorAvailabilityWrite> &availability,
    const std::vector<BoundActorConditionCreation> &creations) {
  const auto capture = common.actor_condition_capture_identity();
  const auto &drafts = common.actor_condition_execution_drafts();
  if (!capture)
    return common_failure("ConditionCaptureCoverageMismatch");
  for (std::size_t i = 0; i < bindings.size(); ++i) {
    const auto &b = bindings[i];
    if (b.capture_identity != capture || b.capture_sequence < 1 ||
        static_cast<std::size_t>(b.capture_sequence) > drafts.size())
      return common_failure("ConditionCaptureBindingMismatch");
    for (std::size_t j = 0; j < i; ++j)
      if (bindings[j].capture_sequence == b.capture_sequence ||
          bindings[j].receipt_ref.result_id == b.receipt_ref.result_id)
        return common_failure("DuplicateConditionCaptureBinding");
  }
  for (std::size_t i = 0; i < availability.size(); ++i) {
    const auto &b = availability[i];
    if (b.capture_identity != capture || b.capture_sequence < 1 ||
        static_cast<std::size_t>(b.capture_sequence) > drafts.size())
      return common_failure("AvailabilityCaptureBindingMismatch");
    for (std::size_t j = 0; j < i; ++j)
      if (availability[j].capture_sequence == b.capture_sequence ||
          availability[j].transition_id == b.transition_id)
        return common_failure("DuplicateAvailabilityCaptureBinding");
  }
  for (std::size_t i = 0; i < creations.size(); ++i) {
    auto actual =
        read_actual_actor_condition_creation(common, state, creations[i]);
    if (!actual.ok)
      return {false, {}, actual.error};
    for (std::size_t j = 0; j < i; ++j)
      if (creations[j].capture_sequence == creations[i].capture_sequence ||
          creations[j].actor_id == creations[i].actor_id ||
          creations[j].initial_seed_ref.seed_key ==
              creations[i].initial_seed_ref.seed_key)
        return common_failure("DuplicateConditionCreationCapture");
    for (const auto &row : bindings)
      if (row.capture_sequence == creations[i].capture_sequence)
        return common_failure("ConditionCreationMutationOverlap");
    for (const auto &row : availability)
      if (row.capture_sequence == creations[i].capture_sequence)
        return common_failure("ConditionCreationAvailabilityOverlap");
  }
  for (std::size_t n = 0; n < drafts.size(); ++n) {
    const auto &d = drafts[n];
    if (d.kind == ActorConditionExecutionDraft::Kind::creation) {
      const BoundActorConditionCreation *actual = nullptr;
      for (const auto &row : creations)
        if (row.capture_sequence == d.sequence)
          actual = &row;
      if (!actual || d.sequence != static_cast<Integer>(n + 1))
        return common_failure("ConditionCreationCoverageMismatch", d.actor_id);
      continue;
    }
    if (d.sequence != static_cast<Integer>(n + 1) ||
        d.kind != ActorConditionExecutionDraft::Kind::mutation || !d.before ||
        d.next_instance_before != d.next_instance_after)
      return common_failure("ConditionCaptureBindingMismatch");
    const BoundActorConditionWrite *body = nullptr;
    const BoundActorAvailabilityWrite *available = nullptr;
    for (const auto &b : bindings)
      if (b.capture_sequence == d.sequence)
        body = &b;
    for (const auto &b : availability)
      if (b.capture_sequence == d.sequence)
        available = &b;
    const bool scalar = d.hp_written || d.fatigue_written || d.injury_written ||
                        d.alive_written;
    if (scalar != bool(body))
      return common_failure("ConditionCaptureCoverageMismatch", d.actor_id);
    const ConditionReceiptV2 *receipt = nullptr;
    if (body) {
      auto actual = read_actual_actor_condition_write(common, state, *body);
      if (!actual.ok)
        return {false, {}, actual.error};
      receipt = actual.value;
    } else if (d.before->hp != d.after.hp ||
               d.before->fatigue != d.after.fatigue ||
               d.before->injury != d.after.injury ||
               d.before->alive != d.after.alive)
      return common_failure("ConditionCaptureWriteSetMismatch", d.actor_id);
    if (available) {
      const auto *transition =
          state.character_condition_extensions.capacity_transitions.find(
              available->transition_id);
      if (!d.available_written || !transition ||
          !((std::holds_alternative<MilitaryAvailabilityCauseV2>(
                 transition->cause_ref) &&
             transition->purpose.id == "original_military_availability") ||
            ((std::holds_alternative<NamedMilitaryCaptureAvailabilityCause>(
                  transition->cause_ref) ||
              std::holds_alternative<NamedMilitaryReleaseAvailabilityCause>(
                  transition->cause_ref)) &&
             transition->purpose.id ==
                 "original_named_custody_availability")) ||
          transition->actor_id != d.actor_id ||
          transition->from != d.before->available ||
          transition->to != d.after.available || scalar)
        return common_failure("AvailabilityCaptureSourceMismatch", d.actor_id);
    } else if (d.available_written) {
      bool real_death = false;
      if (receipt && receipt->life_transition_ref) {
        if (const auto *cause =
                std::get_if<MilitaryCasualtyConditionCause>(&receipt->cause))
          real_death = cause->disposition ==
                       MilitaryCasualtyConditionCause::Disposition::dead;
        else
          real_death = std::holds_alternative<LifeTerminationConditionCause>(
              receipt->cause);
      }
      if (!real_death || !d.alive_written || !d.before->alive ||
          d.after.alive || d.after.available)
        return common_failure("AvailabilityCaptureSourceRequired", d.actor_id);
    } else if (d.before->available != d.after.available)
      return common_failure("AvailabilityCaptureWriteSetMismatch", d.actor_id);
    if (!scalar && !d.available_written)
      return common_failure("EmptyActorConditionCapture", d.actor_id);
  }
  return common_success();
}
} // namespace hanmo::resident
