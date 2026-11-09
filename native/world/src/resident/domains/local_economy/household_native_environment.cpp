#include "household_market_resolver.hpp"
#include "household_native_environment.hpp"
#include "resident/domains/travel/movement_result_read.hpp"
#include "resident/domains/travel/movement_control.hpp"
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
template <class T, class F> Outcome<T> read(F &&f) {
  try {
    return {true, f(), {}};
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
template <class F> Status check(F &&f) {
  try {
    f();
    return common_success();
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
int cmp(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b));
}
std::size_t order(const CommonStore &c, const SourceRef &source) {
  std::size_t n = 0;
  for (const auto &row : c.state().facts.rows()) {
    if (row.first == source.id) {
      if (row.second.id != source.id || row.second.domain != source.domain ||
          row.second.kind != source.kind)
        fail("HouseholdEnvironmentSourceMismatch", source.id);
      return n;
    }
    ++n;
  }
  fail("HouseholdEnvironmentSourceMissing", source.id);
}
const HouseholdPurchaseAcceptedTerms &
accepted(const HouseholdPurchaseContext &x, const HouseholdPurchase &p) {
  const auto *a = x.state.accepted_terms.find(p.accepted_terms_ref.id);
  if (!a || a->plan.plan_id != p.plan_ref ||
      !same_source_ref(a->source, p.accepted_terms_ref))
    fail("HouseholdEnvironmentAcceptanceRequired", p.purchase_id);
  order(x.common, a->source);
  return *a;
}
const HouseholdMarketServiceDefinition &
market(const HouseholdPurchaseContext &x, const HouseholdPurchasePlan &plan) {
  return *take(read_household_market_service_for_plan(
      x.common, x.catalog, x.state, x.private_market_custody, plan));
}
void current(const HouseholdPurchaseContext &x, const WorldMoment &at) {
  if (cmp(x.now, at))
    fail("HouseholdEnvironmentNotCurrent");
}
void historical(const HouseholdPurchaseContext &x, const WorldMoment &at) {
  if (cmp(at, x.now) > 0)
    fail("HouseholdEnvironmentFutureSource");
}
SourceRef market_cause(const MarketServiceChangeCause &c) {
  return std::visit(
      [](const auto &v) -> SourceRef {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, MarketServiceInitialized>)
          return v.original_opening_acceptance_ref;
        else if constexpr (std::is_same_v<T, MarketServiceOpeningChanged>)
          return v.original_opening_change_ref;
        else if constexpr (std::is_same_v<T, MarketServiceStaffingChanged>)
          return v.original_staffing_event_ref;
        else if constexpr (std::is_same_v<T, MarketServiceFacilityChanged>)
          return v.original_condition_result_ref;
        else
          return v.original_right_change_ref;
      },
      c);
}
bool actual_market_change(const NativeHouseholdEnvironmentContext &z,
                          const HouseholdPurchaseContext &x,
                          const HouseholdPurchase &p, const SourceRef &cause,
                          const WorldMoment &at) {
  const auto &service = *take(read_household_market_service_accepted(
      x.common, x.catalog, x.state, x.private_market_custody, accepted(x, p)));
  bool found = false;
  NativeMarketServiceCapacityRead source{z.market};
  for (const auto *table : {&x.state.market_service_sources.public_sources,
                            &x.state.market_service_sources.private_sources})
    for (const auto &row : *table)
      if (row.slot_ref == service.service_slot &&
          !std::holds_alternative<MarketServiceInitialized>(row.change_cause) &&
          (same_source_ref(market_cause(row.change_cause), cause) ||
           same_source_ref({row.slot_ref.provider_domain,
                            "market_service_capacity", row.source_id},
                           cause))) {
        if (found || cmp(row.occurred_at, at))
          fail("HouseholdMarketChangeIdentityMismatch", cause.id);
        take(source.validate_market_source(row));
        found = true;
      }
  if (found)
    order(x.common, cause);
  return found;
}
const GroupMovementJourney &
journey_for_purchase(const NativeHouseholdEnvironmentContext &z,
                     const HouseholdPurchaseContext &x,
                     const HouseholdPurchase &p, const StableId &id) {
  const auto *j = z.cohort.carriers.journeys.find(id);
  const auto *i =
      j ? std::get_if<HouseholdPolicyGroupInitiator>(&j->initiator) : nullptr;
  const auto *carrier =
      j ? std::get_if<TransportUnitCarrierRef>(&j->carrier) : nullptr;
  const auto *unit = carrier
                         ? z.cohort.carriers.transport_units.find(
                               carrier->transport_unit_ref.transport_unit_id)
                         : nullptr;
  const auto *reg = unit ? z.cohort.registrations.registrations.find(
                               unit->registration_source_ref.id)
                         : nullptr;
  const auto &a = accepted(x, p);
  if (!j || !i || !unit || !reg || i->purchase_plan_ref != p.plan_ref ||
      i->accepted_household_policy_ref != a.plan.accepted_policy_ref.id ||
      i->policy_revision != a.plan.accepted_policy_revision ||
      !i->purchase_ref || *i->purchase_ref != p.purchase_id ||
      !same_source_ref(
          reg->accepted_scope.business_source,
          {"local_economy", "household_purchase_planned", p.plan_ref}))
    fail("HouseholdOriginalDeliveryJourneyRequired", id);
  auto subject = take(read_registered_transport_subject(x.common, x.economy,
                                                        x.population, *reg));
  if (j->sponsor.kind != subject.kind || j->sponsor.id != subject.id)
    fail("HouseholdDeliveryJourneySubjectMismatch", id);
  return *j;
}
const GroupMovementJourney &
journey_for_source(const NativeHouseholdEnvironmentContext &z,
                   const HouseholdPurchaseContext &x,
                   const HouseholdPurchase &p, const SourceRef &cause,
                   const WorldMoment &at) {
  StableId id;
  if (cause.domain != "travel")
    fail("HouseholdActualJourneySourceRequired", cause.id);
  if (cause.kind == "group_movement_opening") {
    const auto *r = z.group_executions.openings.find(cause.id);
    if (r)
      id = r->journey_id;
  } else if (cause.kind == "group_movement_control") {
    const auto *r = z.group_executions.controls.find(cause.id);
    if (r)
      id = r->journey_id;
  } else
    fail("HouseholdActualJourneySourceRequired", cause.id);
  if (id.empty() ||
      cmp(take(read_group_source_moment(x.common, z.cohort.carriers,
                                        z.cohort.movement, z.group_executions,
                                        z.cohort.registrations, cause, 1)),
          at))
    fail("HouseholdJourneyCauseTimeMismatch", cause.id);
  return journey_for_purchase(z, x, p, id);
}
WorldMoment completion(const HouseholdPurchaseContext &x,
                       const HouseholdPurchase &p, const SourceRef &source) {
  order(x.common, source);
  if (source.domain != "local_economy")
    fail("HouseholdServiceCompletionRequired", source.id);
  if (std::holds_alternative<NamedHouseholdBuyer>(p.executor)) {
    const auto *e = x.state.named_service_events.find(source.id);
    if (source.kind != "household_purchase_service" || !e ||
        e->purchase_id != p.purchase_id ||
        e->kind != HouseholdServiceEventKind::completed)
      fail("HouseholdServiceCompletionRequired", source.id);
    // Completion source validation is kept below the cargo projection: the full
    // service validator itself consumes all cargo and would recurse here.
    const auto *binding =
        std::get_if<ActorActionWorkBinding>(&p.execution_binding);
    const auto *elapsed =
        binding ? find_fixed_elapsed(x.elapsed, binding->action_id) : nullptr;
    const auto *terminal =
        elapsed ? std::get_if<ElapsedTerminal>(&elapsed->timing) : nullptr;
    if (!binding || e->original_action_ref != binding->action_id || !elapsed ||
        !terminal || terminal->outcome_ref != e->event_id ||
        cmp(elapsed->last_settled_at, e->occurred_at))
      fail("HouseholdActualServiceTerminalRequired", source.id);
    auto required =
        take(fixed_duration_work_units(1, x.scale.units_per_half_day));
    if (take(compare_fractions(elapsed->credited_units.value, required.value)))
      fail("HouseholdActualServiceCreditRequired", source.id);
    return e->occurred_at;
  }
  const auto *r = x.state.service_results.find(source.id);
  const auto *w = x.state.work_periods.find(source.id);
  if (source.kind != "household_purchase_service_completed" || !r || !w ||
      r->purchase_id != p.purchase_id ||
      w->disposition != WorkPeriodDisposition::provided || !r->period_ref ||
      !same_work_period_ref(*r->period_ref, w->ref) ||
      !same_source_ref(r->coverage_source, source) ||
      cmp(r->completed_at, {w->ref.to_tick, {}}))
    fail("HouseholdActualServicePeriodRequired", source.id);
  return r->completed_at;
}
} // namespace
HouseholdPurchaseContext
NativeHouseholdPurchaseEnvironmentRead::context() const {
  const auto &n = x_.named;
  const auto &c = x_.cohort;
  const auto at = take(n.clock.read_current_moment());
  if (n.private_market_custody != x_.private_market_custody ||
      c.private_market_custody != x_.private_market_custody ||
      &n.common != &c.common || &n.state != &c.state ||
      &n.catalog != &c.catalog || &n.carriers != &c.carriers ||
      &n.movement != &c.movement || &n.registrations != &c.registrations ||
      &n.spatial != &c.spatial || &n.clock != &c.clock ||
      &x_.market.common != &n.common || &x_.market.purchases != &n.state ||
      &x_.market.claims != &n.claims ||
      &x_.inventory_history.mobile.common != &n.common ||
      &x_.inventory_history.mobile.carriers != &n.carriers ||
      &x_.inventory_history.mobile.movement != &n.movement ||
      &x_.inventory_history.mobile.registrations != &n.registrations ||
      &x_.inventory_history.group_executions != &x_.group_executions ||
      &x_.inventory_history.mobile.clock != &n.clock ||
      &x_.inventory_history.mobile.spatial_sources != &n.spatial ||
      &x_.inventory_history.mobile.common_catalog != &n.shared ||
      &x_.retail.common != &n.common ||
      &x_.retail.common_catalog != &n.shared || cmp(at, x_.retail.now) ||
      cmp(at, x_.market.now) || cmp(at, take(c.spatial.current_moment())) ||
      (x_.selected_household_version != 2 &&
       x_.selected_household_version != 3))
    fail("HouseholdEnvironmentSnapshotMismatch");
  return HouseholdPurchaseContext{n.common,
                                  n.shared,
                                  c.economy,
                                  c.population,
                                  n.catalog,
                                  n.state,
                                  x_.controls,
                                  *this,
                                  n.claims,
                                  x_.claim_catalog,
                                  n.facilities,
                                  x_.retail,
                                  n.elapsed,
                                  x_.scale,
                                  at,
                                  x_.transport_expenses,
                                  &n.registrations,
                                  x_.selected_household_version,
                                  x_.mobile_custody,
                                  x_.private_market_custody};
}
Outcome<HouseholdPurchasePresence>
NativeHouseholdPurchaseEnvironmentRead::read_purchase_presence(
    const HouseholdPurchasePlan &plan, const StableId &place,
    const WorldMoment &at) const {
  return read<HouseholdPurchasePresence>([&] {
    auto x = context();
    current(x, at);
    const auto &service = market(x, plan);
    if (service.place_id != place)
      fail("HouseholdMarketPlaceMismatch", plan.plan_id);
    auto actual = take(read_household_purchase_presence(
        x.common, x.economy, x.population, x_.cohort.carriers,
        x_.cohort.movement, x_.cohort.spatial, plan, place,
        service.custody_inventory_id, plan.destination_inventory,
        service.handling_rule_ref, at));
    if (const auto *buyer =
            std::get_if<CohortHouseholdBuyer>(&plan.execution_choice)) {
      auto ref = take(read_civilian_population_ref(x.economy, x.population,
                                                   buyer->cohort_id));
      auto population =
          take(read_population_revision_source(x.economy, x.population, ref));
      if (population.headcount < 1 || population.labor_capacity < 1)
        fail("HouseholdCohortEligiblePersonMissing", buyer->cohort_id);
      if (population.condition_source_ref)
        fail("CohortHouseholdConditionCoverageRequired",
             population.condition_source_ref->id);
    }
    HouseholdPurchasePresence result{actual.actual_presence_source, place, {}};
    result.versions.push_back(x.common.read_version(
        {ResourceKey::Kind::inventory, service.custody_inventory_id}));
    if (plan.destination_inventory != service.custody_inventory_id)
      result.versions.push_back(x.common.read_version(
          {ResourceKey::Kind::inventory, plan.destination_inventory}));
    if (const auto *named =
            std::get_if<NamedHouseholdBuyer>(&plan.execution_choice))
      result.versions.push_back(
          x.common.read_version({ResourceKey::Kind::actor, named->actor_id}));
    return result;
  });
}
Outcome<HouseholdPurchasePresence>
NativeHouseholdPurchaseEnvironmentRead::read_purchase_presence_for_service(
    const HouseholdPurchasePlan &plan,
    const HouseholdMarketServiceDefinition &service,
    const WorldMoment &at) const {
  return read<HouseholdPurchasePresence>([&] {
    auto x = context();
    current(x, at);
    take(validate_household_resolved_market_service(
        x.common, x.catalog, x.state, x.private_market_custody, plan, service));
    const auto &place = service.place_id;
    auto actual = take(read_household_purchase_presence(
        x.common, x.economy, x.population, x_.cohort.carriers,
        x_.cohort.movement, x_.cohort.spatial, plan, place,
        service.custody_inventory_id, plan.destination_inventory,
        service.handling_rule_ref, at));
    if (const auto *buyer =
            std::get_if<CohortHouseholdBuyer>(&plan.execution_choice)) {
      auto ref = take(read_civilian_population_ref(x.economy, x.population,
                                                   buyer->cohort_id));
      auto population =
          take(read_population_revision_source(x.economy, x.population, ref));
      if (population.headcount < 1 || population.labor_capacity < 1)
        fail("HouseholdCohortEligiblePersonMissing", buyer->cohort_id);
      if (population.condition_source_ref)
        fail("CohortHouseholdConditionCoverageRequired",
             population.condition_source_ref->id);
    }
    HouseholdPurchasePresence result{actual.actual_presence_source, place, {}};
    result.versions.push_back(x.common.read_version(
        {ResourceKey::Kind::inventory, service.custody_inventory_id}));
    if (plan.destination_inventory != service.custody_inventory_id)
      result.versions.push_back(x.common.read_version(
          {ResourceKey::Kind::inventory, plan.destination_inventory}));
    if (const auto *named =
            std::get_if<NamedHouseholdBuyer>(&plan.execution_choice))
      result.versions.push_back(
          x.common.read_version({ResourceKey::Kind::actor, named->actor_id}));
    return result;
  });
}
Outcome<HouseholdPurchaseCoverage>
NativeHouseholdPurchaseEnvironmentRead::read_purchase_service_coverage(
    const HouseholdPurchase &p, const MomentSpan &span) const {
  if (std::holds_alternative<NamedHouseholdBuyer>(p.executor))
    return read_named_household_actual_coverage(x_.named, p, span);
  return read_cohort_household_actual_coverage(x_.cohort, p, span);
}
Status
NativeHouseholdPurchaseEnvironmentRead::validate_purchase_service_segment(
    const HouseholdPurchase &p, const HouseholdServiceSegment &segment) const {
  if (std::holds_alternative<NamedHouseholdBuyer>(p.executor))
    return validate_named_household_actual_segment(x_.named, p, segment);
  return validate_cohort_household_actual_segment(x_.cohort, p, segment);
}
Outcome<bool>
NativeHouseholdPurchaseEnvironmentRead::read_local_purchase_handover(
    const HouseholdPurchase &p, const StableId &destination,
    const WorldMoment &at) const {
  return read<bool>([&] {
    auto x = context();
    current(x, at);
    const auto &a = accepted(x, p);
    if (destination != a.plan.destination_inventory)
      fail("HouseholdDestinationAcceptanceMismatch", p.purchase_id);
    const auto &service = *take(read_household_market_service_accepted(
        x.common, x.catalog, x.state, x.private_market_custody, a));
    auto reach = take(read_storage_reachability(
        x.common, x_.cohort.carriers, x_.cohort.movement, x_.cohort.spatial,
        p.holding_inventory_ref, destination, service.handling_rule_ref));
    return std::holds_alternative<SameSiteReachability>(reach);
  });
}
Outcome<HouseholdPurchaseDeliverySource>
NativeHouseholdPurchaseEnvironmentRead::read_purchase_delivery(
    const HouseholdPurchase &p, const SourceRef &source) const {
  return read<HouseholdPurchaseDeliverySource>([&] {
    auto x = context();
    if (source.domain == "travel" && source.kind == "cargo_handling_result")
      return take(read_household_cargo_handling_delivery(
          {x, x_.handling, x_.cohort.carriers, x_.cohort.movement,
           x_.cohort.registrations, x_.cohort.spatial, x_.travel,
           x_.cohort.clock},
          p, source));
    const auto at = completion(x, p, source);
    historical(x, at);
    const auto &a = accepted(x, p);
    const HouseholdPurchaseHandover *found = nullptr;
    for (const auto &row : x.state.handovers.rows())
      if (same_source_ref(row.second.actual_delivery_source, source)) {
        if (found || row.second.purchase_id != p.purchase_id)
          fail("DuplicateHouseholdLocalHandover", source.id);
        found = &row.second;
      }
    if (!found || cmp(found->occurred_at, at) ||
        found->from_inventory_id != p.holding_inventory_ref ||
        found->to_inventory_id != a.plan.destination_inventory)
      fail("HouseholdLocalHandoverRequired", source.id);
    const SourceRef handover{"local_economy", "household_purchase_delivered",
                             found->id};
    if (order(x.common, handover) <= order(x.common, source))
      fail("HouseholdLocalHandoverOrderMismatch", source.id);
    const auto &service = *take(read_household_market_service_accepted(
        x.common, x.catalog, x.state, x.private_market_custody, a));
    auto reach = take(read_storage_reachability_before_source(
        x_.inventory_history, found->from_inventory_id, found->to_inventory_id,
        handover, at, service.handling_rule_ref));
    if (!std::holds_alternative<SameSiteReachability>(reach))
      fail("HouseholdLocalHandoverNotAtSameSite", source.id);
    const auto *receipt = x.retail.state.settlements.find(p.retail_receipt_ref);
    if (!receipt || receipt->terms.purchase_id != p.purchase_id ||
        receipt->lots.size() != found->actual_lots.size())
      fail("HouseholdLocalHandoverRetailMismatch", source.id);
    HouseholdPurchaseDeliverySource actual{source,
                                           at,
                                           p.purchase_id,
                                           found->from_inventory_id,
                                           found->to_inventory_id,
                                           {},
                                           false};
    for (const auto &lot : receipt->lots) {
      if (lot.quantity <= 0 ||
          std::count_if(found->actual_lots.begin(), found->actual_lots.end(),
                        [&](const auto &flow) {
                          return flow.original_lot_id ==
                                     lot.destination_lot_id &&
                                 flow.quantity == lot.quantity;
                        }) != 1)
        fail("HouseholdLocalHandoverLotMismatch", source.id);
      actual.cargo.push_back(
          {found->from_inventory_id, lot.destination_lot_id, lot.quantity});
    }
    return actual;
  });
}
Status NativeHouseholdPurchaseEnvironmentRead::validate_named_purchase_service(
    const NamedHouseholdBuyer &b, const HouseholdPurchasePlan &p,
    const WorldMoment &at) const {
  return validate_named_household_current_participation(x_.named, b, p, at);
}
Outcome<std::vector<FixedServiceInputRef>>
NativeHouseholdPurchaseEnvironmentRead::read_named_service_adoption_inputs(
    const HouseholdPurchase &p, const HouseholdServiceEvent2 &e) const {
  return capture_named_household_adoption_inputs(x_.named, p, e);
}
Status
NativeHouseholdPurchaseEnvironmentRead::validate_named_service_adoption_inputs(
    const HouseholdPurchase &p, const HouseholdServiceEvent2 &e,
    const std::vector<FixedServiceInputRef> &inputs) const {
  return validate_named_household_adoption_inputs(x_.named, p, e, inputs);
}
Outcome<FacilityClaimCloseReason>
NativeHouseholdPurchaseEnvironmentRead::read_named_service_interruption(
    const HouseholdPurchase &p, const SourceRef &cause, const WorldMoment &at,
    bool terminate) const {
  return read<FacilityClaimCloseReason>([&] {
    auto x = context();
    historical(x, at);
    if (cause.domain == "actor_foundation")
      return take(read_named_household_condition_interruption(
          x_.named, p, cause, at, terminate));
    if (terminate)
      fail("HouseholdActualTerminationSourceRequired", cause.id);
    if (cause.domain == "travel" && cause.kind == "travel_exertion_basis") {
      const auto *b = x_.named.movement.exertion_bases.find(cause.id);
      const auto *buyer = std::get_if<NamedHouseholdBuyer>(&p.executor);
      if (!buyer || !b ||
          cmp(take(read_movement_source_moment(x_.named.movement, cause, 1)),
              at) ||
          std::count(b->actual_participants.begin(),
                     b->actual_participants.end(), buyer->actor_id) != 1)
        fail("HouseholdNamedActualDepartureRequired", cause.id);
      order(x.common, cause);
      return FacilityClaimCloseReason::left_place;
    }
    take(validate_household_market_service_interruption(
        x_.market, p.purchase_id, cause, at));
    return FacilityClaimCloseReason::suspended;
  });
}
Outcome<std::vector<SourceRef>>
NativeHouseholdPurchaseEnvironmentRead::read_named_service_participation(
    const HouseholdPurchase &p, const MomentSpan &span) const {
  return read_named_household_actual_participation(x_.named, p, span);
}
Status
NativeHouseholdPurchaseEnvironmentRead::validate_named_service_continuation(
    const HouseholdPurchase &p, const WorldMoment &at) const {
  return check([&] {
    auto x = context();
    current(x, at);
    const auto *b = std::get_if<NamedHouseholdBuyer>(&p.executor);
    const auto *binding =
        std::get_if<ActorActionWorkBinding>(&p.execution_binding);
    if (!b || !binding)
      fail("NamedHouseholdActionBindingRequired", p.purchase_id);
    const auto &a = accepted(x, p);
    take(validate_named_household_current_participation(
        x_.named, *b, a.plan, at, binding->action_id));
    take(read_purchase_presence_for_service(
        a.plan,
        *take(read_household_market_service_accepted(
            x.common, x.catalog, x.state, x.private_market_custody, a)),
        at));
  });
}
Status NativeHouseholdPurchaseEnvironmentRead::validate_purchase_phase_cause(
    const HouseholdPurchase &p, HouseholdPurchasePhase next,
    const SourceRef &cause, const WorldMoment &at) const {
  return check([&] {
    auto x = context();
    historical(x, at);
    accepted(x, p);
    if (p.state == HouseholdPurchasePhase::servicing &&
        next == HouseholdPurchasePhase::blocked) {
      take(validate_household_market_service_interruption(
          x_.market, p.purchase_id, cause, at));
      return;
    }
    if (p.state == HouseholdPurchasePhase::blocked &&
        next == HouseholdPurchasePhase::servicing) {
      if (actual_market_change(x_, x, p, cause, at))
        return;
      fail("HouseholdActualServiceRecoverySourceRequired", cause.id);
    }
    if (next == HouseholdPurchasePhase::in_transit) {
      const auto &journey = journey_for_source(x_, x, p, cause, at);
      const auto &initiator =
          std::get<HouseholdPolicyGroupInitiator>(journey.initiator);
      if (initiator.phase != "delivery")
        fail("HouseholdLoadedReturnRequired", cause.id);
      if (cause.kind == "group_movement_control") {
        const auto *c = x_.group_executions.controls.find(cause.id);
        if (!c || c->operation != GroupControlOperation::resume)
          fail("HouseholdActualJourneyResumeRequired", cause.id);
      } else if (cause.kind != "group_movement_opening")
        fail("HouseholdActualDispatchSourceRequired", cause.id);
      return;
    }
    if (next == HouseholdPurchasePhase::blocked ||
        next == HouseholdPurchasePhase::settling) {
      if (cause.domain == "travel" && cause.kind == "group_movement_control") {
        journey_for_source(x_, x, p, cause, at);
        const auto *control = x_.group_executions.controls.find(cause.id);
        if (!control ||
            (next == HouseholdPurchasePhase::blocked
                 ? control->operation != GroupControlOperation::pause
                 : control->operation != GroupControlOperation::cancel))
          fail("HouseholdJourneyDeferCauseMismatch", cause.id);
        return;
      }
      if (cause.domain == "travel" && cause.kind == "cargo_handling_result") {
        const auto *r = x_.handling.results.find(cause.id);
        const auto *e =
            r ? x_.handling.executions.find(r->execution_ref) : nullptr;
        const auto *t = e ? x_.handling.terms.find(e->terms_ref) : nullptr;
        if (!r || !e || !t ||
            !same_source_ref(r->business_source, p.accepted_terms_ref) ||
            !same_source_ref(t->business_source, p.accepted_terms_ref) ||
            r->terms_ref != t->terms_id || !e->terminal_result_ref ||
            *e->terminal_result_ref != r->result_id || !r->reason_source_ref ||
            cmp(r->occurred_at, at) ||
            (next == HouseholdPurchasePhase::blocked
                 ? r->status != CargoHandlingResultStatus::interrupted
                 : r->status != CargoHandlingResultStatus::cancelled))
          fail("HouseholdHandlingDeferCauseMismatch", cause.id);
        order(x.common, cause);
        return;
      }
    }
    fail("HouseholdOriginalPhaseCauseRequired", cause.id);
  });
}
} // namespace hanmo::resident
