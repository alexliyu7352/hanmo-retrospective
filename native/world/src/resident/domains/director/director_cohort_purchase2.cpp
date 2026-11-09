#include "director_cohort_purchase2.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/domains/local_economy/household_buyer_execution.hpp"
#include "resident/domains/local_economy/population_selection_source.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {
namespace {
template <class T> Outcome<T> fail(const char *code, const StableId &id = {}) {
  return {false, {}, common_failure(code, id).error};
}
bool same(const SourceRef &a, const SourceRef &b) {
  return a.domain == b.domain && a.kind == b.kind && a.id == b.id;
}
bool same(const SubjectRef &a, const SubjectRef &b) {
  return a.kind == b.kind && a.id == b.id;
}
bool closed(HouseholdPurchasePhase phase) {
  return phase == HouseholdPurchasePhase::received ||
         phase == HouseholdPurchasePhase::cancelled ||
         phase == HouseholdPurchasePhase::lost;
}
Outcome<const HouseholdPurchasePlan *>
plan_for(const HouseholdPurchaseContext &x, const SubjectRef &subject,
         const SourceRef &ref) {
  if (subject.kind != SubjectKind::cohort || ref.domain != "local_economy" ||
      ref.kind != "household_purchase_planned")
    return fail<const HouseholdPurchasePlan *>(
        "DirectorCohortPurchasePlanRequired", ref.id);
  const auto *p = x.state.plans.find(ref.id);
  const auto *h = x.common.state().facts.find(ref.id);
  const auto *buyer =
      p ? std::get_if<CohortHouseholdBuyer>(&p->execution_choice) : nullptr;
  if (!p || !h || h->domain != ref.domain || h->kind != ref.kind || !buyer ||
      buyer->cohort_id != subject.id || buyer->workers != 1)
    return fail<const HouseholdPurchasePlan *>(
        "DirectorCohortPurchasePlanMismatch", ref.id);
  auto selected = population_selection_source_ref(
      x.economy, x.population, buyer->population_selection_ref);
  if (!selected.ok)
    return {false, {}, selected.error};
  auto original = read_population_selection_source(x.economy, x.population,
                                                   selected.value, 1);
  if (!original.ok)
    return {false, {}, original.error};
  if (original.value->owner_business_ref.id != p->plan_id ||
      original.value->purpose !=
          PopulationSelectionPurpose::household_purchase ||
      original.value->selected_persons != 1 ||
      original.value->selected_labor_eligible != 1)
    return fail<const HouseholdPurchasePlan *>(
        "DirectorCohortSelectionMismatch", ref.id);
  return {true, p, {}};
}
} // namespace
Status
CohortPurchaseDirectorRoute2::validate_effect(const SubjectRef &subject,
                                              const SourceRef &source) const {
  if (source.domain != "local_economy" ||
      source.kind != "household_purchase_accepted")
    return common_failure("DirectorCohortAppliedSourceRequired", source.id);
  auto valid = validate_household_purchases(purchase_);
  if (!valid.ok)
    return valid;
  const auto *accepted = purchase_.state.accepted_terms.find(source.id);
  const auto *actual = purchase_.state.purchases.find(source.id);
  if (!accepted || !actual || !same(accepted->source, source) ||
      !same(actual->accepted_terms_ref, source) ||
      actual->plan_ref != accepted->plan.plan_id)
    return common_failure("DirectorCohortAppliedSourceMismatch", source.id);
  auto plan = plan_for(
      purchase_, subject,
      {"local_economy", "household_purchase_planned", actual->plan_ref});
  if (!plan.ok)
    return {false, {}, plan.error};
  return validate_household_buyer_adoption({purchase_.common, purchase_.economy,
                                            purchase_.population,
                                            purchase_.transport_registrations},
                                           *plan.value, *accepted);
}
Status CohortPurchaseDirectorRoute2::validate_binding(
    const SubjectPlanBinding &binding) const {
  const auto *b = std::get_if<CohortPurchaseBinding>(&binding.binding);
  if (!b || binding.registration_epoch < 1 || binding.plan_id.empty())
    return common_failure("DirectorCohortBindingRequired", binding.plan_id);
  auto plan = plan_for(purchase_, binding.subject, b->purchase_plan_ref);
  if (!plan.ok)
    return {false, {}, plan.error};
  if (!same(binding.policy_source_ref, plan.value->accepted_policy_ref))
    return common_failure("DirectorCohortPolicyBindingMismatch",
                          binding.plan_id);
  if (b->execution_ref) {
    auto valid = validate_effect(binding.subject, *b->execution_ref);
    if (!valid.ok)
      return valid;
    const auto *purchase = purchase_.state.purchases.find(b->execution_ref->id);
    if (!purchase || purchase->plan_ref != plan.value->plan_id)
      return common_failure("DirectorCohortExecutionBindingMismatch",
                            binding.plan_id);
  }
  return common_success();
}
Outcome<std::optional<SubjectDecisionProposal2>>
CohortPurchaseDirectorRoute2::propose(const DirectorRegistration2 &registration,
                                      const CohortKnownSubjectView &known,
                                      Integer candidates) const {
  using Result = Outcome<std::optional<SubjectDecisionProposal2>>;
  const auto *admission =
      std::get_if<CohortDirectorAdmission>(&registration.admission_source);
  const auto *schedule =
      director_.schedules.find(director_subject_key2(registration.subject));
  if (registration.subject.kind != SubjectKind::cohort ||
      !same(registration.subject, known.subject) || !admission ||
      !same(admission->accepted_household_policy_ref,
            known.accepted_policy_ref) ||
      !schedule || candidates < 1 || candidates > 256 ||
      schedule->decision_seq == std::numeric_limits<Integer>::max())
    return fail<std::optional<SubjectDecisionProposal2>>(
        "DirectorCohortInputMismatch", registration.subject.id);
  const auto *population =
      purchase_.population.source_revisions.find(registration.subject.id);
  if (!population || population->revision != known.source_revision)
    return fail<std::optional<SubjectDecisionProposal2>>(
        "StaleDirectorPopulationRevision", registration.subject.id);
  // Revocation closes new optional spending. Existing accepted business remains
  // with S5.
  if (registration.obligations_only)
    return {true, {}, {}};
  if (admission->source_revision != known.source_revision)
    return fail<std::optional<SubjectDecisionProposal2>>(
        "StaleDirectorAdmissionRevision", registration.subject.id);
  // Responsibility and knowledge stay with the original accepted Plan cohort.
  // A real materialization may give the one worker another execution cohort.
  for (const auto &row : purchase_.state.purchases.rows()) {
    const auto *plan = purchase_.state.plans.find(row.second.plan_ref);
    const auto *b =
        plan ? std::get_if<CohortHouseholdBuyer>(&plan->execution_choice)
             : nullptr;
    if (b && b->cohort_id == registration.subject.id &&
        !closed(row.second.state))
      return {true, {}, {}};
  }
  auto whole =
      compare_fractions(purchase_.now.sub_half_day, CanonicalFraction{});
  if (!whole.ok)
    return {false, {}, whole.error};
  if (whole.value)
    return {true, {}, {}};
  Integer considered = 0;
  std::optional<SubjectDecisionProposal2> result;
  auto attempt = [&](const HouseholdPurchasePlan &plan,
                     const HouseholdRetailSeller &seller) -> Status {
    if (result || considered >= candidates)
      return common_success();
    ++considered;
    auto quote = preview_household_reserve_purchase(purchase_, knowledge_,
                                                    plan.plan_id, seller);
    if (!quote.ok) {
      if (quote.error.kind == ErrorKind::rule)
        return common_success();
      return {false, {}, quote.error};
    }
    if (!quote.value)
      return common_success();
    if (!household_market_knowledge_belongs_to_plan(quote.value->known_market,
                                                    plan))
      return common_failure("DirectorCohortKnowledgeReceiverMismatch",
                            plan.plan_id);
    SubjectDecisionProposal2 p;
    p.subject = registration.subject;
    p.registration_epoch = registration.epoch;
    p.decision_seq = schedule->decision_seq + 1;
    p.policy_id = registration.policy_id;
    p.policy_source_ref = admission->accepted_household_policy_ref;
    p.owner_source_revision = known.source_revision;
    p.plan_id = "subject2-plan:" + director_subject_key2(p.subject) + ":" +
                std::to_string(p.registration_epoch);
    p.route = DirectorDecisionRoute2::cohort_purchase;
    p.template_id = "economic_household/1";
    p.goal_ref = plan.plan_id;
    p.binding = CohortPurchaseBinding{
        {"local_economy", "household_purchase_planned", plan.plan_id}, {}};
    p.next_step_index = 4;
    p.phase = PlanPhase::purchasing;
    p.commitment_ids = {plan.plan_id};
    p.known_report_ids = {
        household_market_known_report_id(quote.value->known_market)};
    p.expected_versions = quote.value->demand.versions;
    p.expected_versions.insert(p.expected_versions.end(),
                               quote.value->quote.retail.versions.begin(),
                               quote.value->quote.retail.versions.end());
    p.expected_versions.insert(p.expected_versions.end(),
                               quote.value->quote.presence.versions.begin(),
                               quote.value->quote.presence.versions.end());
    p.cohort_purchase_quote =
        checkpoint_make_shared<const HouseholdReservePurchaseProposal>(
            std::move(*quote.value));
    result = std::move(p);
    return common_success();
  };
  for (const auto &row : purchase_.state.plans.rows()) {
    const auto &plan = row.second;
    const auto *buyer =
        std::get_if<CohortHouseholdBuyer>(&plan.execution_choice);
    if (!buyer || buyer->cohort_id != registration.subject.id ||
        !same(plan.accepted_policy_ref,
              admission->accepted_household_policy_ref))
      continue;
    auto actual_plan =
        plan_for(purchase_, registration.subject,
                 {"local_economy", "household_purchase_planned", row.first});
    if (!actual_plan.ok)
      return {false, {}, actual_plan.error};
    bool used = false;
    for (const auto &purchase : purchase_.state.purchases.rows())
      if (purchase.second.plan_ref == row.first)
        used = true;
    if (used)
      continue;
    if (std::find(purchase_.retail.catalog.public_market_shop_ids.begin(),
                  purchase_.retail.catalog.public_market_shop_ids.end(),
                  plan.known_market_ref) !=
        purchase_.retail.catalog.public_market_shop_ids.end()) {
      auto valid =
          attempt(plan, PublicHouseholdRetailSeller{plan.known_market_ref});
      if (!valid.ok)
        return {false, {}, valid.error};
    }
    for (const auto &operation : purchase_.retail.shops.operations.rows()) {
      if (result || considered >= candidates)
        break;
      const auto *shop =
          purchase_.retail.shops.shops.find(operation.second.shop_id);
      if (!shop || shop->terms.market_shop_id != plan.known_market_ref)
        continue;
      auto valid = attempt(
          plan, PrivateHouseholdRetailSeller{shop->id, operation.first});
      if (!valid.ok)
        return {false, {}, valid.error};
    }
    if (result || considered >= candidates)
      break;
  }
  return Result{true, std::move(result), {}};
}
Outcome<SubjectDecisionApplied2> apply_director_cohort_purchase_in_candidate(
    CommonStore &common, LocalEconomyState &economy,
    HouseholdPurchaseState &state, FacilityClaimsState &claims,
    ActionElapsedStore &elapsed, HouseholdRetailState &retail,
    const HouseholdPurchaseContext &context,
    const HouseholdPurchaseKnownMarketRead &knowledge,
    const SubjectDecisionProposal2 &p) {
  if (p.route != DirectorDecisionRoute2::cohort_purchase ||
      p.subject.kind != SubjectKind::cohort || !p.cohort_purchase_quote ||
      p.actor_proposal || !p.binding)
    return fail<SubjectDecisionApplied2>(
        "DirectorCohortPurchaseProposalRequired", p.plan_id);
  const auto *binding = std::get_if<CohortPurchaseBinding>(&*p.binding);
  if (!binding || binding->execution_ref ||
      binding->purchase_plan_ref.id != p.cohort_purchase_quote->quote.plan_id)
    return fail<SubjectDecisionApplied2>(
        "DirectorCohortPurchaseProposalMismatch", p.plan_id);
  const auto *population =
      context.population.source_revisions.find(p.subject.id);
  if (!population || population->revision != p.owner_source_revision)
    return fail<SubjectDecisionApplied2>("StaleDirectorPopulationRevision",
                                         p.subject.id);
  auto plan = plan_for(context, p.subject, binding->purchase_plan_ref);
  if (!plan.ok)
    return {false, {}, plan.error};
  if (!same(plan.value->accepted_policy_ref, p.policy_source_ref) ||
      !household_market_knowledge_belongs_to_plan(
          p.cohort_purchase_quote->known_market, *plan.value))
    return fail<SubjectDecisionApplied2>("DirectorCohortPurchasePolicyMismatch",
                                         p.plan_id);
  auto accepted = accept_household_reserve_purchase_in_candidate(
      common, economy, state, claims, elapsed, retail, context, knowledge,
      *p.cohort_purchase_quote);
  if (!accepted.ok)
    return {false, {}, accepted.error};
  const auto *purchase = state.purchases.find(accepted.value);
  const auto *terms = state.accepted_terms.find(accepted.value);
  const auto *buyer =
      purchase ? std::get_if<CohortHouseholdBuyer>(&purchase->executor)
               : nullptr;
  if (!purchase || !terms || !buyer ||
      purchase->plan_ref != plan.value->plan_id ||
      !std::holds_alternative<OwnerPeriodWorkBinding>(
          purchase->execution_binding) ||
      purchase->retail_receipt_ref.empty())
    return fail<SubjectDecisionApplied2>("DirectorCohortActualPurchaseMissing",
                                         accepted.value);
  auto actual_buyer = validate_household_buyer_adoption(
      {common, economy, context.population, context.transport_registrations},
      *plan.value, *terms);
  if (!actual_buyer.ok)
    return {false, {}, actual_buyer.error};
  auto actual_purchase = validate_household_purchases(context);
  if (!actual_purchase.ok)
    return {false, {}, actual_purchase.error};
  SubjectDecisionApplied2 result;
  result.outcome.kind = DecisionOutcome::Kind::adopted;
  result.outcome.code = "HouseholdPurchaseAccepted";
  result.outcome.receipt_ids = {accepted.value, purchase->retail_receipt_ref};
  result.applied_effect_ref = terms->source;
  result.actual_binding =
      CohortPurchaseBinding{binding->purchase_plan_ref, terms->source};
  return {true, std::move(result), {}};
}
} // namespace hanmo::resident
