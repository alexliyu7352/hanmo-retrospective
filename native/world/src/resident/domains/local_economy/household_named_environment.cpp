#include "household_market_resolver.hpp"
#include "household_named_environment.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const StableId &code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{value.error};
  return std::move(value.value);
}
void require(Status value) {
  if (!value.ok)
    throw Failure{value.error};
}
template <class T, class F> Outcome<T> outcome(F &&call) {
  try {
    return {true, call(), {}};
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
template <class F> Status checked(F &&call) {
  try {
    call();
    return common_success();
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
int cmp(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b));
}
SourceRef service_source(const HouseholdServiceEvent2 &e) {
  return {"local_economy", "household_purchase_service", e.event_id};
}
void header(const CommonStore &c, const SourceRef &r) {
  const auto *h = c.state().facts.find(r.id);
  if (!h || h->id != r.id || h->domain != r.domain || h->kind != r.kind)
    fail("HouseholdOriginalSourceMissing", r.id);
}
const NamedHouseholdBuyer &buyer(const HouseholdPurchase &p) {
  const auto *v = std::get_if<NamedHouseholdBuyer>(&p.executor);
  if (!v)
    fail("NamedHouseholdBuyerRequired", p.purchase_id);
  return *v;
}
const HouseholdPurchaseAcceptedTerms &
accepted(const HouseholdNamedEnvironmentContext &x,
         const HouseholdPurchase &p) {
  const auto *a = x.state.accepted_terms.find(p.accepted_terms_ref.id);
  if (!a || !same_source_ref(a->source, p.accepted_terms_ref) ||
      a->plan.plan_id != p.plan_ref ||
      !std::holds_alternative<NamedHouseholdBuyer>(a->plan.execution_choice) ||
      std::get<NamedHouseholdBuyer>(a->plan.execution_choice).actor_id !=
          buyer(p).actor_id)
    fail("HouseholdOriginalAcceptanceMissing", p.purchase_id);
  header(x.common, a->source);
  return *a;
}
void now(const HouseholdNamedEnvironmentContext &x, const WorldMoment &at) {
  if (cmp(at, take(x.clock.read_current_moment())) ||
      cmp(at, x.body.actual_now) || cmp(at, take(x.spatial.current_moment())))
    fail("HouseholdEnvironmentClockMismatch");
}
void not_future(const HouseholdNamedEnvironmentContext &x,
                const WorldMoment &at) {
  if (cmp(at, take(x.clock.read_current_moment())) > 0 ||
      cmp(at, x.body.actual_now) > 0)
    fail("HouseholdEnvironmentFutureSource");
}
bool condition_source(const SourceRef &s) {
  return s.domain == "actor_foundation" &&
         (s.kind == "initial_condition_seed" || s.kind == "condition_receipt");
}
void append(std::vector<FixedServiceInputRef> &into, const SourceRef &source,
            Integer schema) {
  for (const auto &v : into)
    if (const auto *p = std::get_if<FixedAppliedInput>(&v))
      if (same_source_ref(p->source_ref, source)) {
        if (p->source_schema != schema)
          fail("HouseholdInputSchemaConflict", source.id);
        return;
      }
  into.push_back(FixedAppliedInput{source, schema});
}
void append(std::vector<SourceRef> &into, const SourceRef &ref) {
  if (std::none_of(into.begin(), into.end(),
                   [&](const auto &p) { return same_source_ref(p, ref); }))
    into.push_back(ref);
}
const FixedAppliedInput &
adopted_body(const std::vector<FixedServiceInputRef> &inputs) {
  const FixedAppliedInput *found = nullptr;
  for (const auto &v : inputs)
    if (const auto *p = std::get_if<FixedAppliedInput>(&v))
      if (condition_source(p->source_ref)) {
        if (found)
          fail("DuplicateHouseholdAdoptedCondition");
        found = p;
      }
  if (!found)
    fail("HouseholdAdoptedConditionRequired");
  return *found;
}
const HouseholdServiceEvent2 &
admission_for_span(const HouseholdNamedEnvironmentContext &x,
                   const HouseholdPurchase &p, const MomentSpan &span) {
  const HouseholdServiceEvent2 *found = nullptr;
  for (const auto &ep : x.state.named_service_claim_episodes) {
    if (ep.purchase_id != p.purchase_id)
      continue;
    const auto *event =
        x.state.named_service_events.find(ep.admission_result_ref.id);
    if (!event ||
        !same_source_ref(ep.admission_result_ref, service_source(*event)) ||
        event->purchase_id != p.purchase_id ||
        (event->kind != HouseholdServiceEventKind::accepted &&
         event->kind != HouseholdServiceEventKind::resumed))
      fail("HouseholdAdmissionSourceMissing", ep.claim_id);
    if (cmp(event->occurred_at, span.from) > 0)
      continue;
    if (ep.close_result_ref) {
      const auto *closed =
          x.state.named_service_events.find(ep.close_result_ref->id);
      if (!closed ||
          !same_source_ref(service_source(*closed), *ep.close_result_ref) ||
          closed->purchase_id != p.purchase_id)
        fail("HouseholdEpisodeCloseSourceMissing", ep.claim_id);
      if (cmp(span.to, closed->occurred_at) > 0)
        continue;
    }
    if (found)
      fail("HouseholdSpanMultipleAdmissions", p.purchase_id);
    found = event;
  }
  if (!found)
    fail("HouseholdSpanAdmissionRequired", p.purchase_id);
  header(x.common, service_source(*found));
  return *found;
}
const FixedServiceTransition &
adoption_transition(const HouseholdNamedEnvironmentContext &x,
                    const HouseholdPurchase &p,
                    const HouseholdServiceEvent2 &event) {
  const auto *binding =
      std::get_if<ActorActionWorkBinding>(&p.execution_binding);
  if (!binding || binding->action_kind != "household_purchase")
    fail("NamedHouseholdActionBindingRequired", p.purchase_id);
  const FixedServiceTransition *found = nullptr;
  for (const auto &t : x.elapsed.fixed_service_transitions)
    if (t.transition_id == event.event_id) {
      if (found || t.action_ref != binding->action_id ||
          cmp(t.occurred_at, event.occurred_at) ||
          !same_source_ref(t.execution_source_ref, p.accepted_terms_ref) ||
          (t.kind != FixedTransitionKind::started &&
           t.kind != FixedTransitionKind::resumed))
        fail("HouseholdAdoptionTransitionMismatch", event.event_id);
      found = &t;
    }
  if (!found)
    fail("HouseholdAdoptionTransitionRequired", event.event_id);
  return *found;
}
ActorHistoricalSpatialPoint
adoption_position(const HouseholdNamedEnvironmentContext &x,
                  const HouseholdPurchase &p,
                  const HouseholdServiceEvent2 &event) {
  const auto &a = accepted(x, p);
  auto point = take(read_actor_spatial_location_before_source(
      x.common, x.shared, x.movement, x.carriers, x.registrations, x.body.state,
      x.body.catalog, x.spatial, buyer(p).actor_id, a.presence_source_ref,
      service_source(event), event.occurred_at, x.military_handoffs, {},
      x.body.family_catalog));
  auto site = take(x.spatial.original_site_anchor(
      take(read_household_market_service_accepted(x.common, x.catalog, x.state,
                                                  x.private_market_custody, a))
          ->place_id));
  if (!take(same_spatial_anchor(point.actual_anchor, site)))
    fail("HouseholdAdoptionNotAtMarket", p.purchase_id);
  return point;
}
CharacterExpressionView
body_for_span(const HouseholdNamedEnvironmentContext &x,
              const HouseholdPurchase &p, const MomentSpan &span,
              const std::optional<FixedAppliedInput> &frozen) {
  if (cmp(span.from, span.to) >= 0)
    fail("HouseholdPositiveServiceSpanRequired", p.purchase_id);
  not_future(x, span.to);
  const auto &event = admission_for_span(x, p, span);
  const auto &transition = adoption_transition(x, p, event);
  require(validate_named_household_adoption_inputs(
      x, p, event, transition.input_basis_refs));
  CharacterConditionAtHead body;
  if (frozen)
    body = take(read_character_condition_at_source(
        x.body, frozen->source_ref, frozen->source_schema, span.from));
  else
    body = take(read_character_condition_at_moment(x.body, buyer(p).actor_id,
                                                   span.from));
  if (body.head.actor_id != buyer(p).actor_id)
    fail("HouseholdConditionActorMismatch", p.purchase_id);
  auto expression =
      take(read_character_expression_for_span(x.body, body.head, span));
  if (!expression.allowed)
    fail("HouseholdExpressionUnavailable", p.purchase_id);
  return expression;
}
ActorStationaryLocationSpan
stationary(const HouseholdNamedEnvironmentContext &x,
           const HouseholdPurchase &p, const MomentSpan &span) {
  const auto &a = accepted(x, p);
  auto original = take(read_actor_stationary_location_span(
      x.common, x.shared, x.movement, x.carriers, x.registrations, x.body.state,
      x.body.catalog, x.spatial, buyer(p).actor_id, a.presence_source_ref, span,
      x.military_handoffs, {}, x.body.family_catalog));
  auto site = take(x.spatial.original_site_anchor(
      take(read_household_market_service_accepted(x.common, x.catalog, x.state,
                                                  x.private_market_custody, a))
          ->place_id));
  if (!take(same_spatial_anchor(original.actual_anchor, site)))
    fail("HouseholdServiceNotAtMarket", p.purchase_id);
  return original;
}
} // namespace
Outcome<std::vector<FixedServiceInputRef>>
capture_named_household_adoption_inputs(
    const HouseholdNamedEnvironmentContext &x, const HouseholdPurchase &p,
    const HouseholdServiceEvent2 &event) {
  return outcome<std::vector<FixedServiceInputRef>>([&] {
    now(x, event.occurred_at);
    if (event.purchase_id != p.purchase_id ||
        (event.kind != HouseholdServiceEventKind::accepted &&
         event.kind != HouseholdServiceEventKind::resumed))
      fail("HouseholdAdoptionEventRequired", event.event_id);
    header(x.common, service_source(event));
    const auto condition =
        take(read_current_character_condition(x.body, buyer(p).actor_id));
    const auto expression = take(read_character_expression_at_head(
        x.body, condition.head, event.occurred_at));
    if (!expression.allowed)
      fail("HouseholdExpressionUnavailable", p.purchase_id);
    const auto source = take(read_character_condition_head_source(
        x.body, condition.head, event.occurred_at));
    auto spatial = adoption_position(x, p, event);
    std::vector<FixedServiceInputRef> result;
    append(result, source.source, source.source_schema);
    for (const auto &s : spatial.original_sources)
      append(result, s.source_ref, s.source_schema);
    return result;
  });
}
Status validate_named_household_adoption_inputs(
    const HouseholdNamedEnvironmentContext &x, const HouseholdPurchase &p,
    const HouseholdServiceEvent2 &event,
    const std::vector<FixedServiceInputRef> &inputs) {
  return checked([&] {
    if (event.purchase_id != p.purchase_id ||
        (event.kind != HouseholdServiceEventKind::accepted &&
         event.kind != HouseholdServiceEventKind::resumed))
      fail("HouseholdAdoptionEventRequired", event.event_id);
    not_future(x, event.occurred_at);
    header(x.common, service_source(event));
    const auto &source = adopted_body(inputs);
    auto body = take(read_character_condition_at_source(
        x.body, source.source_ref, source.source_schema, event.occurred_at));
    if (body.head.actor_id != buyer(p).actor_id ||
        !take(read_character_expression_at_head(x.body, body.head,
                                                event.occurred_at))
             .allowed)
      fail("HouseholdAdoptedExpressionMismatch", event.event_id);
    auto spatial = adoption_position(x, p, event);
    for (const auto &original : spatial.original_sources) {
      Integer count = 0;
      for (const auto &input : inputs)
        if (const auto *a = std::get_if<FixedAppliedInput>(&input))
          if (same_source_ref(a->source_ref, original.source_ref)) {
            if (a->source_schema != original.source_schema)
              fail("HouseholdAdoptedSpatialSchemaMismatch",
                   original.source_ref.id);
            ++count;
          }
      if (count != 1)
        fail("HouseholdAdoptedSpatialSourceRequired", original.source_ref.id);
    }
    const NamedServiceClaimEpisode2 *episode = nullptr;
    for (const auto &e : x.state.named_service_claim_episodes)
      if (same_source_ref(e.admission_result_ref, service_source(event))) {
        if (episode || e.purchase_id != p.purchase_id)
          fail("HouseholdAdoptionClaimMismatch", event.event_id);
        episode = &e;
      }
    if (!episode)
      fail("HouseholdAdoptionClaimMissing", event.event_id);
    auto receipt = take(
        read_facility_claim_receipt(x.claims, episode->admission_receipt_ref));
    auto claim = take(read_facility_claim_episode(
        x.claims, episode->claim_id, take(x.clock.read_current_moment())));
    if (!same_source_ref(receipt.source_result_ref, service_source(event)) ||
        cmp(receipt.occurred_at, event.occurred_at) ||
        !same_source_ref(claim.original_claim.source_result_ref,
                         service_source(event)))
      fail("HouseholdAdoptionReceiptMismatch", event.event_id);
    std::vector<FixedServiceInputRef> expected;
    append(expected, service_source(event), 2);
    if (event.kind == HouseholdServiceEventKind::accepted)
      append(expected, p.accepted_terms_ref, 1);
    append(expected, source.source_ref, source.source_schema);
    for (const auto &original : spatial.original_sources)
      append(expected, original.source_ref, original.source_schema);
    for (const auto &capacity : receipt.capacity_source_refs) {
      take(x.facilities.read_verified_capacity(
          capacity, claim.original_claim.slot_ref,
          {claim.original_claim.from, claim.original_claim.until}));
      append(expected, capacity.source_ref, capacity.source_schema);
    }
    if (inputs.size() != expected.size())
      fail("HouseholdAdoptionInputCoverageMismatch", event.event_id);
    for (std::size_t n = 0; n < inputs.size(); ++n) {
      const auto *actual = std::get_if<FixedAppliedInput>(&inputs[n]);
      const auto *original = std::get_if<FixedAppliedInput>(&expected[n]);
      if (!actual || !original ||
          actual->source_schema != original->source_schema ||
          !same_source_ref(actual->source_ref, original->source_ref))
        fail("HouseholdAdoptionInputSourceMismatch", event.event_id);
    }
  });
}
Status validate_named_household_current_participation(
    const HouseholdNamedEnvironmentContext &x, const NamedHouseholdBuyer &b,
    const HouseholdPurchasePlan &plan, const WorldMoment &at,
    const StableId &owned) {
  return checked([&] {
    now(x, at);
    const auto *actor = x.common.actor(b.actor_id);
    const auto *selected =
        std::get_if<NamedHouseholdBuyer>(&plan.execution_choice);
    if (!actor || !selected || selected->actor_id != b.actor_id ||
        !actor->available || actor->action_id != owned)
      fail("HouseholdNamedParticipationMismatch", b.actor_id);
    auto current = take(read_current_character_condition(x.body, b.actor_id));
    if (!take(read_character_expression_at_head(x.body, current.head, at))
             .allowed)
      fail("HouseholdExpressionUnavailable", b.actor_id);
  });
}
Outcome<std::vector<SourceRef>> read_named_household_actual_participation(
    const HouseholdNamedEnvironmentContext &x, const HouseholdPurchase &p,
    const MomentSpan &span) {
  return outcome<std::vector<SourceRef>>([&] {
    auto expression = body_for_span(x, p, span, {});
    auto source = take(read_character_condition_head_source(
        x.body, expression.condition_head, span.from));
    auto location = stationary(x, p, span);
    std::vector<SourceRef> refs;
    append(refs, source.source);
    for (const auto &original : location.original_sources)
      append(refs, original.source_ref);
    return refs;
  });
}
Outcome<HouseholdPurchaseCoverage>
read_named_household_actual_coverage(const HouseholdNamedEnvironmentContext &x,
                                     const HouseholdPurchase &p,
                                     const MomentSpan &span) {
  return outcome<HouseholdPurchaseCoverage>([&] {
    auto expression = body_for_span(x, p, span, {});
    stationary(x, p, span);
    const auto source = take(read_character_condition_head_source(
        x.body, expression.condition_head, span.from));
    return HouseholdPurchaseCoverage{source.source, span, {}, {}};
  });
}
Status validate_named_household_actual_segment(
    const HouseholdNamedEnvironmentContext &x, const HouseholdPurchase &p,
    const HouseholdServiceSegment &segment) {
  return checked([&] {
    if (segment.purchase_id != p.purchase_id ||
        !segment.labor_allocation_refs.empty() ||
        !segment.exertion_source_refs.empty() ||
        !condition_source(segment.actual_coverage_source))
      fail("HouseholdNamedCoverageSourceMismatch", p.purchase_id);
    Integer schema = 2;
    if (segment.actual_coverage_source.kind == "initial_condition_seed")
      schema = take(read_initial_condition_seed_schema(x.body.catalog));
    body_for_span(x, p, segment.window,
                  FixedAppliedInput{segment.actual_coverage_source, schema});
    stationary(x, p, segment.window);
  });
}
Outcome<FacilityClaimCloseReason> read_named_household_condition_interruption(
    const HouseholdNamedEnvironmentContext &x, const HouseholdPurchase &p,
    const SourceRef &source, const WorldMoment &at, bool terminate) {
  return outcome<FacilityClaimCloseReason>([&] {
    not_future(x, at);
    if (source.domain != "actor_foundation" ||
        source.kind != "condition_receipt")
      fail("HouseholdActualConditionChangeRequired", source.id);
    auto changed = take(read_character_condition_source(x.body, source, 2));
    if (changed.actor_id != buyer(p).actor_id || cmp(changed.occurred_at, at))
      fail("HouseholdConditionChangeIdentityMismatch", source.id);
    auto body = take(read_character_condition_at_source(x.body, source, 2, at));
    if (!body.body.alive) {
      if (!terminate)
        fail("HouseholdDeathMustTerminate", source.id);
      return FacilityClaimCloseReason::holder_died;
    }
    if (terminate ||
        take(read_character_expression_at_head(x.body, body.head, at)).allowed)
      fail("HouseholdConditionDoesNotInterrupt", source.id);
    return FacilityClaimCloseReason::suspended;
  });
}
} // namespace hanmo::resident
