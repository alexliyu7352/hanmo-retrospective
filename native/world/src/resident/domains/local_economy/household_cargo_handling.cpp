#include "household_market_resolver.hpp"
#include "household_cargo_handling.hpp"
#include "household_mobile_custody.hpp"
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
void take(Status s) {
  if (!s.ok)
    throw Failure{s.error};
}
int cmp(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b));
}
void header(const CommonStore &c, const SourceRef &r) {
  const auto *h = c.state().facts.find(r.id);
  if (!h || h->domain != r.domain || h->kind != r.kind)
    fail("HouseholdHandlingOriginalSourceMissing", r.id);
}
ReservationRef holder(const StableId &id) {
  return {ReservationRef::Kind::domain_activity, id, "local_economy",
          "household_purchase"};
}
const HouseholdPurchaseAcceptedTerms &
accepted(const HouseholdCargoHandlingContext &x, const HouseholdPurchase &p) {
  const auto *a =
      x.household.state.accepted_terms.find(p.accepted_terms_ref.id);
  if (!a || !same_source_ref(a->source, p.accepted_terms_ref) ||
      a->source.domain != "local_economy" ||
      a->source.kind != "household_purchase_accepted" ||
      a->source.id != p.purchase_id)
    fail("HouseholdHandlingAcceptanceRequired", p.purchase_id);
  header(x.household.common, a->source);
  return *a;
}
const HouseholdPurchase &purchase(const HouseholdCargoHandlingContext &x,
                                  const CargoHandlingTerms &t) {
  const auto *p = x.household.state.purchases.find(t.business_source.id);
  if (!p || !same_source_ref(t.business_source, p->accepted_terms_ref))
    fail("HouseholdHandlingBusinessSourceRequired", t.business_source.id);
  return *p;
}
// Cross-reference the original accepted batch. S1 independently validates the
// actual rule, service history, labor and terminal coverage; this reader never
// infers full service from an elapsed amount or current inventory.
void work_source(const HouseholdCargoHandlingContext &x,
                 const CargoHandlingTerms &t, const CargoHandlingExecution &e,
                 const CargoHandlingWorkResult &w) {
  const auto *actual_t = x.handling.terms.find(t.terms_id);
  const auto *actual_e = x.handling.executions.find(e.execution_id);
  const auto *actual_w = x.handling.work_results.find(w.result_id);
  if (actual_t != &t || actual_e != &e || actual_w != &w ||
      e.terms_ref != t.terms_id || w.terms_ref != t.terms_id ||
      w.execution_ref != e.execution_id || !e.work_result_ref ||
      *e.work_result_ref != w.result_id ||
      !same_source_ref(e.business_source, t.business_source) ||
      e.batch_ordinal != t.batch_ordinal || cmp(e.started_at, t.accepted_at) ||
      cmp(w.service_started_at, e.started_at) ||
      cmp(w.service_completed_at, w.service_started_at) <= 0)
    fail("HouseholdHandlingActualWorkRequired", e.execution_id);
  header(x.household.common, {"travel", "cargo_handling_terms", t.terms_id});
  header(x.household.common,
         {"travel", "cargo_handling_execution", e.execution_id});
  header(x.household.common,
         {"travel", "cargo_handling_work_result", w.result_id});
  const auto *r = x.travel.cargo_handling_rules.find(t.handling_rule_ref);
  if (!r || r->version != t.accepted_rule_version || w.rule_ref != r->id ||
      w.rule_version != r->version || w.labor_coverage_source_refs.empty())
    fail("HouseholdHandlingRuleSourceMismatch", w.result_id);
  take(validate_cargo_handling_rule(*r));
}
void purchase_terms(const HouseholdCargoHandlingContext &x,
                    const HouseholdPurchase &p, const CargoHandlingTerms &t) {
  const auto &a = accepted(x, p);
  const auto *receipt =
      x.household.retail.state.settlements.find(p.retail_receipt_ref);
  if (!receipt || receipt->terms.purchase_id != p.purchase_id ||
      !same_source_ref(t.business_source, a.source) || t.batch_ordinal < 0 ||
      t.cargo.empty() || t.source_inventory_id == t.destination_inventory_id ||
      t.source_reservation != holder(p.purchase_id) ||
      t.matching_owners.size() != 1 ||
      !owner_equal(t.matching_owners.front(), receipt->terms.buyer) ||
      !t.destination_owner ||
      !owner_equal(*t.destination_owner, receipt->terms.buyer))
    fail("HouseholdHandlingPurchaseTermsMismatch", t.terms_id);
  if (t.operation == CargoHandlingOperation::load) {
    if (t.source_inventory_id != p.holding_inventory_ref ||
        !same_source_ref(t.source_custody_ref,
                         receipt->terms.custody_terms_ref))
      fail("HouseholdHandlingOriginalCustodyRequired", t.terms_id);
  } else if (t.operation == CargoHandlingOperation::unload) {
    if (t.destination_inventory_id != a.plan.destination_inventory ||
        !same_source_ref(t.receiver_acceptance_source_ref, a.source))
      fail("HouseholdHandlingReceiverAcceptanceRequired", t.terms_id);
  } else
    fail("UnsupportedHouseholdHandlingOperation", t.terms_id);
  if (const auto *actor = std::get_if<ActorCarrierRef>(&t.carrier)) {
    const auto *buyer = std::get_if<NamedHouseholdBuyer>(&p.executor);
    const auto *person = x.household.common.actor(actor->actor_id);
    if (!buyer || buyer->actor_id != actor->actor_id || !person ||
        (t.operation == CargoHandlingOperation::load
             ? t.destination_inventory_id
             : t.source_inventory_id) != person->inventory_id)
      fail("HouseholdHandlingNamedCarrierMismatch", t.terms_id);
    if (t.operation == CargoHandlingOperation::load &&
        !same_source_ref(t.receiver_acceptance_source_ref, a.source))
      fail("HouseholdHandlingNamedAcceptanceRequired", t.terms_id);
  } else if (const auto *carrier =
                 std::get_if<TransportUnitCarrierRef>(&t.carrier)) {
    const auto *unit = x.carriers.transport_units.find(
        carrier->transport_unit_ref.transport_unit_id);
    const auto *reg = unit ? x.registrations.registrations.find(
                                 unit->registration_source_ref.id)
                           : nullptr;
    const auto *buyer = std::get_if<CohortHouseholdBuyer>(&p.executor);
    const auto *initiator = reg ? std::get_if<HouseholdPolicyGroupInitiator>(
                                      &reg->accepted_scope.initiator)
                                : nullptr;
    const StableId &inventory = t.operation == CargoHandlingOperation::load
                                    ? t.destination_inventory_id
                                    : t.source_inventory_id;
    if (x.household.selected_household_version == 3) {
      if (!a.plan.mobile_custody_adoption || !x.household.mobile_custody ||
          a.plan.mobile_custody_adoption->actual_view.inventory_id != inventory)
        fail("HouseholdHandlingAdoptedMobileCustodyRequired", t.terms_id);
      take(validate_household_mobile_custody(
          x.household, *x.household.mobile_custody, a.plan));
    }
    if (!unit || !reg || !buyer || !initiator ||
        reg->transport_unit_id != unit->transport_unit_id ||
        initiator->purchase_plan_ref != a.plan.plan_id ||
        initiator->accepted_household_policy_ref !=
            a.plan.accepted_policy_ref.id ||
        initiator->policy_revision != a.plan.accepted_policy_revision ||
        std::find(reg->accepted_scope.accepted_selection_ids.begin(),
                  reg->accepted_scope.accepted_selection_ids.end(),
                  buyer->population_selection_ref) ==
            reg->accepted_scope.accepted_selection_ids.end() ||
        std::find(reg->accepted_scope.inventory_ids.begin(),
                  reg->accepted_scope.inventory_ids.end(),
                  inventory) == reg->accepted_scope.inventory_ids.end())
      fail("HouseholdHandlingRegisteredCarrierRequired", t.terms_id);
    header(x.household.common, unit->registration_source_ref);
    if (t.operation == CargoHandlingOperation::load &&
        !same_source_ref(t.receiver_acceptance_source_ref,
                         unit->registration_source_ref))
      fail("HouseholdHandlingCarrierAcceptanceRequired", t.terms_id);
  } else
    fail("HouseholdHandlingAcceptedCarrierRequired", t.terms_id);
}
// Only the current validated S1 completion is visible before its late Result.
// Every other source is read through the real environment, including old cargo.
class InFlightDeliveryRead final : public HouseholdPurchaseEnvironmentRead {
  const HouseholdPurchaseEnvironmentRead &original_;
  HouseholdPurchaseDeliverySource actual_;

public:
  InFlightDeliveryRead(const HouseholdPurchaseEnvironmentRead &o,
                       HouseholdPurchaseDeliverySource a)
      : original_(o), actual_(std::move(a)) {}
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
    if (p.purchase_id == actual_.purchase_id &&
        same_source_ref(r, actual_.actual_source))
      return {true, actual_, {}};
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
    return original_.validate_purchase_phase_cause(p, v, r, m);
  }
};
} // namespace
Outcome<HouseholdPurchaseDeliverySource>
read_household_cargo_handling_delivery(const HouseholdCargoHandlingContext &x,
                                       const HouseholdPurchase &p,
                                       const SourceRef &ref) {
  try {
    if (ref.domain != "travel" || ref.kind != "cargo_handling_result")
      fail("HouseholdActualHandlingResultRequired", ref.id);
    header(x.household.common, ref);
    const auto *result = x.handling.results.find(ref.id);
    const auto *e =
        result ? x.handling.executions.find(result->execution_ref) : nullptr;
    const auto *t = e ? x.handling.terms.find(e->terms_ref) : nullptr;
    const auto *w = result && result->work_result_ref
                        ? x.handling.work_results.find(*result->work_result_ref)
                        : nullptr;
    if (!result || result->result_id != ref.id || !e || !t || !w ||
        result->status != CargoHandlingResultStatus::completed ||
        e->phase != CargoHandlingPhase::completed || !e->terminal_result_ref ||
        *e->terminal_result_ref != ref.id || result->terms_ref != t->terms_id ||
        result->batch_ordinal != t->batch_ordinal ||
        !same_source_ref(result->business_source, t->business_source) ||
        cmp(result->occurred_at, w->service_completed_at) < 0)
      fail("HouseholdHandlingNotDelivered", ref.id);
    work_source(x, *t, *e, *w);
    purchase_terms(x, p, *t);
    const HouseholdPurchaseHandover *found = nullptr;
    for (const auto &row : x.household.state.handovers.rows())
      if (same_source_ref(row.second.actual_delivery_source, ref)) {
        if (found || row.second.purchase_id != p.purchase_id)
          fail("HouseholdHandlingDuplicateBusinessResult", ref.id);
        found = &row.second;
      }
    if (!found || found->from_inventory_id != t->source_inventory_id ||
        found->to_inventory_id != t->destination_inventory_id ||
        cmp(found->occurred_at, result->occurred_at))
      fail("HouseholdHandlingBusinessResultMissing", ref.id);
    const auto &a = accepted(x, p);
    SourceRef original{"local_economy",
                       found->to_inventory_id == a.plan.destination_inventory
                           ? "household_purchase_delivered"
                           : "household_purchase_cargo_handover",
                       found->id};
    header(x.household.common, original);
    for (const auto *refs :
         {&result->actual_transfer_source_refs,
          &result->actual_resource_effect_refs, &result->actual_lot_flow_refs,
          &result->original_business_result_refs})
      if (refs->size() != 1 || !same_source_ref(refs->front(), original))
        fail("HouseholdHandlingBusinessBacklinkMismatch", ref.id);
    return {true,
            {ref, result->occurred_at, p.purchase_id, t->source_inventory_id,
             t->destination_inventory_id, t->cargo, false},
            {}};
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
Outcome<CargoHandlingBusinessTransfer>
complete_household_cargo_handling_transfer_in_candidate(
    CommonStore &c, HouseholdPurchaseState &state,
    const HouseholdCargoHandlingContext &x, const CargoHandlingTerms &t,
    const CargoHandlingExecution &e, const CargoHandlingWorkResult &w,
    const SourceRef &pending, const WorldMoment &at) {
  try {
    if (&c != &x.household.common || &state != &x.household.state ||
        cmp(at, take(x.clock.read_current_moment())) ||
        cmp(at, x.household.now) || pending.domain != "travel" ||
        pending.kind != "cargo_handling_result" || pending.id.empty() ||
        c.state().facts.find(pending.id) || x.handling.results.find(pending.id))
      fail("HouseholdHandlingCandidateMismatch", pending.id);
    work_source(x, t, e, w);
    const auto &p = purchase(x, t);
    purchase_terms(x, p, t);
    if (e.phase != CargoHandlingPhase::ready_to_transfer ||
        e.terminal_result_ref || cmp(at, w.service_completed_at) < 0)
      fail("HouseholdHandlingTransferBeforeWork", e.execution_id);
    const auto &a = accepted(x, p);
    const auto *market = take(read_household_market_service_accepted(
        x.household.common, x.household.catalog, x.household.state,
        x.household.private_market_custody, a));
    if (!market || market->handling_rule_ref.id != t.handling_rule_ref)
      fail("HouseholdHandlingOriginalRuleRequired", t.handling_rule_ref);
    auto reach = take(read_storage_reachability(
        c, x.carriers, x.movement, x.spatial, t.source_inventory_id,
        t.destination_inventory_id, market->handling_rule_ref));
    const auto *same = std::get_if<SameSiteReachability>(&reach);
    if (!same)
      fail("HouseholdHandlingActualSameSiteRequired", t.terms_id);
    auto manifest = take(read_transport_manifest(c, x.carriers, x.movement,
                                                 x.spatial, t.carrier));
    if (!take(same_spatial_anchor(same->resolved_site, manifest.actual_anchor)))
      fail("HouseholdHandlingCarrierAtAnotherSite", t.terms_id);
    auto capacity =
        take(quote_cargo_capacity(c, x.carriers, x.movement, x.spatial,
                                  {t.carrier, t.cargo,
                                   t.operation == CargoHandlingOperation::load
                                       ? CargoCapacityOperation::load
                                       : CargoCapacityOperation::unload,
                                   manifest.manifest_revision}));
    if (t.operation == CargoHandlingOperation::load &&
        a.plan.mobile_custody_adoption) {
      const auto &mobile = *a.plan.mobile_custody_adoption;
      const auto maximum = take(read_mobile_container_maximum_weight(
          x.household.shared, x.travel, mobile.actual_view.capacity_source));
      const auto used = take(c.inventory_weight(t.destination_inventory_id));
      if (capacity.selected_weight < 0 || used < 0 || used > maximum ||
          capacity.selected_weight > maximum - used)
        fail("HouseholdMobileContainerOverCapacity",
             t.destination_inventory_id);
    }
    HouseholdPurchaseDeliverySource actual{pending,
                                           at,
                                           p.purchase_id,
                                           t.source_inventory_id,
                                           t.destination_inventory_id,
                                           t.cargo,
                                           false};
    InFlightDeliveryRead in_flight{x.household.environment, actual};
    const auto &h = x.household;
    HouseholdPurchaseContext bound{h.common,
                                   h.shared,
                                   h.economy,
                                   h.population,
                                   h.catalog,
                                   h.state,
                                   h.controls,
                                   in_flight,
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
    auto result = take(apply_household_purchase_delivery_in_candidate(
        c, state, bound, p.purchase_id, pending));
    const auto *handover = state.handovers.find(result);
    if (!handover)
      fail("HouseholdHandlingTransferResultMissing", result);
    SourceRef original{"local_economy",
                       handover->to_inventory_id ==
                               accepted(x, p).plan.destination_inventory
                           ? "household_purchase_delivered"
                           : "household_purchase_cargo_handover",
                       result};
    return {true, {{original}, {original}, {original}, {}, {original}}, {}};
  } catch (const Failure &error) {
    return {false, {}, error.error};
  }
}
Status validate_household_cargo_handling_result(
    const HouseholdCargoHandlingContext &x, const CargoHandlingTerms &t,
    const CargoHandlingExecution &e, const CargoHandlingResult &r) {
  try {
    const auto &p = purchase(x, t);
    purchase_terms(x, p, t);
    if (r.status != CargoHandlingResultStatus::completed ||
        r.execution_ref != e.execution_id || r.terms_ref != t.terms_id ||
        r.batch_ordinal != t.batch_ordinal ||
        !same_source_ref(r.business_source, t.business_source) ||
        !r.work_result_ref || !e.work_result_ref ||
        *r.work_result_ref != *e.work_result_ref || r.reason_source_ref)
      fail("HouseholdHandlingCompletedResultRequired", r.result_id);
    const auto *w = x.handling.work_results.find(*r.work_result_ref);
    if (!w)
      fail("HouseholdHandlingActualWorkRequired", r.result_id);
    work_source(x, t, e, *w);
    if (cmp(r.occurred_at, w->service_completed_at) < 0 ||
        cmp(r.occurred_at, take(x.clock.read_current_moment())) > 0)
      fail("HouseholdHandlingResultTimeMismatch", r.result_id);
    SourceRef source{"travel", "cargo_handling_result", r.result_id};
    const HouseholdPurchaseHandover *h = nullptr;
    for (const auto &row : x.household.state.handovers.rows())
      if (same_source_ref(row.second.actual_delivery_source, source)) {
        if (h || row.second.purchase_id != p.purchase_id)
          fail("HouseholdHandlingDuplicateBusinessResult", r.result_id);
        h = &row.second;
      }
    if (!h || h->from_inventory_id != t.source_inventory_id ||
        h->to_inventory_id != t.destination_inventory_id ||
        cmp(h->occurred_at, r.occurred_at) ||
        h->actual_lots.size() != t.cargo.size())
      fail("HouseholdHandlingBusinessResultMissing", r.result_id);
    for (const auto &flow : h->actual_lots)
      if (std::count_if(t.cargo.begin(), t.cargo.end(), [&](const auto &pick) {
            return pick.inventory_id == h->from_inventory_id &&
                   pick.lot_id == flow.original_lot_id &&
                   pick.quantity == flow.quantity;
          }) != 1)
        fail("HouseholdHandlingActualLotFlowMismatch", r.result_id);
    SourceRef original{"local_economy",
                       h->to_inventory_id ==
                               accepted(x, p).plan.destination_inventory
                           ? "household_purchase_delivered"
                           : "household_purchase_cargo_handover",
                       h->id};
    header(x.household.common, original);
    for (const auto *refs :
         {&r.actual_transfer_source_refs, &r.actual_resource_effect_refs,
          &r.actual_lot_flow_refs, &r.original_business_result_refs})
      if (refs->size() != 1 || !same_source_ref(refs->front(), original))
        fail("HouseholdHandlingBusinessBacklinkMismatch", r.result_id);
    return common_success();
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
} // namespace hanmo::resident
