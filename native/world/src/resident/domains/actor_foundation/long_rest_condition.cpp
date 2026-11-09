#include "long_rest_condition.hpp"
#include "authored_origin.hpp"
#include "condition_application_fact.hpp"
#include "condition_capacity_contract.hpp"
#include "resident/domains/growth/growth_internal.hpp"
#include "resident/domains/growth/growth_recovery_progress.hpp"
#include "resident/domains/growth/long_rest_mouth_sources.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
template <class T, class F> Outcome<T> guarded(F fn) {
  try {
    return {true, fn(), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
bool same_identity(const WorldIdentity &a, const WorldIdentity &b) {
  return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch &&
         a.revision == b.revision && a.tick == b.tick;
}
bool same_head(const ActorConditionHead &a, const ActorConditionHead &b) {
  return a.actor_id == b.actor_id &&
         a.condition_revision == b.condition_revision &&
         a.initial_condition_seed_ref.seed_key ==
             b.initial_condition_seed_ref.seed_key &&
         bool(a.latest_receipt_ref) == bool(b.latest_receipt_ref) &&
         (!a.latest_receipt_ref ||
          a.latest_receipt_ref->result_id == b.latest_receipt_ref->result_id);
}
const OriginalLongRestConditionRule &
selected_rule(const CharacterConditionSources &s) {
  const auto &value = s.catalog.original_long_rest_rule;
  if (!value || value->rule_ref.id != "original_estate_long_rest" ||
      value->rule_ref.version != 1 ||
      value->rule_ref.catalog_digest !=
          s.common.state().metadata.content_digest)
    fail("OriginalLongRestConditionRuleRequired");
  return *value;
}
LongRestPeriodSources sources(const CharacterConditionSources &s) {
  if (!s.growth || !s.growth_catalog || !s.shared || !s.elapsed || !s.scale ||
      !s.action_registrations || !s.rest_estates)
    fail("LongRestActualWorldSourcesRequired");
  return {s.common,
          *s.growth,
          *s.growth_catalog,
          *s.shared,
          *s.elapsed,
          *s.scale,
          s.catalog.identity,
          *s.action_registrations,
          *s.rest_estates,
          s.actual_now,
          &s};
}
Integer hp_after(const OriginalLongRestConditionRule &rule,
                 const FrozenBodyScalars &before) {
  if (!before.alive || before.hp < 0 || before.hp > 100 || before.fatigue < 0 ||
      before.fatigue > 100 || before.injury < 0 || before.injury > 3)
    fail("LongRestBodyBeforeInvalid");
  return std::min<Integer>(rule.personal_injury_recovery
                               ? growth_injury_hp_limit(before.injury)
                               : 100,
                           before.hp + 1);
}
bool key(const ConditionReceiptV2 &row, const StableId &action, Integer ordinal,
         const StableId &actor) {
  const auto *k = std::get_if<RestConditionBusinessKey>(&row.business_key);
  return k && k->action_id == action && k->completion_ordinal == ordinal &&
         k->actor_id == actor;
}
void check_food_draft(const ResourceExecutionDraft &draft,
                      const WorldIdentity &identity, const RestFact &rest,
                      const LongRestActualPeriod &period) {
  if (!same_identity(draft.identity, identity) ||
      draft.recipe.operations.size() != (period.consumed_quantity ? 1u : 0u) ||
      draft.operations.size() != draft.recipe.operations.size())
    fail("LongRestOriginalConsumptionWriteRequired");
  if (!period.consumed_quantity)
    return;
  const auto *consumed =
      std::get_if<ConsumeLots>(&draft.recipe.operations.front());
  if (!consumed || consumed->inventory_id != period.inventory_id ||
      consumed->item_id != period.food_item_id ||
      consumed->quantity != period.consumed_quantity ||
      consumed->owners.size() != 1 ||
      consumed->owners.front().kind != Owner::Kind::actor ||
      consumed->owners.front().id != rest.actor_id ||
      !consumed->reservation.id.empty())
    fail("LongRestOriginalConsumptionMismatch");
  Integer quantity = 0;
  for (const auto &flow : draft.operations.front().lot_flows) {
    if (flow.kind != ResourceLotFlowDraft::Kind::consume ||
        flow.source_inventory_id != period.inventory_id ||
        flow.item_id != period.food_item_id || flow.quantity < 1 ||
        flow.quantity > period.consumed_quantity - quantity)
      fail("LongRestOriginalConsumptionFlowMismatch");
    quantity += flow.quantity;
  }
  if (quantity != period.consumed_quantity)
    fail("LongRestOriginalConsumptionFlowMissing");
}
} // namespace
Outcome<GrowthFixedStudyApplication> apply_long_rest_fixed_gate_in_candidate(
    CommonStore &common, GrowthState &growth,
    AuthoredActorFoundationState &foundation,
    const CharacterConditionSources &body, const GrowthCatalog &catalog,
    const CommonCatalog &shared, const GrowthAccess &access,
    const EstateState &estates, const ActionTimeRegistration &registration,
    const GrowthFixedStepPermit &gate) {
  return guarded<GrowthFixedStudyApplication>([&] {
    if (&body.common != &common || &body.state != &foundation ||
        body.growth != &growth || body.growth_catalog != &catalog ||
        body.shared != &shared || !catalog.complete_exact_period_sources ||
        !body.rest_estates || registration.owner_domain != "growth" ||
        registration.action_kind != "long_rest")
      fail("LongRestSameCandidateRequired", gate.action_id());
    const auto &rule = selected_rule(body);
    if (rule.personal_injury_recovery != bool(access.personal_recovery))
      fail("LongRestSelectedRecoveryModuleMismatch", gate.action_id());
    if (take(compare_world_moments(gate.at(), body.actual_now)))
      fail("LongRestActualClockMismatch", gate.action_id());
    const auto *active = common.action(gate.action_id());
    if (!active || active->kind != "long_rest")
      fail("LongRestOriginalActionRequired", gate.action_id());
    const auto action = *active;
    take(validate_growth_fixed_step_permit(gate, common, growth, action));
    auto *raw = growth.facts.find(action.id);
    auto *rest = raw ? std::get_if<RestFact>(raw) : nullptr;
    if (!rest || rest->elapsed + 1 != gate.ordinal() ||
        rest->actual_periods.size() != static_cast<std::size_t>(rest->elapsed))
      fail("LongRestPeriodAlreadyConsumed", action.id);
    const auto capture = common.actor_condition_capture_identity();
    const auto resources = common.resource_execution_capture_identity();
    if (!capture || !resources)
      fail("LongRestCandidateCapturesRequired", action.id);
    auto savepoint = take(common.actor_condition_savepoint_boundary());
    const auto identity = common.state().identity;
    const auto draft_offset = common.actor_condition_execution_drafts().size();
    const auto resource_offset = common.resource_execution_drafts().size();
    const auto consumed_before = rest->consumed;
    LongRestActualPeriod period;
    period.ordinal = gate.ordinal();
    period.provided_from = gate.provided_from();
    period.applied_at = gate.at();
    period.estate_basis = take(capture_estate_rest_adoption(
        *body.rest_estates, estates, rest->estate_id));
    if (take(compare_world_moments(period.estate_basis.adopted_at, gate.at())))
      fail("LongRestEstateAdoptionClockMismatch", action.id);
    auto estate = take(
        read_estate_rest_at_adoption(*body.rest_estates, period.estate_basis));
    auto used =
        access.rest_estate ? access.rest_estate(rest->estate_id) : std::nullopt;
    if (!used)
      fail("LongRestOriginalEstateRequired", action.id);
    take(validate_estate_rest_adopted_view(*body.rest_estates,
                                           period.estate_basis, *used));
    period.inventory_id = used->inventory_id;
    period.food_item_id = rest->food_item_id;
    period.mouth_actor_ids =
        take(read_growth_rest_mouths(common, access, action.actor_id, *used));
    if (rest->ration_per_person < 0 ||
        period.mouth_actor_ids.size() >
            static_cast<std::size_t>(std::numeric_limits<Integer>::max()) ||
        (rest->ration_per_person &&
         period.mouth_actor_ids.size() >
             static_cast<std::size_t>(std::numeric_limits<Integer>::max() /
                                      rest->ration_per_person)))
      fail("LongRestQuantityOverflow", action.id);
    period.consumed_quantity =
        static_cast<Integer>(period.mouth_actor_ids.size()) *
        rest->ration_per_person;
    if (consumed_before < 0 ||
        period.consumed_quantity >
            std::numeric_limits<Integer>::max() - consumed_before)
      fail("LongRestConsumptionOverflow", action.id);
    for (const auto &span : gate.provided().intervals) {
      const StableId *actual = nullptr;
      for (const auto &ref : span.input_basis_refs)
        if (const auto *applied = std::get_if<FixedAppliedInput>(&ref);
            applied && applied->source_ref.domain == "growth" &&
            applied->source_ref.kind == "growth_fixed_service_boundary") {
          if (actual || applied->source_schema != 1)
            fail("LongRestActualInputMissing", action.id);
          actual = &applied->source_ref.id;
        }
      if (!actual)
        fail("LongRestActualInputMissing", action.id);
      if (period.input_boundary_refs.empty() ||
          period.input_boundary_refs.back() != *actual)
        period.input_boundary_refs.push_back(*actual);
    }
    if (catalog.complete_exact_rest_mouth_sources)
      period.mouth_inputs =
          take(capture_long_rest_mouth_inputs(body, *rest, period, *used));
    struct Before {
      StableId actor;
      ActorConditionHead head;
      FrozenBodyScalars body;
    };
    std::vector<Before> before;
    before.reserve(period.mouth_actor_ids.size());
    for (const auto &person : period.mouth_actor_ids) {
      auto actual = take(read_current_character_condition(body, person));
      for (const auto &row :
           foundation.character_condition_extensions.applied_receipts.rows())
        if (key(row.second, action.id, gate.ordinal(), person))
          fail("LongRestConditionAlreadyApplied", action.id);
      if (actual.head.condition_revision == std::numeric_limits<Integer>::max())
        fail("LongRestConditionRevisionExhausted", person);
      before.push_back(
          {person, std::move(actual.head), std::move(actual.body)});
    }
    // This is the original food/scalar/action algorithm, not a second reducer.
    GrowthFixedStudyApplication result;
    result.result = take(
        growth::step_rest_at_fixed_gate(common, growth, access, action, gate));
    take(common.validate_actor_condition_savepoint_boundary(savepoint));
    if (capture != common.actor_condition_capture_identity() ||
        resources != common.resource_execution_capture_identity() ||
        !same_identity(identity, common.state().identity))
      fail("LongRestCandidateCaptureExpired", action.id);
    if (!result.result.effect.success) {
      if (common.actor_condition_execution_drafts().size() != draft_offset ||
          common.resource_execution_drafts().size() != resource_offset ||
          rest->elapsed + 1 != gate.ordinal() ||
          rest->consumed != consumed_before)
        fail("LongRestFailedGateChangedResources", action.id);
      return result;
    }
    if (rest->elapsed != gate.ordinal() ||
        rest->last_mouths != period.mouth_actor_ids ||
        rest->consumed - consumed_before != period.consumed_quantity ||
        common.resource_execution_drafts().size() != resource_offset + 1)
      fail("LongRestOriginalPeriodNotApplied", action.id);
    check_food_draft(common.resource_execution_drafts()[resource_offset],
                     identity, *rest, period);
    const auto &drafts = common.actor_condition_execution_drafts();
    if (drafts.size() != draft_offset + before.size())
      fail("LongRestActualBodyWriteCountMismatch", action.id);
    for (std::size_t i = 0; i < before.size(); ++i) {
      const auto &saved = before[i];
      const auto &draft = drafts[draft_offset + i];
      auto *head =
          foundation.character_condition_extensions.actor_condition_heads.find(
              saved.actor);
      const auto *actor = common.actor(saved.actor);
      if (!head || !actor || !same_head(*head, saved.head) ||
          draft.kind != ActorConditionExecutionDraft::Kind::mutation ||
          draft.actor_id != saved.actor ||
          !same_identity(draft.identity, identity) ||
          draft.sequence != static_cast<Integer>(draft_offset + i + 1) ||
          !draft.before || draft.before->alive != saved.body.alive ||
          draft.before->hp != saved.body.hp ||
          draft.before->fatigue != saved.body.fatigue ||
          draft.before->injury != saved.body.injury || !draft.hp_written ||
          !draft.fatigue_written || draft.injury_written ||
          draft.alive_written || draft.after.alive != saved.body.alive ||
          draft.after.injury != saved.body.injury ||
          draft.after.hp != hp_after(rule, saved.body) ||
          draft.after.fatigue != std::max<Integer>(0, saved.body.fatigue - 4) ||
          actor->hp != draft.after.hp ||
          actor->fatigue != draft.after.fatigue ||
          draft.next_instance_before != draft.next_instance_after)
        fail("LongRestActualBodyWriteMismatch", saved.actor);
      ConditionReceiptV2 receipt;
      receipt.result_id = take(common.next_id("condition"));
      receipt.actor_id = saved.actor;
      receipt.business_key =
          RestConditionBusinessKey{action.id, saved.actor, gate.ordinal()};
      receipt.cause = OriginalRestProvidedCause{
          action.id, "long_rest", rest->actor_id, used->place_id, 1, action.id};
      receipt.rule_ref = rule.rule_ref;
      receipt.effective_at.exact_moment = gate.at();
      receipt.condition_revision_before = head->condition_revision;
      receipt.condition_revision_after = head->condition_revision + 1;
      receipt.scalar_changes = {
          {ConditionScalarChange::Field::hp, saved.body.hp, draft.after.hp},
          {ConditionScalarChange::Field::fatigue, saved.body.fatigue,
           draft.after.fatigue}};
      const ConditionReceiptRefV2 ref{receipt.result_id};
      if (!foundation.character_condition_extensions.applied_receipts.insert(
              ref.result_id, std::move(receipt)))
        fail("DuplicateConditionReceipt", ref.result_id);
      head->condition_revision += 1;
      head->latest_receipt_ref = ref;
      BoundActorConditionWrite actual{ref, capture, draft.sequence};
      take(append_condition_application_fact_in_candidate(
          common, foundation, body.catalog, actual));
      period.condition_receipt_refs.push_back(ref);
      result.actual_condition_writes.push_back(std::move(actual));
    }
    rest->actual_periods.push_back(std::move(period));
    return result;
  });
}
Status
validate_long_rest_condition_receipt(const CharacterConditionSources &body,
                                     const ConditionReceiptV2 &receipt,
                                     const FrozenBodyScalars &before) {
  return guarded<Done>([&] {
    const auto &rule = selected_rule(body);
    const auto *cause = std::get_if<OriginalRestProvidedCause>(&receipt.cause);
    const auto *business =
        std::get_if<RestConditionBusinessKey>(&receipt.business_key);
    if (!cause || !business || cause->rest_kind != "long_rest" ||
        business->actor_id != receipt.actor_id ||
        business->action_id != cause->action_id ||
        !actor_foundation_rule_equal(receipt.rule_ref, rule.rule_ref) ||
        receipt.effective_at.kind != ActorFoundationAt::Kind::exact_moment ||
        receipt.scalar_changes.size() != 2 ||
        receipt.selected_injury_result.kind !=
            SelectedInjuryResult::Kind::not_an_injury ||
        !receipt.created_effect_keys.empty() ||
        !receipt.recovery_resolution_refs.empty() ||
        receipt.life_transition_ref ||
        !original_condition_capacity_refs_shape(body.catalog, receipt))
      fail("LongRestConditionReceiptMismatch", receipt.result_id);
    auto original = take(read_long_rest_actual_period(
        sources(body), cause->action_id, business->completion_ordinal));
    const auto &period = *original.period;
    if (cause->provider_actor_id != original.rest->actor_id ||
        cause->site_id != original.estate.view.place_id ||
        cause->provided_half_days != 1 ||
        cause->service_fulfillment_ref != cause->action_id ||
        take(compare_world_moments(period.applied_at,
                                   receipt.effective_at.exact_moment)))
      fail("LongRestConditionSourceMismatch", receipt.result_id);
    std::size_t count = 0;
    for (std::size_t n = 0; n < period.mouth_actor_ids.size(); ++n)
      if (period.mouth_actor_ids[n] == receipt.actor_id &&
          period.condition_receipt_refs[n].result_id == receipt.result_id)
        ++count;
    if (count != 1)
      fail("LongRestConditionMouthMismatch", receipt.result_id);
    const auto &hp = receipt.scalar_changes[0];
    const auto &fatigue = receipt.scalar_changes[1];
    if (hp.field != ConditionScalarChange::Field::hp ||
        fatigue.field != ConditionScalarChange::Field::fatigue ||
        hp.before != before.hp || hp.after != hp_after(rule, before) ||
        fatigue.before != before.fatigue ||
        fatigue.after != std::max<Integer>(0, before.fatigue - 4))
      fail("LongRestConditionFormulaMismatch", receipt.result_id);
    return Done{};
  });
}
Status
validate_long_rest_condition_coverage(const CharacterConditionSources &body) {
  return guarded<Done>([&] {
    const auto actual = sources(body);
    take(validate_long_rest_actual_periods(actual));
    for (const auto &row : actual.growth.facts.rows())
      if (const auto *rest = std::get_if<RestFact>(&row.second))
        for (const auto &period : rest->actual_periods)
          for (std::size_t n = 0; n < period.mouth_actor_ids.size(); ++n) {
            const auto *receipt =
                body.state.character_condition_extensions.applied_receipts.find(
                    period.condition_receipt_refs[n].result_id);
            if (!receipt ||
                !key(*receipt, row.first, period.ordinal,
                     period.mouth_actor_ids[n]) ||
                !std::holds_alternative<OriginalRestProvidedCause>(
                    receipt->cause) ||
                std::get<OriginalRestProvidedCause>(receipt->cause).rest_kind !=
                    "long_rest")
              fail("LongRestConditionReverseCoverageMismatch", row.first);
          }
    return Done{};
  });
}
} // namespace hanmo::resident
