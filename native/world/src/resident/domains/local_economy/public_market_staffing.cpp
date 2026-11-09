#include "public_market_staffing.hpp"
#include "household_purchase_contract.hpp"
#include <algorithm>
#include <limits>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace market_staffing {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const StableId &code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> out) {
  if (!out.ok)
    throw Failure{out.error};
  return std::move(out.value);
}
template <class T, class F> Outcome<T> guard(F &&f) {
  try {
    return {true, f(), {}};
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
int compare(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b));
}
WorldMoment tick(Integer n) {
  if (n < 0)
    fail("InvalidPublicStaffingTick");
  return {n, {}};
}
bool same(const CivilianPopulationRef &a, const CivilianPopulationRef &b) {
  return std::tie(a.cohort_id, a.population_revision, a.transport_binding_id) ==
         std::tie(b.cohort_id, b.population_revision, b.transport_binding_id);
}
bool same(const LaborAllocation &a, const LaborAllocation &b) {
  return std::tie(a.id, a.cohort_id, a.activity_or_production_id, a.workers,
                  a.from_tick, a.through_tick, a.commitment_id) ==
         std::tie(b.id, b.cohort_id, b.activity_or_production_id, b.workers,
                  b.from_tick, b.through_tick, b.commitment_id);
}
bool same(const HouseholdProvisionControlProof &a,
          const HouseholdProvisionControlProof &b) {
  return std::tie(a.actor_id, a.household_group_id, a.fact_id, a.source_role_id,
                  a.operation, a.source_revision, a.effective_from_tick,
                  a.through_tick) ==
         std::tie(b.actor_id, b.household_group_id, b.fact_id, b.source_role_id,
                  b.operation, b.source_revision, b.effective_from_tick,
                  b.through_tick);
}
SourceRef ref(const char *kind, const StableId &id) {
  return {"local_economy", kind, id};
}
bool header(const CommonStore &common, const SourceRef &source) {
  const auto *f = common.state().facts.find(source.id);
  return valid_source_ref(source) && f && f->domain == source.domain &&
         f->kind == source.kind;
}
StableId fact(CommonStore &c, const char *kind) {
  auto id = take(c.next_id(kind));
  take(c.append_fact({id, kind, "local_economy"}));
  return id;
}
const PublicMarketStaffingOffer &offer(const PublicMarketStaffingContext &x,
                                       const StableId &id) {
  const auto *p = x.purchases.public_staffing_offers.find(id);
  if (!p || !header(x.common, ref("public_market_staffing_offer", id)))
    fail("PublicStaffingOfferMissing", id);
  return *p;
}
const PublicMarketStaffingBinding &binding(const PublicMarketStaffingContext &x,
                                           const StableId &id) {
  const auto *b = x.purchases.public_staffing_bindings.find(id);
  if (!b)
    fail("PublicStaffingBindingMissing", id);
  return *b;
}
MomentSpan span(const PublicMarketStaffingTerms &t) {
  if (t.workers <= 0 || t.terms_revision <= 0 || t.from_tick < 0 ||
      t.until_tick <= t.from_tick)
    fail("InvalidPublicStaffingTerms");
  return {tick(t.from_tick), tick(t.until_tick)};
}
void candidate(CommonStore &c, HouseholdPurchaseState &p,
               const PublicMarketStaffingContext &x) {
  if (&c != &x.common || &p != &x.purchases)
    fail("PublicStaffingCandidateMismatch");
}
HouseholdProvisionControlProof control(const PublicMarketStaffingContext &x,
                                       const PublicMarketStaffingTerms &terms,
                                       const StableId &actor,
                                       const char *operation) {
  const auto *cohort = x.economy.cohorts.find(terms.cohort_ref.cohort_id);
  if (!cohort || cohort->household_group_id.empty())
    fail("PublicStaffingCohortControllerRequired", terms.cohort_ref.cohort_id);
  auto proof = take(x.controls.current(actor, cohort->household_group_id,
                                       operation, x.now.half_day_tick));
  if (!proof)
    fail("PublicStaffingCohortConsentAuthorityRequired", actor);
  return *proof;
}
void verify_consent(const PublicMarketStaffingContext &x,
                    const PublicMarketStaffingOffer &o,
                    const PublicMarketStaffingConsent &c, bool current) {
  if (c.offer_ref != o.offer_id || c.terms_revision != o.terms.terms_revision ||
      !same(c.cohort_ref, o.terms.cohort_ref) ||
      !header(x.common, ref("public_market_staffing_consent", c.consent_id)) ||
      compare(c.consented_at, o.issued_at) < 0 ||
      compare(c.consented_at, o.expires_at) >= 0 ||
      c.controller_authority_ref.operation !=
          household_market_staffing_operation)
    fail("PublicStaffingConsentMismatch", c.consent_id);
  take(x.controls.validate_accepted(c.controller_authority_ref,
                                    c.consented_at.half_day_tick));
  take(x.sources.validate_staffing_contact(c.local_contact_source_ref, o.terms,
                                           c.controller_authority_ref.actor_id,
                                           c.consented_at));
  if (current && !same(control(x, o.terms, c.controller_authority_ref.actor_id,
                               household_market_staffing_operation),
                       c.controller_authority_ref))
    fail("PublicStaffingConsentAuthorityChanged", c.consent_id);
}
SourceRef event(CommonStore &c, HouseholdPurchaseState &p,
                const StableId &binding_id, PublicMarketStaffingEventKind kind,
                const SourceRef &cause, const WorldMoment &at) {
  Integer ordinal = 0;
  for (const auto &row : p.public_staffing_events.rows())
    if (row.second.binding_ref == binding_id) {
      if (row.second.event_ordinal != ordinal ||
          row.second.kind == PublicMarketStaffingEventKind::completed ||
          row.second.kind == PublicMarketStaffingEventKind::stop_at_boundary ||
          compare(row.second.occurred_at, at) > 0)
        fail("PublicStaffingEventChainMismatch", binding_id);
      if (ordinal == std::numeric_limits<Integer>::max())
        fail("PublicStaffingOrdinalOverflow", binding_id);
      ++ordinal;
    }
  if ((ordinal == 0) != (kind == PublicMarketStaffingEventKind::accepted) ||
      !valid_source_ref(cause))
    fail("PublicStaffingEventGenesisMismatch", binding_id);
  auto id = fact(c, "public_market_staffing");
  p.public_staffing_events.insert(id,
                                  {id, binding_id, ordinal, at, kind, cause});
  return ref("public_market_staffing", id);
}
const PublicMarketLaborOrigin &origin(const PublicMarketStaffingContext &x,
                                      const PublicMarketStaffingBinding &b) {
  if (b.labor_origin_ref.domain != "local_economy" ||
      b.labor_origin_ref.kind != "public_market_labor_origin")
    fail("PublicStaffingOriginReferenceMismatch", b.binding_id);
  const auto *o =
      x.population.public_market_labor_origins.find(b.labor_origin_ref.id);
  if (!o || o->binding_ref != b.binding_id || o->source_schema != 1 ||
      !header(x.common, b.labor_origin_ref))
    fail("PublicStaffingLaborOriginMissing", b.binding_id);
  return *o;
}
void verify_origin(const PublicMarketStaffingContext &x,
                   const PublicMarketStaffingBinding &b,
                   const PublicMarketLaborOrigin &o) {
  const auto &accepted = offer(x, b.offer_ref);
  const auto *consent =
      x.purchases.public_staffing_consents.find(b.consent_ref);
  if (!consent)
    fail("PublicStaffingConsentMissing", b.consent_ref);
  verify_consent(x, accepted, *consent, false);
  take(x.sources.validate_staffing_offer_authority(accepted));
  const auto &t = accepted.terms;
  auto committed = span(t);
  const auto &a = o.original_allocation;
  if (a.id != b.original_labor_allocation_ref ||
      a.cohort_id != t.cohort_ref.cohort_id ||
      a.activity_or_production_id != b.original_org_activity_ref ||
      a.commitment_id != b.original_org_activity_ref ||
      a.workers != t.workers || a.from_tick != t.from_tick ||
      a.through_tick != t.until_tick - 1 ||
      compare(o.committed_span.from, committed.from) ||
      compare(o.committed_span.to, committed.to) ||
      compare(o.accepted_at, committed.from) ||
      !same(o.population_source_ref, t.cohort_ref) ||
      !same_source_ref(o.accepted_labor_authority_ref,
                       t.original_labor_authority_ref))
    fail("PublicStaffingOriginPayloadMismatch", o.origin_id);
  auto approval = take(x.sources.read_verified_public_market_staffing_approval(
      o.approved_org_fact_ref));
  if (approval.activity_id != b.original_org_activity_ref ||
      approval.organization_id != t.organization_ref ||
      approval.office_id != t.responsible_office_ref ||
      approval.tick != t.from_tick ||
      approval.code != "public_market_staffing_approved" ||
      approval.cause_fact_ids !=
          std::vector<StableId>{accepted.offer_id, consent->consent_id})
    fail("PublicStaffingApprovalMismatch", o.origin_id);
  const auto *accepted_event =
      x.purchases.public_staffing_events.find(b.accepted_event_ref.id);
  if (!accepted_event ||
      !same_source_ref(b.accepted_event_ref, ref("public_market_staffing",
                                                 accepted_event->event_id)) ||
      accepted_event->binding_ref != b.binding_id ||
      accepted_event->event_ordinal != 0 ||
      accepted_event->kind != PublicMarketStaffingEventKind::accepted ||
      !same_source_ref(accepted_event->cause_ref, o.approved_org_fact_ref) ||
      compare(accepted_event->occurred_at, o.accepted_at))
    fail("PublicStaffingAcceptedEventMismatch", b.binding_id);
  take(x.sources.validate_staffing_terms(t, o.accepted_at, false));
  take(x.sources.validate_staffing_compensation(t, o.committed_span));
}
SourceRef change_cause(const PublicMarketLaborChange &change) {
  return std::visit(
      [](const auto &c) -> SourceRef {
        using T = std::decay_t<decltype(c)>;
        if constexpr (std::is_same_v<T, PublicMarketEligibilityChanged>)
          return c.qualification_change_cause_ref;
        else if constexpr (std::is_same_v<T, PublicMarketAllocationAdjusted>)
          return c.original_population_change_ref;
        else
          return c.original_org_boundary_fact_ref;
      },
      change);
}
PublicMarketLaborTransition transition(CommonStore &c,
                                       const PopulationTransportState &p,
                                       const PublicMarketLaborOrigin &o,
                                       const WorldMoment &at,
                                       const PublicMarketLaborChange &change) {
  PublicMarketLaborTransition t;
  t.origin_ref = ref("public_market_labor_origin", o.origin_id);
  t.occurred_at = at;
  t.change = change;
  for (const auto &prior : p.public_market_labor_transitions)
    if (same_source_ref(prior.origin_ref, t.origin_ref)) {
      if (prior.transition_ordinal != t.transition_ordinal ||
          compare(prior.occurred_at, at) > 0 ||
          std::holds_alternative<PublicMarketLaborReleased>(prior.change))
        fail("PublicStaffingLaborTransitionChain", o.origin_id);
      if (t.transition_ordinal == std::numeric_limits<Integer>::max())
        fail("PublicStaffingOrdinalOverflow", o.origin_id);
      ++t.transition_ordinal;
      t.previous_transition_ref =
          ref("public_market_labor_transition", prior.transition_id);
    }
  t.transition_id = fact(c, "public_market_labor_transition");
  return t;
}
// 原公市人员事实头的唯一反向覆盖；完整验证与未选分支共用，不复制业务规则。
void validate_staffing_fact_coverage(const CommonStore &common,
                                     const HouseholdPurchaseState &purchases,
                                     const PopulationTransportState &population,
                                     const std::set<StableId> &events) {
  for (const auto &f : common.state().facts.rows()) {
    if (f.second.domain != "local_economy")
      continue;
    const auto &k = f.second.kind;
    if ((k == "public_market_staffing_offer" &&
         !purchases.public_staffing_offers.find(f.first)) ||
        (k == "public_market_staffing_consent" &&
         !purchases.public_staffing_consents.find(f.first)) ||
        (k == "public_market_staffing_release_consent" &&
         !purchases.public_staffing_release_consents.find(f.first)) ||
        (k == "public_market_labor_origin" &&
         !population.public_market_labor_origins.find(f.first)) ||
        (k == "public_market_staffing" && !events.count(f.first)))
      fail("OrphanPublicStaffingFactHeader", f.first);
  }
  // 原劳动变更同样有真实Common头；孤头不得冒充未选公市或原人口变更行。
  for (const auto &f : common.state().facts.rows()) {
    if (f.second.domain != "local_economy" ||
        f.second.kind != "public_market_labor_transition")
      continue;
    bool found = false;
    for (const auto &t : population.public_market_labor_transitions)
      if (t.transition_id == f.first) {
        found = true;
        break;
      }
    if (!found)
      fail("OrphanPublicStaffingFactHeader", f.first);
  }
}
} // namespace market_staffing
using namespace market_staffing;
Outcome<PublicMarketStaffingQuote> preview_public_market_staffing(
    const PublicMarketStaffingContext &x,
    const PublicMarketStaffingTerms &terms, const ActingAuthority &acting,
    const SourceRef &contact, const WorldMoment &expires) {
  return guard<PublicMarketStaffingQuote>([&] {
    auto committed = span(terms);
    if (compare(x.now, expires) >= 0 || compare(expires, committed.to) > 0 ||
        compare(x.now, committed.from) > 0 ||
        acting.organization_id != terms.organization_ref ||
        acting.office_id != terms.responsible_office_ref)
      fail("InvalidPublicStaffingOfferWindow");
    take(x.sources.validate_staffing_terms(terms, x.now, true));
    take(x.sources.validate_staffing_office(terms, acting, x.now));
    take(x.sources.validate_staffing_contact(contact, terms, acting.actor_id,
                                             x.now));
    take(x.sources.validate_staffing_compensation(terms, committed));
    return PublicMarketStaffingQuote{x.common.state().identity, terms, contact,
                                     acting, expires};
  });
}
Outcome<StableId> offer_public_market_staffing_in_candidate(
    CommonStore &c, HouseholdPurchaseState &state,
    const PublicMarketStaffingContext &x,
    PublicMarketStaffingOrganizationWrite &organization,
    const PublicMarketStaffingQuote &quote) {
  return guard<StableId>([&] {
    candidate(c, state, x);
    const auto &id = c.state().identity;
    if (std::tie(id.world_id, id.execution_epoch, id.revision, id.tick) !=
        std::tie(quote.identity.world_id, quote.identity.execution_epoch,
                 quote.identity.revision, quote.identity.tick))
      fail("StalePublicStaffingQuote");
    take(preview_public_market_staffing(x, quote.terms, quote.acting,
                                        quote.local_contact_source,
                                        quote.expires_at));
    auto offer_id = fact(c, "public_market_staffing_offer");
    auto authority = take(
        organization.record_public_market_staffing_offer_authority_in_candidate(
            ref("public_market_staffing_offer", offer_id), quote.terms,
            quote.acting));
    if (!valid_source_ref(authority))
      fail("PublicStaffingOfferAuthoritySourceRequired");
    PublicMarketStaffingOffer o{offer_id,  quote.terms,
                                authority, quote.local_contact_source,
                                x.now,     quote.expires_at};
    state.public_staffing_offers.insert(offer_id, o);
    take(x.sources.validate_staffing_offer_authority(o));
    return offer_id;
  });
}
Outcome<StableId> consent_public_market_staffing_in_candidate(
    CommonStore &c, HouseholdPurchaseState &state,
    const PublicMarketStaffingContext &x, const StableId &id, Integer revision,
    const StableId &actor, const SourceRef &contact) {
  return guard<StableId>([&] {
    candidate(c, state, x);
    const auto &o = offer(x, id);
    if (revision != o.terms.terms_revision || compare(x.now, o.issued_at) < 0 ||
        compare(x.now, o.expires_at) >= 0)
      fail("StalePublicStaffingOffer", id);
    auto authority =
        control(x, o.terms, actor, household_market_staffing_operation);
    take(x.sources.validate_staffing_contact(contact, o.terms, actor, x.now));
    for (const auto &row : state.public_staffing_consents.rows())
      if (row.second.offer_ref == id && row.second.terms_revision == revision &&
          same(row.second.controller_authority_ref, authority) &&
          same_source_ref(row.second.local_contact_source_ref, contact))
        return row.first;
    auto consent_id = fact(c, "public_market_staffing_consent");
    state.public_staffing_consents.insert(
        consent_id, {consent_id, id, revision, o.terms.cohort_ref, authority,
                     contact, x.now});
    return consent_id;
  });
}
Outcome<StableId> accept_public_market_staffing_in_candidate(
    CommonStore &c, LocalEconomyState &economy,
    PopulationTransportState &population, HouseholdPurchaseState &state,
    const PublicMarketStaffingContext &x,
    PublicMarketStaffingOrganizationWrite &organization,
    PublicMarketStaffingEffectsWrite &effects, const StableId &offer_id,
    const StableId &consent_id, const ActingAuthority &acting) {
  return guard<StableId>([&] {
    candidate(c, state, x);
    if (&economy != &x.economy || &population != &x.population)
      fail("PublicStaffingCandidateMismatch");
    for (const auto &row : state.public_staffing_bindings.rows())
      if (row.second.offer_ref == offer_id) {
        if (row.second.consent_ref != consent_id)
          fail("PublicStaffingAlreadyAccepted", offer_id);
        return row.first;
      }
    const auto &o = offer(x, offer_id);
    const auto &t = o.terms;
    const auto *consent = state.public_staffing_consents.find(consent_id);
    if (!consent)
      fail("PublicStaffingConsentMissing", consent_id);
    auto committed = span(t);
    if (compare(x.now, committed.from) || compare(x.now, o.expires_at) >= 0)
      fail("PublicStaffingOpeningRequired", offer_id);
    verify_consent(x, o, *consent, true);
    take(x.sources.validate_staffing_terms(t, x.now, true));
    take(x.sources.validate_staffing_office(t, acting, x.now));
    take(x.sources.validate_staffing_compensation(t, committed));
    auto current = take(read_civilian_population_ref(economy, population,
                                                     t.cohort_ref.cohort_id));
    if (!same(current, t.cohort_ref))
      fail("StalePublicStaffingPopulation", t.cohort_ref.cohort_id);
    auto original =
        take(read_population_revision_source(economy, population, current));
    std::vector<SourceRef> inputs;
    if (original.condition_source_ref)
      inputs.push_back(*original.condition_source_ref);
    auto qualified = take(x.sources.read_staffing_qualification(
        t, current, original.location_binding_ref, inputs, committed));
    if (!same(qualified.population_source, current) ||
        !same_source_ref(qualified.location_source,
                         original.location_binding_ref) ||
        qualified.eligible_workers < t.workers ||
        qualified.eligible_workers > original.labor_capacity)
      fail("PublicStaffingLaborUnavailable", offer_id);
    auto activity_id = take(c.next_id("organization_activity"));
    auto allocation_id = take(c.next_id("public_market_labor"));
    LaborAllocation allocation{
        allocation_id, current.cohort_id, activity_id, t.workers,
        t.from_tick,   t.until_tick - 1,  activity_id};
    take(allocate_labor(c, economy, allocation));
    auto approved =
        take(organization.approve_public_market_staffing_in_candidate(
            ref("public_market_staffing_offer", offer_id),
            ref("public_market_staffing_consent", consent_id), activity_id,
            allocation_id, acting));
    std::optional<SourceRef> approval;
    for (const auto &id : approved.fact_ids) {
      SourceRef r{"organization", "organization_activity_approved", id};
      auto read = x.sources.read_verified_public_market_staffing_approval(r);
      if (read.ok && read.value.activity_id == activity_id) {
        if (approval)
          fail("DuplicatePublicStaffingApproval", activity_id);
        approval = r;
      }
    }
    if (!approval)
      fail("PublicStaffingApprovalMissing", activity_id);
    auto binding_id = take(c.next_id("public_market_staffing_binding"));
    auto origin_id = fact(c, "public_market_labor_origin");
    PublicMarketLaborOrigin origin{origin_id,
                                   1,
                                   binding_id,
                                   allocation,
                                   committed,
                                   current,
                                   qualified.location_source,
                                   qualified.eligibility_sources,
                                   t.original_labor_authority_ref,
                                   *approval,
                                   x.now};
    population.public_market_labor_origins.insert(origin_id, origin);
    auto accepted =
        event(c, state, binding_id, PublicMarketStaffingEventKind::accepted,
              *approval, x.now);
    PublicMarketStaffingBinding bound{
        binding_id,
        offer_id,
        consent_id,
        activity_id,
        allocation_id,
        accepted,
        ref("public_market_labor_origin", origin_id)};
    state.public_staffing_bindings.insert(binding_id, bound);
    verify_origin(x, bound, origin);
    take(effects.apply_public_market_staffing_capacity_change(bound, accepted));
    return binding_id;
  });
}
Outcome<StableId> consent_public_market_staffing_release_in_candidate(
    CommonStore &c, HouseholdPurchaseState &state,
    const PublicMarketStaffingContext &x, const StableId &id,
    Integer release_tick, Integer revision, const StableId &actor,
    const SourceRef &contact) {
  return guard<StableId>([&] {
    candidate(c, state, x);
    const auto &b = binding(x, id);
    const auto &o = offer(x, b.offer_ref);
    if (revision != o.terms.terms_revision ||
        release_tick <= o.terms.from_tick ||
        release_tick >= o.terms.until_tick ||
        compare(tick(release_tick), x.now) < 0)
      fail("InvalidPublicStaffingReleaseBoundary", id);
    auto history =
        take(read_public_market_staffing_labor_history(x, id, span(o.terms)));
    if (history.release_transition_ref)
      fail("PublicStaffingAlreadyReleased", id);
    auto authority =
        control(x, o.terms, actor, household_market_staffing_release_operation);
    take(x.sources.validate_staffing_contact(contact, o.terms, actor, x.now));
    for (const auto &row : state.public_staffing_release_consents.rows())
      if (row.second.binding_ref == id &&
          row.second.release_tick == release_tick &&
          row.second.terms_revision == revision &&
          same(row.second.controller_authority_ref, authority) &&
          same_source_ref(row.second.local_contact_source_ref, contact))
        return row.first;
    auto consent_id = fact(c, "public_market_staffing_release_consent");
    state.public_staffing_release_consents.insert(
        consent_id,
        {consent_id, id, revision, release_tick, authority, contact, x.now});
    return consent_id;
  });
}
Outcome<const PublicMarketLaborOrigin *>
read_public_market_staffing_labor_origin(const PublicMarketStaffingContext &x,
                                         const StableId &id) {
  return guard<const PublicMarketLaborOrigin *>([&] {
    const auto &b = binding(x, id);
    const auto &o = origin(x, b);
    verify_origin(x, b, o);
    return &o;
  });
}
Outcome<PublicMarketStaffingLaborHistoryView>
read_public_market_staffing_labor_history(const PublicMarketStaffingContext &x,
                                          const StableId &binding_id,
                                          const MomentSpan &requested) {
  return guard<PublicMarketStaffingLaborHistoryView>([&] {
    if (compare(requested.from, requested.to) >= 0)
      fail("InvalidPublicStaffingHistoryWindow");
    const auto &b = binding(x, binding_id);
    const auto &o = origin(x, b);
    verify_origin(x, b, o);
    const auto &terms = offer(x, b.offer_ref).terms;
    PublicMarketStaffingLaborHistoryView out{b.labor_origin_ref,
                                             o.original_allocation.id,
                                             o.committed_span,
                                             {},
                                             {},
                                             {}};
    std::optional<LaborAllocation> allocation = o.original_allocation;
    CivilianPopulationRef population = o.population_source_ref;
    SourceRef location = o.location_source_ref;
    std::vector<SourceRef> qualification = o.eligibility_source_refs;
    SourceRef allocation_source = b.labor_origin_ref;
    WorldMoment cursor = o.committed_span.from;
    Integer ordinal = 0;
    std::optional<SourceRef> previous;
    bool released = false;
    auto emit = [&](const WorldMoment &to) {
      if (compare(cursor, to) > 0 || compare(to, o.committed_span.to) > 0)
        fail("PublicStaffingHistoryOrder", binding_id);
      auto from = compare(cursor, requested.from) > 0 ? cursor : requested.from;
      auto end = compare(to, requested.to) < 0 ? to : requested.to;
      if (compare(from, end) < 0 && allocation) {
        MomentSpan segment{from, end};
        out.effective_allocation_segments.push_back(
            {segment, allocation->workers, allocation_source});
        auto raw = take(read_population_revision_source(x.economy, x.population,
                                                        population));
        if (!same_source_ref(location, raw.location_binding_ref))
          fail("PublicStaffingHistoricalLocationMismatch", binding_id);
        auto eligible = take(x.sources.read_staffing_qualification(
            terms, population, location, qualification, segment));
        if (!same(eligible.population_source, population) ||
            !same_source_ref(eligible.location_source, location) ||
            eligible.eligible_workers < 0 ||
            eligible.eligible_workers > raw.labor_capacity)
          fail("PublicStaffingHistoricalQualificationMismatch", binding_id);
        out.eligible_service_segments.push_back(
            {segment, std::min(allocation->workers, eligible.eligible_workers),
             eligible.eligibility_sources});
      }
      cursor = to;
    };
    for (const auto &t : x.population.public_market_labor_transitions)
      if (same_source_ref(t.origin_ref, b.labor_origin_ref)) {
        if (released || t.source_schema != 1 ||
            t.transition_ordinal != ordinal ||
            bool(previous) != bool(t.previous_transition_ref) ||
            (previous &&
             !same_source_ref(*previous, *t.previous_transition_ref)) ||
            !header(x.common,
                    ref("public_market_labor_transition", t.transition_id)))
          fail("PublicStaffingHistoryChainMismatch", t.transition_id);
        take(x.sources.validate_staffing_labor_change(o, t));
        emit(t.occurred_at);
        std::visit(
            [&](const auto &change) {
              using T = std::decay_t<decltype(change)>;
              if constexpr (std::is_same_v<T, PublicMarketEligibilityChanged>) {
                if (!same(population, change.before_population_source_ref) ||
                    change.after_population_source_ref.cohort_id !=
                        population.cohort_id ||
                    change.after_population_source_ref.population_revision <=
                        population.population_revision)
                  fail("PublicStaffingEligibilityPredecessorMismatch",
                       t.transition_id);
                population = change.after_population_source_ref;
                auto actual = take(read_population_revision_source(
                    x.economy, x.population, population));
                location = actual.location_binding_ref;
                qualification = change.actual_location_or_condition_source_refs;
              } else if constexpr (std::is_same_v<
                                       T, PublicMarketAllocationAdjusted>) {
                if (!allocation ||
                    !same(*allocation, change.before_allocation) ||
                    compare(change.effective_span.from, t.occurred_at) ||
                    compare(change.effective_span.to, o.committed_span.to))
                  fail("PublicStaffingAllocationPredecessorMismatch",
                       t.transition_id);
                if (change.after_allocation) {
                  const auto &a = *change.after_allocation;
                  const auto &before = *allocation;
                  if (a.id != before.id || a.cohort_id != before.cohort_id ||
                      a.activity_or_production_id !=
                          before.activity_or_production_id ||
                      a.commitment_id != before.commitment_id ||
                      a.workers <= 0 || a.workers >= before.workers ||
                      a.from_tick != before.from_tick ||
                      a.through_tick != before.through_tick)
                    fail("InvalidPublicStaffingAllocationReduction",
                         t.transition_id);
                }
                allocation = change.after_allocation;
                allocation_source =
                    ref("public_market_labor_transition", t.transition_id);
              } else {
                auto boundary = take(
                    x.sources.read_verified_public_market_staffing_boundary(
                        change.original_org_boundary_fact_ref));
                if (boundary.activity_id != b.original_org_activity_ref ||
                    compare(t.occurred_at, tick(boundary.tick)) ||
                    compare(change.ended_committed_span.from,
                            o.committed_span.from) ||
                    compare(change.ended_committed_span.to, t.occurred_at) ||
                    (change.release_reason ==
                             PublicMarketLaborReleaseReason::completed
                         ? (boundary.code !=
                                "public_market_staffing_term_ended" ||
                            boundary.tick != terms.until_tick)
                         : (boundary.code !=
                                "public_market_staffing_mutual_release" ||
                            boundary.tick <= terms.from_tick ||
                            boundary.tick >= terms.until_tick)))
                  fail("PublicStaffingReleaseSourceMismatch", t.transition_id);
                allocation.reset();
                released = true;
                out.release_transition_ref =
                    ref("public_market_labor_transition", t.transition_id);
              }
            },
            t.change);
        if (ordinal == std::numeric_limits<Integer>::max())
          fail("PublicStaffingOrdinalOverflow", binding_id);
        ++ordinal;
        previous = ref("public_market_labor_transition", t.transition_id);
      }
    if (!released)
      emit(o.committed_span.to);
    const auto *live =
        x.economy.labor_allocations.find(o.original_allocation.id);
    if (allocation ? (!live || !same(*live, *allocation)) : live != nullptr)
      fail("PublicStaffingLiveAllocationHistoryMismatch", binding_id);
    return out;
  });
}
Outcome<StableId> apply_public_market_labor_change_in_candidate(
    CommonStore &c, PopulationTransportState &population,
    HouseholdPurchaseState &purchases, const PublicMarketStaffingContext &x,
    PublicMarketStaffingEffectsWrite &effects, const SourceRef &origin_ref,
    const SourceRef &cause) {
  return guard<StableId>([&] {
    candidate(c, purchases, x);
    if (&population != &x.population ||
        origin_ref.kind != "public_market_labor_origin" ||
        origin_ref.domain != "local_economy")
      fail("PublicStaffingCandidateMismatch");
    const auto *original =
        population.public_market_labor_origins.find(origin_ref.id);
    if (!original)
      fail("PublicStaffingOriginMissing", origin_ref.id);
    const auto &b = binding(x, original->binding_ref);
    for (const auto &t : population.public_market_labor_transitions)
      if (same_source_ref(t.origin_ref, origin_ref) &&
          same_source_ref(change_cause(t.change), cause))
        return t.transition_id;
    auto change = take(
        x.sources.read_applied_staffing_labor_change(*original, cause, x.now));
    if (std::holds_alternative<PublicMarketLaborReleased>(change) ||
        !same_source_ref(change_cause(change), cause))
      fail("PublicStaffingBoundaryWriterRequired", origin_ref.id);
    take(effects.settle_public_market_staffing_before_change(b, x.now, cause));
    auto actual = transition(c, population, *original, x.now, change);
    take(x.sources.validate_staffing_labor_change(*original, actual));
    population.public_market_labor_transitions.push_back(actual);
    auto changed = event(
        c, purchases, b.binding_id,
        PublicMarketStaffingEventKind::qualification_changed,
        ref("public_market_labor_transition", actual.transition_id), x.now);
    take(effects.apply_public_market_staffing_capacity_change(b, changed));
    return actual.transition_id;
  });
}
Status on_public_market_staffing_boundary_in_candidate(
    CommonStore &c, LocalEconomyState &economy,
    PopulationTransportState &population, HouseholdPurchaseState &purchases,
    const PublicMarketStaffingContext &x,
    PublicMarketStaffingEffectsWrite &effects, const StableId &activity,
    const SourceRef &cause) {
  return guard<Done>([&] {
    candidate(c, purchases, x);
    if (&population != &x.population || &economy != &x.economy)
      fail("PublicStaffingCandidateMismatch");
    const PublicMarketStaffingBinding *bound = nullptr;
    for (const auto &row : purchases.public_staffing_bindings.rows())
      if (row.second.original_org_activity_ref == activity) {
        if (bound)
          fail("DuplicatePublicStaffingActivity", activity);
        bound = &row.second;
      }
    if (!bound)
      fail("PublicStaffingBindingMissing", activity);
    const auto b = *bound;
    const auto &o = origin(x, b);
    const auto &terms = offer(x, b.offer_ref).terms;
    auto boundary =
        take(x.sources.read_verified_public_market_staffing_boundary(cause));
    const bool completed = boundary.code == "public_market_staffing_term_ended";
    if (boundary.activity_id != activity ||
        compare(x.now, tick(boundary.tick)) ||
        (completed
             ? boundary.tick != terms.until_tick
             : (boundary.code != "public_market_staffing_mutual_release" ||
                boundary.tick <= terms.from_tick ||
                boundary.tick >= terms.until_tick)))
      fail("PublicStaffingBoundaryMismatch", activity);
    for (const auto &t : population.public_market_labor_transitions)
      if (same_source_ref(t.origin_ref, b.labor_origin_ref) &&
          std::holds_alternative<PublicMarketLaborReleased>(t.change)) {
        if (!same_source_ref(std::get<PublicMarketLaborReleased>(t.change)
                                 .original_org_boundary_fact_ref,
                             cause))
          fail("PublicStaffingAlreadyReleased", activity);
        return Done{};
      }
    take(read_public_market_staffing_labor_history(x, b.binding_id,
                                                   o.committed_span));
    take(effects.settle_public_market_staffing_before_change(b, x.now, cause));
    PublicMarketLaborReleased released{
        cause,
        completed ? PublicMarketLaborReleaseReason::completed
                  : PublicMarketLaborReleaseReason::mutually_cancelled,
        {o.committed_span.from, x.now}};
    auto actual = transition(c, population, o, x.now, released);
    take(x.sources.validate_staffing_labor_change(o, actual));
    if (economy.labor_allocations.find(b.original_labor_allocation_ref))
      economy.labor_allocations.erase(b.original_labor_allocation_ref);
    population.public_market_labor_transitions.push_back(actual);
    auto terminal =
        event(c, purchases, b.binding_id,
              completed ? PublicMarketStaffingEventKind::completed
                        : PublicMarketStaffingEventKind::stop_at_boundary,
              cause, x.now);
    take(effects.apply_public_market_staffing_capacity_change(b, terminal));
    return Done{};
  });
}
// 仅当作者定义、原状态与原事实均未选择公市时，允许外层不借公市适配器。
Status validate_unselected_public_household_market(
    const CommonStore &common, const HouseholdPurchaseCatalog &catalog,
    const HouseholdPurchaseState &purchases,
    const PopulationTransportState &population,
    const HouseholdRetailCatalog &retail) {
  return guard<Done>([&] {
    // 存在任何真实公市选择即要求完整原来源，不把空表判成成功能力。
    const auto require_sources = []() {
      throw Failure{common_failure("NativeHouseholdPublicMarketSourcesRequired",
                                   {}, ErrorKind::unsupported_capability)
                        .error};
    };
    if (!retail.public_market_shop_ids.empty() ||
        !purchases.market_service_sources.public_sources.empty() ||
        !purchases.public_staffing_offers.rows().empty() ||
        !purchases.public_staffing_consents.rows().empty() ||
        !purchases.public_staffing_bindings.rows().empty() ||
        !purchases.public_staffing_events.rows().empty() ||
        !purchases.public_staffing_release_consents.rows().empty() ||
        !population.public_market_labor_origins.rows().empty() ||
        !population.public_market_labor_transitions.empty())
      require_sources();
    for (const auto &d : catalog.market_service_definitions)
      if (d.slot_ref.provider_domain == "local_economy")
        require_sources();
    for (const auto &d : purchases.market_service_definitions)
      if (d.slot_ref.provider_domain == "local_economy")
        require_sources();
    for (const auto &row : catalog.market_services.rows())
      if (row.second.service_slot.provider_domain == "local_economy")
        require_sources();
    const std::set<StableId> no_events;
    validate_staffing_fact_coverage(common, purchases, population, no_events);
    return Done{};
  });
}
Status validate_public_market_staffing(const PublicMarketStaffingContext &x) {
  return guard<Done>([&] {
    std::set<StableId> origins, activities, allocations, events, consents;
    for (const auto &row : x.purchases.public_staffing_offers.rows()) {
      const auto &o = offer(x, row.first);
      if (row.first != o.offer_id || compare(o.issued_at, o.expires_at) >= 0 ||
          compare(o.issued_at, x.now) > 0)
        fail("InvalidPublicStaffingOffer", row.first);
      span(o.terms);
      take(x.sources.validate_staffing_offer_authority(o));
    }
    for (const auto &row : x.purchases.public_staffing_consents.rows()) {
      if (row.first != row.second.consent_id)
        fail("PublicStaffingConsentKeyMismatch", row.first);
      verify_consent(x, offer(x, row.second.offer_ref), row.second, false);
    }
    for (const auto &row : x.purchases.public_staffing_bindings.rows()) {
      const auto &b = row.second;
      if (row.first != b.binding_id ||
          !origins.insert(b.labor_origin_ref.id).second ||
          !activities.insert(b.original_org_activity_ref).second ||
          !allocations.insert(b.original_labor_allocation_ref).second ||
          !consents.insert(b.consent_ref).second)
        fail("PublicStaffingBindingUniqueness", row.first);
      auto history = take(read_public_market_staffing_labor_history(
          x, b.binding_id, span(offer(x, b.offer_ref).terms)));
      Integer ordinal = 0;
      bool ended = false;
      WorldMoment prior{};
      for (const auto &r : x.purchases.public_staffing_events.rows())
        if (r.second.binding_ref == b.binding_id) {
          const auto &e = r.second;
          if (r.first != e.event_id || ended ||
              !events.insert(r.first).second || e.event_ordinal != ordinal ||
              !header(x.common, ref("public_market_staffing", e.event_id)) ||
              compare(e.occurred_at, prior) < 0 ||
              compare(e.occurred_at, x.now) > 0)
            fail("PublicStaffingEventHistoryMismatch", r.first);
          if (e.kind == PublicMarketStaffingEventKind::accepted) {
            if (ordinal != 0 ||
                !same_source_ref(b.accepted_event_ref,
                                 ref("public_market_staffing", e.event_id)))
              fail("PublicStaffingAcceptedMismatch", r.first);
          } else if (e.kind ==
                     PublicMarketStaffingEventKind::qualification_changed) {
            bool match = false;
            for (const auto &t : x.population.public_market_labor_transitions)
              if (same_source_ref(t.origin_ref, b.labor_origin_ref) &&
                  same_source_ref(
                      e.cause_ref,
                      ref("public_market_labor_transition", t.transition_id)) &&
                  compare(t.occurred_at, e.occurred_at) == 0 &&
                  !std::holds_alternative<PublicMarketLaborReleased>(t.change))
                match = true;
            if (!match)
              fail("PublicStaffingQualificationEventMissing", r.first);
          } else {
            if (!history.release_transition_ref)
              fail("PublicStaffingTerminalWithoutRelease", r.first);
            const auto found = std::find_if(
                x.population.public_market_labor_transitions.begin(),
                x.population.public_market_labor_transitions.end(),
                [&](const auto &t) {
                  return t.transition_id == history.release_transition_ref->id;
                });
            if (found == x.population.public_market_labor_transitions.end() ||
                !same_source_ref(
                    e.cause_ref,
                    std::get<PublicMarketLaborReleased>(found->change)
                        .original_org_boundary_fact_ref) ||
                compare(e.occurred_at, found->occurred_at))
              fail("PublicStaffingTerminalSourceMismatch", r.first);
            ended = true;
          }
          ++ordinal;
          prior = e.occurred_at;
        }
      if (!ordinal || ended != bool(history.release_transition_ref))
        fail("PublicStaffingEventCoverage", row.first);
    }
    if (origins.size() != x.population.public_market_labor_origins.size() ||
        events.size() != x.purchases.public_staffing_events.size())
      fail("PublicStaffingOriginalSourceCoverage");
    for (const auto &t : x.population.public_market_labor_transitions)
      if (!origins.count(t.origin_ref.id))
        fail("OrphanPublicStaffingLaborTransition", t.transition_id);
    for (const auto &row :
         x.purchases.public_staffing_release_consents.rows()) {
      const auto &r = row.second;
      const auto &b = binding(x, r.binding_ref);
      const auto &terms = offer(x, b.offer_ref).terms;
      if (row.first != r.consent_id ||
          !header(x.common, ref("public_market_staffing_release_consent",
                                r.consent_id)) ||
          r.terms_revision != terms.terms_revision ||
          r.release_tick <= terms.from_tick ||
          r.release_tick >= terms.until_tick ||
          r.controller_authority_ref.operation !=
              household_market_staffing_release_operation ||
          compare(r.consented_at, tick(r.release_tick)) > 0)
        fail("PublicStaffingReleaseConsentMismatch", row.first);
      take(x.controls.validate_accepted(r.controller_authority_ref,
                                        r.consented_at.half_day_tick));
      take(x.sources.validate_staffing_contact(
          r.local_contact_source_ref, terms,
          r.controller_authority_ref.actor_id, r.consented_at));
    }
    validate_staffing_fact_coverage(x.common, x.purchases, x.population,
                                    events);
    return Done{};
  });
}
} // namespace hanmo::resident
