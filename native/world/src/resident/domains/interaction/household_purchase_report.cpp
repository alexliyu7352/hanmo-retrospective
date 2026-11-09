// 计划专属市场告知：同候选写真实消息源，冷热均回验原控制权、知情与接触。
#include "household_purchase_report.hpp"
#include "interaction_model.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/checkpoint/projection_writer.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include "resident/domains/family/household_purchase_disclosure_history.hpp"
#include "resident/domains/local_economy/household_market_resolver.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *c, const StableId &id = {}) {
  throw Failure{common_failure(c, id).error};
}
template <class T> T take(Outcome<T> v) {
  if (!v.ok)
    throw Failure{std::move(v.error)};
  return std::move(v.value);
}
void take(Status v) {
  if (!v.ok)
    throw Failure{std::move(v.error)};
}
template <class T, class F> Outcome<T> guard(F f) {
  try {
    return {true, f(), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
bool same(const SourceRef &a, const SourceRef &b) {
  return a.domain == b.domain && a.kind == b.kind && a.id == b.id;
}
int cmp(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b));
}
// 临时引用也按原普通字符串所有权先预留，生命期覆盖整次原源调用。
struct OwnedSource {
  CheckpointMemoryCharge memory;
  SourceRef value;
  static CheckpointMemoryCharge reserve(const StableId &id) {


    return CheckpointMemoryCharge(0);
  }
  explicit OwnedSource(const StableId &id)
      : memory(reserve(id)),
        value{"interaction", household_purchase_report_source_kind, id} {}
  operator const SourceRef &() const { return value; }
};
OwnedSource source(const StableId &id) { return OwnedSource{id}; }
struct OwnedMomentSource {
  CheckpointMemoryCharge memory;
  MomentOwnerSource value;
  static CheckpointMemoryCharge
  reserve(const StableId &domain, const StableId &kind, const StableId &id) {


    return CheckpointMemoryCharge(0);
  }
  OwnedMomentSource(const StableId &domain, const StableId &kind,
                    const StableId &id, Integer schema)
      : memory(reserve(domain, kind, id)), value{{domain, kind, id}, schema} {}
};
const FaceToFaceContactSources &state(const HouseholdPurchaseReportContext &x) {
  if (!x.interaction.face_to_face_contacts ||
      x.interaction.face_to_face_contacts->version != 3)
    fail("HouseholdPurchaseDisclosure3Required");
  if (&x.common != &x.purchase.common || &x.common != &x.population.common ||
      &x.common != &x.actors.common ||
      &x.purchase.shared != &x.actors.common_catalog ||
      &x.actors.spatial != &x.population.spatial ||
      &x.common != &x.disclosure.common ||
      &x.common != &x.controller_knowledge.common ||
      &x.disclosure.social != &x.controller_knowledge.social ||
      &x.disclosure.results != &x.controller_knowledge.results ||
      &x.purchase.catalog != &x.population.catalog ||
      &x.purchase.state != &x.population.state ||
      &x.purchase.economy != &x.population.economy ||
      &x.purchase.population != &x.population.population ||
      &x.actors.movement != &x.population.movement ||
      &x.actors.carriers != &x.population.carriers ||
      &x.actors.registrations != &x.population.registrations)
    fail("HouseholdDisclosureWorldMismatch");
  if (!x.controller_history ||
      &x.controller_history->condition.common != &x.common ||
      &x.controller_history->condition.state != &x.actors.foundation ||
      &x.controller_history->condition.catalog != &x.actors.actors ||
      &x.controller_history->interaction != &x.interaction ||
      &x.controller_history->shared != &x.purchase.shared ||
      &x.controller_history->controls != &x.purchase.controls)
    fail("HouseholdDisclosureControllerHistoryWorldMismatch");
  const auto now = take(x.disclosure.clock.read_current_moment());
  if (cmp(now, take(x.controller_history->clock.read_current_moment())) ||
      cmp(now, x.controller_history->condition.actual_now))
    fail("HouseholdDisclosureControllerHistoryClockMismatch");
  if (cmp(now, x.purchase.now) ||
      cmp(now, take(x.population.clock.read_current_moment())) ||
      cmp(now, take(x.population.spatial.current_moment())))
    fail("HouseholdDisclosureClockMismatch");
  return *x.interaction.face_to_face_contacts;
}
bool authority(const HouseholdProvisionControlProof &a,
               const HouseholdProvisionControlProof &b) {
  return a.actor_id == b.actor_id &&
         a.household_group_id == b.household_group_id &&
         a.fact_id == b.fact_id && a.source_role_id == b.source_role_id &&
         a.operation == b.operation && a.source_revision == b.source_revision &&
         a.effective_from_tick == b.effective_from_tick &&
         a.through_tick == b.through_tick;
}
struct Plan {
  const HouseholdPurchasePlan *plan;
  const HouseholdPurchasePolicy *policy;
  const CohortHouseholdBuyer *buyer;
  const PopulationSelection *selection;
};
Plan plan(const HouseholdPurchaseReportContext &x, const SourceRef &r) {
  if (r.domain != "local_economy" || r.kind != "household_purchase_planned")
    fail("HouseholdDisclosurePlanSourceRequired", r.id);
  take(read_actual_common_fact_position(x.common, r));
  const auto *p = x.purchase.state.plans.find(r.id);
  if (!p || p->plan_id != r.id)
    fail("HouseholdDisclosurePlanMissing", r.id);
  const auto *policy =
      x.purchase.state.policies.find(p->accepted_policy_ref.id);
  const auto *b = std::get_if<CohortHouseholdBuyer>(&p->execution_choice);
  if (!policy || policy->id != p->accepted_policy_ref.id ||
      p->accepted_policy_ref.domain != "local_economy" ||
      p->accepted_policy_ref.kind != "household_purchase_policy_accepted" ||
      !authority(p->controller_authority_ref,
                 policy->controller_authority_ref) ||
      policy->controller_actor_id !=
          policy->controller_authority_ref.actor_id ||
      !b || b->workers != 1 || !policy->allow_cohort_buyer ||
      !same(policy->acceptance_source_ref, p->accepted_policy_ref) ||
      policy->revision != p->accepted_policy_revision ||
      policy->household_id != p->household_id ||
      !same(b->accepted_household_policy_ref, p->accepted_policy_ref) ||
      std::find(policy->allowed_market_refs.begin(),
                policy->allowed_market_refs.end(),
                p->known_market_ref) == policy->allowed_market_refs.end())
    fail("HouseholdDisclosureAcceptedPlanMismatch", r.id);
  if (take(
          read_actual_common_fact_position(x.common, p->accepted_policy_ref)) >=
      take(read_actual_common_fact_position(x.common, r)))
    fail("HouseholdDisclosurePolicyOrderMismatch", r.id);
  take(x.purchase.controls.validate_accepted(policy->controller_authority_ref,
                                             policy->accepted_tick));
  auto selref = take(population_selection_source_ref(
      x.purchase.economy, x.purchase.population, b->population_selection_ref));
  const auto *selection = take(read_population_selection_source(
      x.purchase.economy, x.purchase.population, selref, 1));
  if (take(read_actual_common_fact_position(x.common, selref)) <=
      take(read_actual_common_fact_position(x.common, r)))
    fail("HouseholdDisclosureSelectionOrderMismatch", r.id);
  if (!selection || selection->id != b->population_selection_ref ||
      selection->source.cohort_id != b->cohort_id ||
      selection->partition_policy !=
          PopulationPartitionPolicy::labor_eligible_only ||
      !same(selection->release_gate, selection->owner_business_ref) ||
      !same(selection->owner_business_ref, r) ||
      selection->purpose != PopulationSelectionPurpose::household_purchase ||
      selection->selected_persons != 1 ||
      selection->selected_labor_eligible != 1)
    fail("HouseholdDisclosureBuyerSelectionMismatch", r.id);


  auto labor_memory = CheckpointMemoryCharge(0);
  const auto *labor =
      x.purchase.economy.labor_allocations.find(b->labor_allocation_id);
  if (!labor || labor->id != b->labor_allocation_id ||
      labor->cohort_id != b->cohort_id ||
      labor->activity_or_production_id != p->plan_id ||
      labor->commitment_id != p->plan_id ||
      labor->from_tick != selection->created_tick ||
      take(read_actual_common_fact_position(
          x.common, SourceRef{"local_economy", "household_buyer_labor_reserved",
                              labor->id})) <=
          take(read_actual_common_fact_position(x.common, selref)))
    fail("HouseholdDisclosureOriginalBuyerLaborRequired", r.id);
  // 原人口劳动重放以唯一已接受 plan + 一人 selection 确定出生量。
  // 今日 allocation.workers 可被真实运输/损失改写，不倒验历史告知。
  for (const auto &row : x.purchase.state.plans.rows())
    if (const auto *other =
            std::get_if<CohortHouseholdBuyer>(&row.second.execution_choice);
        other && other->labor_allocation_id == b->labor_allocation_id &&
        &row.second != p)
      fail("HouseholdDisclosureLaborOriginAmbiguous", b->labor_allocation_id);
  take(read_population_revision_source(
      x.purchase.economy, x.purchase.population, selection->source));
  return {p, policy, b, selection};
}

const Report &report(const HouseholdPurchaseReportContext &x, const Plan &p,
                     const StableId &id) {
  const auto *d =
      x.disclosure.social.dynamic ? &*x.disclosure.social.dynamic : nullptr;
  const auto *r = d ? d->reports.find(id) : nullptr;
  if (!r || !r->observation.supply)
    fail("HouseholdDisclosureSupplyReportRequired", id);
  take(validate_exact_report_source(x.common, *r, x.disclosure.results,
                                    x.disclosure.result_stamp));
  const auto &o = *r->observation.supply;
  const auto *market_place = take(read_household_original_market_place(
      x.common, x.purchase.catalog, x.purchase.state,
      x.purchase.private_market_custody, p.plan->known_market_ref));
  const auto *place =
      market_place ? x.purchase.shared.places.find(*market_place) : nullptr;
  if (!place || place->settlement_id != o.site_id ||
      std::none_of(p.plan->allowed_items.begin(), p.plan->allowed_items.end(),
                   [&](const auto &i) { return i.item_id == o.item_id; }))
    fail("HouseholdDisclosureReportOutsidePlan", id);
  // The eventual seller/inventory match remains the original market quote's
  // exact input. No current stock value or newly observed price is copied here.
  return *r;
}
ActorReportKnowledgeReceiptView known(const HouseholdPurchaseReportContext &x,
                                      const StableId &actor, const StableId &id,
                                      const WorldMoment &at) {
  auto v = take(
      read_actor_report_knowledge_receipt(x.controller_knowledge, actor, id));
  if (!v || !v->acquisition || cmp(v->knowledge.learned_at, at) > 0)
    fail("HouseholdDisclosureControllerNotKnown", id);
  return *v;
}
OwnedMomentSource observation(const HouseholdPurchaseReportContext &x,
                              const Report &r) {
  const auto *f = take(x.disclosure.results.fact_v2_borrowed(
      r.fact_id, x.disclosure.result_stamp));
  if (!f)
    fail("HouseholdDisclosureObservationMissing", r.id);
  if (const auto *t = std::get_if<OwnerFactMoment>(&f->time_basis))
    return {t->source.domain, t->source.kind, t->source.id, t->source_schema};
  const auto *l = std::get_if<LegacyFactBoundary>(&f->time_basis);
  const auto *h = x.common.state().facts.find(r.fact_id);
  if (!l || !h || l->original_source_schema < 1)
    fail("HouseholdDisclosureObservationSchemaRequired", r.id);
  return {h->domain, h->kind, h->id, l->original_source_schema};
}
struct OwnedPermission {
  CheckpointMemoryCharge memory;
  ReportDisclosurePermission value;
  OwnedPermission(ReportDisclosureBasisKind kind, const MomentOwnerSource &src)
      : memory(OwnedMomentSource::reserve(
            src.source_ref.domain, src.source_ref.kind, src.source_ref.id)),
        value{kind, src} {}
};
OwnedPermission permission(const HouseholdPurchaseReportContext &x,
                           const Plan &p, const Report &r,
                           const StableId &actor, const WorldMoment &at,
                           const ReadStamp &stamp) {
  if (stamp.mode() != ReadMode::phase_current &&
      stamp.mode() != ReadMode::committed_current)
    fail("CurrentReportDisclosureStampRequired", r.id);
  take(x.disclosure.authority.validate(stamp, x.common.state().identity,
                                       stamp.mode(), x.common,
                                       x.disclosure.versions));
  if (r.visibility_scope == ReportVisibility::local_public &&
      r.observation.publicly_visible)
    return {ReportDisclosureBasisKind::public_report, observation(x, r).value};


  auto subject_memory = CheckpointMemoryCharge(0);
  const SubjectRef sender{SubjectKind::actor, actor},
      receiver{SubjectKind::cohort, p.buyer->cohort_id};
  for (const auto &v : r.observation.participants)
    if (same_social_subject(v.subject, receiver))
      return {ReportDisclosureBasisKind::original_participant,
              observation(x, r).value};
  if (!x.disclosure.originals)
    fail("HouseholdDisclosureOriginalPermissionRequired", r.id);
  auto v = take(x.disclosure.originals->read_report_disclosure_permission(
      r, sender, receiver, at, stamp));
  if (!v || v->source_schema < 1 || v->source_ref.id.empty())
    fail("HouseholdDisclosureNotPermitted", r.id);
  return {ReportDisclosureBasisKind::original_business_permission, *v};
}
void historical_permission(const HouseholdPurchaseReportContext &x,
                           const Plan &p, const Report &r,
                           const HouseholdPurchaseReportDisclosure &d) {
  const auto original = observation(x, r);
  const auto &perm = d.permission;


  auto subject_memory = CheckpointMemoryCharge(0);
  const SubjectRef sender{SubjectKind::actor, d.controller_actor_id},
      receiver{SubjectKind::cohort, d.cohort_id};
  if (perm.kind == ReportDisclosureBasisKind::public_report) {
    if (r.visibility_scope != ReportVisibility::local_public ||
        !r.observation.publicly_visible ||
        !same(perm.original_source.source_ref, original.value.source_ref) ||
        perm.original_source.source_schema != original.value.source_schema)
      fail("HouseholdDisclosurePublicPermissionMismatch", d.id);
  } else if (perm.kind == ReportDisclosureBasisKind::original_participant) {
    if (!same(perm.original_source.source_ref, original.value.source_ref) ||
        perm.original_source.source_schema != original.value.source_schema ||
        std::none_of(r.observation.participants.begin(),
                     r.observation.participants.end(), [&](const auto &v) {
                       return same_social_subject(v.subject, receiver);
                     }))
      fail("HouseholdDisclosureParticipantPermissionMismatch", d.id);
  } else if (perm.kind ==
             ReportDisclosureBasisKind::original_business_permission) {
    if (!x.disclosure.originals)
      fail("HouseholdDisclosureOriginalPermissionRequired", d.id);
    take(x.disclosure.originals->validate_report_disclosure_permission(
        r, sender, receiver, perm.original_source, d.disclosed_at,
        source(d.id)));
  } else
    fail("HouseholdDisclosureUnknownPermission", d.id);
}
// Original S1 finite source-head replay. This captures no second position.
void contact(const HouseholdPurchaseReportContext &x,
             const HouseholdPurchaseReportDisclosure &d,
             bool current_prefix = false,
             HouseholdPurchaseReportProvenanceSink *pins = nullptr) {
  const auto &a = x.actors;
  auto actor = take(read_actor_spatial_location_at_actual_fact_prefix(
      x.common, a.common_catalog, a.movement, a.carriers, a.registrations,
      a.foundation, a.actors, a.spatial, d.controller_actor_id, d.disclosed_at,
      d.actual_contact_prefix, x.military_handoffs, a.budget, a.family_catalog,
      a.births, a.military_actions, x.custody_releases, x.field_exits));
  const auto *anchor = take(require_actor_ordinary_movement_anchor(actor));
  if (pins)
    for (const auto &original : actor.actual_location_sources)
      take(pins->source(HouseholdPurchaseReportProvenanceRole::actor_location,
                        original.source, original.schema));
  const auto population = take(read_population_revision_source(
      x.population.economy, x.population.population, d.actual_population));
  if (population.headcount < 1 || population.labor_capacity < 1 ||
      !same(population.location_binding_ref, d.population_location_source))
    fail("HouseholdDisclosurePopulationSourceMismatch", d.id);
  const auto cut =
      take(read_actual_common_fact_prefix(x.common, d.actual_contact_prefix));
  for (const auto &e : x.population.population.revision_sources)
    if (e.cohort_id == d.cohort_id) {
      if (!x.population.population_history)
        fail("HouseholdDisclosurePopulationHistoryRequired", d.id);
      const auto &history = *x.population.population_history;
      if (&history.common != &x.common ||
          &history.economy != &x.population.economy ||
          &history.population != &x.population.population ||
          &history.transport != &x.population.registrations ||
          cmp(take(history.clock.read_current_moment()),
              take(x.disclosure.clock.read_current_moment())))
        fail("HouseholdDisclosurePopulationHistoryOwnerMismatch", d.id);
      auto t = take(read_population_original_change_time(
          *x.population.population_history, e));
      const auto index = take(read_actual_common_fact_position(
          x.common, t.actual_owner_source.source_ref));
      if ((index < cut) !=
          (e.revision_after <= d.actual_population.population_revision))
        fail("HouseholdDisclosurePopulationCutMismatch", d.id);
      if (index < cut && cmp(t.occurred_at, d.disclosed_at) > 0)
        fail("HouseholdDisclosureFuturePopulation", d.id);
      if (pins)
        take(pins->population_revision(x.population.population, e,
                                       t.actual_owner_source, index < cut));
    }
  MovementAnchor population_anchor;
  if (d.population_location_source.domain == "local_economy" &&
      d.population_location_source.kind == "initial_cohort_site") {
    const auto *seed = x.population.economy_catalog.cohorts.find(d.cohort_id);
    // 数量/劳力原修订可以保留原 seed 位置；上方真实 revision/prefix
    // 已独立反扫，不能把位置未变误当 population_revision 必为 1。
    if (!seed || d.population_location_source.id != d.cohort_id)
      fail("HouseholdDisclosureInitialPopulationSourceRequired", d.id);
    population_anchor =
        take(x.population.spatial.original_site_anchor(seed->site_id));
  } else if (current_prefix) {
    population_anchor = take(read_population_location_at_actual_fact_prefix(
        x.common, x.population.carriers, x.population.movement,
        x.population.registrations, x.population.population,
        d.actual_population, d.population_location_source, d.disclosed_at,
        d.actual_contact_prefix, x.population.clock));
  } else {
    auto location = take(read_population_location_at_source(
        x.common, x.population.carriers, x.population.movement,
        x.population.registrations, x.population.population,
        d.actual_population, d.population_location_source, d.disclosed_at,
        source(d.id)));
    if (pins)
      for (const auto &original : location.original_sources)
        take(pins->source(
            HouseholdPurchaseReportProvenanceRole::population_location,
            original.source_ref, original.source_schema));
    population_anchor = std::move(location.actual_anchor);
  }
  if (!take(same_spatial_anchor(*anchor, population_anchor)))
    fail("HouseholdDisclosureActualContactRequired", d.id);
}
} // namespace
Outcome<const HouseholdPurchaseReportDisclosure *>
read_original_household_purchase_report_source(const CommonStore &c,
                                               const InteractionState &i,
                                               const SourceRef &r,
                                               Integer schema) {
  return guard<const HouseholdPurchaseReportDisclosure *>([&] {
    if (schema != 1 || r.domain != "interaction" ||
        r.kind != household_purchase_report_source_kind ||
        !i.face_to_face_contacts || i.face_to_face_contacts->version != 3)
      fail("HouseholdPurchaseDisclosure3Required", r.id);
    const auto *d =
        i.face_to_face_contacts->purchase_plan_disclosures.find(r.id);
    if (!d || d->id != r.id || d->controller_actor_id.empty() ||
        d->cohort_id.empty() || d->report_id.empty() ||
        d->actual_population.cohort_id != d->cohort_id)
      fail("HouseholdDisclosureOriginalMissing", r.id);
    const auto index = take(read_actual_common_fact_position(c, r));
    if (take(read_actual_common_fact_prefix(c, d->actual_contact_prefix)) !=
        index)
      fail("HouseholdDisclosureActualPrefixMismatch", r.id);
    for (const auto *ref :
         {&d->purchase_plan_ref, &d->accepted_policy_ref,
          &d->population_selection_ref, &d->original_observation.source_ref})
      if (take(read_actual_common_fact_position(c, *ref)) >= index)
        fail("HouseholdDisclosureBeforeAcceptedPlan", r.id);
    if (cmp(d->controller_learned_at, d->disclosed_at) > 0)
      fail("HouseholdDisclosureBeforeKnowledge", r.id);
    return d;
  });
}
Outcome<HouseholdPurchaseReportView>
read_household_purchase_report_source(const HouseholdPurchaseReportContext &x,
                                      const SourceRef &r, Integer schema) {
  return guard<HouseholdPurchaseReportView>([&] {
    state(x);
    const auto *d = take(read_original_household_purchase_report_source(
        x.common, x.interaction, r, schema));
    auto p = plan(x, d->purchase_plan_ref);
    if (p.policy->controller_actor_id != d->controller_actor_id ||
        p.buyer->cohort_id != d->cohort_id ||
        !same(p.plan->accepted_policy_ref, d->accepted_policy_ref) ||
        p.plan->accepted_policy_revision != d->accepted_policy_revision ||
        !same(take(population_selection_source_ref(
                  x.purchase.economy, x.purchase.population,
                  p.buyer->population_selection_ref)),
              d->population_selection_ref) ||
        d->controller_authority.actor_id != d->controller_actor_id ||
        d->controller_authority.household_group_id != p.plan->household_id ||
        d->controller_authority.operation != household_purchase_operation)
      fail("HouseholdDisclosureFrozenBuyerMismatch", r.id);
    const auto controller =
        take(read_household_purchase_disclosure_controller_before_fact(
            *x.controller_history, r, schema));
    if (!controller.authored_role || !controller.role_enabled ||
        !controller.present || !controller.alive || !controller.available ||
        controller.authored_role->id != d->controller_authority.source_role_id)
      fail("HouseholdDisclosureHistoricalControllerUnavailable", r.id);
    if (d->disclosed_at.half_day_tick < p.policy->valid_from_tick ||
        d->disclosed_at.half_day_tick >= p.policy->valid_until_tick ||
        d->disclosed_at.half_day_tick > p.plan->latest_start_tick)
      fail("HouseholdDisclosureOutsideAcceptedPeriod", r.id);
    if (p.policy->closed_source_ref &&
        take(read_actual_common_fact_position(x.common,
                                              *p.policy->closed_source_ref)) <
            take(read_actual_common_fact_position(x.common, r)))
      fail("HouseholdDisclosureAfterPolicyClosed", r.id);
    if (cmp(d->disclosed_at, take(x.disclosure.clock.read_current_moment())) >
        0)
      fail("HouseholdDisclosureFromFuture", r.id);
    const auto &rep = report(x, p, d->report_id);
    const auto observed = observation(x, rep);
    if (d->original_observation.source_schema != observed.value.source_schema ||
        !same(d->original_observation.source_ref, observed.value.source_ref))
      fail("HouseholdDisclosureObservationSourceMismatch", r.id);
    auto k = known(x, d->controller_actor_id, d->report_id, d->disclosed_at);
    take(validate_actor_report_knowledge_before_source(x.controller_knowledge,
                                                       k, r, d->disclosed_at));
    if (!same(k.acquisition->actual_source_ref,
              d->controller_knowledge_source) ||
        cmp(k.knowledge.learned_at, d->controller_learned_at))
      fail("HouseholdDisclosureKnowledgeSourceMismatch", r.id);


    auto labor_memory = CheckpointMemoryCharge(0);
    const SourceRef labor_source{"local_economy",
                                 "household_buyer_labor_reserved",
                                 p.buyer->labor_allocation_id};
    if (take(read_actual_common_fact_position(x.common, labor_source)) >=
        take(read_actual_common_fact_position(x.common, r)))
      fail("HouseholdDisclosureBeforeBuyerLabor", r.id);
    historical_permission(x, p, rep, *d);
    contact(x, *d);
    return HouseholdPurchaseReportView{d, p.plan, &rep,
                                       &*rep.observation.supply};
  });
}
// 先完整原源校验，随后沿同一不可变借图枚举；不以 pin 枚举替代语义。
Status visit_household_purchase_report_provenance(
    const HouseholdPurchaseReportContext &x, const SourceRef &r, Integer schema,
    HouseholdPurchaseReportProvenanceSink &sink) {
  auto result = guard<Done>([&] {
    const auto full = take(read_household_purchase_report_source(x, r, schema));
    const auto &d = *full.delivery;
    const auto accepted = plan(x, d.purchase_plan_ref);
    const auto *labor = x.purchase.economy.labor_allocations.find(
        accepted.buyer->labor_allocation_id);
    if (!labor)
      fail("HouseholdDisclosureOriginalBuyerLaborRequired", r.id);
    const auto knowledge =
        known(x, d.controller_actor_id, d.report_id, d.disclosed_at);
    take(sink.household(x.purchase, *accepted.plan, *accepted.policy,
                        *accepted.selection, *labor));
    take(sink.social(x.disclosure.social, *full.report, knowledge));
    const auto selected_population = take(read_population_revision_source(
        x.population.economy, x.population.population,
        accepted.selection->source));
    const auto actual_population = take(read_population_revision_source(
        x.population.economy, x.population.population, d.actual_population));
    take(sink.population(x.population, accepted.selection->source,
                         selected_population));
    take(sink.population(x.population, d.actual_population, actual_population));
    take(sink.source(HouseholdPurchaseReportProvenanceRole::actual_disclosure,
                     r, schema));
    take(
        sink.source(HouseholdPurchaseReportProvenanceRole::original_observation,
                    d.original_observation.source_ref,
                    d.original_observation.source_schema));
    take(sink.source(HouseholdPurchaseReportProvenanceRole::original_permission,
                     d.permission.original_source.source_ref,
                     d.permission.original_source.source_schema));
    contact(x, d, false, &sink);
    return Done{};
  });
  return result.ok ? common_success()
                   : Status{false, {}, std::move(result.error)};
}
namespace {
// 临时候选承载普通拥有字符串的原预算；没有 ID/事实，不能发布或跨候选保存。
struct PreparedDisclosure {
  CheckpointMemoryCharge memory;
  HouseholdPurchaseReportDisclosure row;
  const HouseholdPurchaseReportDisclosure *replay = nullptr;
};
PreparedDisclosure
prepare_disclosure(const HouseholdPurchaseReportContext &x,
                   const StableId &actor,
                   const TellHouseholdPurchaseMarketReport &q) {
  const auto &selected = state(x);
  auto p = plan(x, q.purchase_plan_ref);
  if (actor.empty() || p.policy->controller_actor_id != actor)
    fail("HouseholdDisclosureControllerRequired", actor);
  // A real repeated delivery retains its original M and receiver; never
  // refreshes a stale report, alters the plan or manufactures a general
  // knowledge receipt.
  for (const auto &row : selected.purchase_plan_disclosures.rows())
    if (same(row.second.purchase_plan_ref, q.purchase_plan_ref) &&
        row.second.report_id == q.report_id) {
      auto original =
          take(read_household_purchase_report_source(x, source(row.first), 1));
      if (original.delivery->controller_actor_id != actor)
        fail("HouseholdDisclosureReplayControllerMismatch", actor);
      return PreparedDisclosure{{}, {}, original.delivery};
    }
  const auto now = take(x.disclosure.clock.read_current_moment());
  // 新告知仍保原当前劳动可用门；重复告知已先走真实旧源，不借此刷新。
  const auto *current_labor =
      x.purchase.economy.labor_allocations.find(p.buyer->labor_allocation_id);
  if (!current_labor || current_labor->workers != 1)
    fail("HouseholdDisclosureCurrentBuyerLaborRequired", p.plan->plan_id);
  const auto *speaker = x.common.actor(actor);
  if (!speaker || !speaker->alive || !speaker->available ||
      !speaker->action_id.empty())
    fail("HouseholdDisclosureControllerUnavailable", actor);
  if (p.policy->closed_source_ref ||
      now.half_day_tick < p.policy->valid_from_tick ||
      now.half_day_tick >= p.policy->valid_until_tick ||
      now.half_day_tick > p.plan->latest_start_tick ||
      p.selection->state == PopulationSelectionPhase::released)
    fail("HouseholdDisclosurePlanNotActive", q.purchase_plan_ref.id);
  auto proof = take(x.purchase.controls.current(actor, p.plan->household_id,
                                                household_purchase_operation,
                                                now.half_day_tick));
  if (!proof || proof->actor_id != actor)
    fail("HouseholdDisclosureCurrentControllerRequired", actor);
  const auto &rep = report(x, p, q.report_id);
  auto k = known(x, actor, q.report_id, now);


  auto request_memory = CheckpointMemoryCharge(0);
  std::vector<ResourceKey> resources{{ResourceKey::Kind::actor, actor}};
  // Report 以真实不可变 fact/P10 快照核验，不制造无原版本源的域 revision。
  std::vector<DomainReadKey> domains;
  std::vector<StableId> ids{
      q.purchase_plan_ref.id, p.plan->accepted_policy_ref.id,
      p.buyer->population_selection_ref, rep.fact_id, proof->fact_id};
  auto stamp = take(x.disclosure.clock.capture(resources, domains, ids));
  auto perm = permission(x, p, rep, actor, now, *stamp);
  auto actual_population = take(read_civilian_population_ref(
      x.population.economy, x.population.population, p.buyer->cohort_id));
  auto population = take(read_population_revision_source(
      x.population.economy, x.population.population, actual_population));
  auto selection = take(
      population_selection_source_ref(x.purchase.economy, x.purchase.population,
                                      p.buyer->population_selection_ref));
  auto original_observation = observation(x, rep);
  const auto prefix = take(read_current_common_fact_prefix(x.common));


  auto build_memory = CheckpointMemoryCharge(0);
  HouseholdPurchaseReportDisclosure d;
  d.controller_actor_id = actor;
  d.cohort_id = p.buyer->cohort_id;
  d.report_id = q.report_id;
  d.purchase_plan_ref = q.purchase_plan_ref;
  d.accepted_policy_ref = p.plan->accepted_policy_ref;
  d.accepted_policy_revision = p.plan->accepted_policy_revision;
  d.population_selection_ref = selection;
  d.controller_authority = *proof;
  d.controller_knowledge_source = k.acquisition->actual_source_ref;
  d.original_observation = original_observation.value;
  d.controller_learned_at = k.knowledge.learned_at;
  d.disclosed_at = now;
  d.permission = perm.value;
  d.actual_population = actual_population;
  d.population_location_source = population.location_binding_ref;
  d.actual_contact_prefix.common_fact_count = prefix.common_fact_count;
  if (prefix.last_common_fact)
    d.actual_contact_prefix.last_common_fact =
        SourceRef{prefix.last_common_fact->domain,
                  prefix.last_common_fact->kind, prefix.last_common_fact->id};
  // 原同处算法先于任何 ID/事实写入；真实当前前缀，不创造临时来源。
  contact(x, d, true);
  // 返回前重新预留同一原普通字段，再复制 row；原租约仍保活，Moment 可抛
  // 复制失败也不会先搬走原 payload 的计量。返回 prvalue 直接构造调用方。
  return PreparedDisclosure{build_memory, d, nullptr};
}
} // namespace
Status validate_tell_household_purchase_market_report_input(
    const HouseholdPurchaseReportContext &x, const StableId &actor,
    const TellHouseholdPurchaseMarketReport &q) {
  auto result = guard<Done>([&] {
    auto prepared = prepare_disclosure(x, actor, q);
    return Done{};
  });
  return result.ok ? common_success()
                   : Status{false, {}, std::move(result.error)};
}
Outcome<const FactRecord *>
tell_household_purchase_market_report_fact_in_candidate(
    CommonStore &common, InteractionState &interaction,
    const HouseholdPurchaseReportContext &x, const StableId &actor,
    const TellHouseholdPurchaseMarketReport &q) {
  return guard<const FactRecord *>([&] {
    if (&common != &x.common || &interaction != &x.interaction)
      fail("HouseholdDisclosureCandidateMismatch");
    auto prepared = prepare_disclosure(x, actor, q);
    if (prepared.replay) {
      const auto *original = common.state().facts.find(prepared.replay->id);
      if (!original || original->id != prepared.replay->id ||
          original->domain != "interaction" ||
          original->kind != household_purchase_report_source_kind)
        fail("HouseholdDisclosureOriginalFactRequired");
      return original;
    }
    auto &d = prepared.row;
    auto id = take(common.next_owned_id("household_purchase_report"));


    // 新 ID 的副本随原 prepared 首字段租约保活到 row 析构。
    (prepared.memory).grow(0);
    d.id = id.id();
    const auto ref = source(d.id);


    auto header_memory = CheckpointMemoryCharge(0);
    take(common.append_fact({ref.value.id, ref.value.kind, ref.value.domain}));
    if (!interaction.face_to_face_contacts->purchase_plan_disclosures.insert(
            d.id, d))
      fail("DuplicateHouseholdDisclosure", d.id);
    // Original actor/selection/arrival/knowledge replay at this exact append
    // cut. Any failure rolls back this whole candidate, not just the disclosure
    // row.
    take(read_household_purchase_report_source(x, ref, 1));
    const auto *actual = common.state().facts.find(d.id);
    if (!actual || actual->id != d.id || actual->domain != "interaction" ||
        actual->kind != household_purchase_report_source_kind)
      fail("HouseholdDisclosureActualFactRequired");
    return actual;
  });
}
// 旧裸 ID ABI 仅保兼容；正式命令桥借上面的原事实，不复制返回 ID。
Outcome<StableId> tell_household_purchase_market_report_in_candidate(
    CommonStore &common, InteractionState &interaction,
    const HouseholdPurchaseReportContext &x, const StableId &actor,
    const TellHouseholdPurchaseMarketReport &q) {
  return guard<StableId>([&] {
    const auto *actual =
        take(tell_household_purchase_market_report_fact_in_candidate(
            common, interaction, x, actor, q));


    auto memory = CheckpointMemoryCharge(0);
    return actual->id;
  });
}
Outcome<std::optional<HouseholdPurchaseReportView>>
read_known_household_purchase_report(const HouseholdPurchaseReportContext &x,
                                     const SourceRef &planref,
                                     const StableId &inventory,
                                     const StableId &site, const StableId &item,
                                     const WorldMoment &now) {
  return guard<std::optional<HouseholdPurchaseReportView>>([&] {
    const auto &s = state(x);
    if (cmp(now, take(x.disclosure.clock.read_current_moment())))
      fail("HouseholdDisclosureQueryMomentMismatch");
    plan(x, planref);
    std::optional<HouseholdPurchaseReportView> best;
    for (const auto &row : s.purchase_plan_disclosures.rows())
      if (same(row.second.purchase_plan_ref, planref)) {
        auto v = take(
            read_household_purchase_report_source(x, source(row.first), 1));
        if (v.observation->inventory_id != inventory ||
            v.observation->site_id != site || v.observation->item_id != item ||
            cmp(v.delivery->disclosed_at, now) > 0 ||
            (v.report->valid_until_tick &&
             now.half_day_tick > *v.report->valid_until_tick))
          continue;
        const auto *f = take(x.disclosure.results.fact_v2_borrowed(
            v.report->fact_id, x.disclosure.result_stamp));
        if (!f || cmp(f->occurred_at, now) > 0)
          fail("HouseholdDisclosureObservationTimeMismatch", row.first);
        if (best) {
          const auto *old = take(x.disclosure.results.fact_v2_borrowed(
              best->report->fact_id, x.disclosure.result_stamp));
          if (!old)
            fail("HouseholdDisclosureObservationMissing");
          const auto c = cmp(f->occurred_at, old->occurred_at);
          if (c < 0 || (c == 0 && v.observation->source_revision <=
                                      best->observation->source_revision))
            continue;
        }
        best = v;
      }
    return best;
  });
}
Status validate_household_purchase_report_sources(
    const HouseholdPurchaseReportContext &x) {
  auto r = guard<Done>([&] {
    const auto &s = state(x);
    for (std::size_t n = 0; n < s.purchase_plan_disclosures.rows().size();
         ++n) {
      const auto &row = s.purchase_plan_disclosures.rows()[n];
      take(read_household_purchase_report_source(x, source(row.first), 1));
      for (std::size_t j = 0; j < n; ++j) {
        const auto &old = s.purchase_plan_disclosures.rows()[j].second;
        if (same(old.purchase_plan_ref, row.second.purchase_plan_ref) &&
            old.report_id == row.second.report_id)
          fail("DuplicateHouseholdPlanReportDisclosure", row.first);
      }
    }
    for (const auto &h : x.common.state().facts.rows())
      if (h.second.domain == "interaction" &&
          h.second.kind == household_purchase_report_source_kind &&
          !s.purchase_plan_disclosures.find(h.first))
        fail("UnownedHouseholdDisclosureSource", h.first);
    return Done{};
  });
  return r.ok ? common_success() : Status{false, {}, std::move(r.error)};
}
Outcome<ExactOwnerFactRead>
project_household_purchase_report_source(const CommonStore &common,
                                         const InteractionState &interaction,
                                         const SourceRef &ref, Integer schema) {
  return guard<ExactOwnerFactRead>([&] {
    const auto *d = take(read_original_household_purchase_report_source(
        common, interaction, ref, schema));


    ExactOwnerFactRead out;
    out.payload_memory = CheckpointCopyLease::reserve();
    out.reportable = false;
    DynamicFactViewV2 f;
    f.id = d->id;
    f.kind = household_purchase_report_source_kind;
    f.rule_version = "household_purchase_report_v1";
    f.occurred_at = d->disclosed_at;
    f.time_basis = OwnerFactMoment{ref, 1};
    f.publicly_visible = false;
    f.cause_fact_ids = {d->purchase_plan_ref.id, d->accepted_policy_ref.id,
                        d->population_selection_ref.id,
                        d->original_observation.source_ref.id};
    f.participants.push_back(
        {{SubjectKind::actor, d->controller_actor_id}, "sender"});
    out.fact = std::move(f);
    return out;
  });
}

