// 已批准首批市场关账：本文件具名 plan 仅停内存计量，原数量、来源、复制及租约流程不变。
// 买方知情只消费真实原 receipt 或当前显式 /3 计划告知；不复制全户知识。
#include "household_purchase_knowledge.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/domains/interaction/household_purchase_report.hpp"
namespace hanmo::resident {
Outcome<HouseholdKnownMarketEvidence>
NativeHouseholdPurchaseKnownMarketRead::read_known_household_market(
    const HouseholdPurchasePlan &plan, const StableId &market,
    const StableId &inventory, const StableId &place_id, const StableId &item,
    const WorldMoment &now) const {
  const auto *place = catalog_.places.find(place_id);
  if (!place || place->settlement_id.empty() || market != plan.known_market_ref)
    return {
        false, {}, common_failure("HouseholdKnownMarketScopeMismatch").error};
  if (selection_ != HouseholdMarketKnowledgeSelection::original_subject &&
      selection_ != HouseholdMarketKnowledgeSelection::accepted_plan_scope)
    return {false,
            {},
            common_failure("UnsupportedHouseholdKnowledgeSelection").error};
  if (const auto *buyer =
          std::get_if<CohortHouseholdBuyer>(&plan.execution_choice);
      buyer &&
      selection_ == HouseholdMarketKnowledgeSelection::accepted_plan_scope) {
    if (!plan_reports_ || &plan_reports_->common != &common_ ||
        &plan_reports_->disclosure.social != &social_ ||
        &plan_reports_->disclosure.results != &sources_ ||
        plan_reports_->purchase.state.plans.find(plan.plan_id) != &plan)
      return {
          false,
          {},
          common_failure("HouseholdPlanScopedKnowledgeOwnerRequired").error};


    auto plan_memory = CheckpointMemoryCharge(0);
    auto known = read_known_household_purchase_report(
        *plan_reports_,
        {"local_economy", "household_purchase_planned", plan.plan_id},
        inventory, place->settlement_id, item, now);
    if (!known.ok)
      return {false, {}, known.error};
    if (!known.value)
      return {false, {}, common_failure("HouseholdPlanMarketNotKnown").error};
    const auto &v = *known.value;
    const auto *h = common_.state().facts.find(v.report->fact_id);
    if (!h || v.delivery->cohort_id != buyer->cohort_id)
      return {false,
              {},
              common_failure("HouseholdPlanKnowledgeSourceMismatch").error};
    HouseholdKnownMarketEvidence out;
    out.selection = selection_;


    (out.memory).grow(0);
    out.report_ref = {"social", "report", v.report->id};
    out.observation_source = {h->domain, h->kind, h->id};
    out.knowledge = HouseholdPurchaseScopedKnowledgeRef{
        {{"interaction", household_purchase_report_source_kind, v.delivery->id},
         1},
        v.delivery->purchase_plan_ref,
        v.delivery->cohort_id,
        v.delivery->report_id,
        v.delivery->disclosed_at};
    auto original = sources_.fact_v2_borrowed(v.report->fact_id, stamp_);
    if (!original.ok)
      return {false, {}, original.error};
    if (!original.value)
      return {
          false, {}, common_failure("HouseholdPlanObservationMissing").error};
    out.observed_at = original.value->occurred_at;
    return {true, std::move(out), {}};
  }

  if (const auto *named =
          std::get_if<NamedHouseholdBuyer>(&plan.execution_choice))
    (void)0;
  else
    (void)0;
  auto receiver_memory = CheckpointMemoryCharge(0);
  SubjectRef receiver;
  if (const auto *named =
          std::get_if<NamedHouseholdBuyer>(&plan.execution_choice))
    receiver = {SubjectKind::actor, named->actor_id};
  else
    receiver = {
        SubjectKind::cohort,
        std::get<CohortHouseholdBuyer>(plan.execution_choice).cohort_id};
  auto known = read_known_supply_observation2(common_, social_, sources_,
                                              stamp_, receiver, inventory,
                                              place->settlement_id, item, now);
  if (!known.ok)
    return {false, {}, known.error};
  if (!known.value || !known.value->report || !known.value->observation)
    return {false, {}, common_failure("HouseholdMarketNotKnown").error};
  const auto *original =
      common_.state().facts.find(known.value->report->fact_id);
  if (!original)
    return {false,
            {},
            common_failure("HouseholdKnownObservationSourceMissing").error};
  HouseholdKnownMarketEvidence result;
  result.selection = selection_;
  result.memory = std::move(known.value->memory);


  (result.memory).grow(0);
  result.report_ref = {"social", "report", known.value->report->id};
  result.observation_source = {original->domain, original->kind, original->id};
  result.knowledge = std::move(known.value->receipt);
  result.observed_at = std::move(known.value->observed_at);
  return {true, std::move(result), {}};
}
} // namespace hanmo::resident
