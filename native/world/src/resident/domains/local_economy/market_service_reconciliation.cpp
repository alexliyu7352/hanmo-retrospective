#include "market_service_reconciliation.hpp"
#include "cohort_household_service.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace hanmo::resident {
namespace market_reconciliation {
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
    fail("MarketClaimRevisionOverflow", slot.facility_id);
  return result;
}
const MarketServiceDefinition &definition(const MarketServiceCapacityContext &x,
                                          const FacilitySlotRef &slot) {
  const MarketServiceDefinition *found = nullptr;
  for (const auto &d : x.purchases.market_service_definitions)
    if (d.slot_ref == slot) {
      if (found)
        fail("DuplicateMarketSlotDefinition", slot.facility_id);
      found = &d;
    }
  if (!found)
    fail("MarketSlotDefinitionMissing", slot.facility_id);
  return *found;
}
MarketServiceChangeCause
changed(const MarketServiceCapacityContext &x,
        const FacilityCapacityReconciliationRequest &r) {
  if (cmp(x.now, r.occurred_at))
    fail("MarketReconcileMomentMismatch");
  const auto &d = definition(x, r.slot_ref);
  auto result = take(
      x.originals.resolve_original_market_service_change(d, r.original_change));
  if (cmp(take(x.originals.read_original_market_change_at(r.slot_ref, result)),
          r.occurred_at))
    fail("MarketReconcileOriginalTimeMismatch");
  return result;
}
void original_header(const CommonStore &common, const SourceRef &source) {
  const auto *h = common.state().facts.find(source.id);
  if (!h || h->id != source.id || h->domain != source.domain ||
      h->kind != source.kind)
    fail("MarketOriginalSourceHeaderMissing", source.id);
}
FacilitySourceRef adopted_original_change(const MarketServiceCapacityContext &x,
                                          const FacilitySlotRef &slot,
                                          const SourceRef &source,
                                          const WorldMoment &at) {
  std::optional<FacilitySourceRef> found;
  for (const auto *rows : {&x.purchases.market_service_sources.public_sources,
                           &x.purchases.market_service_sources.private_sources})
    for (const auto &row : *rows)
      if (row.slot_ref == slot && cmp(row.occurred_at, at) == 0) {
        auto original = take(x.originals.read_original_market_change_source(
            slot, row.change_cause));
        if (same_source_ref(original.source_ref, source)) {
          if (found)
            fail("DuplicateMarketCapacityCause", source.id);
          found = std::move(original);
        }
      }
  if (!found)
    fail("MarketAdoptedChangeRequired", source.id);
  return *found;
}
bool overlaps(const FacilityClaim2 &c, const MomentSpan &span) {
  return cmp(c.from, span.to) < 0 && cmp(span.from, c.until) < 0;
}
bool closed(const FacilityClaimMutationReceipt &r, const StableId &id) {
  for (const auto &c : r.closed_claims)
    if (c.claim_id == id)
      return true;
  return false;
}
bool fits(const FacilityClaim2 &claim, const MomentSpan &window,
          const std::vector<ProspectiveMarketCapacitySegment> &capacity,
          const std::vector<const FacilityClaim2 *> &earlier) {
  WorldMoment cursor = window.from;
  for (const auto &part : capacity) {
    if (cmp(part.window.from, cursor) || cmp(part.window.to, window.to) > 0 ||
        part.capacity < 0)
      fail("MarketProspectiveCoverageMismatch", claim.claim_id);
    if (!same_source_ref(part.usage_right_ref, claim.usage_right_ref))
      return false;
    std::vector<WorldMoment> cuts{part.window.from, part.window.to};
    for (const auto *prior : earlier) {
      if (cmp(part.window.from, prior->from) < 0 &&
          cmp(prior->from, part.window.to) < 0)
        cuts.push_back(prior->from);
      if (cmp(part.window.from, prior->until) < 0 &&
          cmp(prior->until, part.window.to) < 0)
        cuts.push_back(prior->until);
    }
    std::sort(cuts.begin(), cuts.end(),
              [](const auto &a, const auto &b) { return cmp(a, b) < 0; });
    cuts.erase(std::unique(
                   cuts.begin(), cuts.end(),
                   [](const auto &a, const auto &b) { return cmp(a, b) == 0; }),
               cuts.end());
    for (std::size_t n = 0; n + 1 < cuts.size(); ++n) {
      MomentSpan slice{cuts[n], cuts[n + 1]};
      Integer used = claim.units;
      for (const auto *prior : earlier)
        if (overlaps(*prior, slice)) {
          if (prior->units <= 0 ||
              used > std::numeric_limits<Integer>::max() - prior->units)
            fail("MarketOccupancyOverflow");
          used += prior->units;
        }
      if (used > part.capacity)
        return false;
    }
    cursor = part.window.to;
  }
  if (cmp(cursor, window.to))
    fail("MarketProspectiveCoverageMissing", claim.claim_id);
  return true;
}
class ActualMarketInterruptionRead final
    : public HouseholdPurchaseEnvironmentRead {
  const HouseholdPurchaseEnvironmentRead &original_;
  const MarketServiceCapacityContext &market_;
  const FacilityCapacityReconciliationRequest &request_;
  const FacilityReconciliationProgress &progress_;
  bool matches(const HouseholdPurchase &p, const SourceRef &r,
               const WorldMoment &at) const {
    return progress_.position < progress_.ordered_claims.size() &&
           progress_.ordered_claims[progress_.position].holder.id ==
               p.purchase_id &&
           same_source_ref(r, request_.original_change.source_ref) &&
           cmp(at, request_.occurred_at) == 0;
  }
  Status proof() const;

public:
  ActualMarketInterruptionRead(const HouseholdPurchaseEnvironmentRead &o,
                               const MarketServiceCapacityContext &m,
                               const FacilityCapacityReconciliationRequest &r,
                               const FacilityReconciliationProgress &p)
      : original_(o), market_(m), request_(r), progress_(p) {}
  Outcome<HouseholdPurchasePresence>
  read_purchase_presence_for_service(const HouseholdPurchasePlan &p,
                                     const HouseholdMarketServiceDefinition &s,
                                     const WorldMoment &m) const override {
    return original_.read_purchase_presence_for_service(p, s, m);
  }
  Outcome<HouseholdPurchasePresence>
  read_purchase_presence(const HouseholdPurchasePlan &p, const StableId &s,
                         const WorldMoment &m) const override {
    return original_.read_purchase_presence(p, s, m);
  }
  Outcome<HouseholdPurchaseCoverage>
  read_purchase_service_coverage(const HouseholdPurchase &p,
                                 const MomentSpan &s) const override {
    return original_.read_purchase_service_coverage(p, s);
  }
  Status validate_purchase_service_segment(
      const HouseholdPurchase &p,
      const HouseholdServiceSegment &s) const override {
    return original_.validate_purchase_service_segment(p, s);
  }
  Outcome<bool>
  read_local_purchase_handover(const HouseholdPurchase &p, const StableId &s,
                               const WorldMoment &m) const override {
    return original_.read_local_purchase_handover(p, s, m);
  }
  Outcome<HouseholdPurchaseDeliverySource>
  read_purchase_delivery(const HouseholdPurchase &p,
                         const SourceRef &r) const override {
    return original_.read_purchase_delivery(p, r);
  }
  Status validate_named_purchase_service(const NamedHouseholdBuyer &b,
                                         const HouseholdPurchasePlan &p,
                                         const WorldMoment &m) const override {
    return original_.validate_named_purchase_service(b, p, m);
  }
  Outcome<std::vector<FixedServiceInputRef>> read_named_service_adoption_inputs(
      const HouseholdPurchase &p,
      const HouseholdServiceEvent2 &e) const override {
    return original_.read_named_service_adoption_inputs(p, e);
  }
  Status validate_named_service_adoption_inputs(
      const HouseholdPurchase &p, const HouseholdServiceEvent2 &e,
      const std::vector<FixedServiceInputRef> &i) const override {
    return original_.validate_named_service_adoption_inputs(p, e, i);
  }
  Outcome<FacilityClaimCloseReason>
  read_named_service_interruption(const HouseholdPurchase &p,
                                  const SourceRef &r, const WorldMoment &m,
                                  bool t) const override {
    if (!t && matches(p, r, m)) {
      auto checked = proof();
      if (!checked.ok)
        return {false, {}, checked.error};
      return {true, FacilityClaimCloseReason::suspended, {}};
    }
    return original_.read_named_service_interruption(p, r, m, t);
  }
  Outcome<std::vector<SourceRef>>
  read_named_service_participation(const HouseholdPurchase &p,
                                   const MomentSpan &s) const override {
    return original_.read_named_service_participation(p, s);
  }
  Status
  validate_named_service_continuation(const HouseholdPurchase &p,
                                      const WorldMoment &m) const override {
    return original_.validate_named_service_continuation(p, m);
  }
  Status validate_purchase_phase_cause(const HouseholdPurchase &p,
                                       HouseholdPurchasePhase v,
                                       const SourceRef &r,
                                       const WorldMoment &m) const override {
    if (p.state == HouseholdPurchasePhase::servicing &&
        v == HouseholdPurchasePhase::blocked && matches(p, r, m))
      return proof();
    return original_.validate_purchase_phase_cause(p, v, r, m);
  }
};
HouseholdPurchaseContext bound_interruption_context(
    const HouseholdPurchaseContext &h,
    const HouseholdPurchaseEnvironmentRead &environment) {
  return {h.common,
          h.shared,
          h.economy,
          h.population,
          h.catalog,
          h.state,
          h.controls,
          environment,
          h.claims,
          h.claim_catalog,
          h.claim_sources,
          h.retail,
          h.elapsed,
          h.scale,
          h.now,
          h.transport_expenses,
          h.transport_registrations,
          h.selected_household_version,
          h.mobile_custody,
          h.private_market_custody};
}

} // namespace market_reconciliation
using namespace market_reconciliation;
Status validate_market_service_reconciliation_request(
    const MarketServiceCapacityContext &x,
    const FacilityCapacityReconciliationRequest &r) {
  return guard<Done>([&] {
    changed(x, r);
    const auto &sources =
        r.slot_ref.provider_domain == "local_economy"
            ? x.purchases.market_service_sources.public_sources
            : x.purchases.market_service_sources.private_sources;
    for (const auto &s : sources)
      if (s.slot_ref == r.slot_ref) {
        auto cause = take(x.originals.resolve_original_market_service_change(
            definition(x, r.slot_ref), r.original_change));
        (void)cause;
        if (s.source_revision <= 0)
          fail("InvalidMarketSourceRevision", s.source_id);
      }
    return Done{};
  });
}
Outcome<bool> can_continue_market_service_claim(
    const MarketServiceCapacityContext &x,
    const FacilityCapacityReconciliationRequest &r,
    const FacilityReconciliationProgress &progress) {
  return guard<bool>([&] {
    const auto cause = changed(x, r);
    if (progress.position >= progress.ordered_claims.size() ||
        progress.prior_owner_results.size() != progress.position)
      fail("InvalidMarketReconciliationProgress");
    const auto &claim = progress.ordered_claims[progress.position];
    if (claim.slot_ref != r.slot_ref || claim.units <= 0)
      fail("MarketReconcileClaimMismatch", claim.claim_id);
    if (cmp(claim.until, r.occurred_at) <= 0)
      return false;
    MomentSpan window{cmp(claim.from, r.occurred_at) < 0 ? r.occurred_at
                                                         : claim.from,
                      claim.until};
    auto capacity = take(read_prospective_market_service_capacity(
        x, definition(x, r.slot_ref).definition_ref, cause, window));
    std::vector<const FacilityClaim2 *> earlier;
    std::set<StableId> ids;
    for (std::size_t n = 0; n < progress.position; ++n) {
      const auto &original = progress.ordered_claims[n];
      const auto &result = progress.prior_owner_results[n];
      if (original.slot_ref != r.slot_ref ||
          cmp(result.occurred_at, r.occurred_at) ||
          result.claim_set_revision != revision(x.claims, r.slot_ref) + 1)
        fail("MarketPriorDispositionMismatch", original.claim_id);
      for (const auto &close : result.closed_claims)
        if (close.claim_id != original.claim_id)
          fail("MarketPriorForeignClose", close.claim_id);
      if (!closed(result, original.claim_id)) {
        earlier.push_back(&original);
        if (!ids.insert(original.claim_id).second)
          fail("DuplicateMarketPriorClaim");
      }
      for (const auto &admitted : result.admitted_claims) {
        if (admitted.slot_ref != r.slot_ref ||
            !ids.insert(admitted.claim_id).second)
          fail("InvalidMarketPriorAdmission");
        earlier.push_back(&admitted);
      }
    }
    return fits(claim, window, capacity, earlier);
  });
}
Status market_reconciliation::ActualMarketInterruptionRead::proof() const {
  return guard<Done>([&] {
    if (take(can_continue_market_service_claim(market_, request_, progress_)))
      fail("MarketClaimCanContinue");
    return Done{};
  });
}
Status validate_household_market_service_interruption(
    const MarketServiceCapacityContext &x, const StableId &purchase_id,
    const SourceRef &cause, const WorldMoment &at) {
  return guard<Done>([&] {
    if (cmp(at, x.now) > 0)
      fail("FutureMarketInterruption", purchase_id);
    const auto *purchase = x.purchases.purchases.find(purchase_id);
    const auto *accepted =
        purchase
            ? x.purchases.accepted_terms.find(purchase->accepted_terms_ref.id)
            : nullptr;
    if (!purchase || !accepted)
      fail("MarketHouseholdAcceptanceMissing", purchase_id);
    const FacilitySlotHistory *history = nullptr;
    const FacilityClaimMutationReceipt *mutation = nullptr;
    for (const auto &h : x.claims.slot_histories)
      for (const auto &r : h.mutations)
        if (same_source_ref(r.source_result_ref, cause) &&
            cmp(r.occurred_at, at) == 0) {
          bool owns = false;
          for (const auto &close : r.closed_claims)
            for (const auto &previous : h.mutations)
              for (const auto &admitted : previous.admitted_claims)
                if (admitted.claim_id == close.claim_id &&
                    admitted.holder.kind ==
                        ReservationRef::Kind::domain_activity &&
                    admitted.holder.domain_id == "local_economy" &&
                    admitted.holder.activity_kind == "household_purchase" &&
                    admitted.holder.id == purchase_id)
                  owns = true;
          if (owns) {
            if (mutation)
              fail("DuplicateMarketHouseholdInterruption", purchase_id);
            history = &h;
            mutation = &r;
          }
        }
    FacilityClaimMutationReceipt pending_mutation;
    if (!mutation) {
      // During late finalization the original owner results and actual capacity
      // already exist, but S0 has not installed the one receipt yet. Read
      // exactly those immutable owner results; no caller-supplied close list is
      // accepted.
      if (cmp(at, x.now))
        fail("MarketActualCapacityDispositionRequired", cause.id);
      const FacilityClaim2 *target = nullptr;
      for (const auto &claim : x.claims.claims)
        if (claim.holder.kind == ReservationRef::Kind::domain_activity &&
            claim.holder.domain_id == "local_economy" &&
            claim.holder.activity_kind == "household_purchase" &&
            claim.holder.id == purchase_id) {
          if (target)
            fail("MarketHouseholdMultipleLiveClaims", purchase_id);
          target = &claim;
        }
      if (!target)
        fail("MarketActualCapacityDispositionRequired", cause.id);
      for (const auto &h : x.claims.slot_histories)
        if (h.slot_ref == target->slot_ref) {
          if (history)
            fail("DuplicateMarketSlotHistory", cause.id);
          history = &h;
        }
      if (!history)
        fail("MarketSlotHistoryRequired", cause.id);
      const auto original_cause =
          adopted_original_change(x, target->slot_ref, cause, at);
      pending_mutation = {revision(x.claims, target->slot_ref) + 1,
                          at,
                          cause,
                          original_cause.source_schema,
                          {},
                          {},
                          {}};
      for (const auto *rows :
           {&x.purchases.market_service_sources.public_sources,
            &x.purchases.market_service_sources.private_sources})
        for (const auto &row : *rows)
          if (row.slot_ref == target->slot_ref &&
              cmp(row.occurred_at, at) == 0) {
            const auto actual = std::visit(
                [](const auto &c) -> SourceRef {
                  using T = std::decay_t<decltype(c)>;
                  if constexpr (std::is_same_v<T, MarketServiceInitialized>)
                    return c.original_opening_acceptance_ref;
                  else if constexpr (std::is_same_v<
                                         T, MarketServiceOpeningChanged>)
                    return c.original_opening_change_ref;
                  else if constexpr (std::is_same_v<
                                         T, MarketServiceStaffingChanged>)
                    return c.original_staffing_event_ref;
                  else if constexpr (std::is_same_v<
                                         T, MarketServiceFacilityChanged>)
                    return c.original_condition_result_ref;
                  else
                    return c.original_right_change_ref;
                },
                row.change_cause);
            if (same_source_ref(actual, cause))
              pending_mutation.capacity_source_refs.push_back(
                  {{row.slot_ref.provider_domain, "market_service_capacity",
                    row.source_id},
                   row.source_schema});
          }
      if (pending_mutation.capacity_source_refs.size() != 1)
        fail("MarketAdoptedChangeRequired", cause.id);
      for (const auto &claim : x.claims.claims)
        if (claim.slot_ref == target->slot_ref) {
          const auto *owner = x.purchases.purchases.find(claim.holder.id);
          if (!owner)
            fail("MarketHouseholdOwnerMissing", claim.claim_id);
          const bool due = cmp(claim.until, at) == 0;
          bool actual = false;
          if (std::holds_alternative<NamedHouseholdBuyer>(owner->executor)) {
            for (const auto &ep : x.purchases.named_service_claim_episodes)
              if (ep.claim_id == claim.claim_id && ep.close_result_ref &&
                  ep.close_receipt_ref &&
                  ep.close_receipt_ref->slot_ref == target->slot_ref &&
                  ep.close_receipt_ref->claim_set_revision ==
                      pending_mutation.claim_set_revision) {
                const auto *event = x.purchases.named_service_events.find(
                    ep.close_result_ref->id);
                if (!event || event->purchase_id != owner->purchase_id ||
                    cmp(event->occurred_at, at) ||
                    ep.close_result_ref->domain != "local_economy" ||
                    ep.close_result_ref->kind != "household_purchase_service")
                  fail("MarketPendingOwnerSourceMismatch", claim.claim_id);
                original_header(x.common, *ep.close_result_ref);
                if ((due &&
                     event->kind == HouseholdServiceEventKind::completed) ||
                    (!due &&
                     event->kind == HouseholdServiceEventKind::suspended &&
                     event->cause_ref &&
                     same_source_ref(*event->cause_ref, cause))) {
                  if (actual)
                    fail("DuplicateMarketPendingOwnerResult", claim.claim_id);
                  actual = true;
                }
              }
          } else if (std::holds_alternative<CohortHouseholdBuyer>(
                         owner->executor)) {
            if (due) {
              for (const auto &row : x.purchases.service_results.rows()) {
                const auto &r = row.second;
                if (r.purchase_id == owner->purchase_id &&
                    cmp(r.completed_at, at) == 0 &&
                    r.close_receipt.slot_ref == target->slot_ref &&
                    r.close_receipt.claim_set_revision ==
                        pending_mutation.claim_set_revision) {
                  const auto *period = x.purchases.work_periods.find(r.id);
                  if (!period ||
                      period->disposition != WorkPeriodDisposition::provided ||
                      period->facility_claim_refs !=
                          std::vector<StableId>{claim.claim_id} ||
                      actual)
                    fail("MarketPendingCompletedPeriodMismatch",
                         claim.claim_id);
                  original_header(x.common, r.coverage_source);
                  actual = true;
                }
              }
            } else
              for (const auto &row : x.purchases.phase_changes.rows()) {
                const auto &r = row.second;
                if (r.purchase_id == owner->purchase_id &&
                    r.before == HouseholdPurchasePhase::servicing &&
                    r.after == HouseholdPurchasePhase::blocked &&
                    cmp(r.occurred_at, at) == 0 &&
                    same_source_ref(r.actual_cause, cause)) {
                  if (actual)
                    fail("DuplicateMarketPendingOwnerResult", claim.claim_id);
                  original_header(x.common,
                                  {"local_economy",
                                   "household_purchase_phase_changed", r.id});
                  actual = true;
                }
              }
          }
          if (actual)
            pending_mutation.closed_claims.push_back(
                {claim.claim_id, due ? FacilityClaimCloseReason::completed
                                     : FacilityClaimCloseReason::suspended});
        }
      mutation = &pending_mutation;
    }
    if (!history || !mutation ||
        mutation->source_schema !=
            adopted_original_change(x, history->slot_ref, cause, at)
                .source_schema ||
        !mutation->admitted_claims.empty())
      fail("MarketActualCapacityDispositionRequired", cause.id);
    std::vector<FacilityClaim2> ordered;
    Integer previous =
        std::holds_alternative<VacantPrunedFacilityPrefix>(history->base)
            ? std::get<VacantPrunedFacilityPrefix>(history->base)
                  .last_removed_revision
            : 0;
    for (const auto &r : history->mutations) {
      if (previous == std::numeric_limits<Integer>::max() ||
          r.claim_set_revision != previous + 1)
        fail("MarketClaimHistoryGap", cause.id);
      previous = r.claim_set_revision;
      if (&r == mutation)
        break;
      for (const auto &close : r.closed_claims) {
        auto old =
            std::find_if(ordered.begin(), ordered.end(), [&](const auto &c) {
              return c.claim_id == close.claim_id;
            });
        if (old == ordered.end())
          fail("MarketClaimHistoryCloseMissing", close.claim_id);
        ordered.erase(old);
      }
      for (const auto &a : r.admitted_claims) {
        if (a.slot_ref != history->slot_ref ||
            std::any_of(ordered.begin(), ordered.end(), [&](const auto &c) {
              return c.claim_id == a.claim_id;
            }))
          fail("MarketClaimHistoryAdmissionMismatch", a.claim_id);
        ordered.push_back(a);
      }
    }
    std::vector<StableId> started;
    for (const auto &claim : ordered) {
      if (claim.holder.kind != ReservationRef::Kind::domain_activity ||
          claim.holder.domain_id != "local_economy" ||
          claim.holder.activity_kind != "household_purchase")
        fail("MarketOriginalHolderPriorityRequired", claim.claim_id);
      if (take(x.holders.has_actual_facility_service_started(claim, at)))
        started.push_back(claim.claim_id);
    }
    std::sort(
        ordered.begin(), ordered.end(), [&](const auto &a, const auto &b) {
          bool aa = std::find(started.begin(), started.end(), a.claim_id) !=
                    started.end(),
               bb = std::find(started.begin(), started.end(), b.claim_id) !=
                    started.end();
          if (aa != bb)
            return aa;
          int time = cmp(a.from, b.from);
          return time ? time < 0 : a.claim_id < b.claim_id;
        });
    std::vector<const FacilityClaim2 *> retained;
    bool target = false;
    std::size_t closes = 0;
    for (const auto &claim : ordered) {
      const ClosedFacilityClaim *closed_claim = nullptr;
      for (const auto &c : mutation->closed_claims)
        if (c.claim_id == claim.claim_id) {
          if (closed_claim)
            fail("DuplicateMarketClaimClose", claim.claim_id);
          closed_claim = &c;
        }
      if (cmp(claim.until, at) <= 0) {
        if (cmp(claim.until, at) || !closed_claim ||
            closed_claim->reason != FacilityClaimCloseReason::completed)
          fail("MarketDueClaimDispositionMismatch", claim.claim_id);
        ++closes;
        continue;
      }
      MomentSpan window{cmp(claim.from, at) < 0 ? at : claim.from, claim.until};
      auto capacity = take(read_market_service_adopted_change_projection(
          x, history->slot_ref, cause, at, window));
      for (const auto &part : capacity) {
        bool adopted = false;
        for (const auto &ref : mutation->capacity_source_refs)
          if (ref.source_schema == 1 &&
              ref.source_ref.domain == history->slot_ref.provider_domain &&
              ref.source_ref.kind == "market_service_capacity")
            for (const auto *rows :
                 {&x.purchases.market_service_sources.public_sources,
                  &x.purchases.market_service_sources.private_sources})
              for (const auto &row : *rows)
                if (row.source_id == ref.source_ref.id &&
                    row.slot_ref == history->slot_ref &&
                    row.source_revision == part.prospective_revision)
                  adopted = true;
        if (!adopted)
          fail("MarketActualCapacityAdoptionMissing", cause.id);
      }
      const bool can_stay = fits(claim, window, capacity, retained);
      if (can_stay) {
        if (closed_claim)
          fail("MarketUnnecessaryClaimInterruption", claim.claim_id);
        retained.push_back(&claim);
      } else {
        if (!closed_claim ||
            (closed_claim->reason != FacilityClaimCloseReason::suspended &&
             closed_claim->reason !=
                 FacilityClaimCloseReason::facility_changed &&
             closed_claim->reason != FacilityClaimCloseReason::right_ended))
          fail("MarketRequiredClaimInterruptionMissing", claim.claim_id);
        ++closes;
        if (claim.holder.id == purchase_id)
          target = true;
      }
    }
    if (!target || closes != mutation->closed_claims.size())
      fail("MarketHouseholdInterruptionNotProven", purchase_id);
    return Done{};
  });
}
Outcome<FacilityClaimMutationReceipt>
reconcile_named_household_market_service_in_candidate(
    CommonStore &common, HouseholdPurchaseState &state,
    ActionElapsedStore &elapsed, const HouseholdPurchaseContext &household,
    const MarketServiceCapacityContext &market,
    const FacilityCapacityReconciliationRequest &request,
    const FacilityReconciliationProgress &progress) {
  return guard<FacilityClaimMutationReceipt>([&] {
    if (&common != &household.common || &state != &household.state ||
        &state != &market.purchases || &elapsed != &household.elapsed ||
        &household.claims != &market.claims ||
        cmp(household.now, request.occurred_at))
      fail("MarketHouseholdCandidateMismatch");
    if (progress.position >= progress.ordered_claims.size())
      fail("InvalidMarketReconciliationProgress");
    const auto &claim = progress.ordered_claims[progress.position];
    const auto *purchase = state.purchases.find(claim.holder.id);
    if (!purchase ||
        !std::holds_alternative<NamedHouseholdBuyer>(purchase->executor))
      fail("NamedMarketHolderRequired", claim.claim_id);
    changed(market, request);
    if (cmp(claim.until, request.occurred_at) == 0)
      return take(close_household_purchase_service_owner_in_candidate(
          common, state, elapsed, household, purchase->purchase_id));
    if (take(can_continue_market_service_claim(market, request, progress)))
      return FacilityClaimMutationReceipt{
          revision(market.claims, request.slot_ref) + 1,
          request.occurred_at,
          request.original_change.source_ref,
          request.original_change.source_schema,
          {},
          {},
          {}};
    ActualMarketInterruptionRead actual{household.environment, market, request,
                                        progress};
    auto bound = bound_interruption_context(household, actual);
    return take(suspend_named_household_service_owner_in_candidate(
        common, state, elapsed, bound, purchase->purchase_id,
        request.original_change.source_ref));
  });
}
Outcome<FacilityClaimMutationReceipt>
reconcile_household_market_service_in_candidate(
    CommonStore &common, HouseholdPurchaseState &state,
    ActionElapsedStore &elapsed, const HouseholdPurchaseContext &household,
    const MarketServiceCapacityContext &market,
    const FacilityCapacityReconciliationRequest &request,
    const FacilityReconciliationProgress &progress) {
  return guard<FacilityClaimMutationReceipt>([&] {
    if (progress.position >= progress.ordered_claims.size())
      fail("InvalidMarketReconciliationProgress");
    const auto &claim = progress.ordered_claims[progress.position];
    const auto *purchase = state.purchases.find(claim.holder.id);
    if (!purchase)
      fail("HouseholdMarketHolderMissing", claim.claim_id);
    if (std::holds_alternative<NamedHouseholdBuyer>(purchase->executor))
      return take(reconcile_named_household_market_service_in_candidate(
          common, state, elapsed, household, market, request, progress));
    if (&common != &household.common || &state != &household.state ||
        &state != &market.purchases || &household.claims != &market.claims ||
        cmp(household.now, request.occurred_at))
      fail("MarketHouseholdCandidateMismatch");
    changed(market, request);
    if (cmp(claim.until, request.occurred_at) == 0)
      return take(close_household_purchase_service_owner_in_candidate(
          common, state, elapsed, household, purchase->purchase_id));
    if (take(can_continue_market_service_claim(market, request, progress)))
      return FacilityClaimMutationReceipt{
          revision(market.claims, request.slot_ref) + 1,
          request.occurred_at,
          request.original_change.source_ref,
          request.original_change.source_schema,
          {},
          {},
          {}};
    ActualMarketInterruptionRead actual{household.environment, market, request,
                                        progress};
    auto bound = bound_interruption_context(household, actual);
    return take(interrupt_cohort_household_service_owner_in_candidate(
        common, state, bound, purchase->purchase_id,
        request.original_change.source_ref));
  });
}
Outcome<std::optional<FacilityClaimMutationReceipt>>
finalize_market_service_capacity_in_candidate(
    CommonStore &common, HouseholdPurchaseState &state,
    const MarketServiceCapacityContext &x,
    const FacilityCapacityReconciliationRequest &r,
    const std::vector<FacilityClaimMutationReceipt> &results) {
  return guard<std::optional<FacilityClaimMutationReceipt>>([&] {
    if (&common != &x.common || &state != &x.purchases)
      fail("MarketServiceCandidateMismatch");
    const auto cause = changed(x, r);
    const auto &d = definition(x, r.slot_ref);
    std::size_t active = 0;
    for (const auto &claim : x.claims.claims)
      if (claim.slot_ref == r.slot_ref)
        ++active;
    if (results.size() != active)
      fail("MarketOwnerDispositionCoverageMismatch");
    const auto source_id =
        take(record_market_service_capacity_source_in_candidate(
            common, state, x, d.definition_ref, cause));
    const auto expected = revision(x.claims, r.slot_ref) + 1;
    FacilityClaimMutationReceipt combined{expected,
                                          r.occurred_at,
                                          r.original_change.source_ref,
                                          r.original_change.source_schema,
                                          {},
                                          {},
                                          {}};
    std::set<StableId> seen;
    for (const auto &result : results) {
      if (result.claim_set_revision != expected ||
          cmp(result.occurred_at, r.occurred_at) ||
          !result.admitted_claims.empty())
        fail("MarketOwnerDispositionMismatch");
      if (result.closed_claims.empty()) {
        if (result.source_schema != r.original_change.source_schema ||
            !same_source_ref(result.source_result_ref,
                             r.original_change.source_ref))
          fail("MarketContinuationOriginalSourceRequired");
        continue;
      }
      take(x.holders.validate_holder_mutation_source(r.slot_ref, result));
      for (const auto &close : result.closed_claims) {
        if (!seen.insert(close.claim_id).second)
          fail("DuplicateMarketDisposition", close.claim_id);
        bool actual = false;
        for (const auto &claim : x.claims.claims)
          if (claim.slot_ref == r.slot_ref && claim.claim_id == close.claim_id)
            actual = true;
        if (!actual)
          fail("MarketDispositionClaimMissing", close.claim_id);
        combined.closed_claims.push_back(close);
      }
    }
    combined.capacity_source_refs = {
        {{r.slot_ref.provider_domain, "market_service_capacity", source_id},
         1}};
    if (!active)
      return std::optional<FacilityClaimMutationReceipt>{};
    NativeMarketServiceCapacityRead read{x};
    take(read.validate_mutation_source(r.slot_ref, combined));
    return std::optional<FacilityClaimMutationReceipt>{std::move(combined)};
  });
}
} // namespace hanmo::resident
