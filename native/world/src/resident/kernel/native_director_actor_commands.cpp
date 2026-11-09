#include "native_director_actor_commands.hpp"
#include "common_command_sources.hpp"
#include "complete_growth_command_binding.hpp"
#include "complete_task_command_binding.hpp"
#include "dispatcher_exact_bindings.hpp"
#include "exact_complete_world.hpp"
#include "native_complete_world_core.hpp"
#include "native_growth_market_sources.hpp"
#include "native_interaction_world.hpp"
#include "native_public_supply_inspection.hpp"
#include "resident/domains/director/director_command_acceptance.hpp"
#include "resident/domains/growth/growth_fixed_start.hpp"
#include "resident/domains/interaction/visit_appointments/visit_appointment_world.hpp"
#include "resident/domains/interaction/visit_messages/visit_message_receipt_world.hpp"
#include "resident/domains/interaction/visit_replies/visit_reply_world.hpp"
#include "resident/storage/resource_execution_coverage.hpp"
#include "world_bundle.hpp"
#include "world_read_stamp.hpp"
#include <limits>
#include <optional>
namespace hanmo::resident {
namespace {
// 仅比较同一原有 source；不把名称存在当作来源验证。
bool same_source(const SourceRef &a, const SourceRef &b) {
  return a.domain == b.domain && a.kind == b.kind && a.id == b.id;
}
bool growth_start_source(const SourceRef &source) {
  return source.domain == "growth" && !source.id.empty() &&
         (source.kind == "training" || source.kind == "skill_study" ||
          source.kind == "long_rest" || source.kind == "life_activity");
}
bool message_source(const SourceRef &source) {
  return source.domain == "visit_messages" && !source.id.empty() &&
         (source.kind == visit_message_posted_kind ||
          source.kind == visit_message_received_kind);
}
bool message_command(const CommandPayload &command) {
  const auto *typed = std::get_if<VisitMessageCommand>(&command.data);
  return typed && visit_message_command_kind(*typed) == command.kind &&
         (std::holds_alternative<LeaveVisitMessage>(*typed) ||
          std::holds_alternative<ReceiveVisitMessage>(*typed));
}
bool authored_reply_source(const SourceRef &source) {
  return source.domain == visit_reply_module &&
         source.kind == visit_reply_posted_kind && !source.id.empty();
}
bool authored_invitation_source(const SourceRef &source) {
  return source.domain == visit_appointment_module &&
         source.kind == visit_appointment_offered_kind && !source.id.empty();
}
// 只认实际 Inspect 变体和原命令名；普通同处观察不能取得该能力。
const Inspect *inspection_command(const CommandPayload &command) {
  const auto *interaction = std::get_if<InteractionCommand>(&command.data);
  return command.kind == "inspect" && interaction
             ? std::get_if<Inspect>(interaction)
             : nullptr;
}
bool original_interaction_source(const SourceRef &source) {
  return message_source(source) || authored_reply_source(source) ||
         authored_invitation_source(source) ||
         is_public_supply_inspection_source(source);
}
// 直接借当前世界实际根和钟，不在采用尾重开完整工厂或捕获 P10。
template <class Consume>
Status with_inspection_context(const WorldBundle &world,
                               const CompleteExactWorldReadBindings *actual,
                               Consume consume) {
  if (!actual || !actual->original_core ||
      &actual->original_core->characters.early.world != &world ||
      !world.catalog || !world.public_supply_inspection)
    return common_failure("DirectorInspectionActualOwnerRequired", {},
                          ErrorKind::unsupported_capability);
  auto valid = validate_native_public_supply_inspection_layout(
      *world.catalog, world.common, &*world.public_supply_inspection);
  if (!valid.ok)
    return valid;
  WorldCurrentMomentRead clock(world);
  PublicSupplyInspectionReadContext context{
      world.common,
      world.social,
      *world.catalog->interaction,
      *world.catalog->local_economy,
      world.local_economy,
      *world.catalog->public_supply_inspection,
      *world.public_supply_inspection,
      clock};
  return consume(context);
}

Outcome<const VisitReplyReadContext *>
reply_context(const WorldBundle &w,
              const CompleteExactWorldReadBindings *actual) {
  if (!actual || !w.catalog->visit_message_replies ||
      w.catalog->visit_message_replies->version != 4 ||
      !w.catalog->visit_reply_intents || !w.interaction.visit_message_replies ||
      w.interaction.visit_message_replies->version != 4)
    return {false,
            {},
            common_failure("DirectorOriginalReply4SourcesRequired", {},
                           ErrorKind::unsupported_capability)
                .error};
  auto owner = read_native_interaction_world(w, *actual);
  if (!owner.ok)
    return {false, {}, std::move(owner.error)};
  if (!owner.value->replies())
    return {false,
            {},
            common_failure("DirectorOriginalReplyOwningRequired", {},
                           ErrorKind::unsupported_capability)
                .error};
  return owner.value->replies()->context();
}
Outcome<const VisitAppointmentMeetingTime2Context *>
invitation_context(const WorldBundle &w,
                   const CompleteExactWorldReadBindings *actual) {
  if (!actual || !w.catalog->visit_appointment_invitations ||
      w.catalog->visit_appointment_invitations->version != 2 ||
      !w.catalog->visit_appointment_intents ||
      !w.interaction.visit_appointment_invitations ||
      w.interaction.visit_appointment_invitations->version != 2)
    return {false,
            {},
            common_failure("DirectorOriginalInvitation2SourcesRequired", {},
                           ErrorKind::unsupported_capability)
                .error};
  auto owner = read_native_interaction_world(w, *actual);
  if (!owner.ok)
    return {false, {}, std::move(owner.error)};
  if (!owner.value->visit())
    return {false,
            {},
            common_failure("DirectorOriginalInvitationOwningRequired", {},
                           ErrorKind::unsupported_capability)
                .error};
  return owner.value->visit()->context();
}
Outcome<MomentOwnerSource>
interaction_effect(const WorldBundle &before, const WorldBundle &after,
                   const StableId &actor, const CommandPayload &command,
                   const ReceiptDetails &details,
                   const CompleteExactWorldReadBindings *actual,
                   CheckpointCopyLease &source_memory) {
  const auto &kind = command.kind;
  if (const auto *inspect = inspection_command(command)) {
    if (before.catalog.get() != after.catalog.get() ||
        !before.public_supply_inspection ||
        !std::holds_alternative<InteractionEffect>(details.data))
      return {false,
              {},
              common_failure("DirectorInspectionPreimageMismatch").error};
    const PublicSupplyInspection *source = nullptr;
    auto valid =
        with_inspection_context(after, actual, [&](const auto &context) {
          auto now = after.actual_phase_moment();
          if (!now.ok)
            return Status{false, {}, std::move(now.error)};
          auto old_now = before.actual_phase_moment();
          if (!old_now.ok)
            return Status{false, {}, std::move(old_now.error)};
          auto equal = compare_world_moments(old_now.value, now.value);
          if (!equal.ok)
            return Status{false, {}, std::move(equal.error)};
          if (equal.value)
            return common_failure("DirectorInspectionPreimageMomentMismatch");
          auto read = read_public_supply_inspection_command_effect(
              *before.public_supply_inspection, context, actor,
              inspect->prop_id, now.value);
          if (!read.ok)
            return Status{false, {}, std::move(read.error)};
          source = read.value;
          return common_success();
        });
    if (!valid.ok)
      return {false, {}, std::move(valid.error)};


    source_memory = CheckpointCopyLease::reserve();
    return {true,
            {{"interaction", public_supply_inspection_source_kind,
              checkpoint_fresh_string_copy(source->id)},
             1},
            {}};
  }

  if (kind == authored_visit_reply_command) {
    auto x = reply_context(after, actual);
    if (!x.ok)
      return {false, {}, std::move(x.error)};
    return read_visit_reply_director_command_effect_world(before, after, actor,
                                                          details, x.value);
  }
  if (kind == authored_visit_appointment_command) {
    auto x = invitation_context(after, actual);
    if (!x.ok)
      return {false, {}, std::move(x.error)};
    return read_visit_appointment_director_command_effect_world(
        before, after, actor, details, x.value);
  }
  return read_visit_message_director_command_effect_world(
      before, after, actor, kind, details, actual);
}
Status
validate_interaction_effect(const WorldBundle &w, const StableId &actor,
                            const SourceRef &effect,
                            const CompleteExactWorldReadBindings *actual) {
  if (is_public_supply_inspection_source(effect))
    return with_inspection_context(w, actual, [&](const auto &context) {
      auto source = read_public_supply_inspection_source(context, effect, 1);
      if (!source.ok)
        return Status{false, {}, std::move(source.error)};
      return source.value->actor_id == actor
                 ? common_success()
                 : common_failure("DirectorInspectionActorMismatch", effect.id);
    });

  if (authored_reply_source(effect)) {
    auto x = reply_context(w, actual);
    if (!x.ok)
      return {false, {}, std::move(x.error)};
    return validate_visit_reply_director_actor_effect_world(w, actor, effect,
                                                            x.value);
  }
  if (authored_invitation_source(effect)) {
    auto x = invitation_context(w, actual);
    if (!x.ok)
      return {false, {}, std::move(x.error)};
    return validate_visit_appointment_director_actor_effect_world(
        w, actor, effect, x.value);
  }
  if (message_source(effect))
    return validate_visit_message_director_actor_effect_world(w, actor, effect,
                                                              actual);
  return common_failure("DirectorActorOriginalEffectReaderRequired", effect.id,
                        ErrorKind::unsupported_capability);
}
Outcome<SubjectDecisionApplied2> failed(const Status &status) {
  return {false, {}, status.error};
}
// 写前纯业务拒绝才可作为原 Director rejected；缺能力及写后失败均上送外层。
Outcome<SubjectDecisionApplied2> rejected_before_write(const RuleError &error) {
  if (error.kind != ErrorKind::rule)
    return {false, {}, error};
  SubjectDecisionApplied2 value;
  value.outcome.kind = DecisionOutcome::Kind::rejected;
  value.outcome.code = error.original.code;
  return {true, std::move(value), {}};
}
} // namespace
// 临时前像由原 WorldBundle copy 构造拥有，捕获/保存点身份来自实际 Common。
// 不持第二结果表；该借用只活到原 adoption 同栈 finalizer。
struct NativeDirectorActorCommandBindings::Pending {
  // 新 Inspect 来源的普通字段预留必须随原采用保存点活到最终核验。
  CheckpointCopyLease source_memory;
  std::shared_ptr<const WorldBundle> before;
  // The private preimage keeps the real invocation moment until its final delta
  // read. This scope dies before the world it borrows; it creates no business
  // source.
  std::unique_ptr<PhaseCurrentReadScope> before_phase;
  const SubjectDecisionProposal2 *proposal = nullptr;
  std::shared_ptr<const CommitResult> result;
  MomentOwnerSource effect;
  WorldMoment at;
  Integer sequence = 0;
  std::shared_ptr<const ActorConditionCaptureIdentity> capture;
  ActorConditionSavepointBoundary savepoint;
  Pending(std::shared_ptr<const WorldBundle> old,
          std::unique_ptr<PhaseCurrentReadScope> phase,
          const SubjectDecisionProposal2 &p,
          std::shared_ptr<const CommitResult> sealed, MomentOwnerSource source,
          const WorldMoment &moment, Integer seq,
          std::shared_ptr<const ActorConditionCaptureIdentity> actual_capture,
          ActorConditionSavepointBoundary boundary,
          CheckpointCopyLease source_lease)
      : source_memory(std::move(source_lease)), before(std::move(old)),
        before_phase(std::move(phase)), proposal(&p), result(std::move(sealed)),
        effect(std::move(source)), at(moment), sequence(seq),
        capture(std::move(actual_capture)), savepoint(std::move(boundary)) {}
};
NativeDirectorActorCommandBindings::NativeDirectorActorCommandBindings(
    const WorldBundle &world, const CompleteExactWorldReadBindings *actual)
    : world_(world), actual_(actual) {}
NativeDirectorActorCommandBindings::NativeDirectorActorCommandBindings(
    WorldBundle &world, const CompleteExactWorldReadBindings *actual)
    : world_(world), writable_(&world), actual_(actual) {}
NativeDirectorActorCommandBindings::~NativeDirectorActorCommandBindings() =
    default;
bool NativeDirectorActorCommandBindings::bound_to(
    const CommonStore &common) const noexcept {
  return &common == &world_.common;
}
bool NativeDirectorActorCommandBindings::bound_to(
    const CommonStore &common, const DirectorState &director) const noexcept {
  return writable_ == &world_ && &common == &world_.common &&
         &director == &world_.director;
}
Status NativeDirectorActorCommandBindings::selection() const {
  if (!world_.catalog || !world_.catalog->exact_time ||
      !exact_complete_selected(*world_.catalog->exact_time) ||
      !world_.catalog->complete_exact_time_runtime || !world_.catalog->growth ||
      !world_.catalog->director || !world_.catalog->director->subjects2 ||
      !world_.catalog->director->command_acceptance_bindings ||
      !world_.director.subject2 ||
      world_.director.subject2->command_binding_version != 1)
    return common_failure("DirectorActualCommandCompleteSelectionRequired", {},
                          ErrorKind::unsupported_capability);
  return common_success();
}
Status NativeDirectorActorCommandBindings::message_selection() const {
  if (!actual_ || !actual_->family_control_sources ||
      !world_.catalog->visit_message_receipts_enabled ||
      !world_.interaction.visit_messages ||
      world_.interaction.visit_messages->version < 2 ||
      !world_.interaction.visit_message_receipts ||
      world_.interaction.visit_message_receipts->version != 2)
    return common_failure("DirectorMessageActualReceipt2OwnerRequired", {},
                          ErrorKind::unsupported_capability);
  auto physical = visit_message_receipt_selected_version(
      world_.catalog->identity.module_versions);
  if (!physical.ok)
    return {false, {}, std::move(physical.error)};
  if (physical.value != 2 ||
      &actual_->runtime != world_.catalog->complete_exact_time_runtime.get())
    return common_failure("DirectorMessageActualCandidateMismatch", {},
                          ErrorKind::internal);
  auto body = actual_->character_conditions.sources();
  if (!body.ok)
    return {false, {}, std::move(body.error)};
  return &body.value.common == &world_.common
             ? common_success()
             : common_failure("DirectorMessageActualCandidateMismatch", {},
                              ErrorKind::internal);
}
Status NativeDirectorActorCommandBindings::pending_identity(
    const StableId &actor, const SourceRef &effect) const {
  if (!writable_ || !pending_ || !pending_->proposal || !pending_->before ||
      !pending_->before_phase || !pending_->result ||
      pending_->proposal->subject.id != actor ||
      !same_source(pending_->effect.source_ref, effect) ||
      !world_.active_exact_write_context() ||
      world_.common.actor_condition_capture_identity() != pending_->capture ||
      pending_->before->catalog.get() != world_.catalog.get())
    return common_failure("DirectorActualPendingCommandMismatch", effect.id);
  auto boundary = world_.common.validate_actor_condition_savepoint_boundary(
      pending_->savepoint);
  if (!boundary.ok)
    return boundary;
  auto now = world_.actual_phase_moment();
  if (!now.ok)
    return {false, {}, std::move(now.error)};
  auto at = compare_world_moments(now.value, pending_->at);
  if (!at.ok)
    return {false, {}, std::move(at.error)};
  if (at.value)
    return common_failure("DirectorPendingCommandMomentChanged", effect.id);
  auto before_at = pending_->before->actual_phase_moment();
  if (!before_at.ok)
    return {false, {}, std::move(before_at.error)};
  auto frozen_at = compare_world_moments(before_at.value, pending_->at);
  if (!frozen_at.ok)
    return {false, {}, std::move(frozen_at.error)};
  if (frozen_at.value)
    return common_failure("DirectorPendingPreimageMomentChanged", effect.id,
                          ErrorKind::internal);
  auto sealed = read_common_command_source(
      world_.common, {"common", "command_result", pending_->result->result_id});
  if (!sealed.ok)
    return {false, {}, std::move(sealed.error)};
  if (sealed.value.result != pending_->result.get() ||
      sealed.value.history->actor_id != actor ||
      sealed.value.request_seq != pending_->sequence)
    return common_failure("DirectorPendingOriginalResultMismatch", effect.id);
  if (original_interaction_source(effect)) {
    if (message_source(effect)) {
      auto ready = message_selection();
      if (!ready.ok)
        return ready;
    }
    if (!pending_->result->details)
      return common_failure("DirectorPendingActualMessageResultRequired",
                            effect.id, ErrorKind::internal);
    if (!pending_->proposal->actor_proposal ||
        !pending_->proposal->actor_proposal->command ||
        pending_->proposal->actor_proposal->command->kind !=
            pending_->result->kind)
      return common_failure("DirectorPendingActualCommandMismatch", effect.id);
    CheckpointCopyLease source_memory;
    auto source =
        interaction_effect(*pending_->before, world_, actor,
                           *pending_->proposal->actor_proposal->command,
                           *pending_->result->details, actual_, source_memory);
    if (!source.ok)
      return {false, {}, std::move(source.error)};
    return source.value.source_schema == pending_->effect.source_schema &&
                   same_source(source.value.source_ref, effect)
               ? common_success()
               : common_failure("DirectorPendingActualEffectMismatch",
                                effect.id, ErrorKind::internal);
  }
  const auto *detail =
      pending_->result->details
          ? std::get_if<ActionStarted>(&pending_->result->details->data)
          : nullptr;
  const auto *action =
      detail ? world_.common.action(detail->action_id) : nullptr;
  if (!action || action->actor_id != actor ||
      action->kind != pending_->result->kind || action->id != effect.id)
    return common_failure("DirectorPendingActualActionRequired", effect.id);
  auto source = read_growth_started_business_source(
      pending_->before->growth, world_.growth, world_.common, *action,
      pending_->at);
  if (!source.ok)
    return {false, {}, std::move(source.error)};
  return source.value.source_schema == pending_->effect.source_schema &&
                 same_source(source.value.source_ref, effect)
             ? common_success()
             : common_failure("DirectorPendingActualEffectMismatch", effect.id);
}
Status NativeDirectorActorCommandBindings::validate_started_effect(
    const StableId &actor, const SourceRef &effect) const {
  auto now = world_.actual_phase_moment();
  if (!now.ok)
    return {false, {}, std::move(now.error)};
  auto identity = read_growth_started_business_identity(
      world_.common, world_.growth, {effect, 1}, now.value, &world_.interaction,
      world_.catalog->interaction.get());
  if (!identity.ok)
    return {false, {}, std::move(identity.error)};
  const auto &actual = identity.value;
  if (!actual.actor_id || *actual.actor_id != actor || !actual.action_kind ||
      !actual.actual_start || !actual.actual_command)
    return common_failure("DirectorStartedBusinessActorMismatch", effect.id);
  const auto &command = actual.actual_command->command_binding;
  auto sealed = read_common_command_source(
      world_.common, {"common", "command_result", command.result_id});
  if (!sealed.ok)
    return {false, {}, std::move(sealed.error)};
  const auto &result = *sealed.value.result;
  const auto *details = result.details
                            ? std::get_if<ActionStarted>(&result.details->data)
                            : nullptr;
  if (sealed.value.history->actor_id != actor ||
      sealed.value.request_seq != command.request_seq ||
      result.kind != command.command_kind ||
      result.kind != *actual.action_kind ||
      result.tick != actual.actual_start->occurred_at.half_day_tick ||
      !details || details->action_id != effect.id)
    return common_failure("DirectorStartedOriginalCommandMismatch", effect.id);
  return common_success();
}
Status NativeDirectorActorCommandBindings::validate_actor_command_effect(
    const StableId &actor, const SourceRef &effect) const {
  auto selected = selection();
  if (!selected.ok)
    return selected;
  if (!growth_start_source(effect) && !original_interaction_source(effect))
    return common_failure("DirectorActorOriginalEffectReaderRequired",
                          effect.id, ErrorKind::unsupported_capability);
  if (pending_ && same_source(pending_->effect.source_ref, effect))
    return pending_identity(actor, effect);
  if (original_interaction_source(effect)) {
    if (message_source(effect)) {
      auto ready = message_selection();
      if (!ready.ok)
        return ready;
    }
    return validate_interaction_effect(world_, actor, effect, actual_);
  }
  return validate_started_effect(actor, effect);
}
Outcome<SubjectDecisionApplied2>
NativeDirectorActorCommandBindings::apply_actor_command_in_candidate(
    const SubjectDecisionProposal2 &proposal) {
  auto selected = selection();
  if (!selected.ok)
    return failed(selected);
  if (!writable_ || pending_ || !world_.active_exact_write_context() ||
      !world_.common.actor_condition_capture_identity())
    return failed(common_failure("DirectorActorActualWriteCaptureRequired",
                                 proposal.plan_id));
  auto shape = validate_director_actor_proposal2(proposal);
  if (!shape.ok)
    return failed(shape);
  const auto *original =
      proposal.actor_proposal ? &*proposal.actor_proposal : nullptr;
  if (!original || !original->command || !original->transition ||
      !proposal.wait_reason.empty() ||
      proposal.subject.id == world_.common.state().metadata.player_actor_id)
    return failed(common_failure("DirectorActorOriginalCommandRequired",
                                 proposal.plan_id));
  const auto &command = *original->command;
  const auto *growth = std::get_if<GrowthCommand>(&command.data);
  const bool original_growth = growth && growth->kind == command.kind &&
                               growth_action_kind(command.kind);
  const bool message = message_command(command);
  const bool authored_reply =
      std::holds_alternative<AuthoredVisitReply>(command.data) &&
      command.kind == authored_visit_reply_command;
  const bool authored_invitation =
      std::holds_alternative<AuthoredVisitAppointment>(command.data) &&
      command.kind == authored_visit_appointment_command;
  const bool inspection = inspection_command(command) != nullptr;
  const bool interaction =
      message || authored_reply || authored_invitation || inspection;
  if (inspection && !actual_)
    return failed(
        common_failure("DirectorActorActualInteractionSourcesRequired"));
  if (!original_growth && !interaction)
    return failed(common_failure("DirectorActorOriginalCommandTailRequired",
                                 command.kind,
                                 ErrorKind::unsupported_capability));
  if (message) {
    auto ready = message_selection();
    if (!ready.ok)
      return failed(ready);
  }
  if (authored_reply) {
    auto ready = reply_context(world_, actual_);
    if (!ready.ok)
      return {false, {}, std::move(ready.error)};
  }
  if (authored_invitation) {
    auto ready = invitation_context(world_, actual_);
    if (!ready.ok)
      return {false, {}, std::move(ready.error)};
  }
  if (original_growth) {
    const ActionTimeRegistration *registration = nullptr;
    for (const auto &row :
         world_.catalog->complete_exact_time_runtime->action_times)
      if (row.action_kind == command.kind) {
        if (registration)
          return failed(common_failure("DuplicateActualActionTimeRegistration",
                                       command.kind));
        registration = &row;
      }
    if (!registration || registration->owner_domain != "growth" ||
        registration->profile_id != "growth_fixed_service_time2" ||
        registration->source_schema != 1)
      return failed(common_failure(
          "DirectorGrowthActualTimeRegistrationRequired", command.kind));
  }
  if (original->transition->expected_revision != world_.common.revision())
    return rejected_before_write(
        common_failure("StaleDirectorRevision", proposal.subject.id).error);
  for (const auto &expected : original->expected_versions)
    if (world_.common.read_version(expected.resource).version !=
        expected.version)
      return rejected_before_write(
          common_failure("StaleResourceRecipe", expected.resource.id).error);
  const auto *prior_sequence =
      world_.common.state().request_sequences.find(proposal.subject.id);
  if (!prior_sequence || *prior_sequence < 0 || *prior_sequence >= 2147483647)
    return failed(common_failure("DirectorActualCommandSequenceExhausted",
                                 proposal.subject.id));
  const Integer sequence = *prior_sequence + 1;
  auto now = world_.actual_phase_moment();
  if (!now.ok)
    return {false, {}, std::move(now.error)};
  auto prepared = interaction
                      ? prepare_command_with_exact_bindings(
                            world_, proposal.subject.id, command, *actual_)
                      : prepare_command(world_, proposal.subject.id, command);
  if (!prepared.ok)
    return rejected_before_write(prepared.error);
  if (!prepared.value || !prepared.value->operation ||
      prepared.value->actor_id != proposal.subject.id ||
      prepared.value->kind != command.kind)
    return failed(common_failure("DirectorActualPreparedCommandRequired",
                                 proposal.plan_id, ErrorKind::internal));
  auto before = checkpoint_make_shared<WorldBundle>(world_);
  // World copies deliberately omit transient phase state. Preserve the actual
  // captured moment for this read-only command preimage, never (tick, 0).
  auto before_phase =
      before->enter_phase_read_context("director_command_preimage", "director",
                                       {now.value, now.value}, now.value);
  if (!before_phase.ok)
    return {false, {}, std::move(before_phase.error)};
  auto &candidate = *writable_;
  // 原观察腿只触及已获知报告，绝不把提案引用变成新的 Delivery/Knowledge。
  for (const auto &report : original->transition->known_report_ids) {
    ReadStampWriteBoundary boundary(*candidate.read_stamps);
    auto observed =
        observe_dynamic_report(candidate.common, candidate.social, report,
                               proposal.subject, now.value.half_day_tick);
    if (!observed.ok)
      return failed(observed);
  }
  // 仅真 History2 交易需要原资源执行捕获；已有同候选捕获只借，不夺取终结权。
  std::optional<ResourceExecutionCoverageBoundary> resource_coverage;
  if (original_growth && growth->kind == "trade") {
    auto market =
        selected_native_growth_market_version(candidate.catalog->identity);
    if (!market.ok)
      return {false, {}, std::move(market.error)};
    if (market.value == 2 &&
        !candidate.common.resource_execution_capture_identity()) {
      auto opened = begin_resource_execution_coverage(candidate.common);
      if (!opened.ok)
        return {false, {}, std::move(opened.error)};
      resource_coverage.emplace(std::move(opened.value));
    }
  }
  auto applied = interaction ? apply_command_with_exact_bindings(
                                   candidate, proposal.subject.id,
                                   *prepared.value->operation, *actual_)
                             : apply_command(candidate, proposal.subject.id,
                                             *prepared.value->operation);
  if (!applied.ok)
    return {false, {}, std::move(applied.error)};
  if (candidate.growth_market_capture.open_scopes != 0 ||
      candidate.growth_market_capture.unbound_business)
    return failed(
        common_failure("GrowthMarketUnboundBusiness", {}, ErrorKind::internal));
  // 原 FIFO 及市场正文已完成同次提交，完整资源覆盖通过后才准封存命令结果。
  // 失败直接撤整个外层候选；绝不补空 draft，也不关闭借来的捕获。
  if (resource_coverage) {
    auto covered = finish_resource_execution_coverage(candidate.common,
                                                      *resource_coverage);
    if (!covered.ok)
      return {false, {}, std::move(covered.error)};
  }
  const auto *details = std::get_if<ActionStarted>(&applied.value.data);
  const auto *action =
      details ? candidate.common.action(details->action_id) : nullptr;
  if (original_growth && (!action || action->actor_id != proposal.subject.id ||
                          action->kind != command.kind))
    return failed(common_failure("DirectorActualGrowthStartRequired",
                                 proposal.plan_id, ErrorKind::internal));
  if (interaction && action)
    return failed(common_failure("DirectorInteractionUnexpectedAction",
                                 proposal.plan_id, ErrorKind::internal));
  CheckpointCopyLease source_memory;
  auto source =
      interaction
          ? interaction_effect(*before, candidate, proposal.subject.id, command,
                               applied.value, actual_, source_memory)
          : read_growth_started_business_source(
                before->growth, candidate.growth, candidate.common, *action,
                now.value);
  if (!source.ok)
    return {false, {}, std::move(source.error)};
  auto sealed = seal_complete_command_result_in_candidate(
      candidate, proposal.subject.id, sequence, command.kind, applied.value);
  if (!sealed.ok)
    return {false, {}, std::move(sealed.error)};
  auto tasks_bound =
      bind_complete_task_command_result(candidate, *sealed.value);
  if (!tasks_bound.ok)
    return failed(tasks_bound);
  auto accepted = capture_director_accepted_command(
      candidate.common, *candidate.catalog->director,
      candidate.catalog->identity, proposal.policy_id, proposal.subject.id,
      sequence, *sealed.value, source.value, now.value);
  if (!accepted.ok)
    return {false, {}, std::move(accepted.error)};
  auto boundary = candidate.common.actor_condition_savepoint_boundary();
  if (!boundary.ok)
    return {false, {}, std::move(boundary.error)};
  SubjectDecisionApplied2 result;
  if (inspection) {
    // 返回的来源引用及执行绑定各有一份真实普通字段，租约随采用值转移。


    result.payload_memory = CheckpointCopyLease::reserve();
  }
  result.outcome.kind = DecisionOutcome::Kind::adopted;
  if (action)
    result.outcome.action_id = action->id;
  result.applied_effect_ref = source.value.source_ref;
  result.actual_binding = SubjectExecutionBinding{
      ActorExecutionBinding{source.value.source_ref, std::nullopt}};
  result.accepted_command = std::move(accepted.value);
  pending_ = checkpoint_make_unique<Pending>(
      std::move(before), std::move(before_phase.value), proposal,
      std::move(sealed.value), std::move(source.value), now.value, sequence,
      candidate.common.actor_condition_capture_identity(),
      std::move(boundary.value), std::move(source_memory));
  return {true, std::move(result), {}};
}
Status NativeDirectorActorCommandBindings::finalize_actor_adoption_in_candidate(
    const SubjectDecisionProposal2 &proposal,
    const DirectorAdoptionResult2 &adoption, const WorldMoment &at) {
  auto selected = selection();
  if (!selected.ok)
    return selected;
  if (!writable_ || !pending_ || pending_->proposal != &proposal ||
      !adoption.accepted_command)
    return common_failure("DirectorActorActualPendingAdoptionRequired",
                          proposal.plan_id);
  const auto *row = world_.director.subject2->adoption_results.find(
      director_adoption_key2(proposal.subject, proposal.registration_epoch,
                             proposal.decision_seq));
  if (row != &adoption || adoption.status != DirectorAdoptionStatus2::adopted ||
      adoption.accepted_command->result_id != pending_->result->result_id ||
      adoption.accepted_command->request_seq != pending_->sequence ||
      adoption.accepted_command->actor_id != proposal.subject.id ||
      adoption.accepted_command->command_kind != pending_->result->kind ||
      !same_source(adoption.accepted_command->actual_effect.source_ref,
                   pending_->effect.source_ref) ||
      adoption.accepted_command->actual_effect.source_schema !=
          pending_->effect.source_schema)
    return common_failure("DirectorActorOriginalAdoptionMismatch",
                          proposal.plan_id);
  auto same = compare_world_moments(at, pending_->at);
  if (!same.ok)
    return {false, {}, std::move(same.error)};
  if (same.value)
    return common_failure("DirectorActorOriginalAdoptionMomentMismatch",
                          proposal.plan_id);
  auto accepted_at = compare_world_moments(
      adoption.accepted_command->accepted_at, pending_->at);
  if (!accepted_at.ok)
    return {false, {}, std::move(accepted_at.error)};
  if (accepted_at.value)
    return common_failure("DirectorActorOriginalAcceptanceMomentMismatch",
                          proposal.plan_id);
  auto actual =
      pending_identity(proposal.subject.id, pending_->effect.source_ref);
  if (!actual.ok)
    return actual;
  auto accepted = read_director_accepted_command(
      world_.common, world_.director, *world_.catalog->director,
      world_.catalog->identity,
      {proposal.subject.id, proposal.registration_epoch, proposal.decision_seq,
       pending_->result->kind, pending_->sequence,
       pending_->result->result_id});
  if (!accepted.ok)
    return {false, {}, std::move(accepted.error)};
  const auto &effect = pending_->effect.source_ref;
  const bool interaction = original_interaction_source(effect);
  Status bound;
  if (message_source(effect))
    bound = bind_visit_message_director_actor_result_world(*writable_, adoption,
                                                           actual_);
  else if (authored_reply_source(effect)) {
    auto x = reply_context(world_, actual_);
    if (!x.ok)
      return {false, {}, std::move(x.error)};
    bound = finalize_visit_reply_director_actor_result_world(world_, adoption,
                                                             x.value);
  } else if (authored_invitation_source(effect)) {
    auto x = invitation_context(world_, actual_);
    if (!x.ok)
      return {false, {}, std::move(x.error)};
    bound = finalize_visit_appointment_director_actor_result_world(
        world_, adoption, x.value);
  } else if (is_public_supply_inspection_source(effect)) {
    // Inspect 无第二张采用账；已封 Common 与原 Director 接受行由上文反读。
    bound = validate_interaction_effect(world_, proposal.subject.id, effect,
                                        actual_);
  } else
    bound = bind_complete_growth_command_result(
        pending_->before.get(), *writable_, proposal.subject.id,
        pending_->sequence, *pending_->result);
  if (!bound.ok)
    return bound;
  if (interaction)
    writable_->note_write_boundary();
  // Only after the real adoption row do original authored readers join its
  // accepted command to their unchanged claim/intent source. No player receipt.
  auto stable = interaction
                    ? validate_interaction_effect(world_, proposal.subject.id,
                                                  effect, actual_)
                    : validate_started_effect(proposal.subject.id, effect);
  if (!stable.ok)
    return stable;
  pending_.reset();
  return common_success();
}
} // namespace hanmo::resident
