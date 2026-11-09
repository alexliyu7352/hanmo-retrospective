#include "household_market_resolver.hpp"
#include "household_retail_acceptance.hpp"
#include "household_buyer_execution.hpp"
#include <tuple>
namespace hanmo::resident {
namespace {
template <class T>
Outcome<T> fail(const StableId &code, const StableId &id = {}) {
  return {false, {}, common_failure(code, id).error};
}
bool same_time(const WorldMoment &a, const WorldMoment &b) {
  auto c = compare_world_moments(a, b);
  return c.ok && !c.value;
}
bool header(const CommonStore &c, const StableId &id, const StableId &kind) {
  const auto *h = c.state().facts.find(id);
  return h && h->domain == "local_economy" && h->kind == kind;
}
SourceRef accepted_source(const StableId &id) {
  return {"local_economy", "household_purchase_accepted", id};
}
bool same_optional(const std::optional<SourceRef> &a,
                   const std::optional<SourceRef> &b) {
  return bool(a) == bool(b) && (!a || same_source_ref(*a, *b));
}
bool same_buyer(const HouseholdBuyer &a, const HouseholdBuyer &b) {
  if (a.index() != b.index())
    return false;
  if (const auto *left = std::get_if<NamedHouseholdBuyer>(&a)) {
    const auto &right = std::get<NamedHouseholdBuyer>(b);
    return left->actor_id == right.actor_id &&
           same_optional(left->accepted_service_ref,
                         right.accepted_service_ref);
  }
  const auto &left = std::get<CohortHouseholdBuyer>(a);
  const auto &right = std::get<CohortHouseholdBuyer>(b);
  return std::tie(left.cohort_id, left.population_selection_ref,
                  left.labor_allocation_id, left.workers) ==
             std::tie(right.cohort_id, right.population_selection_ref,
                      right.labor_allocation_id, right.workers) &&
         same_source_ref(left.accepted_household_policy_ref,
                         right.accepted_household_policy_ref);
}
bool same_seller(const HouseholdRetailSeller &a,
                 const HouseholdRetailSeller &b) {
  if (a.index() != b.index())
    return false;
  if (const auto *left = std::get_if<PublicHouseholdRetailSeller>(&a))
    return left->market_shop_id ==
           std::get<PublicHouseholdRetailSeller>(b).market_shop_id;
  const auto &left = std::get<PrivateHouseholdRetailSeller>(a);
  const auto &right = std::get<PrivateHouseholdRetailSeller>(b);
  return left.shop_id == right.shop_id &&
         left.operation_id == right.operation_id;
}
} // namespace
Outcome<HouseholdRetailAcceptance>
NativeHouseholdRetailAcceptanceRead::read_household_retail_acceptance(
    const StableId &id) const {
  const auto *purchase = x_.state.purchases.find(id);
  const auto *accepted =
      purchase ? x_.state.accepted_terms.find(purchase->accepted_terms_ref.id)
               : nullptr;
  const auto *household =
      accepted ? x_.economy.households.find(accepted->plan.household_id)
               : nullptr;
  if (!accepted)
    return fail<HouseholdRetailAcceptance>("HouseholdPurchaseAcceptanceMissing",
                                           id);
  auto resolved_service = read_household_market_service_accepted(
      x_.common, x_.catalog, x_.state, x_.private_market_custody, *accepted);
  if (!resolved_service.ok)
    return {false, {}, resolved_service.error};
  const auto *facility = resolved_service.value;
  if (!purchase || !accepted || !household || !facility ||
      !same_source_ref(accepted->source, accepted_source(id)) ||
      !header(x_.common, id, "household_purchase_accepted"))
    return fail<HouseholdRetailAcceptance>("HouseholdPurchaseAcceptanceMissing",
                                           id);
  const auto *policy =
      x_.state.policies.find(accepted->plan.accepted_policy_ref.id);
  if (!policy ||
      !same_source_ref(policy->acceptance_source_ref,
                       accepted->plan.accepted_policy_ref) ||
      policy->revision != accepted->plan.accepted_policy_revision)
    return fail<HouseholdRetailAcceptance>(
        "HouseholdPurchasePolicySourceMissing", id);
  const auto *original = x_.state.plans.find(accepted->plan.plan_id);
  if (!original ||
      !same_buyer(accepted->plan.execution_choice, purchase->executor))
    return fail<HouseholdRetailAcceptance>("HouseholdAcceptedBuyerMismatch",
                                           id);
  if (std::holds_alternative<CohortHouseholdBuyer>(purchase->executor)) {
    if (!x_.population)
      return fail<HouseholdRetailAcceptance>("HouseholdPopulationReadRequired",
                                             id);
    auto buyer = validate_household_buyer_adoption(
        {x_.common, x_.economy, *x_.population, x_.transport_registrations},
        *original, *accepted);
    if (!buyer.ok)
      return {false, {}, buyer.error};
  } else if (!same_buyer(original->execution_choice, purchase->executor))
    return fail<HouseholdRetailAcceptance>("HouseholdAcceptedBuyerMismatch",
                                           id);
  auto lawful =
      x_.controls.validate_accepted(accepted->plan.controller_authority_ref,
                                    accepted->accepted_at.half_day_tick);
  if (!lawful.ok)
    return {false, {}, lawful.error};
  return {true,
          {id,
           household->id,
           household->wallet_id,
           facility->custody_inventory_id,
           accepted->plan.destination_inventory,
           {Owner::Kind::household, household->id, {}},
           purchase->executor,
           accepted->source,
           facility->custody_terms_ref,
           accepted->quoted_total_qian,
           accepted->plan.terms_version,
           accepted->accepted_at},
          {}};
}
Status
NativeHouseholdRetailAcceptanceRead::validate_household_retail_settlement(
    const HouseholdRetailSettlement &r) const {
  auto value = read_household_retail_acceptance(r.terms.purchase_id);
  if (!value.ok)
    return {false, {}, value.error};
  const auto *purchase = x_.state.purchases.find(r.terms.purchase_id);
  const auto *a = x_.state.accepted_terms.find(purchase->accepted_terms_ref.id);
  const auto &v = value.value;
  const auto &t = r.terms;
  if (purchase->retail_receipt_ref != r.id ||
      !same_time(r.settled_at, a->accepted_at) ||
      !same_time(t.quoted_at, a->accepted_at) ||
      !same_seller(a->seller, t.seller_ref) || a->item_id != t.item_id ||
      a->quantity != t.quantity || a->quoted_total_qian != t.total ||
      v.wallet_id != t.buyer_wallet_id ||
      v.custody_inventory_id != t.custody_inventory_id ||
      v.destination_inventory_id != t.destination_inventory_id ||
      v.household_group_id != t.household_group_id ||
      !owner_equal(v.owner, t.buyer) || !same_buyer(v.executor, t.executor) ||
      !same_source_ref(v.accepted_terms_ref, t.accepted_terms_ref) ||
      !same_source_ref(v.custody_terms_ref, t.custody_terms_ref) ||
      v.terms_version != t.terms_version)
    return common_failure("HouseholdPurchaseRetailBacklinkMismatch", r.id);
  return common_success();
}
} // namespace hanmo::resident
