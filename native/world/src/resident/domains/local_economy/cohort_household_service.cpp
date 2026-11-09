#include "household_market_resolver.hpp"
#include "household_purchase_cargo.hpp"
#include "household_buyer_execution.hpp"
#include "cohort_household_service.hpp"
#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace cohort_household {
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
SourceRef ref(const StableId &kind, const StableId &id) {
  return {"local_economy", kind, id};
}
ReservationRef holder(const StableId &id) {
  return {ReservationRef::Kind::domain_activity, id, "local_economy",
          "household_purchase"};
}
const HouseholdPurchase &purchase(const HouseholdPurchaseContext &x,
                                  const StableId &id) {
  const auto *p = x.state.purchases.find(id);
  if (!p || !std::holds_alternative<CohortHouseholdBuyer>(p->executor) ||
      !std::holds_alternative<OwnerPeriodWorkBinding>(p->execution_binding))
    fail("CohortHouseholdPurchaseRequired", id);
  return *p;
}
const HouseholdPurchaseAcceptedTerms &
accepted(const HouseholdPurchaseContext &x, const HouseholdPurchase &p) {
  const auto *a = x.state.accepted_terms.find(p.accepted_terms_ref.id);
  if (!a || !same_source_ref(a->source, p.accepted_terms_ref))
    fail("HouseholdPurchaseAcceptanceMissing", p.purchase_id);
  return *a;
}
const HouseholdMarketServiceDefinition &
market(const HouseholdPurchaseContext &x, const HouseholdPurchase &p) {
  return *take(read_household_market_service_accepted(
      x.common, x.catalog, x.state, x.private_market_custody, accepted(x, p)));
}
void header(const CommonStore &c, const SourceRef &r) {
  const auto *f = c.state().facts.find(r.id);
  if (!f || f->domain != r.domain || f->kind != r.kind)
    fail("CohortHouseholdOriginalSourceMissing", r.id);
}
Integer revision(const FacilityClaimsState &state,
                 const FacilitySlotRef &slot) {
  Integer result = 0;
  for (const auto &h : state.slot_histories)
    if (h.slot_ref == slot) {
      if (const auto *p = std::get_if<VacantPrunedFacilityPrefix>(&h.base))
        result = p->last_removed_revision;
      for (const auto &r : h.mutations)
        result = r.claim_set_revision;
    }
  if (result == std::numeric_limits<Integer>::max())
    fail("HouseholdClaimRevisionOverflow");
  return result;
}
void candidate(CommonStore &c, HouseholdPurchaseState &s,
               const HouseholdPurchaseContext &x) {
  if (&c != &x.common || &s != &x.state)
    fail("HouseholdPurchaseCandidateMismatch");
}
SourceRef phase(CommonStore &c, HouseholdPurchaseState &s,
                const HouseholdPurchaseContext &x, const HouseholdPurchase &p,
                HouseholdPurchasePhase next, const SourceRef &cause) {
  take(x.environment.validate_purchase_phase_cause(p, next, cause, x.now));
  if (p.phase_revision == std::numeric_limits<Integer>::max())
    fail("HouseholdPurchaseRevisionOverflow");
  auto id = take(c.next_id("household_phase"));
  auto source = ref("household_purchase_phase_changed", id);
  take(c.append_fact({id, source.kind, source.domain}));
  HouseholdPurchasePhaseChange change{id,
                                      p.purchase_id,
                                      p.state,
                                      next,
                                      p.phase_revision,
                                      p.phase_revision + 1,
                                      cause,
                                      x.now};
  s.phase_changes.insert(id, change);
  auto *edited = s.purchases.find(p.purchase_id);
  edited->state = next;
  edited->phase_revision = change.revision_after;
  return source;
}
void labor_span(const HouseholdPurchaseContext &x, const HouseholdPurchase &p,
                const MomentSpan &span) {
  const auto *original = x.state.plans.find(p.plan_ref);
  if (!original)
    fail("HouseholdPurchasePlanMissing", p.plan_ref);
  take(validate_household_buyer_labor_span(
      {x.common, x.economy, x.population, x.transport_registrations}, *original,
      accepted(x, p), span));
}
void coverage(const HouseholdPurchaseContext &x, const HouseholdPurchase &p,
              const MomentSpan &window) {
  WorldMoment cursor = window.from;
  for (const auto &segment : x.state.service_segments)
    if (segment.purchase_id == p.purchase_id &&
        cmp(window.from, segment.window.from) <= 0 &&
        cmp(segment.window.to, window.to) <= 0) {
      if (cmp(segment.window.from, cursor) ||
          cmp(segment.window.from, segment.window.to) >= 0)
        fail("CohortHouseholdCoverageGap", p.purchase_id);
      labor_span(x, p, segment.window);
      take(x.environment.validate_purchase_service_segment(p, segment));
      cursor = segment.window.to;
    }
  if (cmp(cursor, window.to))
    fail("CohortHouseholdCoverageGap", p.purchase_id);
}
const HouseholdPurchaseServiceResult *
completed(const HouseholdPurchaseContext &x, const StableId &id) {
  const HouseholdPurchaseServiceResult *found = nullptr;
  for (const auto &row : x.state.service_results.rows())
    if (row.second.purchase_id == id) {
      const auto *work = x.state.work_periods.find(row.first);
      if (work && work->disposition == WorkPeriodDisposition::provided) {
        if (found)
          fail("DuplicateHouseholdCompletedService", id);
        found = &row.second;
      }
    }
  return found;
}
} // namespace cohort_household
using namespace cohort_household;
Outcome<std::vector<CohortHouseholdServiceAttempt>>
read_cohort_household_service_attempts(const HouseholdPurchaseContext &x,
                                       const StableId &id) {
  return guard<std::vector<CohortHouseholdServiceAttempt>>([&] {
    const auto &p = purchase(x, id);
    const auto &a = accepted(x, p);
    const auto &service = market(x, p);
    std::vector<SourceRef> openings{a.source};
    for (const auto &fact : x.common.state().facts.rows())
      if (const auto *change = x.state.phase_changes.find(fact.first))
        if (change->purchase_id == id &&
            change->after == HouseholdPurchasePhase::servicing) {
          if (change->before != HouseholdPurchasePhase::blocked)
            fail("InvalidCohortServiceResume", id);
          openings.push_back(
              ref("household_purchase_phase_changed", change->id));
        }
    std::vector<CohortHouseholdServiceAttempt> result;
    for (const auto &opening : openings) {
      header(x.common, opening);
      const FacilityClaim2 *claim = nullptr;
      for (const auto &history : x.claims.slot_histories)
        if (history.slot_ref == service.service_slot)
          for (const auto &mutation : history.mutations)
            for (const auto &admitted : mutation.admitted_claims)
              if (admitted.holder == holder(id) &&
                  same_source_ref(admitted.source_result_ref, opening)) {
                if (claim)
                  fail("DuplicateCohortServiceAdmission", id);
                claim = &admitted;
              }
      if (!claim)
        fail("CohortServiceClaimHistoryRequired", opening.id);
      CohortHouseholdServiceAttempt attempt;
      attempt.ordinal = static_cast<Integer>(result.size());
      attempt.opening_source = opening;
      attempt.claim_episode =
          take(read_facility_claim_episode(x.claims, claim->claim_id, x.now));
      if (cmp(claim->from, {claim->from.half_day_tick, {}}) ||
          claim->from.half_day_tick == std::numeric_limits<Integer>::max() ||
          cmp(claim->until, {claim->from.half_day_tick + 1, {}}))
        fail("CohortServiceOriginalPeriodRequired", claim->claim_id);
      for (const auto &row : x.state.work_periods.rows()) {
        const auto &period = row.second;
        if (!same_work_binding(period.ref.binding, p.execution_binding) ||
            period.ref.period_ordinal != attempt.ordinal)
          continue;
        if (attempt.period)
          fail("DuplicateCohortServicePeriod", id);
        const auto *receipt = x.state.service_results.find(row.first);
        if (!receipt || receipt->purchase_id != id || !receipt->period_ref ||
            !same_work_period_ref(*receipt->period_ref, period.ref) ||
            period.facility_claim_refs !=
                std::vector<StableId>{claim->claim_id} ||
            period.ref.from_tick != claim->from.half_day_tick ||
            period.ref.to_tick != claim->until.half_day_tick)
          fail("CohortPeriodOriginalSourceMismatch", row.first);
        attempt.result = receipt;
        attempt.period = &period;
      }
      if (!result.empty()) {
        const auto &previous = result.back();
        if (!previous.period ||
            previous.period->disposition == WorkPeriodDisposition::provided ||
            cmp(previous.claim_episode.original_claim.until, claim->from) > 0)
          fail("CohortServiceResumeBeforePeriodClose", id);
      }
      result.push_back(attempt);
    }
    if (result.empty() ||
        p.service_claim_ref !=
            result.back().claim_episode.original_claim.claim_id)
      fail("CohortCurrentClaimMismatch", id);
    return result;
  });
}
Status provide_cohort_household_service_interval_in_candidate(
    HouseholdPurchaseState &state, const HouseholdPurchaseContext &x,
    const StableId &id, const MomentSpan &span) {
  return guard<Done>([&] {
    if (&state != &x.state)
      fail("HouseholdPurchaseCandidateMismatch");
    const auto &p = purchase(x, id);
    auto attempts = take(read_cohort_household_service_attempts(x, id));
    const auto &current = attempts.back();
    const auto &claim = current.claim_episode.original_claim;
    if (p.state != HouseholdPurchasePhase::servicing || current.result ||
        current.claim_episode.close_receipt_ref ||
        cmp(span.from, span.to) >= 0 || cmp(span.from, claim.from) < 0 ||
        cmp(span.to, claim.until) > 0)
      fail("CohortServiceNotRunning", id);
    WorldMoment cursor = claim.from;
    for (const auto &old : state.service_segments)
      if (old.purchase_id == id && cmp(claim.from, old.window.from) <= 0)
        cursor = old.window.to;
    if (cmp(cursor, span.from))
      fail("CohortHouseholdCoverageGap", id);
    labor_span(x, p, span);
    auto original = take(x.environment.read_purchase_service_coverage(p, span));
    const auto &buyer = std::get<CohortHouseholdBuyer>(p.executor);
    if (cmp(original.window.from, span.from) ||
        cmp(original.window.to, span.to) ||
        original.actual_labor_allocations !=
            std::vector<StableId>{buyer.labor_allocation_id} ||
        !valid_source_ref(original.actual_source))
      fail("CohortServiceActualLaborMismatch", id);
    HouseholdServiceSegment segment{id, span, original.actual_source,
                                    original.actual_labor_allocations,
                                    original.exertion_source_refs};
    take(x.environment.validate_purchase_service_segment(p, segment));
    HouseholdServiceSegment *prior = nullptr;
    for (auto &old : state.service_segments)
      if (old.purchase_id == id && cmp(claim.from, old.window.from) <= 0)
        prior = &old;
    bool same = prior && cmp(prior->window.to, span.from) == 0 &&
                same_source_ref(prior->actual_coverage_source,
                                segment.actual_coverage_source) &&
                prior->labor_allocation_refs == segment.labor_allocation_refs &&
                prior->exertion_source_refs.size() ==
                    segment.exertion_source_refs.size();
    if (same)
      for (std::size_t n = 0; n < segment.exertion_source_refs.size(); ++n)
        same &= same_source_ref(prior->exertion_source_refs[n],
                                segment.exertion_source_refs[n]);
    if (same)
      prior->window.to = span.to;
    else
      state.service_segments.push_back(segment);
    return Done{};
  });
}
Outcome<FacilityClaimMutationReceipt>
interrupt_cohort_household_service_owner_in_candidate(
    CommonStore &c, HouseholdPurchaseState &state,
    const HouseholdPurchaseContext &x, const StableId &id,
    const SourceRef &cause) {
  return guard<FacilityClaimMutationReceipt>([&] {
    candidate(c, state, x);
    const auto p = purchase(x, id);
    auto attempts = take(read_cohort_household_service_attempts(x, id));
    const auto &current = attempts.back();
    const auto &claim = current.claim_episode.original_claim;
    if (current.claim_episode.close_receipt_ref) {
      auto prior = take(read_facility_claim_receipt(
          x.claims, *current.claim_episode.close_receipt_ref));
      const auto *change = state.phase_changes.find(prior.source_result_ref.id);
      if (change && same_source_ref(change->actual_cause, cause))
        return prior;
      fail("CohortServiceAlreadyClosed", id);
    }
    if (p.state != HouseholdPurchasePhase::servicing ||
        cmp(x.now, claim.from) < 0 || cmp(x.now, claim.until) >= 0)
      fail("CohortServiceInterruptionWindowMismatch", id);
    coverage(x, p, {claim.from, x.now});
    auto source = phase(c, state, x, p, HouseholdPurchasePhase::blocked, cause);
    auto capacity =
        take(x.claim_sources.capacity_source_at(claim.slot_ref, x.now));
    return FacilityClaimMutationReceipt{
        revision(x.claims, claim.slot_ref) + 1,
        x.now,
        source,
        1,
        {{claim.claim_id, FacilityClaimCloseReason::suspended}},
        {},
        {capacity}};
  });
}
Outcome<CohortHouseholdPeriodClosed> close_cohort_household_period_in_candidate(
    CommonStore &c, HouseholdPurchaseState &state,
    const HouseholdPurchaseContext &x, const StableId &id) {
  return guard<CohortHouseholdPeriodClosed>([&] {
    candidate(c, state, x);
    const auto p = purchase(x, id);
    auto attempts = take(read_cohort_household_service_attempts(x, id));
    const auto &current = attempts.back();
    const auto &claim = current.claim_episode.original_claim;
    if (current.result)
      return CohortHouseholdPeriodClosed{current.result->coverage_source, {}};
    if (cmp(x.now, claim.until))
      fail("CohortServicePeriodNotDue", id);
    bool supplied = !current.claim_episode.close_receipt_ref;
    if (supplied) {
      if (p.state != HouseholdPurchasePhase::servicing)
        fail("CohortServicePhaseMismatch", id);
      coverage(x, p, {claim.from, claim.until});
    } else if (p.state != HouseholdPurchasePhase::blocked)
      fail("CohortFailedPeriodPhaseMismatch", id);
    auto source = ref(supplied ? "household_purchase_service_completed"
                               : "household_purchase_period_interrupted",
                      take(c.next_id("household_service_period")));
    take(c.append_fact({source.id, source.kind, source.domain}));
    WorkPeriodView view;
    view.ref = {p.execution_binding, current.ordinal, claim.from.half_day_tick,
                claim.until.half_day_tick, 1};
    view.actual_labor_allocations = {
        std::get<CohortHouseholdBuyer>(p.executor).labor_allocation_id};
    view.facility_claim_refs = {claim.claim_id};
    view.work_unit_id = x.scale.definition_id;
    view.units_added = view.units_after =
        supplied ? x.scale.units_per_half_day : 0;
    view.disposition = supplied ? WorkPeriodDisposition::provided
                                : WorkPeriodDisposition::interrupted;
    view.resource_effect_refs = {
        {"commerce", "household_retail_payment", p.retail_receipt_ref}};
    FacilityClaimReceiptRef closure;
    std::optional<FacilityClaimMutationReceipt> mutation;
    if (supplied) {
      closure = {claim.slot_ref, revision(x.claims, claim.slot_ref) + 1};
      auto capacity =
          take(x.claim_sources.capacity_source_at(claim.slot_ref, x.now));
      mutation = FacilityClaimMutationReceipt{
          closure.claim_set_revision,
          x.now,
          source,
          1,
          {{claim.claim_id, FacilityClaimCloseReason::completed}},
          {},
          {capacity}};
    } else {
      closure = *current.claim_episode.close_receipt_ref;
      auto old = take(read_facility_claim_receipt(x.claims, closure));
      view.reason = old.source_result_ref;
    }
    for (const auto &segment : state.service_segments)
      if (segment.purchase_id == id &&
          cmp(claim.from, segment.window.from) <= 0 &&
          cmp(segment.window.to, claim.until) <= 0)
        for (const auto &exertion : segment.exertion_source_refs) {
          bool exists = false;
          for (const auto &earlier : view.exertion_source_refs)
            exists |= same_source_ref(earlier, exertion);
          if (!exists)
            view.exertion_source_refs.push_back(exertion);
        }
    take(validate_work_period_value(view));
    HouseholdPurchaseServiceResult result{
        source.id, id,   {claim.from, claim.until}, source, closure,
        view.ref,  x.now};
    state.service_results.insert(source.id, result);
    state.work_periods.insert(source.id, view);
    auto *edited = state.purchases.find(id);
    edited->last_consumed_period_ref = view.ref;
    if (supplied) {
      if (edited->phase_revision == std::numeric_limits<Integer>::max())
        fail("HouseholdPurchaseRevisionOverflow");
      edited->state = HouseholdPurchasePhase::awaiting_dispatch;
      ++edited->phase_revision;
    }
    return CohortHouseholdPeriodClosed{source, mutation};
  });
}
Outcome<CohortHouseholdResumeQuote>
preview_resume_cohort_household_service(const HouseholdPurchaseContext &x,
                                        const StableId &id,
                                        const SourceRef &cause) {
  return guard<CohortHouseholdResumeQuote>([&] {
    const auto &p = purchase(x, id);
    auto attempts = take(read_cohort_household_service_attempts(x, id));
    const auto &prior = attempts.back();
    if (p.state != HouseholdPurchasePhase::blocked || !prior.period ||
        prior.period->disposition == WorkPeriodDisposition::provided ||
        completed(x, id) || cmp(x.now, {x.now.half_day_tick, {}}) ||
        x.now.half_day_tick == std::numeric_limits<Integer>::max())
      fail("CohortServiceResumeOpeningRequired", id);
    const auto &a = accepted(x, p);
    const auto &service = market(x, p);
    const auto &buyer = std::get<CohortHouseholdBuyer>(p.executor);
    const auto *allocation =
        x.economy.labor_allocations.find(buyer.labor_allocation_id);
    if (!allocation || allocation->cohort_id != buyer.cohort_id ||
        allocation->workers != 1 ||
        allocation->from_tick > x.now.half_day_tick ||
        allocation->through_tick < x.now.half_day_tick ||
        x.now.half_day_tick >= a.plan.delivery_deadline)
      fail("CohortServiceContinuationLaborRequired", id);
    take(x.environment.validate_purchase_phase_cause(
        p, HouseholdPurchasePhase::servicing, cause, x.now));
    take(x.environment.read_purchase_presence_for_service(a.plan, service,
                                                          x.now));
    WorldMoment until{x.now.half_day_tick + 1, {}};
    FacilityClaimRequest2 request{service.service_slot,
                                  holder(id),
                                  x.now,
                                  until,
                                  1,
                                  service.service_usage_right_ref,
                                  {}};
    std::vector<FacilitySourceRef> capacities;
    auto source =
        take(x.claim_sources.capacity_source_at(service.service_slot, x.now));
    capacities.push_back(source);
    for (const auto &change : take(x.claim_sources.enumerate_capacity_changes(
             service.service_slot, x.now, until))) {
      bool exists = false;
      for (const auto &old : capacities)
        exists |= old.source_schema == change.source.source_schema &&
                  same_source_ref(old.source_ref, change.source.source_ref);
      if (!exists)
        capacities.push_back(change.source);
    }
    auto quote = take(quote_facility_claim(x.claims, x.claim_catalog, request,
                                           capacities, x.now, x.claim_sources));
    return CohortHouseholdResumeQuote{x.common.state().identity,
                                      id,
                                      p.phase_revision,
                                      x.now,
                                      quote,
                                      capacities,
                                      cause};
  });
}
Outcome<StableId> resume_cohort_household_service_in_candidate(
    CommonStore &c, HouseholdPurchaseState &state, FacilityClaimsState &claims,
    const HouseholdPurchaseContext &x,
    const CohortHouseholdResumeQuote &quote) {
  return guard<StableId>([&] {
    candidate(c, state, x);
    if (&claims != &x.claims ||
        std::tie(quote.identity.world_id, quote.identity.execution_epoch,
                 quote.identity.revision, quote.identity.tick) !=
            std::tie(c.state().identity.world_id,
                     c.state().identity.execution_epoch,
                     c.state().identity.revision, c.state().identity.tick) ||
        cmp(quote.resumed_at, x.now))
      fail("StaleCohortResumeQuote", quote.purchase_id);
    auto current = take(preview_resume_cohort_household_service(
        x, quote.purchase_id, quote.actual_continuation_source));
    if (current.phase_revision != quote.phase_revision ||
        current.claim.capacity_revision != quote.claim.capacity_revision ||
        current.claim.claim_set_revision != quote.claim.claim_set_revision)
      fail("StaleCohortResumeQuote", quote.purchase_id);
    const auto p = purchase(x, quote.purchase_id);
    auto source = phase(c, state, x, p, HouseholdPurchasePhase::servicing,
                        quote.actual_continuation_source);
    auto id = take(c.next_id("household_service_claim"));
    const auto &requested = current.claim.requested;
    FacilityClaim2 claim{id,
                         requested.slot_ref,
                         holder(p.purchase_id),
                         requested.from,
                         requested.until,
                         1,
                         current.claim.capacity_revision,
                         requested.usage_right_ref,
                         source};
    state.purchases.find(p.purchase_id)->service_claim_ref = id;
    FacilityClaimMutationReceipt receipt{current.claim.claim_set_revision + 1,
                                         x.now,
                                         source,
                                         1,
                                         {},
                                         {claim},
                                         current.capacity_sources};
    auto next = take(apply_facility_claim_mutation(
        claims, x.claim_catalog, claim.slot_ref, receipt,
        current.claim.claim_set_revision, x.now, x.claim_sources));
    claims = std::move(next);
    return source.id;
  });
}
Status validate_cohort_household_service_claim(
    const HouseholdPurchaseContext &x, const FacilityClaim2 &claim,
    const FacilityClaimMutationReceipt &receipt, bool admission) {
  return guard<Done>([&] {
    const auto &p = purchase(x, claim.holder.id);
    const auto &a = accepted(x, p);
    const auto &service = market(x, p);
    if (claim.holder != holder(p.purchase_id) ||
        claim.slot_ref != service.service_slot || claim.units != 1 ||
        !same_source_ref(claim.usage_right_ref,
                         service.service_usage_right_ref) ||
        cmp(claim.from, {claim.from.half_day_tick, {}}) ||
        claim.from.half_day_tick == std::numeric_limits<Integer>::max() ||
        cmp(claim.until, {claim.from.half_day_tick + 1, {}}))
      fail("CohortServiceClaimSourceMismatch", claim.claim_id);
    if (same_source_ref(claim.source_result_ref, a.source)) {
      if (cmp(claim.from, a.accepted_at))
        fail("CohortServiceInitialWindowMismatch", claim.claim_id);
    } else {
      const auto *change =
          x.state.phase_changes.find(claim.source_result_ref.id);
      if (!change || claim.source_result_ref.domain != "local_economy" ||
          claim.source_result_ref.kind != "household_purchase_phase_changed" ||
          change->purchase_id != p.purchase_id ||
          change->before != HouseholdPurchasePhase::blocked ||
          change->after != HouseholdPurchasePhase::servicing ||
          cmp(change->occurred_at, claim.from))
        fail("CohortServiceResumeSourceMismatch", claim.claim_id);
      auto before = p;
      before.state = change->before;
      before.phase_revision = change->revision_before;
      take(x.environment.validate_purchase_phase_cause(
          before, change->after, change->actual_cause, change->occurred_at));
    }
    header(x.common, claim.source_result_ref);
    if (receipt.source_schema != 1)
      fail("CohortServiceReceiptSchemaMismatch");
    if (admission) {
      if (!same_source_ref(receipt.source_result_ref,
                           claim.source_result_ref) ||
          cmp(receipt.occurred_at, claim.from) ||
          receipt.admitted_claims.size() != 1 ||
          receipt.admitted_claims.front().claim_id != claim.claim_id ||
          !receipt.closed_claims.empty())
        fail("CohortServiceAdmissionMismatch", claim.claim_id);
      return Done{};
    }
    if (receipt.closed_claims.size() != 1 ||
        receipt.closed_claims.front().claim_id != claim.claim_id ||
        !receipt.admitted_claims.empty())
      fail("CohortServiceClosureMismatch", claim.claim_id);
    if (receipt.closed_claims.front().reason ==
        FacilityClaimCloseReason::completed) {
      const auto *result =
          x.state.service_results.find(receipt.source_result_ref.id);
      const auto *period =
          result ? x.state.work_periods.find(result->id) : nullptr;
      if (!result || !period || result->purchase_id != p.purchase_id ||
          period->disposition != WorkPeriodDisposition::provided ||
          period->facility_claim_refs !=
              std::vector<StableId>{claim.claim_id} ||
          !same_source_ref(result->coverage_source,
                           receipt.source_result_ref) ||
          cmp(result->completed_at, claim.until) ||
          cmp(receipt.occurred_at, claim.until) ||
          result->close_receipt.claim_set_revision !=
              receipt.claim_set_revision)
        fail("CohortServiceCompletedSourceMismatch", claim.claim_id);
      coverage(x, p, {claim.from, claim.until});
    } else {
      const auto *change =
          x.state.phase_changes.find(receipt.source_result_ref.id);
      if (!change ||
          receipt.closed_claims.front().reason !=
              FacilityClaimCloseReason::suspended ||
          change->purchase_id != p.purchase_id ||
          change->before != HouseholdPurchasePhase::servicing ||
          change->after != HouseholdPurchasePhase::blocked ||
          cmp(change->occurred_at, receipt.occurred_at) ||
          cmp(change->occurred_at, claim.from) < 0 ||
          cmp(change->occurred_at, claim.until) >= 0)
        fail("CohortServiceInterruptionSourceMismatch", claim.claim_id);
      auto before = p;
      before.state = change->before;
      before.phase_revision = change->revision_before;
      take(x.environment.validate_purchase_phase_cause(
          before, change->after, change->actual_cause, change->occurred_at));
      coverage(x, p, {claim.from, change->occurred_at});
    }
    header(x.common, receipt.source_result_ref);
    return Done{};
  });
}
Status validate_cohort_household_service(const HouseholdPurchaseContext &x,
                                         const HouseholdPurchase &p) {
  return guard<Done>([&] {
    purchase(x, p.purchase_id);
    const auto &a = accepted(x, p);
    const auto &binding = std::get<OwnerPeriodWorkBinding>(p.execution_binding);
    if (binding.domain != WorkPeriodOwner::local_economy ||
        binding.activity_kind != "household_purchase" ||
        binding.activity_id != p.purchase_id ||
        binding.registered_period_profile != "household_purchase_periods")
      fail("CohortHouseholdBindingMismatch", p.purchase_id);
    auto attempts =
        take(read_cohort_household_service_attempts(x, p.purchase_id));
    const WorkPeriodRef *last = nullptr;
    std::set<const HouseholdServiceSegment *> covered;
    for (const auto &attempt : attempts) {
      const auto &claim = attempt.claim_episode.original_claim;
      auto admission = take(read_facility_claim_receipt(
          x.claims, attempt.claim_episode.admission_receipt_ref));
      take(validate_cohort_household_service_claim(x, claim, admission, true));
      WorldMoment through = claim.from;
      for (const auto &segment : x.state.service_segments)
        if (segment.purchase_id == p.purchase_id &&
            cmp(claim.from, segment.window.from) <= 0 &&
            cmp(segment.window.to, claim.until) <= 0) {
          if (!covered.insert(&segment).second ||
              cmp(segment.window.from, through) ||
              cmp(segment.window.from, segment.window.to) >= 0 ||
              cmp(segment.window.to, x.now) > 0)
            fail("CohortServiceCoverageOrderMismatch", p.purchase_id);
          labor_span(x, p, segment.window);
          take(x.environment.validate_purchase_service_segment(p, segment));
          through = segment.window.to;
        }
      if (attempt.claim_episode.close_receipt_ref) {
        auto closure = take(read_facility_claim_receipt(
            x.claims, *attempt.claim_episode.close_receipt_ref));
        FacilityClaimCloseReason reason = FacilityClaimCloseReason::completed;
        bool found = false;
        for (const auto &close : closure.closed_claims)
          if (close.claim_id == claim.claim_id) {
            reason = close.reason;
            found = true;
          }
        if (!found || cmp(through, closure.occurred_at))
          fail("CohortServiceClosureCoverageMismatch", p.purchase_id);
        take(x.claim_sources.validate_claim_close(claim, reason, closure));
      } else if (p.state != HouseholdPurchasePhase::servicing ||
                 &attempt != &attempts.back())
        fail("CohortServiceOpenClaimMismatch", p.purchase_id);
      if (attempt.result) {
        const auto &r = *attempt.result;
        const auto &work = *attempt.period;
        if (!attempt.claim_episode.close_receipt_ref ||
            cmp(r.serviced_window.from, claim.from) ||
            cmp(r.serviced_window.to, claim.until) ||
            cmp(r.completed_at, claim.until) ||
            cmp(r.completed_at, x.now) > 0 ||
            r.close_receipt.slot_ref != claim.slot_ref ||
            r.close_receipt.claim_set_revision !=
                attempt.claim_episode.close_receipt_ref->claim_set_revision ||
            work.actual_labor_allocations !=
                std::vector<StableId>{std::get<CohortHouseholdBuyer>(p.executor)
                                          .labor_allocation_id} ||
            work.work_unit_id != x.scale.definition_id ||
            work.units_before != 0 || work.units_after != work.units_added ||
            !work.actual_actor_roles.empty())
          fail("CohortServicePeriodResultMismatch", r.id);
        header(x.common, r.coverage_source);
        if (work.disposition == WorkPeriodDisposition::provided) {
          if (r.coverage_source.kind !=
                  "household_purchase_service_completed" ||
              work.units_added != x.scale.units_per_half_day || work.reason ||
              cmp(through, claim.until))
            fail("CohortServiceProvidedMismatch", r.id);
        } else {
          auto close =
              take(read_facility_claim_receipt(x.claims, r.close_receipt));
          if (r.coverage_source.kind !=
                  "household_purchase_period_interrupted" ||
              work.disposition != WorkPeriodDisposition::interrupted ||
              work.units_added != 0 || !work.reason ||
              !same_source_ref(*work.reason, close.source_result_ref))
            fail("CohortServiceInterruptedMismatch", r.id);
        }
        take(validate_work_period_value(work));
        last = &work.ref;
      } else if (cmp(claim.until, x.now) <= 0)
        fail("CohortServiceUnclosedPeriod", claim.claim_id);
    }
    for (const auto &segment : x.state.service_segments)
      if (segment.purchase_id == p.purchase_id && !covered.count(&segment))
        fail("OrphanCohortServiceCoverage", p.purchase_id);
    if (bool(last) != bool(p.last_consumed_period_ref) ||
        (last && !same_work_period_ref(*last, *p.last_consumed_period_ref)))
      fail("CohortLastPeriodMismatch", p.purchase_id);
    const auto *retail = x.retail.state.settlements.find(p.retail_receipt_ref);
    if (!retail)
      fail("HouseholdRetailSourceMissing", p.purchase_id);
    HouseholdPurchasePhase projected = HouseholdPurchasePhase::servicing;
    Integer version = 1, total = 0;
    bool provided = false;
    std::optional<SourceRef> terminal;
    auto cargo_history = take(read_household_purchase_cargo(x, p.purchase_id));
    for (const auto &fact : x.common.state().facts.rows()) {
      if (const auto *result = x.state.service_results.find(fact.first))
        if (result->purchase_id == p.purchase_id) {
          const auto *work = x.state.work_periods.find(fact.first);
          if (!work)
            fail("CohortPeriodSourceMissing", fact.first);
          if (work->disposition == WorkPeriodDisposition::provided) {
            if (provided || projected != HouseholdPurchasePhase::servicing ||
                version == std::numeric_limits<Integer>::max())
              fail("DuplicateCohortServiceCompletion", p.purchase_id);
            provided = true;
            projected = HouseholdPurchasePhase::awaiting_dispatch;
            ++version;
          }
        }
      if (const auto *change = x.state.phase_changes.find(fact.first))
        if (change->purchase_id == p.purchase_id) {
          if (change->before != projected ||
              change->revision_before != version ||
              version == std::numeric_limits<Integer>::max() ||
              change->revision_after != version + 1)
            fail("CohortHouseholdPhaseHistoryMismatch", fact.first);
          auto original = p;
          original.state = projected;
          original.phase_revision = version;
          take(x.environment.validate_purchase_phase_cause(
              original, change->after, change->actual_cause,
              change->occurred_at));
          projected = change->after;
          version = change->revision_after;
          if (projected == HouseholdPurchasePhase::lost)
            terminal = ref("household_purchase_phase_changed", change->id);
        }
      if (const auto *handover = x.state.handovers.find(fact.first))
        if (handover->purchase_id == p.purchase_id) {
          const bool received =
              handover->to_inventory_id == a.plan.destination_inventory;
          if (!provided || handover->id != fact.first ||
              fact.second.domain != "local_economy" ||
              fact.second.kind != (received
                                       ? "household_purchase_delivered"
                                       : "household_purchase_cargo_handover"))
            fail("CohortHouseholdHandoverSourceMismatch", fact.first);
          if (received) {
            for (const auto &lot : handover->actual_lots) {
              if (lot.quantity <= 0 ||
                  total > std::numeric_limits<Integer>::max() - lot.quantity)
                fail("CohortHouseholdHandoverQuantityMismatch", fact.first);
              total += lot.quantity;
            }
            if (total > a.quantity)
              fail("CohortHouseholdHandoverQuantityMismatch", fact.first);
            if (total == a.quantity) {
              if (version == std::numeric_limits<Integer>::max())
                fail("HouseholdPurchaseRevisionOverflow");
              projected = HouseholdPurchasePhase::received;
              ++version;
              terminal = ref("household_purchase_delivered", handover->id);
            }
          }
        }
    }
    if (projected != p.state || version != p.phase_revision ||
        bool(terminal) != bool(p.terminal_result_ref) ||
        (terminal && !same_source_ref(*terminal, *p.terminal_result_ref)))
      fail("CohortHouseholdPhaseProjectionMismatch", p.purchase_id);
    if (total != cargo_history.received_quantity)
      fail("HouseholdCargoReceivedProjectionMismatch", p.purchase_id);
    take(validate_household_purchase_cargo(x, p));
    return Done{};
  });
}
} // namespace hanmo::resident
