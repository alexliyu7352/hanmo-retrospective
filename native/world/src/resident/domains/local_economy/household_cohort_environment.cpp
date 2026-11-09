#include "household_market_resolver.hpp"
#include "household_cohort_environment.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> v) {
  if (!v.ok)
    throw Failure{v.error};
  return std::move(v.value);
}
void take(Status v) {
  if (!v.ok)
    throw Failure{v.error};
}
int cmp(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b));
}
void header(const CommonStore &c, const SourceRef &r) {
  const auto *h = c.state().facts.find(r.id);
  if (!h || h->id != r.id || h->domain != r.domain || h->kind != r.kind)
    fail("CohortHouseholdOriginalSourceMissing", r.id);
}
HouseholdPurchaseCoverage coverage(const HouseholdCohortEnvironmentContext &x,
                                   const HouseholdPurchase &p,
                                   const MomentSpan &span) {
  const auto now = take(x.clock.read_current_moment());
  if (cmp(now, take(x.spatial.current_moment())) ||
      cmp(span.from, span.to) >= 0 || cmp(span.to, now) > 0)
    fail("CohortHouseholdCoverageWindowMismatch", p.purchase_id);
  const auto *accepted = x.state.accepted_terms.find(p.accepted_terms_ref.id);
  const auto *plan = x.state.plans.find(p.plan_ref);
  const auto *buyer = std::get_if<CohortHouseholdBuyer>(&p.executor);
  if (!accepted || !plan || !buyer || buyer->workers != 1 ||
      accepted->plan.plan_id != p.plan_ref ||
      !same_source_ref(accepted->source, p.accepted_terms_ref))
    fail("CohortHouseholdActualAcceptanceRequired", p.purchase_id);
  const auto *adopted =
      std::get_if<CohortHouseholdBuyer>(&accepted->plan.execution_choice);
  if (!adopted || adopted->cohort_id != buyer->cohort_id ||
      adopted->population_selection_ref != buyer->population_selection_ref ||
      adopted->labor_allocation_id != buyer->labor_allocation_id ||
      adopted->workers != 1 ||
      !same_source_ref(adopted->accepted_household_policy_ref,
                       buyer->accepted_household_policy_ref))
    fail("CohortHouseholdAcceptedExecutorMismatch", p.purchase_id);
  header(x.common, accepted->source);
  take(validate_household_buyer_labor_span(
      {x.common, x.economy, x.population, &x.registrations}, *plan, *accepted,
      span));
  const auto selection_ref = take(population_selection_source_ref(
      x.economy, x.population, buyer->population_selection_ref));
  const auto *selection = take(
      read_population_selection_source(x.economy, x.population, selection_ref));
  if (selection->purpose != PopulationSelectionPurpose::household_purchase ||
      selection->selected_persons != 1 ||
      selection->selected_labor_eligible != 1 ||
      selection->partition_policy !=
          PopulationPartitionPolicy::labor_eligible_only ||
      !same_source_ref(
          selection->owner_business_ref,
          {"local_economy", "household_purchase_planned", plan->plan_id}))
    fail("CohortHouseholdAcceptedSelectionMismatch", selection_ref.id);
  CivilianPopulationRef original = selection->source;
  const auto *materialized = x.population.materializations.find(selection->id);
  const auto *selected_buyer =
      std::get_if<CohortHouseholdBuyer>(&plan->execution_choice);
  if (!selected_buyer)
    fail("CohortHouseholdOriginalBuyerMissing", p.purchase_id);
  if (buyer->labor_allocation_id != selected_buyer->labor_allocation_id) {
    if (!materialized ||
        materialized->result_cohort_ref.cohort_id != buyer->cohort_id ||
        !materialized->result_cohort_ref.transport_binding_id)
      fail("CohortHouseholdMaterializedExecutorMissing", p.purchase_id);
    original = materialized->result_cohort_ref;
  }
  if (original.cohort_id != buyer->cohort_id)
    fail("CohortHouseholdPopulationIdentityMismatch", p.purchase_id);
  const auto population =
      take(read_population_revision_source(x.economy, x.population, original));
  if (population.headcount < 1 || population.labor_capacity < 1)
    fail("CohortHouseholdEligiblePersonMissing", p.purchase_id);
  // An actual injury/disease population source needs its own purpose coverage.
  // Never infer normal labor from a textual condition ref or today's total.
  if (population.condition_source_ref)
    fail("CohortHouseholdConditionCoverageRequired",
         population.condition_source_ref->id);
  MovementAnchor actual;
  if (population.location_binding_ref.domain == "local_economy" &&
      population.location_binding_ref.kind == "initial_cohort_site") {
    const auto *seed = x.economy_catalog.cohorts.find(original.cohort_id);
    if (!seed || population.location_binding_ref.id != seed->id ||
        seed->household_group_id != plan->household_id || seed->site_id.empty())
      fail("CohortHouseholdInitialLocationSourceMissing", original.cohort_id);
    actual = take(x.spatial.original_site_anchor(seed->site_id));
  } else {
    auto location = take(read_population_stationary_location_span(
        x.common, x.carriers, x.movement, x.registrations, x.population,
        original, population.location_binding_ref, span));
    actual = std::move(location.actual_anchor);
  }
  const auto at_market = take(x.spatial.original_site_anchor(
      take(read_household_market_service_accepted(x.common, x.catalog, x.state,
                                                  x.private_market_custody,
                                                  *accepted))
          ->place_id));
  if (!take(same_spatial_anchor(actual, at_market)))
    fail("CohortHouseholdServiceNotAtMarket", p.purchase_id);
  // The original allocation's lifetime is already replayed above. Other changes
  // after that accepted population revision must have a real known time source;
  // current count equality cannot erase an intervening loss or incapacitation.
  for (const auto &event : x.population.revision_sources)
    if (event.cohort_id == original.cohort_id &&
        event.revision_after > original.population_revision) {
      if (x.population_history) {
        const auto &history = *x.population_history;
        if (&history.common != &x.common || &history.economy != &x.economy ||
            &history.population != &x.population ||
            &history.transport != &x.registrations ||
            cmp(take(history.clock.read_current_moment()), now))
          fail("CohortHouseholdPopulationHistorySnapshotMismatch",
               p.purchase_id);
        const auto change =
            take(read_population_original_change_time(history, event));
        if (cmp(change.occurred_at, span.to) < 0)
          fail("CohortHouseholdPopulationChangedDuringService",
               event.actual_source.id);
        continue;
      }
      bool known = false;
      for (const auto &row : x.registrations.registrations.rows())
        if (same_source_ref(event.actual_source,
                            {"travel", "transport_registration", row.first})) {
          known = true;
          if (cmp(row.second.occurred_at, span.to) < 0)
            fail("CohortHouseholdPopulationChangedDuringService",
                 event.actual_source.id);
        }
      for (const auto &row : x.registrations.releases.rows())
        if (same_source_ref(event.actual_source,
                            {"travel", "transport_release", row.first})) {
          known = true;
          if (cmp(row.second.occurred_at, span.to) < 0)
            fail("CohortHouseholdPopulationChangedDuringService",
                 event.actual_source.id);
        }
      // A split source is timed by the unique original S1 registration that
      // caused that exact quantity transfer, not by transfer.effective_tick +
      // zero.
      if (event.actual_source.domain == "local_economy" &&
          event.actual_source.kind == "population_transfer") {
        const auto *transfer =
            x.economy.population_transfers.find(event.actual_source.id);
        const auto *registration =
            transfer
                ? x.registrations.registrations.find(transfer->source_fact_id)
                : nullptr;
        if (registration) {
          header(x.common,
                 {"travel", "transport_registration", registration->id});
          known = true;
          if (cmp(registration->occurred_at, span.to) < 0)
            fail("CohortHouseholdPopulationChangedDuringService",
                 event.actual_source.id);
        }
      }
      if (!known)
        fail("CohortHouseholdPopulationChangeTimeRequired",
             event.actual_source.id);
    }
  return {selection_ref, span, {buyer->labor_allocation_id}, {}};
}
} // namespace
Outcome<HouseholdPurchaseCoverage> read_cohort_household_actual_coverage(
    const HouseholdCohortEnvironmentContext &x, const HouseholdPurchase &p,
    const MomentSpan &span) {
  try {
    return {true, coverage(x, p, span), {}};
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
Status validate_cohort_household_actual_segment(
    const HouseholdCohortEnvironmentContext &x, const HouseholdPurchase &p,
    const HouseholdServiceSegment &segment) {
  try {
    const auto original = coverage(x, p, segment.window);
    if (segment.purchase_id != p.purchase_id ||
        !same_source_ref(segment.actual_coverage_source,
                         original.actual_source) ||
        segment.labor_allocation_refs != original.actual_labor_allocations ||
        !segment.exertion_source_refs.empty())
      fail("CohortHouseholdActualCoverageMismatch", p.purchase_id);
    return common_success();
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
} // namespace hanmo::resident