Outcome<TellHouseholdPurchaseMarketReport>
decode_tell_household_purchase_market_report(const BoundaryValue &v) {
  try {
    using namespace boundary;
    fields(v, {"purchase_plan_ref", "report_id"}, {},
           "tell_household_purchase_market_report");
    const auto &r =
        field(v, "purchase_plan_ref", "tell_household_purchase_market_report");
    fields(r, {"domain", "kind", "id"}, {}, "purchase_plan_ref");
    TellHouseholdPurchaseMarketReport out{
        {get_string(r, "domain", "purchase_plan_ref"),
         get_string(r, "kind", "purchase_plan_ref"),
         get_string(r, "id", "purchase_plan_ref")},
        get_string(v, "report_id", "tell_household_purchase_market_report")};
    if (out.purchase_plan_ref.domain != "local_economy" ||
        out.purchase_plan_ref.kind != "household_purchase_planned" ||
        out.purchase_plan_ref.id.empty() || out.report_id.empty())
      return {
          false,
          {},
          common_failure("HouseholdDisclosureCommandIdentityRequired").error};
    return {true, std::move(out), {}};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
Status emit_tell_household_purchase_market_report_fields(
    ProjectionWriter &w, const TellHouseholdPurchaseMarketReport &v) {
  w.object("purchase_plan_ref", [&](auto &c) {
    c.field("domain", v.purchase_plan_ref.domain);
    c.field("kind", v.purchase_plan_ref.kind);
    return c.field("id", v.purchase_plan_ref.id);
  });
  return w.field("report_id", v.report_id);
}


} // namespace hanmo::resident
