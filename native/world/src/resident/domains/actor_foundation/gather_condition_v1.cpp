#include "gather_condition_v1.hpp"
#include "authored_origin.hpp"
#include "battle_injury_catalog_v1.hpp"
#include "battle_injury_episode_v1.hpp"
#include "character_condition_replay.hpp"
#include "condition_application_fact.hpp"
#include "condition_capacity_contract.hpp"
#include "resident/domains/growth/growth_model.hpp"
#include "resident/domains/growth/growth_permanent_contributions.hpp"
#include "resident/domains/local_economy/resources/resource_gather_condition_source.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
template <class T> T take(Outcome<T> v) {
  if (!v.ok)
    throw Failure{std::move(v.error)};
  return std::move(v.value);
}
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
void selection(const CharacterConditionSources &s) {
  if (!battle_injury_source1_selected(s.catalog.identity) ||
      s.selected_resource_sites != 1)
    fail("GatherConditionExplicitSelectionRequired");
  Integer count = 0;
  for (const auto &m : s.catalog.identity.module_versions)
    if (m.first == "resource_sites") {
      if (++count != 1 || m.second != "1")
        fail("GatherConditionModuleVersionMismatch");
    }
  if (count != 1 || !s.gather_condition || !s.elapsed || !s.scale ||
      &s.gather_condition->common != &s.common ||
      &s.gather_condition->elapsed != s.elapsed ||
      &s.gather_condition->scale != s.scale)
    fail("GatherConditionSameWorldSourcesRequired");
  if (take(compare_world_moments(
          take(s.gather_condition->clock.read_current_moment()), s.actual_now)))
    fail("GatherConditionActualClockRequired");
}
ResourceGatherConditionSource actual(const CharacterConditionSources &s,
                                     const StableId &id) {
  selection(s);


  auto memory = CheckpointMemoryCharge(0);
  auto result = take(read_gather_period_for_condition(
      *s.gather_condition, resource_gather_receipt_source(id)));
  if (!result.accepted || !result.receipt ||
      result.accepted->policy_ref.catalog_digest !=
          s.catalog.identity.content_digest ||
      result.receipt->policy_ref.catalog_digest !=
          s.catalog.identity.content_digest ||
      (result.fatigue_delta != 4 && result.fatigue_delta != 8 &&
       result.fatigue_delta != 14))
    fail("GatherConditionActualAuthoredPolicyRequired", id);
  return result;
}
Integer fatigue_after(Integer before, Integer delta) {
  if (before < 0 || before > 100)
    fail("GatherConditionBeforeFatigueInvalid");
  // The existing S4 ServiceTerms fatigue_delta interpretation, not a fabricated
  // Growth activity or a second fatigue field in the Gather owner.
  ServiceTerms terms;
  terms.fatigue_delta = delta;
  return std::clamp<Integer>(before + terms.fatigue_delta, 0, 100);
}
} // namespace
Status validate_gather_condition_source_v1(const CharacterConditionSources &s,
                                           const ConditionReceiptV2 &r) {
  try {
    const auto *cause = std::get_if<GatherFatigueConditionCauseV1>(&r.cause);
    const auto *key =
        std::get_if<GatherFatigueConditionBusinessKeyV1>(&r.business_key);
    if (!cause || !key || cause->receipt_id.empty() ||
        key->receipt_id != cause->receipt_id || key->actor_id != r.actor_id ||
        s.state.character_condition_extensions.applied_receipts.find(
            r.result_id) != &r)
      fail("GatherConditionActualReceiptRequired", r.result_id);
    const auto original = actual(s, cause->receipt_id);
    if (original.accepted->actor_id != r.actor_id ||
        r.effective_at.kind != ActorFoundationAt::Kind::exact_moment ||
        take(compare_world_moments(r.effective_at.exact_moment,
                                   original.receipt->applied_at)) ||
        r.rule_ref.id != "original_gather_service_exertion" ||
        r.rule_ref.version != 1 ||
        r.rule_ref.catalog_digest != s.catalog.identity.content_digest)
      fail("GatherConditionOriginalCauseMismatch", r.result_id);
    const auto &order = s.common.state().facts.order();
    const auto source_pos =
        std::find(order.begin(), order.end(), cause->receipt_id);
    const auto result_pos = std::find(order.begin(), order.end(), r.result_id);
    if (source_pos == order.end() || result_pos == order.end() ||
        source_pos >= result_pos)
      fail("GatherConditionOriginalSourceOrderMismatch", r.result_id);
    take(validate_condition_application_fact_for_receipt(s.common, s.state,
                                                         s.catalog, r));
    if (r.scalar_changes.size() != 1 ||
        r.scalar_changes[0].field != ConditionScalarChange::Field::fatigue ||
        r.scalar_changes[0].after !=
            fatigue_after(r.scalar_changes[0].before, original.fatigue_delta))
      fail("GatherConditionOriginalFatigueMismatch", r.result_id);
    const ActorFoundationRuleRef empty;
    const auto &effect = r.selected_injury_result;
    if (effect.kind != SelectedInjuryResult::Kind::not_an_injury ||
        !actor_foundation_rule_equal(effect.injury_rule_ref, empty) ||
        !actor_foundation_rule_equal(effect.result_choice_ref, empty) ||
        effect.actual_decision_source_ref || !effect.effect_items.empty() ||
        !r.created_effect_keys.empty() || !r.recovery_resolution_refs.empty() ||
        r.life_transition_ref ||
        !original_condition_capacity_refs_shape(s.catalog, r))
      fail("GatherConditionForeignEffectForbidden", r.result_id);
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Status
validate_gather_condition_coverage_v1(const CharacterConditionSources &s) {
  try {
    if (!s.selected_resource_sites) {
      if (s.gather_condition)
        fail("GatherConditionUnselectedRoot");
      for (const auto &r :
           s.state.character_condition_extensions.applied_receipts.rows())
        if (std::holds_alternative<GatherFatigueConditionCauseV1>(
                r.second.cause))
          fail("GatherConditionExplicitSelectionRequired", r.first);
      return common_success();
    }
    selection(s);
    for (const auto &r :
         s.state.character_condition_extensions.applied_receipts.rows())
      if (std::holds_alternative<GatherFatigueConditionCauseV1>(r.second.cause))
        take(validate_gather_condition_source_v1(s, r.second));
    for (const auto &source : s.gather_condition->gather.receipts) {
      const auto original = actual(s, source.receipt_id);
      const ConditionReceiptV2 *found = nullptr;
      for (const auto &r :
           s.state.character_condition_extensions.applied_receipts.rows()) {
        const auto *cause =
            std::get_if<GatherFatigueConditionCauseV1>(&r.second.cause);
        if (cause && cause->receipt_id == source.receipt_id) {
          if (found || r.second.actor_id != original.accepted->actor_id)
            fail("GatherConditionPrimaryActorCoverageMismatch",
                 source.receipt_id);
          found = &r.second;
        }
      }
      if (!found || source.condition_application_refs.size() != 1)
        fail("GatherConditionOriginalCoverageRequired", source.receipt_id);
      const auto &ref = source.condition_application_refs.front();
      if (ref.domain != "actor_foundation" || ref.kind != "condition_receipt" ||
          ref.id != found->result_id)
        fail("GatherConditionOriginalOutputReferenceMismatch",
             source.receipt_id);
    }
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
} // namespace hanmo::resident

namespace hanmo::resident {
Outcome<BoundActorConditionWrite> apply_gather_period_condition_in_candidate_v1(
    CommonStore &common, AuthoredActorFoundationState &state,
    const CharacterConditionSources &s, const StableId &source_id,
    const GrowthPermanentState &growth,
    GrowthCapabilityRevisionWrite &capability) {
  try {
    if (&s.common != &common || &s.state != &state ||
        s.permanent_growth != &growth || !capability.bound_to(common, growth))
      fail("GatherConditionSameCandidateRequired", source_id);
    bool growth3 = false;
    for (const auto &m : s.catalog.identity.module_versions)
      if (m.first == "growth_permanent_contributions") {
        if (growth3 || m.second != "3")
          fail("GatherConditionCapabilitySelectionRequired", source_id);
        growth3 = true;
      }
    if (!growth3)
      fail("GatherConditionActualCapabilityWriterRequired", source_id);
    const auto source = actual(s, source_id);
    if (take(compare_world_moments(source.receipt->applied_at, s.actual_now)))
      fail("GatherConditionImmediateOriginalWriteRequired", source_id);
    const auto &actor_id = source.accepted->actor_id;
    for (const auto &r :
         state.character_condition_extensions.applied_receipts.rows()) {
      const auto *cause =
          std::get_if<GatherFatigueConditionCauseV1>(&r.second.cause);
      if (cause && cause->receipt_id == source_id)
        fail("GatherConditionAlreadyApplied", source_id);
    }
    auto *head =
        state.character_condition_extensions.actor_condition_heads.find(
            actor_id);
    const auto *actor = common.state().actors.find(actor_id);
    if (!head || !actor || !actor->alive ||
        head->condition_revision == std::numeric_limits<Integer>::max())
      fail("GatherConditionOriginalLivingActorRequired", actor_id);
    const auto capture = common.actor_condition_capture_identity();
    if (!capture)
      fail("GatherConditionActualCaptureRequired", actor_id);
    const auto savepoint = take(common.actor_condition_savepoint_boundary());


    auto before_memory = CheckpointMemoryCharge(0);
    const auto body =
        take(read_character_condition_verified_prefix(s, *head, s.actual_now));
    take(validate_battle_injury_episode_sources_v1(s));
    if (body.body.hp != actor->hp || body.body.fatigue != actor->fatigue ||
        body.body.injury != actor->injury || body.body.alive != actor->alive)
      fail("GatherConditionBeforeBodyMismatch", actor_id);
    const Integer before = actor->fatigue,
                  after = fatigue_after(before, source.fatigue_delta);
    const Integer revision = head->condition_revision;
    const auto identity = common.state().identity;
    const auto offset = common.actor_condition_execution_drafts().size();
    ActorMutation mutation;
    mutation.actor_id = actor_id;
    mutation.fatigue = after;
    take(common.apply_actor(mutation));
    take(common.validate_actor_condition_savepoint_boundary(savepoint));
    const auto &drafts = common.actor_condition_execution_drafts();
    if (capture != common.actor_condition_capture_identity() ||
        drafts.size() != offset + 1)
      fail("GatherConditionSingleActualWriteRequired", actor_id);
    const auto &draft = drafts[offset];
    if (draft.actor_id != actor_id ||
        draft.kind != ActorConditionExecutionDraft::Kind::mutation ||
        draft.sequence != Integer(offset + 1) || !draft.before ||
        !draft.fatigue_written || draft.hp_written || draft.injury_written ||
        draft.alive_written || draft.before->fatigue != before ||
        draft.after.fatigue != after || draft.before->hp != draft.after.hp ||
        draft.before->injury != draft.after.injury ||
        draft.before->alive != draft.after.alive ||
        draft.next_instance_before != draft.next_instance_after ||
        draft.identity.world_id != identity.world_id ||
        draft.identity.execution_epoch != identity.execution_epoch ||
        draft.identity.revision != identity.revision ||
        draft.identity.tick != identity.tick)
      fail("GatherConditionActualDraftMismatch", actor_id);
    ConditionReceiptV2 receipt;
    receipt.result_id = take(common.next_id("condition"));


    auto output_memory = CheckpointMemoryCharge(0);
    receipt.actor_id = actor_id;
    receipt.cause = GatherFatigueConditionCauseV1{source_id};
    receipt.business_key =
        GatherFatigueConditionBusinessKeyV1{source_id, actor_id};
    receipt.rule_ref = {"original_gather_service_exertion", 1,
                        s.catalog.identity.content_digest};
    receipt.effective_at.exact_moment = source.receipt->applied_at;
    receipt.condition_revision_before = revision;
    receipt.condition_revision_after = revision + 1;
    receipt.scalar_changes = {
        {ConditionScalarChange::Field::fatigue, before, after}};
    const auto result_id = receipt.result_id;
    if (!state.character_condition_extensions.applied_receipts.insert(
            result_id, std::move(receipt)))
      fail("GatherConditionReceiptCollision", result_id);
    head->condition_revision = revision + 1;
    head->latest_receipt_ref = ConditionReceiptRefV2{result_id};
    BoundActorConditionWrite bound{{result_id}, capture, draft.sequence};
    take(append_condition_application_fact_in_candidate(common, state,
                                                        s.catalog, bound));
    take(read_actual_actor_condition_write(common, state, bound));
    take(validate_gather_condition_source_v1(
        s, *state.character_condition_extensions.applied_receipts.find(
               result_id)));
    take(capability.record_actual_condition_change(bound));
    // Original Gather owner records this actual Condition SourceRef in its own
    // receipt before publication. It must not supply a caller-authored proof.
    return {true, std::move(bound), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
} // namespace hanmo::resident
