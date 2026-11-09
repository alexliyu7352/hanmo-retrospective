#include "household_market_resolver.hpp"
#include "household_transport_read.hpp"
#include "cohort_household_service.hpp"
#include "resident/domains/travel/transport_authority_codec.hpp"
#include <algorithm>
#include <tuple>
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
std::size_t order(const CommonStore &c, const SourceRef &s) {
  std::size_t n = 0;
  for (const auto &r : c.state().facts.rows()) {
    if (r.first == s.id) {
      if (r.second.id != s.id || r.second.domain != s.domain ||
          r.second.kind != s.kind)
        fail("HouseholdTransportSourceMismatch", s.id);
      return n;
    }
    ++n;
  }
  fail("HouseholdTransportSourceMissing", s.id);
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
bool same_subject(const SubjectRef &a, const SubjectRef &b) {
  return a.kind == b.kind && a.id == b.id;
}
void add_source(std::vector<SourceRef> &out, const SourceRef &s) {
  if (std::none_of(out.begin(), out.end(),
                   [&](const auto &v) { return same_source_ref(v, s); }))
    out.push_back(s);
}
void original_service(const HouseholdPurchaseContext &x,
                      const HouseholdPurchase &p, bool allow_interrupted,
                      std::vector<SourceRef> &sources) {
  take(validate_cohort_household_service(x, p));
  HouseholdPurchaseContracts owner(x);
  bool completed = false, interrupted = false;
  for (const auto &r : x.state.service_results.rows())
    if (r.second.purchase_id == p.purchase_id) {
      if (r.first != r.second.id || !r.second.period_ref ||
          cmp(r.second.completed_at, x.now) > 0)
        fail("HouseholdTransportServiceSourceMismatch", p.purchase_id);
      auto period =
          take(read_verified_work_period(*r.second.period_ref, owner));
      if (cmp(r.second.completed_at, {period.ref.to_tick, {}}))
        fail("HouseholdTransportServiceSourceMismatch", r.first);
      const bool provided =
          period.disposition == WorkPeriodDisposition::provided;
      SourceRef source{"local_economy",
                       provided ? "household_purchase_service_completed"
                                : "household_purchase_period_interrupted",
                       r.first};
      order(x.common, source);
      add_source(sources, source);
      completed |= provided;
      interrupted |= !provided && period.reason.has_value();
    }
  if (allow_interrupted && !completed) {
    auto attempts =
        take(read_cohort_household_service_attempts(x, p.purchase_id));
    const auto &current = attempts.back();
    if (current.claim_episode.close_receipt_ref) {
      auto close = take(read_facility_claim_receipt(
          x.claims, *current.claim_episode.close_receipt_ref));
      for (const auto &closed : close.closed_claims)
        if (closed.claim_id == current.claim_episode.original_claim.claim_id &&
            closed.reason == FacilityClaimCloseReason::suspended &&
            cmp(close.occurred_at, x.now) <= 0) {
          interrupted = true;
          add_source(sources, close.source_result_ref);
        }
    }
  }
  if (!completed && (!allow_interrupted || !interrupted))
    fail("HouseholdTransportActualServiceResultRequired", p.purchase_id);
  if (p.state == HouseholdPurchasePhase::servicing ||
      p.state == HouseholdPurchasePhase::planned)
    fail("HouseholdTransportLiveServiceCannotDepart", p.purchase_id);
}
void no_purchase_return(const HouseholdPurchaseTransportContext &z,
                        const HouseholdPurchasePlan &p,
                        const TransportUnitState &u,
                        const HouseholdPurchasePolicy &policy,
                        std::vector<SourceRef> &sources) {
  const auto &x = z.purchase;
  bool ended = false;
  for (const auto &r : z.carriers.journeys.rows()) {
    const auto *carrier =
        std::get_if<TransportUnitCarrierRef>(&r.second.carrier);
    const auto *i =
        std::get_if<HouseholdPolicyGroupInitiator>(&r.second.initiator);
    if (!carrier ||
        carrier->transport_unit_ref.transport_unit_id != u.transport_unit_id ||
        !i || i->purchase_plan_ref != p.plan_id || i->phase != "approach" ||
        i->purchase_ref)
      continue;
    if (r.second.state != GroupJourneyLifecycle::arrived &&
        r.second.state != GroupJourneyLifecycle::cancelled)
      continue;
    auto terminal = take(read_actual_group_journey_terminal(
        x.common, z.carriers, z.movement, z.executions, r.first));
    if (cmp(terminal.terminal.occurred_at, x.now) > 0)
      fail("HouseholdReturnBeforeApproachTerminal", p.plan_id);
    // An actual cancellation, or a completed approach whose accepted shopping
    // authority was actually withdrawn, supplies the finite return basis.
    if (terminal.terminal.outcome == GroupJourneyTerminalOutcome::cancelled ||
        policy.closed_source_ref) {
      ended = true;
      add_source(sources, terminal.terminal.actual_terminal_source);
    }
  }
  if (!ended)
    fail("HouseholdReturnOriginalEndedOrBlockedSourceRequired", p.plan_id);
}
} // namespace
Outcome<HouseholdPurchaseTransportRead> read_household_purchase_transport(
    const HouseholdPurchaseTransportContext &z,
    const TransportRegistrationSource &requested_registration,
    const TransportUnitState &u, const HouseholdPolicyGroupInitiator &i,
    const SubjectRef &sponsor, const WorldMoment &at) {
  try {
    const auto &x = z.purchase;
    const auto *stored =
        z.registrations.registrations.find(requested_registration.id);
    if (!stored)
      fail("HouseholdTransportRegistrationSourceRequired",
           requested_registration.id);
    const auto &r = *stored;
    if (cmp(at, x.now) || cmp(at, take(z.spatial.current_moment())))
      fail("HouseholdTransportActualCurrentMomentRequired",
           u.transport_unit_id);
    NativeHouseholdTransportRegistrationBusinessRead registration_owner(
        x, z.carriers, z.registrations, z.spatial, z.movement, z.executions);
    take(registration_owner.validate_registration_authority(r.accepted_scope));
    const auto *p = x.state.plans.find(r.accepted_scope.business_source.id);
    if (!p || u.state == TransportUnitLifecycle::dissolved ||
        r.transport_unit_id != u.transport_unit_id ||
        !same_source_ref(u.registration_source_ref,
                         {"travel", "transport_registration", r.id}) ||
        u.custody_ref != r.accepted_scope.custody_ref ||
        u.inventory_ids != r.accepted_scope.inventory_ids ||
        u.vehicle_refs != r.accepted_scope.vehicle_refs)
      fail("HouseholdTransportRegisteredScopeRequired", u.transport_unit_id);
    order(x.common, u.registration_source_ref);
    const auto actual = take(read_registered_transport_subject(
        x.common, x.economy, x.population, r));
    if (!same_subject(actual, u.subject) || !same_subject(actual, sponsor) ||
        actual.kind != SubjectKind::cohort)
      fail("HouseholdTransportActualBuyerMismatch", u.transport_unit_id);
    const auto *buyer = std::get_if<CohortHouseholdBuyer>(&p->execution_choice);
    const auto *policy = x.state.policies.find(p->accepted_policy_ref.id);
    if (!buyer || !policy || i.purchase_plan_ref != p->plan_id ||
        i.accepted_household_policy_ref != p->accepted_policy_ref.id ||
        i.policy_revision != p->accepted_policy_revision ||
        i.policy_revision != policy->revision ||
        policy->household_id != p->household_id ||
        !same_source_ref(policy->acceptance_source_ref,
                         p->accepted_policy_ref) ||
        !same_control(policy->controller_authority_ref,
                      p->controller_authority_ref) ||
        (i.phase != "approach" && i.phase != "delivery" && i.phase != "return"))
      fail("HouseholdTransportInitiatorScopeMismatch", u.transport_unit_id);
    take(x.controls.validate_accepted(policy->controller_authority_ref,
                                      policy->accepted_tick));
    order(x.common, policy->acceptance_source_ref);
    if (cmp(r.occurred_at, at) > 0 ||
        at.half_day_tick < policy->valid_from_tick ||
        cmp(at, {p->delivery_deadline, {}}) > 0 ||
        p->delivery_deadline > policy->valid_until_tick ||
        r.accepted_scope.labor_until_tick != p->delivery_deadline)
      fail("HouseholdTransportOutsideAcceptedWindow", p->plan_id);
    HouseholdPurchaseTransportRead out;
    out.plan_id = p->plan_id;
    out.purchase_id = i.purchase_ref;
    out.phase = i.phase;
    out.household_id = p->household_id;
    out.executor_cohort_id = actual.id;
    out.population_selection_ref = buyer->population_selection_ref;
    out.accepted_policy_ref = p->accepted_policy_ref;
    out.policy_revision = policy->revision;
    out.policy_acceptance_source = policy->acceptance_source_ref;
    out.finite_window = {r.occurred_at, {p->delivery_deadline, {}}};
    out.custody_ref = u.custody_ref;
    out.funding_source_ref = policy->acceptance_source_ref;
    out.original_phase_sources = {r.accepted_scope.business_source,
                                  u.registration_source_ref,
                                  policy->acceptance_source_ref};
    if (policy->closed_source_ref) {
      const auto *next = x.state.policies.find(policy->closed_source_ref->id);
      if (!next ||
          !same_source_ref(next->acceptance_source_ref,
                           *policy->closed_source_ref) ||
          next->household_id != policy->household_id ||
          next->revision != policy->revision + 1 ||
          next->accepted_tick > at.half_day_tick ||
          order(x.common, next->acceptance_source_ref) <=
              order(x.common, policy->acceptance_source_ref))
        fail("HouseholdTransportPolicyClosureSourceRequired", policy->id);
      add_source(out.original_phase_sources, next->acceptance_source_ref);
      if (i.phase != "return")
        fail("HouseholdTransportPolicyNoNewDispatch", policy->id);
    } else {
      auto current = take(
          x.controls.current(policy->controller_actor_id, p->household_id,
                             household_purchase_operation, at.half_day_tick));
      if (!current || !same_control(*current, p->controller_authority_ref))
        fail("HouseholdTransportCurrentControlRequired", p->plan_id);
    }
    const SpatialCarrierRef carrier = TransportUnitCarrierRef{
        {u.subject, u.transport_unit_id}, u.composition_revision};
    out.origin_anchor = take(read_carrier_movement_anchor(z.movement, carrier));
    if (i.phase == "approach") {
      if (i.purchase_ref || at.half_day_tick > p->latest_start_tick)
        fail("HouseholdApproachScopeMismatch", p->plan_id);
      for (const auto &row : x.state.purchases.rows())
        if (row.second.plan_ref == p->plan_id)
          fail("HouseholdApproachAlreadyAcceptedPurchase", p->plan_id);
      out.permitted_destinations.push_back(take(z.spatial.original_site_anchor(
          *take(read_household_original_market_place(
              x.common, x.catalog, x.state, x.private_market_custody,
              p->known_market_ref)))));
    } else if (i.purchase_ref) {
      const auto *purchase = x.state.purchases.find(*i.purchase_ref);
      const auto *a =
          purchase
              ? x.state.accepted_terms.find(purchase->accepted_terms_ref.id)
              : nullptr;
      if (!purchase || !a || purchase->plan_ref != p->plan_id ||
          !same_source_ref(a->source, purchase->accepted_terms_ref) ||
          cmp(a->accepted_at, at) > 0 || a->plan.plan_id != p->plan_id ||
          !same_source_ref(a->plan.accepted_policy_ref, p->accepted_policy_ref))
        fail("HouseholdTransportPurchaseAcceptanceRequired", p->plan_id);
      order(x.common, a->source);
      take(validate_household_buyer_adoption(
          {x.common, x.economy, x.population, &z.registrations}, *p, *a));
      const auto *executor =
          std::get_if<CohortHouseholdBuyer>(&purchase->executor);
      if (!executor || executor->cohort_id != actual.id ||
          executor->population_selection_ref != buyer->population_selection_ref)
        fail("HouseholdTransportPurchaseExecutorMismatch",
             purchase->purchase_id);
      const auto *receipt = take(read_household_retail_settlement(
          x.common, x.retail.state, purchase->retail_receipt_ref));
      HouseholdPurchaseContracts owner(x);
      take(owner.validate_household_retail_settlement(*receipt));
      add_source(out.original_phase_sources, a->source);
      add_source(out.original_phase_sources,
                 {"commerce", "household_retail_settled", receipt->id});
      original_service(x, *purchase, i.phase == "return",
                       out.original_phase_sources);
      take(validate_household_purchase_cargo(x, *purchase));
      auto cargo =
          take(read_household_purchase_cargo(x, purchase->purchase_id));
      for (const auto &pick : cargo.unreceived_cargo)
        if (std::find(u.inventory_ids.begin(), u.inventory_ids.end(),
                      pick.inventory_id) != u.inventory_ids.end())
          out.cargo_lot_refs.push_back(pick);
      for (const auto &source : cargo.actual_handover_sources)
        add_source(out.original_phase_sources, source);
      if (i.phase == "delivery") {
        if (purchase->state == HouseholdPurchasePhase::received ||
            purchase->state == HouseholdPurchasePhase::lost ||
            purchase->state == HouseholdPurchasePhase::cancelled ||
            out.cargo_lot_refs.empty())
          fail("HouseholdDeliveryActualLoadedCargoRequired",
               purchase->purchase_id);
        out.permitted_destinations.push_back(take(read_inventory_spatial_anchor(
            x.common, z.carriers, z.movement, z.spatial,
            a->plan.destination_inventory)));
      } else
        out.permitted_destinations.push_back(r.departure_anchor);
    } else {
      if (i.phase != "return")
        fail("HouseholdTransportPurchaseResultRequired", p->plan_id);
      for (const auto &row : x.state.purchases.rows())
        if (row.second.plan_ref == p->plan_id)
          fail("HouseholdReturnPurchaseReferenceRequired", p->plan_id);
      no_purchase_return(z, *p, u, *policy, out.original_phase_sources);
      out.permitted_destinations.push_back(r.departure_anchor);
    }
    // Gross funding is still projected by the one allowance reader. The route
    // quote is not an accepted payable and this read does not reserve or spend
    // it.
    take(read_household_purchase_allowance(x, policy->acceptance_source_ref,
                                           policy->revision));
    return {true, std::move(out), {}};
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
Status validate_household_transport_phase_adoption(
    const HouseholdPurchaseTransportContext &z,
    const GroupMovementJourney &journey) {
  try {
    const auto &x = z.purchase;
    const auto *i =
        std::get_if<HouseholdPolicyGroupInitiator>(&journey.initiator);
    const auto *carrier =
        std::get_if<TransportUnitCarrierRef>(&journey.carrier);
    const auto *unit = carrier
                           ? z.carriers.transport_units.find(
                                 carrier->transport_unit_ref.transport_unit_id)
                           : nullptr;
    const auto *r = unit ? z.registrations.registrations.find(
                               unit->registration_source_ref.id)
                         : nullptr;
    if (!i || !carrier || !unit || !r)
      fail("HouseholdJourneyOriginalRegistrationRequired", journey.journey_id);
    NativeHouseholdTransportRegistrationBusinessRead registration_owner(
        x, z.carriers, z.registrations, z.spatial, z.movement, z.executions);
    take(registration_owner.validate_registration_authority(r->accepted_scope));
    const auto *p = x.state.plans.find(i->purchase_plan_ref);
    const auto *policy =
        x.state.policies.find(i->accepted_household_policy_ref);
    const auto actual = take(read_registered_transport_subject(
        x.common, x.economy, x.population, *r));
    if (!p || !policy ||
        !same_source_ref(
            r->accepted_scope.business_source,
            {"local_economy", "household_purchase_planned", p->plan_id}) ||
        !same_subject(actual, journey.sponsor) ||
        !same_subject(actual, carrier->transport_unit_ref.subject) ||
        i->policy_revision != p->accepted_policy_revision ||
        policy->revision != i->policy_revision ||
        !same_source_ref(p->accepted_policy_ref,
                         policy->acceptance_source_ref) ||
        p->household_id != policy->household_id ||
        (i->phase != "approach" && i->phase != "delivery" &&
         i->phase != "return"))
      fail("HouseholdJourneyOriginalPolicyMismatch", journey.journey_id);
    const GroupMovementOpeningSource *opening = nullptr;
    for (const auto &row : z.executions.openings.rows())
      if (row.second.journey_id == journey.journey_id) {
        if (opening || row.first != row.second.id)
          fail("HouseholdJourneyOpeningAmbiguous", journey.journey_id);
        opening = &row.second;
      }
    const auto *opened =
        opening ? std::get_if<TransportUnitRef>(&opening->carrier_origin)
                : nullptr;
    if (!opening || !opened ||
        opened->transport_unit_id != unit->transport_unit_id ||
        !same_subject(opened->subject, actual) ||
        cmp(opening->occurred_at, journey.started_at) ||
        cmp(journey.started_at, x.now) > 0 ||
        cmp(r->occurred_at, journey.started_at) > 0 ||
        cmp(journey.started_at, {p->delivery_deadline, {}}) > 0)
      fail("HouseholdJourneyOpeningSourceRequired", journey.journey_id);
    const SourceRef opening_source{"travel", "group_movement_opening",
                                   opening->id};
    const auto cut = order(x.common, opening_source);
    if (order(x.common, unit->registration_source_ref) >= cut ||
        order(x.common, p->accepted_policy_ref) >= cut)
      fail("HouseholdJourneyAcceptanceAfterOpening", journey.journey_id);
    bool closed = false;
    if (policy->closed_source_ref) {
      const auto *next = x.state.policies.find(policy->closed_source_ref->id);
      if (!next ||
          !same_source_ref(next->acceptance_source_ref,
                           *policy->closed_source_ref) ||
          next->household_id != policy->household_id ||
          next->revision != policy->revision + 1)
        fail("HouseholdTransportPolicyClosureSourceRequired", policy->id);
      closed = order(x.common, next->acceptance_source_ref) < cut;
      if (closed && next->accepted_tick > journey.started_at.half_day_tick)
        fail("HouseholdJourneyClosureCausalityMismatch", journey.journey_id);
      if (!closed && next->accepted_tick < journey.started_at.half_day_tick)
        fail("HouseholdJourneyClosureCausalityMismatch", journey.journey_id);
    }
    if (closed && i->phase != "return")
      fail("HouseholdJourneyAdoptedAfterWithdrawal", journey.journey_id);
    if (i->phase == "approach") {
      if (i->purchase_ref ||
          journey.started_at.half_day_tick > p->latest_start_tick)
        fail("HouseholdApproachScopeMismatch", journey.journey_id);
      for (const auto &row : x.state.purchases.rows())
        if (row.second.plan_ref == p->plan_id &&
            order(x.common, row.second.accepted_terms_ref) < cut)
          fail("HouseholdApproachAlreadyAcceptedPurchase", journey.journey_id);
    } else if (i->purchase_ref) {
      const auto *purchase = x.state.purchases.find(*i->purchase_ref);
      const auto *a =
          purchase
              ? x.state.accepted_terms.find(purchase->accepted_terms_ref.id)
              : nullptr;
      if (!purchase || !a || purchase->plan_ref != p->plan_id ||
          !same_source_ref(a->source, purchase->accepted_terms_ref) ||
          order(x.common, a->source) >= cut ||
          cmp(a->accepted_at, journey.started_at) > 0)
        fail("HouseholdJourneyPurchaseAdoptionRequired", journey.journey_id);
      take(validate_household_buyer_adoption(
          {x.common, x.economy, x.population, &z.registrations}, *p, *a));
      take(validate_cohort_household_service(x, *purchase));
      auto attempts = take(
          read_cohort_household_service_attempts(x, purchase->purchase_id));
      const CohortHouseholdServiceAttempt *adopted = nullptr;
      for (const auto &attempt : attempts)
        if (order(x.common, attempt.opening_source) < cut)
          adopted = &attempt;
      if (!adopted || !adopted->claim_episode.close_receipt_ref)
        fail("HouseholdJourneyLiveServiceAtOpening", journey.journey_id);
      auto close = take(read_facility_claim_receipt(
          x.claims, *adopted->claim_episode.close_receipt_ref));
      if (order(x.common, close.source_result_ref) >= cut ||
          cmp(close.occurred_at, journey.started_at) > 0)
        fail("HouseholdJourneyServiceClosureAfterOpening", journey.journey_id);
      bool completed = false, interrupted = false;
      for (const auto &v : close.closed_claims)
        if (v.claim_id == adopted->claim_episode.original_claim.claim_id) {
          completed = v.reason == FacilityClaimCloseReason::completed;
          interrupted = v.reason == FacilityClaimCloseReason::suspended;
        }
      if (completed) {
        if (!adopted->result || !adopted->period ||
            adopted->period->disposition != WorkPeriodDisposition::provided ||
            order(x.common,
                  {"local_economy", "household_purchase_service_completed",
                   adopted->result->id}) >= cut)
          fail("HouseholdJourneyCompletedServiceRequired", journey.journey_id);
      }
      if (!completed && (i->phase != "return" || !interrupted))
        fail("HouseholdJourneyServiceResultRequired", journey.journey_id);
      const auto *retail = take(read_household_retail_settlement(
          x.common, x.retail.state, purchase->retail_receipt_ref));
      HouseholdPurchaseContracts owner(x);
      take(owner.validate_household_retail_settlement(*retail));
      if (order(x.common,
                {"commerce", "household_retail_settled", retail->id}) >= cut)
        fail("HouseholdJourneyRetailAfterOpening", journey.journey_id);
      auto cargo = take(read_household_purchase_cargo_at_adoption(
          x, purchase->purchase_id, journey.started_at, opening_source));
      if (i->phase == "delivery" &&
          std::none_of(cargo.unreceived_cargo.begin(),
                       cargo.unreceived_cargo.end(), [&](const auto &lot) {
                         return std::find(
                                    r->accepted_scope.inventory_ids.begin(),
                                    r->accepted_scope.inventory_ids.end(),
                                    lot.inventory_id) !=
                                r->accepted_scope.inventory_ids.end();
                       }))
        fail("HouseholdJourneyNoActualLoadedCargo", journey.journey_id);
    } else {
      if (i->phase != "return")
        fail("HouseholdJourneyPurchaseReferenceRequired", journey.journey_id);
      for (const auto &row : x.state.purchases.rows())
        if (row.second.plan_ref == p->plan_id &&
            order(x.common, row.second.accepted_terms_ref) < cut)
          fail("HouseholdReturnPurchaseReferenceRequired", journey.journey_id);
      bool basis = false;
      for (const auto &row : z.carriers.journeys.rows()) {
        const auto *old =
            std::get_if<HouseholdPolicyGroupInitiator>(&row.second.initiator);
        const auto *old_carrier =
            std::get_if<TransportUnitCarrierRef>(&row.second.carrier);
        if (!old || !old_carrier ||
            old_carrier->transport_unit_ref.transport_unit_id !=
                unit->transport_unit_id ||
            old->purchase_plan_ref != p->plan_id || old->phase != "approach" ||
            old->purchase_ref)
          continue;
        const auto *t = z.carriers.terminals.find(row.first);
        if (!t)
          continue;
        const auto position = order(x.common, t->actual_terminal_source);
        if (position >= cut)
          continue;
        auto actual_terminal = take(read_actual_group_journey_terminal(
            x.common, z.carriers, z.movement, z.executions, row.first));
        if (cmp(actual_terminal.terminal.occurred_at, journey.started_at) > 0)
          fail("HouseholdReturnTerminalCausalityMismatch", journey.journey_id);
        basis |= closed || actual_terminal.terminal.outcome ==
                               GroupJourneyTerminalOutcome::cancelled;
      }
      if (!basis)
        fail("HouseholdReturnOriginalEndedOrBlockedSourceRequired",
             journey.journey_id);
    }
    return common_success();
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
Status validate_household_transport_journey_request(
    const HouseholdPurchaseTransportContext &z, const StartGroupJourney &q,
    const TransportUnitState &u, const TransportManifestRead &m,
    const MovementAnchor &destination) {
  try {
    const auto *i = std::get_if<HouseholdPolicyGroupInitiator>(&q.initiator);
    const auto *r =
        z.registrations.registrations.find(u.registration_source_ref.id);
    if (!i || !r)
      fail("HouseholdJourneyOriginalRegistrationRequired", u.transport_unit_id);
    auto scope = take(read_household_purchase_transport(z, *r, u, *i, q.sponsor,
                                                        z.purchase.now));
    const SpatialCarrierRef carrier = TransportUnitCarrierRef{
        {u.subject, u.transport_unit_id}, u.composition_revision};
    if (q.transport_unit_ref.transport_unit_id != u.transport_unit_id ||
        !same_subject(q.transport_unit_ref.subject, u.subject) ||
        q.expected_composition_revision != u.composition_revision ||
        m.composition_revision != u.composition_revision ||
        spatial_carrier_key(m.carrier) != spatial_carrier_key(carrier) ||
        !take(same_actual_movement_position(m.actual_anchor,
                                            scope.origin_anchor)))
      fail("HouseholdJourneyManifestScopeMismatch", u.transport_unit_id);
    bool permitted = false;
    for (const auto &anchor : scope.permitted_destinations)
      permitted |= take(same_actual_movement_position(anchor, destination));
    if (!permitted)
      fail("HouseholdJourneyDestinationOutsidePlan", scope.plan_id);
    const auto *home = z.purchase.economy.households.find(scope.household_id);
    const auto *wallet =
        home ? z.purchase.common.wallet(home->wallet_id) : nullptr;
    TransportExpenseAuthority funding = HouseholdPurchaseExpenseAuthority{
        scope.household_id,    scope.accepted_policy_ref.id,
        scope.policy_revision, scope.plan_id,
        scope.purchase_id,     scope.phase};
    if (!wallet ||
        !owner_equal(wallet->owner,
                     {Owner::Kind::household, scope.household_id, {}}) ||
        q.payer_wallet_id != wallet->id ||
        !same_transport_expense_authority(q.expense_authority, funding))
      fail("HouseholdJourneyFundingScopeMismatch", scope.plan_id);
    if (q.cargo_custody_refs != std::vector<StableId>{scope.custody_ref})
      fail("HouseholdJourneyCustodyScopeMismatch", scope.plan_id);
    std::vector<SourceRef> expected;
    for (const auto &s : scope.original_phase_sources)
      if (s.domain == "commerce" && s.kind == "household_retail_settled")
        expected.push_back(s);
    if (q.shipment_refs.size() != expected.size())
      fail("HouseholdJourneyShipmentScopeMismatch", scope.plan_id);
    for (std::size_t n = 0; n < expected.size(); ++n)
      if (!same_source_ref(q.shipment_refs[n], expected[n]))
        fail("HouseholdJourneyShipmentScopeMismatch", scope.plan_id);
    return common_success();
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
} // namespace hanmo::resident
