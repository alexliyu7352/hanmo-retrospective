#include "household_service_runtime.hpp"
#include "resident/kernel/command_types.hpp"
#include <limits>
namespace hanmo::resident {
namespace household_runtime {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const StableId &code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> r) {
  if (!r.ok)
    throw Failure{r.error};
  return std::move(r.value);
}
template <class T, class F> Outcome<T> guard(F &&f) {
  try {
    return {true, f(), {}};
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
int cmp(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b));
}
void binding(const HouseholdServiceWriteBinding &w, const WorldMoment &at) {
  if (&w.common != &w.household.common || &w.state != &w.household.state ||
      &w.state != &w.market.purchases || &w.elapsed != &w.household.elapsed ||
      &w.household.claims != &w.market.claims || cmp(w.household.now, at) ||
      cmp(w.market.now, at))
    fail("HouseholdRuntimeCandidateMismatch");
}
void completion(const HouseholdPurchase &before, const HouseholdPurchase &after,
                ExactPhaseWriteContext &writes) {
  if (const auto *action =
          std::get_if<ActorActionWorkBinding>(&before.execution_binding))
    if (after.terminal_result_ref) {
      ActionCompletion result;
      result.completed = true;
      if (!writes.advance.completed_actions.insert(action->action_id,
                                                   std::move(result)))
        fail("DuplicateHouseholdActionCompletion", action->action_id);
    }
}
} // namespace household_runtime
using namespace household_runtime;
Status provide_household_service_interval(HouseholdServiceWriteBinding &w,
                                          const MomentSpan &span,
                                          ExactPhaseWriteContext &) {
  return guard<Done>([&] {
    binding(w, span.to);
    if (cmp(span.from, span.to) >= 0)
      fail("InvalidHouseholdRuntimeSpan");
    std::vector<StableId> active;
    for (const auto &row : w.state.purchases.rows())
      if (row.second.state == HouseholdPurchasePhase::servicing)
        active.push_back(row.first);
    for (const auto &id : active)
      take(provide_household_purchase_service_interval_in_candidate(
          w.state, w.elapsed, w.household, id, span));
    return Done{};
  });
}
Outcome<std::vector<ExactEventPrediction>>
predict_household_failed_period_closures(const HouseholdServiceReadBinding &r,
                                         const WorldMoment &current,
                                         const WorldMoment &until) {
  return guard<std::vector<ExactEventPrediction>>([&] {
    if (cmp(current, until) > 0)
      fail("InvalidHouseholdPredictionWindow");
    std::vector<ExactEventPrediction> events;
    for (const auto &row : r.household.state.purchases.rows()) {
      const auto &p = row.second;
      if (!std::holds_alternative<CohortHouseholdBuyer>(p.executor))
        continue;
      auto attempts = take(
          read_cohort_household_service_attempts(r.household, p.purchase_id));
      const auto &attempt = attempts.back();
      if (attempt.result || !attempt.claim_episode.close_receipt_ref)
        continue;
      const auto at = attempt.claim_episode.original_claim.until;
      if (cmp(at, current) < 0)
        fail("UnclosedHouseholdOriginalPeriod", p.purchase_id);
      if (cmp(at, until) <= 0)
        events.push_back({at, ExactStage::action_completions,
                          p.accepted_terms_ref, 1, p.phase_revision});
    }
    return events;
  });
}
Outcome<ExactEventApplied>
apply_household_failed_period_closure(HouseholdServiceWriteBinding &w,
                                      const ExactEventPrediction &prediction) {
  return guard<ExactEventApplied>([&] {
    binding(w, prediction.at);
    if (prediction.stage != ExactStage::action_completions ||
        prediction.source_schema != 1 ||
        prediction.actual_source_basis.domain != "local_economy" ||
        prediction.actual_source_basis.kind != "household_purchase_accepted")
      fail("HouseholdPeriodEventSourceMismatch");
    const auto *p = w.state.purchases.find(prediction.actual_source_basis.id);
    if (!p || p->phase_revision != prediction.dependency_revision ||
        !same_source_ref(p->accepted_terms_ref, prediction.actual_source_basis))
      fail("StaleHouseholdPeriodPrediction");
    auto result = take(close_cohort_household_period_in_candidate(
        w.common, w.state, w.household, p->purchase_id));
    if (result.claim_mutation)
      fail("HouseholdDueClaimMustUseRegisteredHolder");
    return ExactEventApplied{result.actual_result, 1};
  });
}
Status
validate_household_service_runtime_sources(const HouseholdServiceReadBinding &r,
                                           const WorldMoment &at) {
  return guard<Done>([&] {
    if (cmp(r.household.now, at) || cmp(r.market.now, at) ||
        &r.household.state != &r.market.purchases ||
        &r.household.claims != &r.market.claims)
      fail("HouseholdRuntimeReadSnapshotMismatch");
    take(validate_household_purchases(r.household));
    take(validate_market_service_sources(r.market));
    if (r.market.public_staffing)
      take(validate_public_market_staffing(*r.market.public_staffing));
    else
      take(validate_unselected_public_household_market(
          r.household.common, r.household.catalog, r.household.state,
          r.household.population, r.household.retail.catalog));
    return Done{};
  });
}
FacilityHolderRegistration make_household_service_holder_registration(
    HouseholdServiceWorldFactory factory) {
  FacilityHolderRegistration r;
  r.selection = {{ReservationRef::Kind::domain_activity, "local_economy",
                  "household_purchase"},
                 "household_purchase_facility",
                 2};
  r.until_stage = ExactStage::action_completions;
  r.apply_until = [factory](WorldBundle &world, const FacilityClaim2 &claim,
                            const WorldMoment &at,
                            ExactPhaseWriteContext &writes) {
    return guard<FacilityClaimMutationReceipt>([&] {
      if (!factory.write)
        fail("HouseholdWorldFactoryRequired");
      auto lease = take(factory.write(world, at));
      if (!lease)
        fail("HouseholdWorldLeaseRequired");
      auto w = lease->bindings();
      binding(w, at);
      const auto *p = w.state.purchases.find(claim.holder.id);
      if (!p || cmp(claim.until, at))
        fail("HouseholdHolderNotDue", claim.claim_id);
      const auto before = *p;
      auto result = take(close_household_purchase_service_owner_in_candidate(
          w.common, w.state, w.elapsed, w.household, p->purchase_id));
      completion(before, *w.state.purchases.find(before.purchase_id), writes);
      return result;
    });
  };
  r.reconcile_capacity =
      [factory](WorldBundle &world,
                const FacilityCapacityReconciliationRequest &request,
                const FacilityReconciliationProgress &progress,
                ExactPhaseWriteContext &writes) {
        return guard<FacilityClaimMutationReceipt>([&] {
          if (!factory.write ||
              progress.position >= progress.ordered_claims.size())
            fail("HouseholdWorldFactoryRequired");
          auto lease = take(factory.write(world, request.occurred_at));
          if (!lease)
            fail("HouseholdWorldLeaseRequired");
          auto w = lease->bindings();
          binding(w, request.occurred_at);
          const auto *p = w.state.purchases.find(
              progress.ordered_claims[progress.position].holder.id);
          if (!p)
            fail("HouseholdHolderMissing");
          const auto before = *p;
          auto result = take(reconcile_household_market_service_in_candidate(
              w.common, w.state, w.elapsed, w.household, w.market, request,
              progress));
          completion(before, *w.state.purchases.find(before.purchase_id),
                     writes);
          return result;
        });
      };
  return r;
}
FacilityCapacityReconciliationRegistration
make_market_service_reconciliation_registration(
    const StableId &provider, HouseholdServiceWorldFactory factory) {
  const unsigned char provider_tag = provider == "local_economy" ? 1
                                     : provider == "commerce"    ? 2
                                                                 : 0;
  FacilityCapacityReconciliationRegistration r;
  r.provider_domain = provider;
  r.profile_id = "named_household_market_service";
  r.profile_version = 1;
  r.validate_request =
      [factory,
       provider_tag](const WorldBundle &world,
                     const FacilityCapacityReconciliationRequest &request) {
        return guard<Done>([&] {
          if (!provider_tag ||
              (provider_tag == 1 ? "local_economy" : "commerce") !=
                  request.slot_ref.provider_domain ||
              !factory.read)
            fail("MarketWorldFactoryRequired");
          auto lease = take(factory.read(world, request.occurred_at));
          if (!lease)
            fail("MarketWorldLeaseRequired");
          take(validate_market_service_reconciliation_request(
              lease->bindings().market, request));
          return Done{};
        });
      };
  r.finalize = [factory, provider_tag](
                   WorldBundle &world,
                   const FacilityCapacityReconciliationRequest &request,
                   const std::vector<FacilityClaimMutationReceipt> &owners,
                   ExactPhaseWriteContext &) {
    return guard<std::optional<FacilityClaimMutationReceipt>>([&] {
      if (!provider_tag ||
          (provider_tag == 1 ? "local_economy" : "commerce") !=
              request.slot_ref.provider_domain ||
          !factory.write)
        fail("MarketWorldFactoryRequired");
      auto lease = take(factory.write(world, request.occurred_at));
      if (!lease)
        fail("MarketWorldLeaseRequired");
      auto w = lease->bindings();
      binding(w, request.occurred_at);
      return take(finalize_market_service_capacity_in_candidate(
          w.common, w.state, w.market, request, owners));
    });
  };
  return r;
}
} // namespace hanmo::resident
