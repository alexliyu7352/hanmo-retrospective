#include "household_market_resolver.hpp"
#include "household_purchase_contract.hpp"
#include "household_buyer_execution.hpp"
#include "household_mobile_custody.hpp"
#include "household_purchase_cargo.hpp"
#include "named_household_service.hpp"
#include "cohort_household_service.hpp"
#include "household_purchase_demand.hpp"
#include "household_retail_acceptance.hpp"
#include "local_economy_internal.hpp"
#include <set>
#include <map>
#include <limits>
#include "hanmo/resident_world_api.hpp"
#include "resident/storage/selected_resource_execution.hpp"
#include <tuple>
namespace hanmo::resident {
namespace {
template <class T>
Outcome<T> fail(const StableId &code, const StableId &id = {}) {
  return {false, {}, common_failure(code, id).error};
}
bool same_time(const WorldMoment &a, const WorldMoment &b) {
  auto r = compare_world_moments(a, b);
  return r.ok && !r.value;
}
bool same_span(const MomentSpan &a, const MomentSpan &b) {
  return same_time(a.from, b.from) && same_time(a.to, b.to);
}
bool identity(const WorldIdentity &a, const WorldIdentity &b) {
  return std::tie(a.world_id, a.execution_epoch, a.revision, a.tick) ==
         std::tie(b.world_id, b.execution_epoch, b.revision, b.tick);
}
bool has(const std::vector<StableId> &a, const StableId &b) {
  return std::find(a.begin(), a.end(), b) != a.end();
}
bool same_control(const HouseholdProvisionControlProof &a,
                  const HouseholdProvisionControlProof &b) {
  return std::tie(a.actor_id, a.household_group_id, a.fact_id, a.source_role_id,
                  a.operation, a.source_revision, a.effective_from_tick,
                  a.through_tick) ==
         std::tie(b.actor_id, b.household_group_id, b.fact_id, b.source_role_id,
                  b.operation, b.source_revision, b.effective_from_tick,
                  b.through_tick);
}
bool same_buyer(const HouseholdBuyer &a, const HouseholdBuyer &b) {
  if (a.index() != b.index())
    return false;
  if (const auto *n = std::get_if<NamedHouseholdBuyer>(&a)) {
    const auto &m = std::get<NamedHouseholdBuyer>(b);
    return n->actor_id == m.actor_id &&
           bool(n->accepted_service_ref) == bool(m.accepted_service_ref) &&
           (!n->accepted_service_ref ||
            same_source_ref(*n->accepted_service_ref, *m.accepted_service_ref));
  }
  const auto &n = std::get<CohortHouseholdBuyer>(a);
  const auto &m = std::get<CohortHouseholdBuyer>(b);
  return std::tie(n.cohort_id, n.population_selection_ref,
                  n.labor_allocation_id, n.workers) ==
             std::tie(m.cohort_id, m.population_selection_ref,
                      m.labor_allocation_id, m.workers) &&
         same_source_ref(n.accepted_household_policy_ref,
                         m.accepted_household_policy_ref);
}
bool same_seller(const HouseholdRetailSeller &a,
                 const HouseholdRetailSeller &b) {
  if (a.index() != b.index())
    return false;
  if (const auto *n = std::get_if<PublicHouseholdRetailSeller>(&a))
    return n->market_shop_id ==
           std::get<PublicHouseholdRetailSeller>(b).market_shop_id;
  const auto &n = std::get<PrivateHouseholdRetailSeller>(a);
  const auto &m = std::get<PrivateHouseholdRetailSeller>(b);
  return n.shop_id == m.shop_id && n.operation_id == m.operation_id;
}
// 原计数器仅分配一次；不让裸 string 在返回后失去自己的租约。
Outcome<ProducedStableId> owned_fact(CommonStore &c, std::string_view prefix,
                                     std::string_view kind) {
  auto id = c.next_owned_id(prefix);
  if (!id.ok)
    return id;
  auto stored = c.append_fact_borrowed(id.value.id(), kind, "local_economy");
  if (!stored.ok)
    return {false, {}, stored.error};
  return id;
}
Outcome<StableId> fact(CommonStore &c, const StableId &prefix,
                       const StableId &kind) {
  auto id = c.next_id(prefix);
  if (!id.ok)
    return id;
  auto stored = c.append_fact({id.value, kind, "local_economy"});
  return stored.ok ? id : Outcome<StableId>{false, {}, stored.error};
}
bool header(const CommonStore &c, const StableId &id, const StableId &kind) {
  const auto *f = c.state().facts.find(id);
  return f && f->id == id && f->kind == kind && f->domain == "local_economy";
}
SourceRef accepted_source(const StableId &id) {
  return {"local_economy", "household_purchase_accepted", id};
}
ReservationRef holder(const StableId &id) {
  return {ReservationRef::Kind::domain_activity, id, "local_economy",
          "household_purchase"};
}
Outcome<WorldMoment> end_of_service(const WorldMoment &start) {
  auto one = normalize_fraction("1", "1");
  if (!one.ok)
    return {false, {}, one.error};
  return add_world_duration(start, HalfDayDuration{one.value});
}
HouseholdRetailReadContext
retail_context(const HouseholdPurchaseContext &x,
               const HouseholdRetailAcceptanceRead &sources) {
  const auto &r = x.retail;
  return {r.common,  r.common_catalog, r.shops,         r.shops_catalog,
          r.markets, r.catalog,        r.state,         r.now,
          sources,   r.market_history, r.market_capture};
}
Status item_caps(const CommonCatalog &catalog,
                 const std::vector<HouseholdItemCap> &rows) {
  if (rows.empty() || rows.size() > 16)
    return common_failure("InvalidHouseholdItemCaps");
  std::set<StableId> seen;
  for (const auto &r : rows)
    if (!catalog.items.find(r.item_id) || r.quantity_cap < 1 ||
        !seen.insert(r.item_id).second)
      return common_failure("InvalidHouseholdItemCaps", r.item_id);
  return common_success();
}
Outcome<const HouseholdPurchasePolicy *>
current_policy(const HouseholdPurchaseContext &x,
               const HouseholdPurchasePlan &p) {
  const auto *policy = x.state.policies.find(p.accepted_policy_ref.id);
  if (!policy ||
      !same_source_ref(policy->acceptance_source_ref, p.accepted_policy_ref) ||
      policy->revision != p.accepted_policy_revision ||
      policy->household_id != p.household_id || policy->closed_source_ref ||
      x.now.half_day_tick < policy->valid_from_tick ||
      x.now.half_day_tick >= policy->valid_until_tick)
    return fail<const HouseholdPurchasePolicy *>(
        "HouseholdPurchasePolicyUnavailable", p.plan_id);
  auto authority =
      x.controls.current(policy->controller_actor_id, policy->household_id,
                         household_purchase_operation, x.now.half_day_tick);
  if (!authority.ok)
    return {false, {}, authority.error};
  if (!authority.value ||
      !same_control(*authority.value, policy->controller_authority_ref))
    return fail<const HouseholdPurchasePolicy *>(
        "HouseholdPurchaseAuthorityChanged", policy->id);
  return {true, policy, {}};
}
Status plan_scope(const HouseholdPurchaseContext &x,
                  const HouseholdPurchasePlan &p,
                  const HouseholdPurchasePolicy &policy,
                  bool execution_required = true) {
  auto caps = item_caps(x.shared, p.allowed_items);
  if (!caps.ok)
    return caps;
  if (!same_control(p.controller_authority_ref,
                    policy.controller_authority_ref) ||
      p.spend_cap_qian < 1 || p.spend_cap_qian > policy.merchandise_cap_qian ||
      !has(policy.allowed_market_refs, p.known_market_ref) ||
      !has(policy.allowed_destination_inventories, p.destination_inventory) ||
      p.terms_version < 1 || p.latest_start_tick < x.now.half_day_tick ||
      p.delivery_deadline <= p.latest_start_tick ||
      p.delivery_deadline > policy.valid_until_tick)
    return common_failure("HouseholdPurchasePlanOutsidePolicy", p.plan_id);
  for (const auto &item : p.allowed_items) {
    auto cap =
        std::find_if(policy.allowed_items.begin(), policy.allowed_items.end(),
                     [&](const auto &i) { return i.item_id == item.item_id; });
    if (cap == policy.allowed_items.end() ||
        item.quantity_cap > cap->quantity_cap)
      return common_failure("HouseholdPurchaseItemOutsidePolicy", item.item_id);
  }
  const auto *home = x.economy.households.find(p.household_id);
  const auto *destination = x.common.inventory(p.destination_inventory);
  const auto *wallet = home ? x.common.wallet(home->wallet_id) : nullptr;
  if (!home || !wallet || !destination ||
      !owner_equal(wallet->owner, {Owner::Kind::household, home->id, {}}) ||
      !owner_equal(destination->owner, wallet->owner))
    return common_failure("HouseholdPurchasePrivateAssetsMismatch",
                          p.household_id);
  if (const auto *buyer =
          std::get_if<NamedHouseholdBuyer>(&p.execution_choice)) {
    const auto *actor = x.common.actor(buyer->actor_id);
    if (!actor || !actor->alive || !actor->available ||
        !actor->action_id.empty())
      return common_failure("HouseholdBuyerUnavailable", buyer->actor_id);
    return x.environment.validate_named_purchase_service(*buyer, p, x.now);
  }
  const auto &buyer = std::get<CohortHouseholdBuyer>(p.execution_choice);
  if (!execution_required) {
    const auto *cohort = x.economy.cohorts.find(buyer.cohort_id);
    return policy.allow_cohort_buyer && cohort &&
                   cohort->household_group_id == p.household_id &&
                   cohort->labor_capacity > 0 && buyer.workers == 1 &&
                   buyer.population_selection_ref.empty() &&
                   buyer.labor_allocation_id.empty() &&
                   same_source_ref(buyer.accepted_household_policy_ref,
                                   policy.acceptance_source_ref)
               ? common_success()
               : common_failure("InvalidHouseholdBuyerPlan");
  }
  const auto *cohort = x.economy.cohorts.find(buyer.cohort_id);
  const auto *selection =
      x.population.selections.find(buyer.population_selection_ref);
  const auto *allocation =
      x.economy.labor_allocations.find(buyer.labor_allocation_id);
  if (!policy.allow_cohort_buyer || buyer.workers != 1 || !cohort ||
      cohort->household_group_id != p.household_id || cohort->headcount < 1 ||
      cohort->labor_capacity < 1 ||
      cohort->status != PopulationStatus::civilian ||
      !same_source_ref(buyer.accepted_household_policy_ref,
                       policy.acceptance_source_ref) ||
      !selection ||
      selection->purpose != PopulationSelectionPurpose::household_purchase ||
      selection->selected_persons != 1 ||
      selection->selected_labor_eligible != 1 ||
      selection->state == PopulationSelectionPhase::released ||
      selection->owner_business_ref.id != p.plan_id || !allocation ||
      allocation->cohort_id != cohort->id || allocation->workers != 1 ||
      allocation->from_tick > x.now.half_day_tick ||
      allocation->through_tick < p.delivery_deadline)
    return common_failure("HouseholdBuyerSelectionMismatch", p.plan_id);
  return common_success();
}
Status validate_service_span(const HouseholdPurchaseContext &,
                             const HouseholdPurchase &, const MomentSpan &);
Outcome<StableId> local_handover(CommonStore &, HouseholdPurchaseState &,
                                 const HouseholdPurchaseContext &,
                                 const StableId &, const SourceRef &);
const HouseholdPurchase *purchase_for_action(const HouseholdPurchaseContext &x,
                                             const StableId &action) {
  for (const auto &row : x.state.purchases.rows())
    if (const auto *a =
            std::get_if<ActorActionWorkBinding>(&row.second.execution_binding))
      if (a->action_id == action)
        return &row.second;
  return nullptr;
}
const HouseholdPurchaseServiceResult *
service_result(const HouseholdPurchaseContext &x, const StableId &purchase) {
  const HouseholdPurchaseServiceResult *found = nullptr;
  for (const auto &r : x.state.service_results.rows())
    if (r.second.purchase_id == purchase) {
      const auto *period = x.state.work_periods.find(r.first);
      if (period && period->disposition != WorkPeriodDisposition::provided)
        continue;
      if (found)
        return nullptr;
      found = &r.second;
    }
  return found;
}
} // namespace
namespace {
bool service_completed(const HouseholdPurchaseContext &x,
                       const HouseholdPurchase &p) {
  if (std::holds_alternative<CohortHouseholdBuyer>(p.executor))
    return service_result(x, p.purchase_id) != nullptr;
  if (!p.terminal_result_ref)
    return false;
  const auto *event =
      x.state.named_service_events.find(p.terminal_result_ref->id);
  return event && event->purchase_id == p.purchase_id &&
         event->kind == HouseholdServiceEventKind::completed &&
         same_source_ref(
             *p.terminal_result_ref,
             {"local_economy", "household_purchase_service", event->event_id});
}
} // namespace
const ActionTimeRegistration &household_purchase_action_time_registration() {
  static const ActionTimeRegistration r{
      "household_purchase",
      "local_economy",
      "household_purchases",
      2,
      1,
      ActionTimeMode::fixed_duration,
      ActionRemainingMeaning::service_estimate,
      {},
      {}};
  return r;
}
const ActionTimeRegistration &household_purchase_action_time_registration_v3() {
  static const ActionTimeRegistration r{
      "household_purchase",
      "local_economy",
      "household_purchases",
      3,
      1,
      ActionTimeMode::fixed_duration,
      ActionRemainingMeaning::service_estimate,
      {},
      {}};
  return r;
}
const ActionTimeRegistration &
household_purchase_action_time_registration(const HouseholdPurchaseContext &x) {
  return x.selected_household_version == 3
             ? household_purchase_action_time_registration_v3()
             : household_purchase_action_time_registration();
}
Status validate_original_household_purchase_policy_input(
    const HouseholdPurchaseContext &x, const HouseholdPurchasePolicy &policy) {
  auto caps = item_caps(x.shared, policy.allowed_items);
  if (!caps.ok)
    return {false, {}, caps.error};
  const auto *household = x.economy.households.find(policy.household_id);
  if (!household || policy.merchandise_cap_qian < 1 ||
      policy.transport_cap_qian < 0 || policy.total_cap_qian < 1 ||
      policy.merchandise_cap_qian > policy.total_cap_qian ||
      policy.transport_cap_qian > policy.total_cap_qian ||
      policy.valid_from_tick < x.now.half_day_tick ||
      policy.valid_until_tick <= policy.valid_from_tick ||
      policy.allowed_market_refs.empty() ||
      policy.allowed_destination_inventories.empty() ||
      policy.closed_source_ref)
    return common_failure("InvalidHouseholdPurchasePolicy");
  auto authority =
      x.controls.current(policy.controller_actor_id, policy.household_id,
                         household_purchase_operation, x.now.half_day_tick);
  if (!authority.ok)
    return {false, {}, authority.error};
  if (!authority.value ||
      !same_control(policy.controller_authority_ref, *authority.value))
    return common_failure("HouseholdPurchaseAuthorityRequired");
  const HouseholdPurchasePolicy *old = nullptr;
  for (auto &row : x.state.policies.rows())
    if (row.second.household_id == policy.household_id &&
        !row.second.closed_source_ref) {
      if (old)
        return common_failure("DuplicateCurrentHouseholdPurchasePolicy");
      old = &row.second;
    }
  if (old && old->revision == std::numeric_limits<Integer>::max())
    return common_failure("HouseholdPolicyRevisionOverflow", old->id);
  if (policy.revision != (old ? old->revision + 1 : 1) ||
      (old && policy.id != old->id) || (!old && !policy.id.empty()))
    return common_failure("StaleHouseholdPurchasePolicy");
  for (const auto &id : policy.allowed_destination_inventories) {
    const auto *destination = x.common.inventory(id);
    if (!destination ||
        !owner_equal(destination->owner,
                     {Owner::Kind::household, policy.household_id, {}}))
      return common_failure("HouseholdDestinationAuthorityRequired", id);
  }
  for (const auto &market : policy.allowed_market_refs) {
    auto known = household_market_catalog_contains(
        x.common, x.catalog, x.state, x.private_market_custody, market);
    if (!known.ok)
      return {false, {}, known.error};
    if (!known.value)
      return common_failure("UnknownHouseholdPurchaseMarket", market);
  }
  return common_success();
}
static Outcome<ProducedStableId> accept_household_purchase_policy_owned(
    CommonStore &c, HouseholdPurchaseState &state,
    const HouseholdPurchaseContext &x, HouseholdPurchasePolicy policy) {
  if (&c != &x.common || &state != &x.state)
    return fail<ProducedStableId>("HouseholdPurchaseCandidateMismatch");
  auto checked = validate_original_household_purchase_policy_input(x, policy);
  if (!checked.ok)
    return {false, {}, checked.error};
  HouseholdPurchasePolicy *old = nullptr;
  for (auto &row : state.policies.rows())
    if (row.second.household_id == policy.household_id &&
        !row.second.closed_source_ref)
      old = &row.second;
  auto id =
      owned_fact(c, "household_policy", "household_purchase_policy_accepted");
  if (!id.ok)
    return id;
  if (old)
    old->closed_source_ref = SourceRef{
        "local_economy", "household_purchase_policy_accepted", id.value.id()};
  policy.id = id.value.id();
  policy.accepted_tick = x.now.half_day_tick;
  policy.acceptance_source_ref = {
      "local_economy", "household_purchase_policy_accepted", id.value.id()};
  state.policies.insert(id.value.id(), std::move(policy));
  return id;
}
// 旧调用仍用同一业务算法，仅保留明确的裸返回兼容边界。
Outcome<StableId> accept_household_purchase_policy_in_candidate(
    CommonStore &c, HouseholdPurchaseState &s,
    const HouseholdPurchaseContext &x, HouseholdPurchasePolicy p) {
  auto id = accept_household_purchase_policy_owned(c, s, x, std::move(p));
  if (!id.ok)
    return {false, {}, id.error};
  return {true, id.value.id(), {}};
}
Outcome<const FactRecord *> accept_household_purchase_policy_fact_in_candidate(
    CommonStore &c, HouseholdPurchaseState &s,
    const HouseholdPurchaseContext &x, HouseholdPurchasePolicy p) {
  auto id = accept_household_purchase_policy_owned(c, s, x, std::move(p));
  if (!id.ok)
    return {false, {}, id.error};
  const auto *head = c.state().facts.find(id.value.id());
  if (!head || head->id != id.value.id() || head->domain != "local_economy" ||
      head->kind != "household_purchase_policy_accepted")
    return fail<const FactRecord *>("HouseholdCommandActualResultRequired");
  return {true, head, {}};
}
Outcome<PopulationSelectionAuthority>
read_household_population_selection_authority(
    const HouseholdPurchaseContext &x,
    const PopulationSelectionRequest &request) {
  const auto *plan = x.state.plans.find(request.owner_business_ref.id);
  if (!plan ||
      !same_source_ref(
          request.owner_business_ref,
          {"local_economy", "household_purchase_planned", plan->plan_id}) ||
      !header(x.common, plan->plan_id, "household_purchase_planned") ||
      request.purpose != PopulationSelectionPurpose::household_purchase)
    return fail<PopulationSelectionAuthority>("HouseholdSelectionPlanRequired");
  auto policy = current_policy(x, *plan);
  if (!policy.ok)
    return {false, {}, policy.error};
  const auto *buyer =
      std::get_if<CohortHouseholdBuyer>(&plan->execution_choice);
  const auto *count = std::get_if<PopulationSelectCount>(&request.portion);
  if (!buyer || !policy.value->allow_cohort_buyer ||
      buyer->cohort_id != request.source.cohort_id || buyer->workers != 1 ||
      !count || count->persons != 1 ||
      count->partition_policy != PopulationPartitionPolicy::labor_eligible_only)
    return fail<PopulationSelectionAuthority>(
        "HouseholdSelectionScopeMismatch");
  return {true,
          {request.owner_business_ref, request.owner_business_ref,
           PopulationSelectionPurpose::household_purchase, plan->household_id,
           1},
          {}};
}
namespace {
class HouseholdSelectionOnlyRead final : public PopulationTransportSourceRead {
  const HouseholdPurchaseContext &x;

public:
  explicit HouseholdSelectionOnlyRead(const HouseholdPurchaseContext &v)
      : x(v) {}
  Outcome<PopulationSelectionAuthority> read_population_selection_authority(
      const PopulationSelectionRequest &r) const override {
    return read_household_population_selection_authority(x, r);
  }
  Outcome<PopulationMaterializationSource>
  read_population_transport_registration(
      const RegisterPopulationTransportFromSelection &) const override {
    return fail<PopulationMaterializationSource>(
        "TransportRegistrationOwnerRequired");
  }
  Status validate_population_transport_binding(
      const PopulationTransportBinding &) const override {
    return common_failure("TransportRegistrationOwnerRequired");
  }
  Status
  validate_population_selection_release(const PopulationSelection &,
                                        const SourceRef &) const override {
    return common_failure("HouseholdSelectionReleaseOwnerRequired");
  }
  Outcome<PopulationTransportReleaseSource>
  read_population_transport_release(const PopulationTransportBinding &,
                                    const SourceRef &) const override {
    return fail<PopulationTransportReleaseSource>(
        "TransportReleaseOwnerRequired");
  }
  Status
  validate_population_transport_release(const PopulationTransportBinding &,
                                        const SourceRef &) const override {
    return common_failure("TransportReleaseOwnerRequired");
  }
};
} // namespace
Status validate_original_household_purchase_plan_input(
    const HouseholdPurchaseContext &x, const HouseholdPurchasePlan &plan,
    const HouseholdMobileCustodyRequest *mobile_request) {
  if (!plan.plan_id.empty() || plan.mobile_custody_adoption)
    return common_failure("HouseholdPlanDraftMustNotContainProducedIdentity");
  if (x.selected_household_version != 2 && x.selected_household_version != 3)
    return common_failure("UnsupportedHouseholdPurchasesVersion");
  if (mobile_request &&
      (x.selected_household_version != 3 || !x.mobile_custody))
    return common_failure("HouseholdMobileCustodyProviderRequired");
  auto policy = current_policy(x, plan);
  if (!policy.ok)
    return {false, {}, policy.error};
  auto valid = plan_scope(x, plan, *policy.value, false);
  if (!valid.ok)
    return {false, {}, valid.error};
  return common_success();
}
static Outcome<ProducedStableId> plan_household_purchase_owned(
    CommonStore &c, LocalEconomyState &economy,
    PopulationTransportState &population, HouseholdPurchaseState &state,
    const HouseholdPurchaseContext &x, HouseholdPurchasePlan plan,
    const HouseholdMobileCustodyRequest *mobile_request) {
  if (&c != &x.common || &economy != &x.economy ||
      &population != &x.population || &state != &x.state ||
      !plan.plan_id.empty() || plan.mobile_custody_adoption)
    return fail<ProducedStableId>("HouseholdPurchaseCandidateMismatch");
  auto preflight =
      validate_original_household_purchase_plan_input(x, plan, mobile_request);
  if (!preflight.ok)
    return {false, {}, preflight.error};
  auto id = owned_fact(c, "household_plan", "household_purchase_planned");
  if (!id.ok)
    return id;
  plan.plan_id = id.value.id();
  if (mobile_request) {
    auto adopted = capture_household_mobile_custody(
        x, *x.mobile_custody, plan, *mobile_request,
        {"local_economy", "household_purchase_planned", id.value.id()});
    if (!adopted.ok)
      return {false, {}, adopted.error};
    plan.mobile_custody_adoption = std::move(adopted.value);
  }
  state.plans.insert(id.value.id(), plan);
  if (const auto *buyer =
          std::get_if<CohortHouseholdBuyer>(&plan.execution_choice)) {
    auto source =
        read_civilian_population_ref(economy, population, buyer->cohort_id);
    if (!source.ok)
      return {false, {}, source.error};
    const auto *revision = population.source_revisions.find(buyer->cohort_id);
    if (!revision || !revision->location_binding_ref)
      return fail<ProducedStableId>("HouseholdBuyerLocationSourceRequired");
    PopulationSelectionRequest request{
        source.value,
        PopulationSelectCount{1,
                              PopulationPartitionPolicy::labor_eligible_only},
        PopulationSelectionPurpose::household_purchase,
        {"local_economy", "household_purchase_planned", id.value.id()},
        *revision->location_binding_ref,
        revision->condition_source_ref};
    HouseholdSelectionOnlyRead sources{x};
    auto selection = accept_population_selection_in_candidate(
        c, economy, population, sources, request, x.now);
    if (!selection.ok)
      return {false, {}, selection.error};
    auto allocation = owned_fact(c, "household_buyer_labor",
                                 "household_buyer_labor_reserved");
    if (!allocation.ok)
      return allocation;
    auto allocated = allocate_labor(c, economy,
                                    {allocation.value.id(), buyer->cohort_id,
                                     id.value.id(), 1, x.now.half_day_tick,
                                     plan.delivery_deadline, id.value.id()});
    if (!allocated.ok)
      return {false, {}, allocated.error};
    auto &stored = std::get<CohortHouseholdBuyer>(
        state.plans.find(id.value.id())->execution_choice);
    stored.population_selection_ref = selection.value;
    stored.labor_allocation_id = allocation.value.id();
  }
  return id;
}

// 原人口选择与劳动写入继续同一次业务调用，未另造旁路算法。
Outcome<StableId> plan_household_purchase_in_candidate(
    CommonStore &c, LocalEconomyState &e, PopulationTransportState &p,
    HouseholdPurchaseState &s, const HouseholdPurchaseContext &x,
    HouseholdPurchasePlan plan, const HouseholdMobileCustodyRequest *m) {
  auto id = plan_household_purchase_owned(c, e, p, s, x, std::move(plan), m);
  if (!id.ok)
    return {false, {}, id.error};
  return {true, id.value.id(), {}};
}
Outcome<const FactRecord *> plan_household_purchase_fact_in_candidate(
    CommonStore &c, LocalEconomyState &e, PopulationTransportState &p,
    HouseholdPurchaseState &s, const HouseholdPurchaseContext &x,
    HouseholdPurchasePlan plan, const HouseholdMobileCustodyRequest *m) {
  auto id = plan_household_purchase_owned(c, e, p, s, x, std::move(plan), m);
  if (!id.ok)
    return {false, {}, id.error};
  const auto *head = c.state().facts.find(id.value.id());
  if (!head || head->id != id.value.id() || head->domain != "local_economy" ||
      head->kind != "household_purchase_planned")
    return fail<const FactRecord *>("HouseholdCommandActualResultRequired");
  return {true, head, {}};
}

Outcome<HouseholdPurchaseAllowance>
read_household_purchase_allowance(const HouseholdPurchaseContext &x,
                                  const SourceRef &ref, Integer revision) {
  const auto *policy = x.state.policies.find(ref.id);
  if (!policy || !same_source_ref(ref, policy->acceptance_source_ref) ||
      policy->revision != revision ||
      !header(x.common, policy->id, "household_purchase_policy_accepted"))
    return fail<HouseholdPurchaseAllowance>(
        "HouseholdPurchasePolicySourceMissing", ref.id);
  HouseholdPurchaseAllowance out;
  out.policy_ref = ref;
  out.policy_revision = revision;
  out.merchandise_cap_qian = policy->merchandise_cap_qian;
  out.transport_cap_qian = policy->transport_cap_qian;
  out.total_cap_qian = policy->total_cap_qian;
  for (const auto &row : x.state.purchases.rows()) {
    const auto *accepted =
        x.state.accepted_terms.find(row.second.accepted_terms_ref.id);
    if (!accepted || !same_source_ref(accepted->plan.accepted_policy_ref, ref))
      continue;
    const auto *retail =
        x.retail.state.settlements.find(row.second.retail_receipt_ref);
    if (!retail || retail->terms.purchase_id != row.first ||
        retail->terms.total < 0)
      return fail<HouseholdPurchaseAllowance>(
          "HouseholdPurchaseRetailSourceMissing", row.first);
    if (__builtin_add_overflow(out.merchandise_gross_paid, retail->terms.total,
                               &out.merchandise_gross_paid))
      return fail<HouseholdPurchaseAllowance>(
          "HouseholdPurchaseBudgetOverflow");
  }
  for (const auto &receipt : x.transport_expenses.payments) {
    const auto *authority = std::get_if<HouseholdPurchaseExpenseAuthority>(
        &receipt.request.authority);
    if (!authority || authority->accepted_policy_ref != ref.id)
      continue;
    if (authority->household_id != policy->household_id ||
        authority->policy_revision != revision || receipt.expense.amount < 0 ||
        !header(x.common, receipt.id, "transport_expense_paid"))
      return fail<HouseholdPurchaseAllowance>(
          "HouseholdTransportPaymentPolicyMismatch", receipt.id);
    if (__builtin_add_overflow(out.transport_gross_paid, receipt.expense.amount,
                               &out.transport_gross_paid))
      return fail<HouseholdPurchaseAllowance>(
          "HouseholdPurchaseBudgetOverflow");
  }
  if (__builtin_add_overflow(out.merchandise_gross_paid,
                             out.transport_gross_paid, &out.total_gross_paid) ||
      out.merchandise_cap_qian < 0 || out.transport_cap_qian < 0 ||
      out.total_cap_qian < 1 ||
      out.merchandise_gross_paid > out.merchandise_cap_qian ||
      out.transport_gross_paid > out.transport_cap_qian ||
      out.total_gross_paid > out.total_cap_qian)
    return fail<HouseholdPurchaseAllowance>("HouseholdPurchaseBudgetExceeded",
                                            ref.id);
  out.total_available = out.total_cap_qian - out.total_gross_paid;
  out.merchandise_available =
      std::min(out.merchandise_cap_qian - out.merchandise_gross_paid,
               out.total_available);
  return {true, out, {}};
}
Outcome<HouseholdPurchaseQuote>
preview_household_purchase(const HouseholdPurchaseContext &x,
                           const StableId &plan_id,
                           const HouseholdRetailTerms &requested) {
  if (!same_time(x.now, x.retail.now))
    return fail<HouseholdPurchaseQuote>("HouseholdRetailClockMismatch");
  const auto *original_plan = x.state.plans.find(plan_id);
  if (!original_plan ||
      !header(x.common, plan_id, "household_purchase_planned"))
    return fail<HouseholdPurchaseQuote>("HouseholdPurchasePlanMissing",
                                        plan_id);
  if (original_plan->mobile_custody_adoption) {
    if (x.selected_household_version != 3 || !x.mobile_custody)
      return fail<HouseholdPurchaseQuote>(
          "HouseholdMobileCustodyProviderRequired", plan_id);
    auto mobile =
        validate_household_mobile_custody(x, *x.mobile_custody, *original_plan);
    if (!mobile.ok)
      return {false, {}, mobile.error};
  }
  auto executor = read_household_buyer_for_adoption(
      {x.common, x.economy, x.population, x.transport_registrations},
      *original_plan, x.now);
  if (!executor.ok)
    return {false, {}, executor.error};
  auto resolved_plan = *original_plan;
  resolved_plan.execution_choice = std::move(executor.value);
  const auto *plan = &resolved_plan;
  auto policy = current_policy(x, *plan);
  if (!policy.ok)
    return {false, {}, policy.error};
  auto valid = plan_scope(x, *plan, *policy.value);
  if (!valid.ok)
    return {false, {}, valid.error};
  auto resolved_service = read_household_market_service_preview(
      x.common, x.catalog, x.state, x.private_market_custody, *plan,
      requested.seller_ref);
  if (!resolved_service.ok)
    return {false, {}, resolved_service.error};
  const auto *facility = resolved_service.value;
  const auto *household = x.economy.households.find(plan->household_id);
  auto cap = std::find_if(
      plan->allowed_items.begin(), plan->allowed_items.end(),
      [&](const auto &item) { return item.item_id == requested.item_id; });
  if (cap == plan->allowed_items.end() || requested.quantity < 1 ||
      requested.quantity > cap->quantity_cap)
    return fail<HouseholdPurchaseQuote>("HouseholdPurchaseQuantityOutsidePlan");
  auto presence =
      x.environment.read_purchase_presence_for_service(*plan, *facility, x.now);
  if (!presence.ok)
    return {false, {}, presence.error};
  if (presence.value.place_id != facility->place_id ||
      !valid_source_ref(presence.value.actual_source))
    return fail<HouseholdPurchaseQuote>("HouseholdPurchasePresenceMismatch");
  auto until = end_of_service(x.now);
  if (!until.ok)
    return {false, {}, until.error};
  if (until.value.half_day_tick > plan->delivery_deadline)
    return fail<HouseholdPurchaseQuote>("HouseholdPurchaseDeadline");
  auto capacity =
      x.claim_sources.capacity_source_at(facility->service_slot, x.now);
  if (!capacity.ok)
    return {false, {}, capacity.error};
  FacilityClaimRequest2 request{facility->service_slot,
                                {ReservationRef::Kind::domain_activity, plan_id,
                                 "local_economy", "household_purchase_plan"},
                                x.now,
                                until.value,
                                1,
                                facility->service_usage_right_ref,
                                {}};
  auto claim = quote_facility_claim(x.claims, x.claim_catalog, request,
                                    {capacity.value}, x.now, x.claim_sources);
  if (!claim.ok)
    return {false, {}, claim.error};
  auto terms = requested;
  terms.purchase_id = plan_id;
  terms.household_group_id = plan->household_id;
  terms.executor = plan->execution_choice;
  terms.buyer_wallet_id = household->wallet_id;
  terms.buyer = {Owner::Kind::household, plan->household_id, {}};
  terms.custody_inventory_id = facility->custody_inventory_id;
  terms.destination_inventory_id = plan->destination_inventory;
  terms.quoted_at = x.now;
  terms.accepted_terms_ref = {"local_economy", "household_purchase_planned",
                              plan_id};
  terms.custody_terms_ref = facility->custody_terms_ref;
  terms.terms_version = plan->terms_version;
  auto retail = preview_household_retail(x.retail, terms);
  if (!retail.ok)
    return {false, {}, retail.error};
  if (retail.value.terms.place_id != facility->place_id)
    return fail<HouseholdPurchaseQuote>("HouseholdPurchaseSellerPlaceMismatch");
  if (const auto *seller =
          std::get_if<PublicHouseholdRetailSeller>(&terms.seller_ref)) {
    if (seller->market_shop_id != plan->known_market_ref)
      return fail<HouseholdPurchaseQuote>("HouseholdPurchaseMarketMismatch");
  } else {
    const auto *shop = x.retail.shops.shops.find(
        std::get<PrivateHouseholdRetailSeller>(terms.seller_ref).shop_id);
    if (!shop || shop->terms.market_shop_id != plan->known_market_ref)
      return fail<HouseholdPurchaseQuote>("HouseholdPurchaseMarketMismatch");
  }
  auto allowance = read_household_purchase_allowance(
      x, plan->accepted_policy_ref, plan->accepted_policy_revision);
  if (!allowance.ok)
    return {false, {}, allowance.error};
  if (retail.value.terms.total > plan->spend_cap_qian ||
      retail.value.terms.total > allowance.value.merchandise_available)
    return fail<HouseholdPurchaseQuote>("HouseholdPurchaseBudgetExceeded");
  return {true,
          {x.common.state().identity,
           plan_id,
           std::move(retail.value),
           std::move(presence.value),
           std::move(claim.value),
           {capacity.value},
           x.now},
          {}};
}
namespace {
bool versions(const std::vector<ReadVersion> &a,
              const std::vector<ReadVersion> &b) {
  if (a.size() != b.size())
    return false;
  for (std::size_t i = 0; i < a.size(); ++i)
    if (a[i].resource.kind != b[i].resource.kind ||
        a[i].resource.id != b[i].resource.id || a[i].version != b[i].version)
      return false;
  return true;
}
bool selections(const std::vector<CargoSelection> &a,
                const std::vector<CargoSelection> &b) {
  if (a.size() != b.size())
    return false;
  for (std::size_t i = 0; i < a.size(); ++i)
    if (a[i].inventory_id != b[i].inventory_id || a[i].lot_id != b[i].lot_id ||
        a[i].quantity != b[i].quantity)
      return false;
  return true;
}
} // namespace
Outcome<HouseholdRetailAcceptance>
HouseholdPurchaseContracts::read_household_retail_acceptance(
    const StableId &id) const {
  NativeHouseholdRetailAcceptanceRead source(
      {x_.common, x_.economy, x_.catalog, x_.state, x_.controls, &x_.population,
       x_.transport_registrations, x_.private_market_custody});
  return source.read_household_retail_acceptance(id);
}
Status HouseholdPurchaseContracts::validate_household_retail_settlement(
    const HouseholdRetailSettlement &r) const {
  NativeHouseholdRetailAcceptanceRead source(
      {x_.common, x_.economy, x_.catalog, x_.state, x_.controls, &x_.population,
       x_.transport_registrations, x_.private_market_custody});
  return source.validate_household_retail_settlement(r);
}
Outcome<ReservationOwnerView>
HouseholdPurchaseContracts::read_domain_activity_reservation_owner(
    const ReservationRef &ref) const {
  if (ref.kind != ReservationRef::Kind::domain_activity ||
      ref.domain_id != "local_economy")
    return fail<ReservationOwnerView>("UnsupportedHouseholdReservationOwner");
  if (ref.activity_kind == "household_purchase_plan") {
    const auto *plan = x_.state.plans.find(ref.id);
    if (!plan || !header(x_.common, ref.id, "household_purchase_planned"))
      return fail<ReservationOwnerView>("HouseholdPurchasePlanMissing", ref.id);
    return {true,
            {ref, plan->terms_version, ReservationOwnerPhase::active, true},
            {}};
  }
  if (ref.activity_kind != "household_purchase")
    return fail<ReservationOwnerView>("UnsupportedHouseholdReservationOwner");
  const auto *purchase = x_.state.purchases.find(ref.id);
  if (!purchase || !header(x_.common, ref.id, "household_purchase_accepted"))
    return fail<ReservationOwnerView>("HouseholdPurchaseAcceptanceMissing",
                                      ref.id);
  const bool terminal = purchase->state == HouseholdPurchasePhase::received ||
                        purchase->state == HouseholdPurchasePhase::cancelled ||
                        purchase->state == HouseholdPurchasePhase::lost;
  const auto phase = terminal ? ReservationOwnerPhase::terminal
                     : purchase->state == HouseholdPurchasePhase::blocked
                         ? ReservationOwnerPhase::paused
                     : purchase->state == HouseholdPurchasePhase::settling
                         ? ReservationOwnerPhase::settling
                         : ReservationOwnerPhase::active;
  return {true, {ref, purchase->phase_revision, phase, !terminal}, {}};
}
Outcome<StableId> accept_household_purchase_in_candidate(
    CommonStore &c, LocalEconomyState &economy, HouseholdPurchaseState &state,
    FacilityClaimsState &claims, ActionElapsedStore &elapsed,
    HouseholdRetailState &retail, const HouseholdPurchaseContext &x,
    const HouseholdPurchaseQuote &quote) {
  if (&c != &x.common || &economy != &x.economy || &state != &x.state ||
      &claims != &x.claims || &elapsed != &x.elapsed ||
      &retail != &x.retail.state ||
      !identity(c.state().identity, quote.identity) ||
      !same_time(x.now, quote.accepted_at))
    return fail<StableId>("HouseholdPurchaseCandidateMismatch");
  auto current =
      preview_household_purchase(x, quote.plan_id, quote.retail.terms);
  if (!current.ok)
    return {false, {}, current.error};
  const auto &before = quote.retail.terms;
  const auto &after = current.value.retail.terms;
  if (!same_buyer(before.executor, after.executor) ||
      before.total != after.total ||
      before.market_revision != after.market_revision ||
      before.market_stock != after.market_stock ||
      before.midpoint != after.midpoint ||
      before.unit_price != after.unit_price ||
      before.base_price != after.base_price ||
      !same_seller(before.seller_ref, after.seller_ref) ||
      !selections(before.selected_cargo, after.selected_cargo) ||
      !versions(quote.retail.versions, current.value.retail.versions) ||
      !versions(quote.presence.versions, current.value.presence.versions) ||
      !same_source_ref(quote.presence.actual_source,
                       current.value.presence.actual_source) ||
      quote.service_claim.capacity_revision !=
          current.value.service_claim.capacity_revision ||
      quote.service_claim.claim_set_revision !=
          current.value.service_claim.claim_set_revision)
    return fail<StableId>("StaleHouseholdPurchaseQuote");
  auto plan = *state.plans.find(quote.plan_id);
  plan.execution_choice = after.executor;
  auto resolved_service = read_household_market_service_preview(
      x.common, x.catalog, x.state, x.private_market_custody, plan,
      after.seller_ref);
  if (!resolved_service.ok)
    return {false, {}, resolved_service.error};
  const auto *facility = resolved_service.value;
  if (std::holds_alternative<CohortHouseholdBuyer>(plan.execution_choice)) {
    auto fraction = compare_fractions(x.now.sub_half_day, CanonicalFraction{});
    if (!fraction.ok || fraction.value)
      return fail<StableId>("HouseholdCohortOriginalPeriodBoundaryRequired");
  }
  auto id = fact(c, "household_purchase", "household_purchase_accepted");
  if (!id.ok)
    return id;
  HouseholdPurchaseAcceptedTerms accepted{accepted_source(id.value),
                                          plan,
                                          after.seller_ref,
                                          after.item_id,
                                          after.quantity,
                                          after.total,
                                          x.now,
                                          current.value.presence.actual_source,
                                          current.value.retail.versions};
  HouseholdPurchase purchase;
  purchase.purchase_id = id.value;
  purchase.plan_ref = plan.plan_id;
  purchase.accepted_terms_ref = accepted.source;
  purchase.executor = plan.execution_choice;
  purchase.service_claim_ref = id.value + "/market_service";
  purchase.holding_inventory_ref = facility->custody_inventory_id;
  purchase.state = HouseholdPurchasePhase::servicing;
  if (const auto *buyer =
          std::get_if<NamedHouseholdBuyer>(&purchase.executor)) {
    auto action = fact(c, "action", "household_purchase_service_started");
    if (!action.ok)
      return action;
    purchase.execution_binding = ActorActionWorkBinding{
        action.value, "household_purchase", accepted.source};
  } else
    purchase.execution_binding = OwnerPeriodWorkBinding{
        WorkPeriodOwner::local_economy, "household_purchase", id.value,
        "household_purchase_periods"};
  state.accepted_terms.insert(id.value, accepted);
  state.purchases.insert(id.value, purchase);
  const auto &requested = current.value.service_claim.requested;
  FacilityClaim2 claim{purchase.service_claim_ref,
                       requested.slot_ref,
                       holder(id.value),
                       requested.from,
                       requested.until,
                       1,
                       current.value.service_claim.capacity_revision,
                       requested.usage_right_ref,
                       accepted.source};
  FacilityClaimMutationReceipt mutation{
      current.value.service_claim.claim_set_revision + 1,
      x.now,
      accepted.source,
      1,
      {},
      {claim},
      current.value.capacity_sources};
  if (std::holds_alternative<NamedHouseholdBuyer>(purchase.executor)) {
    auto named = record_named_household_admission_in_candidate(
        c, state, x, id.value, claim, mutation);
    if (!named.ok)
      return {false, {}, named.error};
  }
  auto admitted = apply_facility_claim_mutation(
      claims, x.claim_catalog, claim.slot_ref, mutation,
      current.value.service_claim.claim_set_revision, x.now, x.claim_sources);
  if (!admitted.ok)
    return {false, {}, admitted.error};
  claims = std::move(admitted.value);
  HouseholdPurchaseContracts source{x};
  if (const auto *binding =
          std::get_if<ActorActionWorkBinding>(&purchase.execution_binding)) {
    const auto &buyer = std::get<NamedHouseholdBuyer>(purchase.executor);
    ActionCoreView action{binding->action_id,  "household_purchase",
                          buyer.actor_id,      {buyer.actor_id},
                          x.now.half_day_tick, 1};
    ActionMutation insert;
    insert.kind = ActionMutation::Kind::insert;
    insert.action = action;
    auto changed = c.apply_action(insert);
    if (!changed.ok)
      return {false, {}, changed.error};
    ActorMutation busy;
    busy.actor_id = buyer.actor_id;
    busy.action_id = action.id;
    changed = c.apply_actor(busy);
    if (!changed.ok)
      return {false, {}, changed.error};
    auto entry = begin_fixed_elapsed(action.id, id.value,
                                     x.catalog.duration_definition_id, id.value,
                                     x.now, 1, x.scale);
    if (!entry.ok)
      return {false, {}, entry.error};
    FixedServiceTransition transition;
    transition.transition_id = mutation.source_result_ref.id;
    transition.action_ref = action.id;
    transition.occurred_at = x.now;
    transition.execution_source_ref = accepted.source;
    transition.execution_source_schema = 1;
    transition.cause = FixedOwnerAdopted{mutation.source_result_ref, 2};
    transition.input_basis_refs = {
        FixedAppliedInput{mutation.source_result_ref, 2},
        FixedAppliedInput{accepted.source, 1}};
    const auto *adoption =
        state.named_service_events.find(mutation.source_result_ref.id);
    if (!adoption)
      return fail<StableId>("HouseholdAdoptionSourceMissing", id.value);
    auto adopted_inputs =
        x.environment.read_named_service_adoption_inputs(purchase, *adoption);
    if (!adopted_inputs.ok)
      return {false, {}, adopted_inputs.error};
    transition.input_basis_refs.insert(transition.input_basis_refs.end(),
                                       adopted_inputs.value.begin(),
                                       adopted_inputs.value.end());
    for (const auto &capacity : current.value.capacity_sources)
      transition.input_basis_refs.push_back(
          FixedAppliedInput{capacity.source_ref, capacity.source_schema});
    auto prepared = prepare_fixed_service_adoption(
        action, entry.value, transition,
        household_purchase_action_time_registration(x), x.scale, source);
    if (!prepared.ok)
      return {false, {}, prepared.error};
    changed = apply_fixed_service_change_in_candidate(
        elapsed, std::move(prepared.value),
        household_purchase_action_time_registration(x), x.scale, source);
    if (!changed.ok)
      return {false, {}, changed.error};
  }
  auto actual_quote = current.value.retail;
  actual_quote.terms.purchase_id = id.value;
  actual_quote.terms.accepted_terms_ref = accepted.source;
  auto retail_read = retail_context(x, source);
  auto settled = settle_household_retail_in_candidate(c, retail, retail_read,
                                                      actual_quote);
  if (!settled.ok)
    return {false, {}, settled.error};
  state.purchases.find(id.value)->retail_receipt_ref = settled.value;
  // 已买货实际预留给该购置，阻止其他purpose从共同暂存容器拿走；未来交付仍转原这些批。
  const auto &receipt = *retail.settlements.find(settled.value);
  std::vector<CargoSelection> selected;
  for (const auto &lot : receipt.lots)
    selected.push_back(
        {purchase.holding_inventory_ref, lot.destination_lot_id, lot.quantity});
  ResourceRecipe reserve;
  reserve.operations = {ReserveSelectedLots{purchase.holding_inventory_ref,
                                            selected,
                                            {receipt.terms.buyer},
                                            holder(id.value)}};
  reserve.expected_versions = {c.read_version(
      {ResourceKey::Kind::inventory, purchase.holding_inventory_ref})};
  auto reserved = c.apply_resources(reserve);
  if (!reserved.ok)
    return {false, {}, reserved.error};
  return id;
}
Outcome<ActionTimeOwnerProof>
HouseholdPurchaseContracts::read_action_time_owner(
    const ActionTimeRegistration &registration, const StableId &action) const {
  return read_named_household_action_time_owner(x_, registration, action);
}
Outcome<WorldMoment> HouseholdPurchaseContracts::read_action_time_source(
    const ElapsedTimeSource &source) const {
  const auto *value = std::get_if<MomentOwnerSource>(&source);
  if (value && value->source_schema == 2) {
    auto event = read_named_household_service_event(x_, value->source_ref);
    if (!event.ok)
      return {false, {}, event.error};
    return {true, event.value->occurred_at, {}};
  }
  if (!value || value->source_schema != 1 ||
      value->source_ref.domain != "local_economy")
    return fail<WorldMoment>("UnsupportedHouseholdTimeSource");
  const auto &r = value->source_ref;
  if (r.kind == "household_purchase_accepted") {
    const auto *a = x_.state.accepted_terms.find(r.id);
    if (a && same_source_ref(r, a->source) && header(x_.common, r.id, r.kind))
      return {true, a->accepted_at, {}};
  }
  if (r.kind == "household_purchase_service_completed" ||
      r.kind == "household_purchase_period_interrupted") {
    const auto *s = x_.state.service_results.find(r.id);
    if (s && header(x_.common, r.id, r.kind))
      return {true, s->completed_at, {}};
  }
  if (const auto *change = x_.state.phase_changes.find(r.id))
    if (r.kind == "household_purchase_phase_changed" &&
        header(x_.common, r.id, r.kind))
      return {true, change->occurred_at, {}};
  return fail<WorldMoment>("HouseholdTimeSourceMissing", r.id);
}
Status HouseholdPurchaseContracts::validate_fixed_action_sources(
    const ActionTimeRegistration &registration,
    const ActionTimeOwnerProof &proof, const ActionElapsedState &entry,
    const std::vector<FixedServiceTransition> &transitions) const {
  if ((x_.selected_household_version != 2 &&
       x_.selected_household_version != 3) ||
      registration.profile_version != x_.selected_household_version ||
      registration.source_schema != 1)
    return common_failure("UnsupportedHouseholdActionTime");
  return validate_named_household_fixed_sources(x_, proof, entry, transitions);
}
Status HouseholdPurchaseContracts::validate_shared_action_sources(
    const ActionTimeRegistration &, const ActionTimeOwnerProof &,
    const SharedWorkElapsedState &) const {
  return common_failure("HouseholdNamedFixedDurationRequired");
}
Outcome<OriginalActionRemaining>
HouseholdPurchaseContracts::read_original_action_remaining(
    const ActionTimeRegistration &reg, const ActionTimeOwnerProof &proof,
    const WorldMoment &) const {
  if (reg.action_kind != "household_purchase")
    return fail<OriginalActionRemaining>("UnsupportedHouseholdActionTime");
  const auto *entry = find_fixed_elapsed(x_.elapsed, proof.action_ref);
  if (!entry)
    return fail<OriginalActionRemaining>("HouseholdElapsedSourceMissing");
  auto units =
      subtract_work_units(entry->required_units, entry->credited_units);
  if (!units.ok)
    return {false, {}, units.error};
  auto remaining =
      scale_units_to_duration(units.value, x_.scale.units_per_half_day);
  if (!remaining.ok)
    return {false, {}, remaining.error};
  return {true,
          ActualServiceRemaining{ActualServiceRemainingBuildView{
              remaining.value,
              {proof.original_execution_source.domain,
               proof.original_execution_source.kind,
               proof.original_execution_source.id},
              1}},
          {}};
}
Outcome<StableId> HouseholdPurchaseContracts::read_gate_result_id(
    const SharedWorkGateResult &) const {
  return fail<StableId>("HouseholdNamedFixedDurationRequired");
}
Outcome<SharedWorkGateResult>
HouseholdPurchaseContracts::read_original_gate_result(const StableId &) const {
  return fail<SharedWorkGateResult>("HouseholdNamedFixedDurationRequired");
}
Status HouseholdPurchaseContracts::validate_original_gate_result(
    const SharedWorkGateResult &) const {
  return common_failure("HouseholdNamedFixedDurationRequired");
}
Outcome<WorkPeriodView>
HouseholdPurchaseContracts::read_work_period(const WorkPeriodRef &ref) const {
  const auto *binding = std::get_if<OwnerPeriodWorkBinding>(&ref.binding);
  if (!binding || binding->domain != WorkPeriodOwner::local_economy ||
      binding->activity_kind != "household_purchase" ||
      binding->registered_period_profile != "household_purchase_periods")
    return fail<WorkPeriodView>("UnsupportedHouseholdWorkPeriod");
  const WorkPeriodView *actual = nullptr;
  const HouseholdPurchaseServiceResult *source = nullptr;
  for (const auto &row : x_.state.work_periods.rows())
    if (same_work_period_ref(row.second.ref, ref)) {
      if (actual)
        return fail<WorkPeriodView>("DuplicateHouseholdWorkPeriod",
                                    binding->activity_id);
      actual = &row.second;
      source = x_.state.service_results.find(row.first);
    }
  if (!actual || !source || source->purchase_id != binding->activity_id ||
      !source->period_ref || !same_work_period_ref(*source->period_ref, ref) ||
      !header(x_.common, source->id,
              actual->disposition == WorkPeriodDisposition::provided
                  ? "household_purchase_service_completed"
                  : "household_purchase_period_interrupted"))
    return fail<WorkPeriodView>("HouseholdWorkPeriodSourceMissing",
                                binding->activity_id);
  auto checked = validate_work_period_value(*actual);
  if (!checked.ok)
    return {false, {}, checked.error};
  return {true, *actual, {}};
}
Status HouseholdPurchaseContracts::validate_actor_role(
    const WorkPeriodRef &, const ActualActorWorkRole &) const {
  return common_failure("CohortHouseholdHasNoActorRole");
}
Status HouseholdPurchaseContracts::validate_labor_allocation(
    const WorkPeriodRef &ref, const StableId &id) const {
  auto period = read_work_period(ref);
  if (!period.ok)
    return {false, {}, period.error};
  const auto *labor = x_.economy.labor_allocations.find(id);
  const auto *binding = std::get_if<OwnerPeriodWorkBinding>(&ref.binding);
  const auto *purchase =
      binding ? x_.state.purchases.find(binding->activity_id) : nullptr;
  const auto *buyer =
      purchase ? std::get_if<CohortHouseholdBuyer>(&purchase->executor)
               : nullptr;
  if (!buyer || buyer->labor_allocation_id != id ||
      !has(period.value.actual_labor_allocations, id) || !labor ||
      labor->cohort_id != buyer->cohort_id)
    return common_failure("HouseholdWorkLaborSourceMismatch", id);
  const auto *a = x_.state.accepted_terms.find(purchase->accepted_terms_ref.id);
  const auto *plan = x_.state.plans.find(purchase->plan_ref);
  if (!a || !plan)
    return common_failure("HouseholdWorkLaborSourceMismatch", id);
  if (period.value.disposition != WorkPeriodDisposition::provided)
    return validate_household_buyer_adoption(
        {x_.common, x_.economy, x_.population, x_.transport_registrations},
        *plan, *a);
  return validate_household_buyer_labor_span(
      {x_.common, x_.economy, x_.population, x_.transport_registrations}, *plan,
      *a, {{ref.from_tick, {}}, {ref.to_tick, {}}});
}
Status HouseholdPurchaseContracts::validate_facility_service(
    const WorkPeriodRef &ref, const StableId &id) const {
  auto period = read_work_period(ref);
  if (!period.ok)
    return {false, {}, period.error};
  if (!has(period.value.facility_claim_refs, id))
    return common_failure("HouseholdWorkClaimSourceMismatch", id);
  auto episode = read_facility_claim_episode(x_.claims, id, x_.now);
  if (!episode.ok)
    return {false, {}, episode.error};
  if (!episode.value.actual_reserved_span ||
      !same_time(episode.value.original_claim.from, {ref.from_tick, {}}) ||
      !same_time(episode.value.original_claim.until, {ref.to_tick, {}}))
    return common_failure("HouseholdWorkClaimCoverageMismatch", id);
  if (period.value.disposition == WorkPeriodDisposition::provided &&
      !same_time(episode.value.actual_reserved_span->to, {ref.to_tick, {}}))
    return common_failure("HouseholdWorkClaimCoverageMismatch", id);
  if (period.value.disposition != WorkPeriodDisposition::provided &&
      !period.value.reason)
    return common_failure("HouseholdInterruptedPeriodReasonRequired", id);
  return common_success();
}
Status HouseholdPurchaseContracts::validate_resource_effect(
    const WorkPeriodRef &ref, const SourceRef &source) const {
  auto period = read_work_period(ref);
  if (!period.ok)
    return {false, {}, period.error};
  for (const auto &s : period.value.resource_effect_refs)
    if (same_source_ref(s, source)) {
      if (source.domain == "commerce" &&
          source.kind == "household_retail_payment") {
        auto r = read_household_retail_settlement(x_.common, x_.retail.state,
                                                  source.id);
        if (!r.ok)
          return {false, {}, r.error};
        return validate_household_retail_settlement(*r.value);
      }
    }
  return common_failure("HouseholdWorkResourceSourceMismatch", source.id);
}
Status HouseholdPurchaseContracts::validate_exertion_source(
    const WorkPeriodRef &ref, const SourceRef &source) const {
  auto period = read_work_period(ref);
  if (!period.ok)
    return {false, {}, period.error};
  const auto &binding = std::get<OwnerPeriodWorkBinding>(ref.binding);
  const auto *p = x_.state.purchases.find(binding.activity_id);
  if (!p)
    return common_failure("HouseholdWorkOwnerMissing");
  for (const auto &segment : x_.state.service_segments)
    if (segment.purchase_id == p->purchase_id) {
      auto lower =
               compare_world_moments({ref.from_tick, {}}, segment.window.from),
           upper = compare_world_moments(segment.window.to, {ref.to_tick, {}});
      if (!lower.ok || !upper.ok)
        return common_failure("InvalidHouseholdServiceMoment");
      if (lower.value > 0 || upper.value > 0)
        continue;
      for (const auto &actual : segment.exertion_source_refs)
        if (same_source_ref(actual, source))
          return x_.environment.validate_purchase_service_segment(*p, segment);
    }
  return common_failure("HouseholdWorkExertionSourceMismatch", source.id);
}
Status HouseholdPurchaseContracts::validate_stage_result(
    const WorkPeriodRef &ref, const SourceRef &source) const {
  auto period = read_work_period(ref);
  if (!period.ok)
    return {false, {}, period.error};
  const auto &binding = std::get<OwnerPeriodWorkBinding>(ref.binding);
  const auto *result = service_result(x_, binding.activity_id);
  return result && same_source_ref(source,
                                   {"local_economy",
                                    "household_purchase_service_completed",
                                    result->id})
             ? common_success()
             : common_failure("HouseholdWorkStageSourceMismatch", source.id);
}
Status validate_household_purchase_claim_source(
    const HouseholdPurchaseContext &x, const FacilityClaim2 &claim,
    const FacilityClaimMutationReceipt &mutation, bool admission) {
  const auto *p = x.state.purchases.find(claim.holder.id);
  if (p && std::holds_alternative<NamedHouseholdBuyer>(p->executor))
    return validate_named_household_service_claim(x, claim, mutation,
                                                  admission);
  return validate_cohort_household_service_claim(x, claim, mutation, admission);
}
namespace {
Status validate_service_span(const HouseholdPurchaseContext &x,
                             const HouseholdPurchase &p,
                             const MomentSpan &span) {
  WorldMoment cursor = span.from;
  for (const auto &segment : x.state.service_segments) {
    if (segment.purchase_id != p.purchase_id)
      continue;
    auto ends = compare_world_moments(segment.window.to, span.from),
         starts = compare_world_moments(segment.window.from, span.to);
    if (!ends.ok || !starts.ok)
      return common_failure("InvalidHouseholdServiceMoment");
    if (ends.value <= 0 || starts.value >= 0)
      continue;
    auto valid = x.environment.validate_purchase_service_segment(p, segment);
    if (!valid.ok)
      return valid;
    auto start = compare_world_moments(segment.window.from, cursor);
    if (!start.ok || start.value > 0)
      return common_failure("HouseholdServiceCoverageGap", p.purchase_id);
    auto end = compare_world_moments(segment.window.to, span.to);
    if (!end.ok)
      return {false, {}, end.error};
    cursor = end.value > 0 ? span.to : segment.window.to;
  }
  return same_time(cursor, span.to)
             ? common_success()
             : common_failure("HouseholdServiceCoverageGap", p.purchase_id);
}
} // namespace
Status provide_household_purchase_service_interval_in_candidate(
    HouseholdPurchaseState &state, ActionElapsedStore &elapsed,
    const HouseholdPurchaseContext &x, const StableId &id,
    const MomentSpan &span) {
  if (&state != &x.state || &elapsed != &x.elapsed)
    return common_failure("HouseholdPurchaseCandidateMismatch");
  const auto *p = state.purchases.find(id);
  if (p && std::holds_alternative<NamedHouseholdBuyer>(p->executor))
    return provide_named_household_service_interval_in_candidate(state, elapsed,
                                                                 x, id, span);
  return provide_cohort_household_service_interval_in_candidate(state, x, id,
                                                                span);
}
Outcome<FacilityClaimMutationReceipt>
close_household_purchase_service_owner_in_candidate(
    CommonStore &c, HouseholdPurchaseState &state, ActionElapsedStore &elapsed,
    const HouseholdPurchaseContext &x, const StableId &id) {
  if (&c != &x.common || &state != &x.state || &elapsed != &x.elapsed)
    return fail<FacilityClaimMutationReceipt>(
        "HouseholdPurchaseCandidateMismatch");
  const auto *p = state.purchases.find(id);
  if (!p)
    return fail<FacilityClaimMutationReceipt>("HouseholdPurchaseMissing", id);
  Outcome<FacilityClaimMutationReceipt> result;
  if (std::holds_alternative<NamedHouseholdBuyer>(p->executor))
    result = close_named_household_service_owner_in_candidate(c, state, elapsed,
                                                              x, id);
  else {
    auto closed = close_cohort_household_period_in_candidate(c, state, x, id);
    if (!closed.ok)
      return {false, {}, closed.error};
    if (!closed.value.claim_mutation)
      return fail<FacilityClaimMutationReceipt>("HouseholdClaimAlreadyClosed",
                                                id);
    result = {true, *closed.value.claim_mutation, {}};
  }
  if (!result.ok)
    return result;
  p = state.purchases.find(id);
  const auto *a = state.accepted_terms.find(p->accepted_terms_ref.id);
  auto local = x.environment.read_local_purchase_handover(
      *p, a->plan.destination_inventory, x.now);
  if (!local.ok)
    return {false, {}, local.error};
  if (local.value) {
    auto delivered =
        local_handover(c, state, x, id, result.value.source_result_ref);
    if (!delivered.ok)
      return {false, {}, delivered.error};
  }
  return result;
}
Outcome<StableId> complete_household_purchase_service_in_candidate(
    CommonStore &c, HouseholdPurchaseState &state, FacilityClaimsState &claims,
    ActionElapsedStore &elapsed, const HouseholdPurchaseContext &x,
    const StableId &id) {
  if (&claims != &x.claims)
    return fail<StableId>("HouseholdPurchaseCandidateMismatch");
  auto mutation = close_household_purchase_service_owner_in_candidate(
      c, state, elapsed, x, id);
  if (!mutation.ok)
    return {false, {}, mutation.error};
  const auto *p = state.purchases.find(id);
  const auto *a =
      p ? state.accepted_terms.find(p->accepted_terms_ref.id) : nullptr;
  if (!a)
    return fail<StableId>("HouseholdMarketServiceMissing", id);
  auto resolved = read_household_market_service_accepted(
      x.common, x.catalog, x.state, x.private_market_custody, *a);
  if (!resolved.ok)
    return {false, {}, resolved.error};
  const auto *m = resolved.value;
  auto applied = apply_facility_claim_mutation(
      claims, x.claim_catalog, m->service_slot, mutation.value,
      mutation.value.claim_set_revision - 1, x.now, x.claim_sources);
  if (!applied.ok)
    return {false, {}, applied.error};
  claims = std::move(applied.value);
  return {true, mutation.value.source_result_ref.id, {}};
}
namespace {
Outcome<StableId>
move_purchase_cargo(CommonStore &c, HouseholdPurchaseState &state,
                    const HouseholdPurchaseContext &x,
                    const HouseholdPurchaseDeliverySource &actual) {
  const auto *current = state.purchases.find(actual.purchase_id);
  const auto *accepted =
      current ? state.accepted_terms.find(current->accepted_terms_ref.id)
              : nullptr;
  const auto *retail =
      current ? x.retail.state.settlements.find(current->retail_receipt_ref)
              : nullptr;
  if (!current || !accepted || !retail || !service_completed(x, *current) ||
      actual.actual_loss || actual.from_inventory_id.empty() ||
      actual.to_inventory_id.empty() ||
      actual.from_inventory_id == actual.to_inventory_id ||
      actual.cargo.empty())
    return fail<StableId>("HouseholdPurchaseHandoverMismatch",
                          actual.purchase_id);
  for (const auto &old : state.handovers.rows())
    if (same_source_ref(old.second.actual_delivery_source,
                        actual.actual_source)) {
      if (old.second.purchase_id != actual.purchase_id ||
          old.second.from_inventory_id != actual.from_inventory_id ||
          old.second.to_inventory_id != actual.to_inventory_id ||
          !same_time(old.second.occurred_at, actual.occurred_at) ||
          old.second.actual_lots.size() != actual.cargo.size())
        return fail<StableId>("HouseholdDeliverySourceAlreadyUsed");
      for (const auto &flow : old.second.actual_lots)
        if (std::count_if(actual.cargo.begin(), actual.cargo.end(),
                          [&](const auto &pick) {
                            return pick.inventory_id ==
                                       actual.from_inventory_id &&
                                   pick.lot_id == flow.original_lot_id &&
                                   pick.quantity == flow.quantity;
                          }) != 1)
          return fail<StableId>("HouseholdDeliveryReplayMismatch");
      return {true, old.first, {}};
    }
  if (!same_time(actual.occurred_at, x.now))
    return fail<StableId>("HouseholdHandoverActualMomentRequired",
                          actual.purchase_id);
  auto cargo = read_household_purchase_cargo(x, actual.purchase_id);
  if (!cargo.ok)
    return {false, {}, cargo.error};
  Integer moved = 0;
  for (std::size_t n = 0; n < actual.cargo.size(); ++n) {
    const auto &pick = actual.cargo[n];
    auto remaining = std::find_if(
        cargo.value.unreceived_cargo.begin(),
        cargo.value.unreceived_cargo.end(), [&](const auto &v) {
          return v.inventory_id == pick.inventory_id && v.lot_id == pick.lot_id;
        });
    if (pick.inventory_id != actual.from_inventory_id || pick.quantity < 1 ||
        remaining == cargo.value.unreceived_cargo.end() ||
        pick.quantity > remaining->quantity ||
        __builtin_add_overflow(moved, pick.quantity, &moved))
      return fail<StableId>("HouseholdDeliveryCargoOutsidePurchase",
                            pick.lot_id);
    for (std::size_t j = 0; j < n; ++j)
      if (actual.cargo[j].lot_id == pick.lot_id)
        return fail<StableId>("DuplicateHouseholdDeliveryLot", pick.lot_id);
  }
  const bool received =
      actual.to_inventory_id == accepted->plan.destination_inventory;
  if (received &&
      moved > cargo.value.purchased_quantity - cargo.value.received_quantity)
    return fail<StableId>("HouseholdDeliveryQuantityOverflow");
  if (!c.resource_execution_capture_identity())
    return fail<StableId>("HouseholdHandoverCaptureRequired");
  ResourceRecipe recipe;
  recipe.operations = {TransferSelectedLots{actual.from_inventory_id,
                                            actual.to_inventory_id,
                                            actual.cargo,
                                            {retail->terms.buyer},
                                            holder(actual.purchase_id),
                                            retail->terms.buyer}};
  recipe.expected_versions = {
      c.read_version({ResourceKey::Kind::inventory, actual.from_inventory_id}),
      c.read_version({ResourceKey::Kind::inventory, actual.to_inventory_id})};
  const auto capture = c.resource_execution_drafts().size();
  auto applied = c.apply_resources(recipe);
  if (!applied.ok)
    return {false, {}, applied.error};
  const auto &drafts = c.resource_execution_drafts();
  if (drafts.size() != capture + 1 || drafts.back().operations.size() != 1)
    return fail<StableId>("HouseholdHandoverExecutionMissing");
  auto valid = validate_selected_resource_execution(
      recipe.operations[0], drafts.back().operations[0]);
  if (!valid.ok)
    return {false, {}, valid.error};
  // The exact output IDs come from the Common operation, including splits. An
  // intermediate carrier leg retains the original purchase reservation; arrival
  // at the final household container releases it for the next actual ration.
  std::vector<VerifiedTradeLot> flows;
  std::vector<CargoSelection> onward;
  for (const auto &flow : drafts.back().operations[0].lot_flows) {
    flows.push_back(
        {flow.source_lot_id, flow.destination_lot_id, flow.quantity});
    onward.push_back(
        {actual.to_inventory_id, flow.destination_lot_id, flow.quantity});
  }
  if (!received) {
    ResourceRecipe reserve;
    reserve.operations = {ReserveSelectedLots{actual.to_inventory_id,
                                              onward,
                                              {retail->terms.buyer},
                                              holder(actual.purchase_id)}};
    reserve.expected_versions = {
        c.read_version({ResourceKey::Kind::inventory, actual.to_inventory_id})};
    auto held = c.apply_resources(reserve);
    if (!held.ok)
      return {false, {}, held.error};
    const auto &actual_drafts = c.resource_execution_drafts();
    if (actual_drafts.size() != capture + 2 ||
        actual_drafts.back().operations.size() != 1)
      return fail<StableId>("HouseholdCargoReservationExecutionMissing");
    auto reserved = validate_selected_resource_execution(
        reserve.operations[0], actual_drafts.back().operations[0]);
    if (!reserved.ok)
      return {false, {}, reserved.error};
    for (const auto &flow : actual_drafts.back().operations[0].lot_flows)
      if (flow.source_lot_id != flow.destination_lot_id)
        return fail<StableId>("HouseholdCargoUnexpectedReservationSplit");
  }
  auto id = fact(c, "household_handover",
                 received ? "household_purchase_delivered"
                          : "household_purchase_cargo_handover");
  if (!id.ok)
    return id;
  state.handovers.insert(id.value,
                         {id.value, actual.purchase_id, actual.actual_source,
                          actual.occurred_at, actual.from_inventory_id,
                          actual.to_inventory_id, std::move(flows)});
  if (received &&
      cargo.value.received_quantity + moved == cargo.value.purchased_quantity) {
    auto *next = state.purchases.find(actual.purchase_id);
    if (next->phase_revision == INT64_MAX)
      return fail<StableId>("HouseholdPurchaseRevisionOverflow");
    next->state = HouseholdPurchasePhase::received;
    ++next->phase_revision;
    if (std::holds_alternative<CohortHouseholdBuyer>(next->executor))
      next->terminal_result_ref =
          SourceRef{"local_economy", "household_purchase_delivered", id.value};
  }
  return id;
}
Outcome<StableId> local_handover(CommonStore &c, HouseholdPurchaseState &state,
                                 const HouseholdPurchaseContext &x,
                                 const StableId &id, const SourceRef &source) {
  const auto *p = state.purchases.find(id);
  const auto *accepted =
      p ? state.accepted_terms.find(p->accepted_terms_ref.id) : nullptr;
  const auto *receipt =
      p ? x.retail.state.settlements.find(p->retail_receipt_ref) : nullptr;
  if (!p || !accepted || !receipt)
    return fail<StableId>("HouseholdPurchaseSourceMissing", id);
  auto permitted = x.environment.read_local_purchase_handover(
      *p, accepted->plan.destination_inventory, x.now);
  if (!permitted.ok)
    return {false, {}, permitted.error};
  if (!permitted.value)
    return fail<StableId>("HouseholdPurchaseJourneyRequired", id);
  HouseholdPurchaseDeliverySource actual{source,
                                         x.now,
                                         id,
                                         p->holding_inventory_ref,
                                         accepted->plan.destination_inventory,
                                         {},
                                         false};
  for (const auto &lot : receipt->lots)
    actual.cargo.push_back(
        {p->holding_inventory_ref, lot.destination_lot_id, lot.quantity});
  return move_purchase_cargo(c, state, x, actual);
}
} // namespace
Outcome<StableId> apply_household_purchase_delivery_in_candidate(
    CommonStore &c, HouseholdPurchaseState &state,
    const HouseholdPurchaseContext &x, const StableId &id,
    const SourceRef &ref) {
  if (&c != &x.common || &state != &x.state)
    return fail<StableId>("HouseholdPurchaseCandidateMismatch");
  const auto *p = state.purchases.find(id);
  bool replay = false;
  for (const auto &row : state.handovers.rows())
    if (row.second.purchase_id == id &&
        same_source_ref(row.second.actual_delivery_source, ref))
      replay = true;
  if (!p || (!replay && p->state != HouseholdPurchasePhase::awaiting_dispatch &&
             p->state != HouseholdPurchasePhase::in_transit &&
             p->state != HouseholdPurchasePhase::blocked &&
             p->state != HouseholdPurchasePhase::settling))
    return fail<StableId>("HouseholdDeliveryPhaseMismatch", id);
  auto actual = x.environment.read_purchase_delivery(*p, ref);
  if (!actual.ok)
    return {false, {}, actual.error};
  if (actual.value.purchase_id != id ||
      !same_source_ref(actual.value.actual_source, ref))
    return fail<StableId>("HouseholdDeliverySourceMismatch", id);
  return move_purchase_cargo(c, state, x, actual.value);
}
Outcome<StableId> change_household_purchase_phase_in_candidate(
    CommonStore &c, HouseholdPurchaseState &state,
    const HouseholdPurchaseContext &x, const StableId &id,
    HouseholdPurchasePhase next, const SourceRef &cause) {
  if (&c != &x.common || &state != &x.state)
    return fail<StableId>("HouseholdPurchaseCandidateMismatch");
  const auto *p = state.purchases.find(id);
  if (!p || p->state == HouseholdPurchasePhase::received ||
      p->state == HouseholdPurchasePhase::lost ||
      p->state == HouseholdPurchasePhase::cancelled || p->state == next)
    return fail<StableId>("HouseholdPurchasePhaseMismatch", id);
  if (!service_completed(x, *p))
    return fail<StableId>("HouseholdServiceCompletionRequired", id);
  const bool dispatch =
      next == HouseholdPurchasePhase::in_transit &&
      (p->state == HouseholdPurchasePhase::awaiting_dispatch ||
       p->state == HouseholdPurchasePhase::blocked);
  const bool defer = (next == HouseholdPurchasePhase::blocked ||
                      next == HouseholdPurchasePhase::settling) &&
                     (p->state == HouseholdPurchasePhase::awaiting_dispatch ||
                      p->state == HouseholdPurchasePhase::in_transit ||
                      p->state == HouseholdPurchasePhase::blocked);
  const bool lost = next == HouseholdPurchasePhase::lost &&
                    p->state != HouseholdPurchasePhase::servicing;
  if (!dispatch && !defer && !lost)
    return fail<StableId>("HouseholdPurchaseOriginalPhaseWriterRequired", id);
  auto valid =
      x.environment.validate_purchase_phase_cause(*p, next, cause, x.now);
  if (!valid.ok)
    return {false, {}, valid.error};
  if (lost) {
    auto actual = x.environment.read_purchase_delivery(*p, cause);
    if (!actual.ok)
      return {false, {}, actual.error};
    if (!actual.value.actual_loss)
      return fail<StableId>("HouseholdPurchaseActualLossRequired", id);
    for (const auto &inventory : c.state().inventories.rows())
      for (const auto &lot : inventory.second.lots)
        if (lot.reserved_by == holder(id))
          return fail<StableId>("HouseholdPurchaseLossCargoStillPresent", id);
  }
  if (p->phase_revision == INT64_MAX)
    return fail<StableId>("HouseholdPurchaseRevisionOverflow");
  HouseholdPurchasePhaseChange change;
  change.purchase_id = id;
  change.before = p->state;
  change.after = next;
  change.revision_before = p->phase_revision;
  change.revision_after = p->phase_revision + 1;
  change.actual_cause = cause;
  change.occurred_at = x.now;
  auto actual = fact(c, "household_phase", "household_purchase_phase_changed");
  if (!actual.ok)
    return actual;
  change.id = actual.value;
  state.phase_changes.insert(actual.value, change);
  auto *edited = state.purchases.find(id);
  edited->state = next;
  edited->phase_revision = change.revision_after;
  if (dispatch)
    edited->delivery_binding = cause;
  if (lost && std::holds_alternative<CohortHouseholdBuyer>(edited->executor))
    edited->terminal_result_ref = SourceRef{
        "local_economy", "household_purchase_phase_changed", actual.value};
  return actual;
}
Outcome<RunStop> HouseholdPurchaseContracts::read_action_continuation_stop(
    const ActionTimeRegistration &r, const ActionTimeOwnerProof &proof,
    const WorldMoment &) const {
  auto actual = read_action_time_owner(r, proof.action_ref);
  if (!actual.ok)
    return {false, {}, actual.error};
  if (actual.value.phase == ActionTimeOwnerPhase::terminal)
    return {true, RunStop::completed, {}};
  if (actual.value.phase == ActionTimeOwnerPhase::suspended)
    return {true, RunStop::paused, {}};
  return {true, RunStop::awaiting_permit, {}};
}
Status validate_household_purchases(const HouseholdPurchaseContext &x) {
  if (x.selected_household_version != 2 && x.selected_household_version != 3)
    return common_failure("UnsupportedHouseholdPurchasesVersion");
  auto reserve_rules = validate_household_reserve_rules(x.common, x.shared,
                                                        x.economy, x.catalog);
  if (!reserve_rules.ok)
    return reserve_rules;
  if (x.catalog.duration_definition_id.empty() ||
      x.catalog.continuation_rule_id.empty())
    return common_failure("HouseholdPurchaseDurationDefinitionRequired");
  std::set<StableId> current_households, receipts, claims, actions,
      covered_results, covered_handovers;
  for (const auto &row : x.state.policies.rows()) {
    const auto &p = row.second;
    auto allowance = read_household_purchase_allowance(
        x, p.acceptance_source_ref, p.revision);
    if (!allowance.ok)
      return {false, {}, allowance.error};
    auto authority = x.controls.validate_accepted(p.controller_authority_ref,
                                                  p.accepted_tick);
    if (!authority.ok)
      return authority;
    auto caps = item_caps(x.shared, p.allowed_items);
    if (!caps.ok)
      return caps;
    if (row.first != p.id ||
        !header(x.common, p.id, "household_purchase_policy_accepted") ||
        !same_source_ref(
            p.acceptance_source_ref,
            {"local_economy", "household_purchase_policy_accepted", p.id}) ||
        p.controller_authority_ref.operation != household_purchase_operation ||
        p.controller_authority_ref.actor_id != p.controller_actor_id ||
        p.controller_authority_ref.household_group_id != p.household_id ||
        p.accepted_tick < 0 || p.accepted_tick > x.now.half_day_tick ||
        p.valid_from_tick < p.accepted_tick ||
        p.valid_until_tick <= p.valid_from_tick || p.revision < 1 ||
        p.merchandise_cap_qian < 1 || p.transport_cap_qian < 0 ||
        p.total_cap_qian < 1 || p.merchandise_cap_qian > p.total_cap_qian ||
        p.transport_cap_qian > p.total_cap_qian ||
        (!p.closed_source_ref &&
         !current_households.insert(p.household_id).second))
      return common_failure("InvalidHouseholdPurchasePolicy", row.first);
    if (p.closed_source_ref) {
      const auto *next = x.state.policies.find(p.closed_source_ref->id);
      if (!next ||
          !same_source_ref(*p.closed_source_ref, next->acceptance_source_ref) ||
          next->household_id != p.household_id ||
          next->revision != p.revision + 1 ||
          next->accepted_tick < p.accepted_tick)
        return common_failure("HouseholdPurchasePolicyRevisionMismatch", p.id);
    }
  }
  for (const auto &row : x.state.plans.rows()) {
    const auto &p = row.second;
    if (p.mobile_custody_adoption) {
      if (x.selected_household_version != 3 || !x.mobile_custody)
        return common_failure("HouseholdMobileCustodyProviderRequired",
                              p.plan_id);
      auto mobile = validate_household_mobile_custody(x, *x.mobile_custody, p);
      if (!mobile.ok)
        return mobile;
    }
    auto known_market = household_market_catalog_contains(
        x.common, x.catalog, x.state, x.private_market_custody,
        p.known_market_ref);
    if (!known_market.ok)
      return {false, {}, known_market.error};
    const auto *policy = x.state.policies.find(p.accepted_policy_ref.id);
    if (row.first != p.plan_id ||
        !header(x.common, p.plan_id, "household_purchase_planned") || !policy ||
        !same_source_ref(policy->acceptance_source_ref,
                         p.accepted_policy_ref) ||
        policy->revision != p.accepted_policy_revision ||
        p.household_id != policy->household_id ||
        !same_control(p.controller_authority_ref,
                      policy->controller_authority_ref) ||
        p.spend_cap_qian < 1 ||
        p.spend_cap_qian > policy->merchandise_cap_qian ||
        !has(policy->allowed_market_refs, p.known_market_ref) ||
        !has(policy->allowed_destination_inventories,
             p.destination_inventory) ||
        !known_market.value)
      return common_failure("InvalidHouseholdPurchasePlan", row.first);
    auto caps = item_caps(x.shared, p.allowed_items);
    if (!caps.ok)
      return caps;
    for (const auto &cap : p.allowed_items) {
      const auto found = std::find_if(
          policy->allowed_items.begin(), policy->allowed_items.end(),
          [&](const auto &r) { return r.item_id == cap.item_id; });
      if (found == policy->allowed_items.end() ||
          cap.quantity_cap > found->quantity_cap)
        return common_failure("HouseholdPlanCapMismatch", p.plan_id);
    }
  }
  HouseholdPurchaseContracts original{x};
  for (const auto &row : x.state.purchases.rows()) {
    const auto &p = row.second;
    const auto *a = x.state.accepted_terms.find(p.accepted_terms_ref.id);
    const auto *plan = x.state.plans.find(p.plan_ref);
    const auto *receipt = x.retail.state.settlements.find(p.retail_receipt_ref);
    if (row.first != p.purchase_id || !a || !plan || !receipt ||
        !header(x.common, p.purchase_id, "household_purchase_accepted") ||
        a->plan.plan_id != p.plan_ref ||
        !same_source_ref(a->source, p.accepted_terms_ref) ||
        !same_source_ref(a->source, accepted_source(p.purchase_id)) ||
        !same_buyer(a->plan.execution_choice, p.executor) ||
        !receipts.insert(receipt->id).second ||
        !claims.insert(p.service_claim_ref).second ||
        p.holding_inventory_ref != receipt->terms.custody_inventory_id ||
        p.phase_revision < 1)
      return common_failure("InvalidHouseholdPurchase", row.first);
    auto executor = validate_household_buyer_adoption(
        {x.common, x.economy, x.population, x.transport_registrations}, *plan,
        *a);
    if (!executor.ok)
      return executor;
    auto retail = original.validate_household_retail_settlement(*receipt);
    if (!retail.ok)
      return retail;
    if (std::holds_alternative<NamedHouseholdBuyer>(p.executor)) {
      auto named = validate_named_household_service(x, p);
      if (!named.ok)
        return named;
      const auto &binding =
          std::get<ActorActionWorkBinding>(p.execution_binding);
      if (!actions.insert(binding.action_id).second)
        return common_failure("DuplicateHouseholdAction", binding.action_id);
      for (const auto &h : x.state.handovers.rows())
        if (h.second.purchase_id == p.purchase_id)
          covered_handovers.insert(h.first);
      for (const auto &ep : x.state.named_service_claim_episodes)
        if (ep.purchase_id == p.purchase_id && ep.ordinal > 0 &&
            !claims.insert(ep.claim_id).second)
          return common_failure("DuplicateNamedHouseholdClaim", ep.claim_id);
      continue;
    }
    auto checked = validate_cohort_household_service(x, p);
    if (!checked.ok)
      return checked;
    auto attempts = read_cohort_household_service_attempts(x, p.purchase_id);
    if (!attempts.ok)
      return {false, {}, attempts.error};
    for (const auto &attempt : attempts.value) {
      const auto &claim = attempt.claim_episode.original_claim;
      if (claim.claim_id != p.service_claim_ref &&
          !claims.insert(claim.claim_id).second)
        return common_failure("DuplicateCohortHouseholdClaim", claim.claim_id);
      if (attempt.result)
        covered_results.insert(attempt.result->id);
    }
    for (const auto &h : x.state.handovers.rows())
      if (h.second.purchase_id == p.purchase_id)
        covered_handovers.insert(h.first);
  }
  for (const auto &row : x.state.named_service_events.rows()) {
    const auto *p = x.state.purchases.find(row.second.purchase_id);
    if (row.first != row.second.event_id || !p ||
        !std::holds_alternative<NamedHouseholdBuyer>(p->executor))
      return common_failure("OrphanNamedHouseholdEvent", row.first);
  }
  for (const auto &ep : x.state.named_service_claim_episodes) {
    const auto *p = x.state.purchases.find(ep.purchase_id);
    if (!p || !std::holds_alternative<NamedHouseholdBuyer>(p->executor))
      return common_failure("OrphanNamedHouseholdEpisode", ep.claim_id);
  }
  for (const auto &f : x.common.state().facts.rows())
    if (f.second.domain == "local_economy" &&
        f.second.kind == "household_purchase_service" &&
        !x.state.named_service_events.find(f.first))
      return common_failure("MissingNamedHouseholdEvent", f.first);
  if (covered_results.size() != x.state.service_results.size() ||
      covered_handovers.size() != x.state.handovers.size() ||
      x.state.accepted_terms.size() != x.state.purchases.size())
    return common_failure("HouseholdPurchaseSourceCoverage");
  for (const auto &segment : x.state.service_segments)
    if (!x.state.purchases.find(segment.purchase_id))
      return common_failure("OrphanHouseholdServiceSegment");
  for (const auto &result : x.state.work_periods.rows())
    if (!covered_results.count(result.first))
      return common_failure("OrphanHouseholdWorkPeriod", result.first);
  for (const auto &change : x.state.phase_changes.rows())
    if (!x.state.purchases.find(change.second.purchase_id) ||
        change.first != change.second.id)
      return common_failure("OrphanHouseholdPhaseChange", change.first);
  for (const auto &f : x.common.state().facts.rows())
    if (f.second.domain == "local_economy" &&
        (f.second.kind == "household_purchase_delivered" ||
         f.second.kind == "household_purchase_cargo_handover") &&
        !x.state.handovers.find(f.first))
      return common_failure("MissingHouseholdCargoHandover", f.first);
  for (const auto &r : x.retail.state.settlements.rows())
    if (!receipts.count(r.first))
      return common_failure("HouseholdRetailBuyerSourceMissing", r.first);
  for (const auto &action : x.common.state().actions.rows())
    if (action.second.kind == "household_purchase" &&
        !actions.count(action.first))
      return common_failure("OrphanHouseholdPurchaseAction", action.first);
  return common_success();
}
Status HouseholdPurchaseContracts::enumerate_action_time_owners(
    const ActionTimeRegistration &registration,
    ActionTimeOwnerIdSink &sink) const {
  if (registration.action_kind != "household_purchase" ||
      registration.owner_domain != "local_economy" ||
      registration.mode != ActionTimeMode::fixed_duration ||
      (x_.selected_household_version != 2 &&
       x_.selected_household_version != 3) ||
      registration.profile_version != x_.selected_household_version ||
      registration.source_schema != 1)
    return common_failure("UnsupportedHouseholdActionTime");
  for (const auto &p : x_.state.purchases.rows())
    if (const auto *binding =
            std::get_if<ActorActionWorkBinding>(&p.second.execution_binding)) {
      auto actual = read_action_time_owner(registration, binding->action_id);
      if (!actual.ok)
        return {false, {}, actual.error};
      auto appended = sink.append(binding->action_id);
      if (!appended.ok)
        return appended;
    }
  return common_success();
}

} // namespace hanmo::resident
