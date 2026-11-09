#include "craft_condition.hpp"
#include "authored_origin.hpp"
#include "character_condition_replay.hpp"
#include "condition_application_fact.hpp"
#include "condition_capacity_contract.hpp"
#include "resident/domains/craft/craft_job.hpp"
#include "resident/domains/growth/growth_model.hpp"
#include "resident/domains/growth/growth_permanent_contributions.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <string_view>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {},
                       ErrorKind kind = ErrorKind::internal) {
  throw Failure{common_failure(code, id, kind).error};
}
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
bool same_time(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b)) == 0;
}
void selected(const CharacterConditionSources &body) {
  if (!body.growth_catalog ||
      !body.growth_catalog->complete_capability_application_chain ||
      !body.craft)
    fail("CraftConditionSelectedSourcesRequired", {},
         ErrorKind::unsupported_capability);
  if (&body.craft->materials.common != &body.common ||
      body.craft->materials.jobs.schema_version != 1)
    fail("CraftConditionCandidateMismatch");
  if (!same_time(take(body.craft->clock.read_current_moment()),
                 body.actual_now))
    fail("CraftConditionActualClockMismatch");
  const auto &rule = body.catalog.work_or_study_rule;
  if (!rule || rule->id != "original_work_or_study_exertion" ||
      rule->version != 1 ||
      rule->catalog_digest != body.common.state().metadata.content_digest)
    fail("CraftConditionOriginalWorkRuleRequired", {},
         ErrorKind::unsupported_capability);
}
bool allocated(const CommonState &common, const StableId &id) {
  constexpr std::string_view prefix = "condition:";
  if (id.size() <= prefix.size() || id.compare(0, prefix.size(), prefix) != 0)
    return false;
  const std::string_view digits(id.data() + prefix.size(),
                                id.size() - prefix.size());
  if (digits.front() == '0')
    return false;
  Integer n = 0;
  auto parsed =
      std::from_chars(digits.data(), digits.data() + digits.size(), n);
  return parsed.ec == std::errc{} &&
         parsed.ptr == digits.data() + digits.size() && n > 0 &&
         n < common.next_instance_id;
}
Integer completion_ordinal(const CraftGateApplied &gate) {
  if (gate.result.gate_ordinal < 0 ||
      gate.result.gate_ordinal == std::numeric_limits<Integer>::max())
    fail("CraftConditionOrdinalOverflow", gate.id);
  return gate.result.gate_ordinal +
         1; // Existing WorkOrStudy completion ordinals are one-based.
}
struct Original {
  const CraftJob *job = nullptr;
  const CraftGateApplied *gate = nullptr;
};
Original original_gate(const CharacterConditionSources &body,
                       const SourceRef &source) {
  selected(body);
  if (source.domain != "crafting" || source.kind != "craft_work_gate" ||
      source.id.empty())
    fail("CraftConditionActualGateRequired", source.id);
  const auto *gate =
      take(read_craft_gate_for_original_effect(*body.craft, source.id));
  const auto *job = body.craft->materials.jobs.jobs.find(gate->job_id);
  if (!job || job->id != gate->job_id ||
      job->terms.maker.kind != SubjectKind::actor ||
      job->execution_binding.action_kind != "craft_job" ||
      job->execution_binding.action_id.empty() ||
      take(compare_world_moments(gate->occurred_at, body.actual_now)) > 0)
    fail("CraftConditionOriginalGateMismatch", source.id);
  completion_ordinal(*gate);
  return {job, gate};
}
Original original_cause(const CharacterConditionSources &body,
                        const WorkOrStudyConditionCause &cause) {
  selected(body);
  if (cause.kind != WorkOrStudyConditionCause::Kind::craft_gate ||
      cause.ordinal < 1 || cause.actor_id.empty() || cause.action_id.empty())
    fail("CraftConditionCauseRequired");
  const CraftJob *job = nullptr;
  for (const auto &row : body.craft->materials.jobs.jobs.rows())
    if (row.second.execution_binding.action_id == cause.action_id) {
      if (job)
        fail("DuplicateCraftConditionAction", cause.action_id);
      job = &row.second;
    }
  if (!job ||
      static_cast<std::uint64_t>(cause.ordinal) > job->gate_result_refs.size())
    fail("CraftConditionOriginalGateMissing", cause.action_id);
  auto actual = original_gate(
      body,
      craft_gate_source(
          job->gate_result_refs[static_cast<std::size_t>(cause.ordinal - 1)]));
  if (actual.job != job || completion_ordinal(*actual.gate) != cause.ordinal)
    fail("CraftConditionOriginalOrdinalMismatch", cause.action_id);
  return actual;
}
void participant(const Original &original, const StableId &actor) {
  const auto &terms = original.job->terms;
  std::size_t count = terms.maker.id == actor ? 1 : 0;
  for (const auto &helper : terms.accepted_helpers)
    if (helper == actor)
      ++count;
  if (actor.empty() || count != 1)
    fail("CraftConditionOriginalParticipantRequired", actor);
}
bool key_matches(const ConditionReceiptV2 &receipt, const CraftJob &job,
                 Integer ordinal, const StableId &actor) {
  const auto *key =
      std::get_if<WorkOrStudyConditionBusinessKey>(&receipt.business_key);
  return key && key->action_id == job.execution_binding.action_id &&
         key->actor_id == actor && key->ordinal == ordinal;
}
const ConditionReceiptV2 *existing(const CharacterConditionSources &body,
                                   const Original &source,
                                   const StableId &actor) {
  const ConditionReceiptV2 *found = nullptr;
  for (const auto &row :
       body.state.character_condition_extensions.applied_receipts.rows())
    if (key_matches(row.second, *source.job, completion_ordinal(*source.gate),
                    actor)) {
      if (found)
        fail("DuplicateCraftConditionReceipt", source.gate->id);
      found = &row.second;
    }
  return found;
}
Integer fatigue_after(Integer before) {
  if (before < 0 || before > 100)
    fail("CraftConditionFatigueRangeInvalid");
  return before >= 92 ? 100 : before + 8;
}
void receipt_shape(const CharacterConditionSources &body,
                   const ConditionReceiptV2 &receipt,
                   const Original &original) {
  const auto *cause = std::get_if<WorkOrStudyConditionCause>(&receipt.cause);
  if (!cause || cause->kind != WorkOrStudyConditionCause::Kind::craft_gate ||
      cause->action_id != original.job->execution_binding.action_id ||
      cause->actor_id != receipt.actor_id ||
      cause->ordinal != completion_ordinal(*original.gate) ||
      !key_matches(receipt, *original.job, cause->ordinal, cause->actor_id))
    fail("CraftConditionBusinessKeyMismatch", receipt.result_id);
  participant(original, receipt.actor_id);
  if (!allocated(body.common.state(), receipt.result_id) ||
      !actor_foundation_rule_equal(receipt.rule_ref,
                                   *body.catalog.work_or_study_rule) ||
      receipt.effective_at.kind != ActorFoundationAt::Kind::exact_moment ||
      !same_time(receipt.effective_at.exact_moment,
                 original.gate->occurred_at) ||
      receipt.condition_revision_before < 0 ||
      receipt.condition_revision_before ==
          std::numeric_limits<Integer>::max() ||
      receipt.condition_revision_after !=
          receipt.condition_revision_before + 1 ||
      receipt.scalar_changes.size() != 1 ||
      receipt.selected_injury_result.kind !=
          SelectedInjuryResult::Kind::not_an_injury ||
      !receipt.created_effect_keys.empty() ||
      !receipt.recovery_resolution_refs.empty() ||
      receipt.life_transition_ref ||
      !original_condition_capacity_refs_shape(body.catalog, receipt))
    fail("CraftConditionReceiptMismatch", receipt.result_id);
  const auto &delta = receipt.scalar_changes.front();
  if (delta.field != ConditionScalarChange::Field::fatigue ||
      delta.after != fatigue_after(delta.before))
    fail("CraftConditionOriginalFormulaMismatch", receipt.result_id);
}
CraftConditionApplication
result(const ConditionReceiptV2 &receipt, bool replay,
       const BoundActorConditionWrite *write = nullptr) {
  CraftConditionApplication out;
  out.text_memory =
      CheckpointMemoryCharge(0);
  out.receipt_ref = {receipt.result_id};
  out.already_applied = replay;
  if (write)
    out.actual_writes.push_back(*write);
  return out;
}
} // namespace
Status
validate_craft_gate_condition_receipt(const CharacterConditionSources &body,
                                      const ConditionReceiptV2 &receipt) {
  try {
    const auto *cause = std::get_if<WorkOrStudyConditionCause>(&receipt.cause);
    if (!cause)
      fail("CraftConditionCauseRequired", receipt.result_id);
    auto actual = original_cause(body, *cause);
    receipt_shape(body, receipt, actual);
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Outcome<CraftConditionApplication> apply_verified_craft_gate_condition(
    CommonStore &common, AuthoredActorFoundationState &foundation,
    const CharacterConditionSources &body, const CraftJobReadContext &craft,
    const SourceRef &gate, const StableId &actor_id, const ReadStamp &stamp,
    const ReadStampAuthority &stamps, const DomainReadVersionSource &versions,
    GrowthCapabilityRevisionWrite &capability) {
  try {
    if (&body.common != &common || &body.state != &foundation ||
        body.craft != &craft || !body.permanent_growth ||
        !capability.bound_to(common, *body.permanent_growth))
      fail("CraftConditionSameCandidateRequired");
    auto actual = original_gate(body, gate);
    participant(actual, actor_id);
    take(stamps.validate(stamp, common.state().identity,
                         ReadMode::phase_current, common, versions));
    bool actor_read = false;
    for (const auto &r : stamp.resources())
      if (r.resource.kind == ResourceKey::Kind::actor &&
          r.resource.id == actor_id)
        actor_read = true;
    bool head_read = false;
    for (const auto &r : stamp.domains())
      if (r.domain == "actor_foundation" && r.kind == "condition_head" &&
          r.id == actor_id)
        head_read = true;
    if (!actor_read || !head_read ||
        std::find(stamp.source_ids().begin(), stamp.source_ids().end(),
                  gate.id) == stamp.source_ids().end() ||
        !same_time(stamp.span().to, actual.gate->occurred_at) ||
        !same_time(body.actual_now, actual.gate->occurred_at))
      fail("CraftConditionActualStampRequired", gate.id);
    if (const auto *old = existing(body, actual, actor_id)) {
      take(validate_craft_gate_condition_receipt(body, *old));
      take(validate_condition_application_fact_for_receipt(common, foundation,
                                                           body.catalog, *old));
      take(read_character_condition_receipt_provenance(body, {old->result_id}));
      if (!old->capability_application)
        fail("CraftConditionCapabilityApplicationMissing", old->result_id);
      return {true, result(*old, true), {}};
    }
    auto *head =
        foundation.character_condition_extensions.actor_condition_heads.find(
            actor_id);
    const auto *actor = common.actor(actor_id);
    if (!head || !actor || !actor->alive ||
        head->condition_revision == std::numeric_limits<Integer>::max())
      fail("CraftConditionOriginalActorRequired", actor_id);
    // Recheck this actor's actual prefix. Full gate reverse coverage
    // intentionally runs at publication: other genuine participants have not
    // been written yet.
    if (head->latest_receipt_ref)
      take(read_character_condition_receipt_provenance(
          body, *head->latest_receipt_ref));
    auto before =
        take(read_character_condition_chain_cut(body, *head, body.actual_now));
    if (!before.body.alive || before.body.fatigue != actor->fatigue)
      fail("CraftConditionOriginalBeforeMismatch", actor_id);
    auto capture = common.actor_condition_capture_identity();
    if (!capture)
      fail("CraftConditionLiveCaptureRequired", gate.id);
    const auto offset = common.actor_condition_execution_drafts().size();
    if (offset >= static_cast<std::size_t>(std::numeric_limits<Integer>::max()))
      fail("CraftConditionCaptureOverflow", gate.id);
    ConditionReceiptV2 receipt;
    receipt.result_id = take(common.next_id("condition"));
    receipt.actor_id = actor_id;
    const auto ordinal = completion_ordinal(*actual.gate);
    receipt.business_key = WorkOrStudyConditionBusinessKey{
        actual.job->execution_binding.action_id, actor_id, ordinal};
    receipt.cause = WorkOrStudyConditionCause{
        WorkOrStudyConditionCause::Kind::craft_gate,
        actual.job->execution_binding.action_id, actor_id, ordinal};
    receipt.rule_ref = *body.catalog.work_or_study_rule;
    receipt.effective_at.exact_moment = actual.gate->occurred_at;
    receipt.condition_revision_before = head->condition_revision;
    receipt.condition_revision_after = head->condition_revision + 1;
    receipt.scalar_changes = {{ConditionScalarChange::Field::fatigue,
                               before.body.fatigue,
                               fatigue_after(before.body.fatigue)}};
    receipt_shape(body, receipt, actual);
    ActorMutation mutation;
    mutation.actor_id = actor_id;
    mutation.fatigue = receipt.scalar_changes.front().after;
    take(common.apply_actor(mutation));
    const auto id = receipt.result_id;
    if (!foundation.character_condition_extensions.applied_receipts.insert(
            id, std::move(receipt)))
      fail("DuplicateConditionReceipt", id);
    head->condition_revision += 1;
    head->latest_receipt_ref = ConditionReceiptRefV2{id};
    if (common.actor_condition_execution_drafts().size() != offset + 1)
      fail("CraftConditionActualWriteCountMismatch", id);
    BoundActorConditionWrite bound{
        {id}, std::move(capture), static_cast<Integer>(offset + 1)};
    take(read_actual_actor_condition_write(common, foundation, bound));
    take(append_condition_application_fact_in_candidate(common, foundation,
                                                        body.catalog, bound));
    take(capability.record_actual_condition_change(bound));
    return {
        true,
        result(*foundation.character_condition_extensions.applied_receipts.find(
                   id),
               false, &bound),
        {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Status
validate_craft_gate_condition_coverage(const CharacterConditionSources &body) {
  try {
    for (const auto &row :
         body.state.character_condition_extensions.applied_receipts.rows())
      if (const auto *cause =
              std::get_if<WorkOrStudyConditionCause>(&row.second.cause);
          cause && cause->kind == WorkOrStudyConditionCause::Kind::craft_gate)
        take(validate_craft_gate_condition_receipt(body, row.second));
    if (!body.craft)
      return common_success();
    selected(body);
    for (const auto &row : body.craft->materials.jobs.gates.rows()) {
      auto actual = original_gate(body, craft_gate_source(row.first));
      const auto &refs = actual.gate->result.exertion_source_refs;
      if (refs.size() != actual.job->terms.accepted_helpers.size() + 1)
        fail("CraftConditionGateResultCoverageMismatch", row.first);
      std::size_t index = 0;
      auto check = [&](const StableId &actor) {
        participant(actual, actor);
        const auto *receipt = existing(body, actual, actor);
        if (!receipt)
          fail("CraftConditionGateCoverageMismatch", row.first);
        receipt_shape(body, *receipt, actual);
        if (!receipt->capability_application)
          fail("CraftConditionCapabilityApplicationMissing",
               receipt->result_id);
        if (refs[index++] != receipt->result_id)
          fail("CraftConditionGateResultCoverageMismatch", row.first);
      };
      check(actual.job->terms.maker.id);
      for (const auto &helper : actual.job->terms.accepted_helpers)
        check(helper);
    }
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Outcome<SourceRef>
read_craft_condition_original_source(const CharacterConditionSources &body,
                                     const ConditionReceiptRefV2 &ref) {
  try {
    const auto *receipt =
        take(read_character_condition_receipt_provenance(body, ref));
    const auto *cause = std::get_if<WorkOrStudyConditionCause>(&receipt->cause);
    if (!cause)
      fail("CraftConditionCauseRequired", ref.result_id);
    auto actual = original_cause(body, *cause);
    return {true, craft_gate_source(actual.gate->id), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
RequiredSourceConsumerSlot craft_condition_source_consumer_slot() {
  return {"actor_foundation", "condition_receipt", "craft_gate"};
}
Status visit_craft_condition_source_pins(const CharacterConditionSources &body,
                                         CraftConditionSourcePinSink &sink) {
  try {
    take(validate_craft_gate_condition_coverage(body));
    for (const auto &row :
         body.state.character_condition_extensions.applied_receipts.rows())
      if (const auto *cause =
              std::get_if<WorkOrStudyConditionCause>(&row.second.cause);
          cause && cause->kind == WorkOrStudyConditionCause::Kind::craft_gate) {
        auto actual = original_cause(body, *cause);
        take(sink.actual_pin(
            {{"actor_foundation", "condition_receipt", row.first, "craft_gate"},
             craft_gate_source(actual.gate->id),
             SourcePinReason::unique_outcome}));
      }
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Status
validate_craft_condition_source_pin(const CharacterConditionSources &body,
                                    const SourcePin &pin) {
  struct Find final : CraftConditionSourcePinSink {
    const SourcePin &target;
    std::size_t count = 0;
    explicit Find(const SourcePin &p) : target(p) {
    }
    Status actual_pin(const SourcePin &p) override {
      if (p.consumer.domain == target.consumer.domain &&
          p.consumer.kind == target.consumer.kind &&
          p.consumer.id == target.consumer.id &&
          p.consumer.slot == target.consumer.slot &&
          same_source_ref(p.source, target.source) && p.reason == target.reason)
        ++count;
      return common_success();
    }
  } find(pin);
  auto checked = visit_craft_condition_source_pins(body, find);
  if (!checked.ok)
    return checked;
  return find.count == 1 ? common_success()
                         : common_failure("CraftConditionPinNotActual",
                                          pin.consumer.id, ErrorKind::internal);
}
} // namespace hanmo::resident
