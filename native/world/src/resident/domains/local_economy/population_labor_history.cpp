#include "population_labor_history.hpp"
#include "local_economy_internal.hpp"
#include "resident/kernel/native_field_passage.hpp"
#include "resident/kernel/native_transport_catalog.hpp"
#include "resident/kernel/world_bundle.hpp"
#include <algorithm>
namespace hanmo::resident {
Status validate_population_labor_transport_sources(
    const PopulationLaborHistoryContext &x) {
  const auto &borrowed = x.transport_sources;
  if (borrowed.original)
    return common_success();
  const auto *w = borrowed.stationary_world;
  if (!w || !w->catalog || !w->population_transport ||
      !w->transport_registrations || &w->common != &x.original.common ||
      &w->local_economy != &x.original.economy ||
      &*w->population_transport != &x.original.population ||
      &*w->transport_registrations != &x.original.transport)
    return common_failure("PopulationLaborOriginalTransportRequired", {},
                          ErrorKind::unsupported_capability);
  auto selected = selected_native_transport_versions(w->catalog->identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  // 明选 Army 的原 S1 不产生民用人口运输；原军队根与下方 P02 全反覆盖
  // 共同证明这一有限组合，不能由空表猜选择或拿假 reader 代替商业来源。
  const bool field_army_only =
      selected.value.group && w->catalog->field_group_passage &&
      std::none_of(w->catalog->identity.module_versions.begin(),
                   w->catalog->identity.module_versions.end(),
                   [](const auto &m) { return m.first == "cargo_handling"; });
  if ((selected.value.group && !field_army_only) || !selected.value.travel)
    return common_failure("PopulationLaborSelectedTransportRequired", {},
                          ErrorKind::unsupported_capability);
  auto valid = validate_native_transport_layout(*w);
  if (!valid.ok)
    return valid;
  if (field_army_only) {
    valid = validate_native_field_army_transport_roots(*w);
    if (!valid.ok)
      return valid;
  }
  const auto &p = x.original.population;
  if (!p.materializations.empty() || !p.transport_bindings.empty() ||
      !p.releases.empty())
    return common_failure("PopulationLaborUnselectedTransportPayload");
  const auto transport_source = [](const SourceRef &s) {
    return s.kind == "transport_registration" ||
           s.kind == "transport_release" ||
           s.kind == "population_transport_position";
  };
  const auto optional_transport = [&](const std::optional<SourceRef> &s) {
    return s && transport_source(*s);
  };
  for (const auto &row : p.source_revisions.rows())
    if (optional_transport(row.second.location_binding_ref) ||
        optional_transport(row.second.condition_source_ref))
      return common_failure("PopulationLaborUnselectedTransportReference",
                            row.first);
  for (const auto &event : p.revision_sources)
    if (transport_source(event.actual_source) ||
        optional_transport(event.location_after) ||
        optional_transport(event.condition_after))
      return common_failure("PopulationLaborUnselectedTransportReference",
                            event.actual_source.id);
  for (const auto &row : p.selections.rows()) {
    const auto &s = row.second;
    if (s.source.transport_binding_id || s.result_transfer_ref ||
        s.state == PopulationSelectionPhase::materialized ||
        transport_source(s.owner_business_ref) ||
        transport_source(s.release_gate))
      return common_failure("PopulationLaborUnselectedTransportSelection",
                            row.first);
  }
  for (const auto &row : x.original.economy.population_transfers.rows())
    if (row.second.reason == "transport_selection")
      return common_failure("PopulationLaborUnselectedTransportTransfer",
                            row.first);
  // Common孤头不能被P02空表掩盖；真实家户选择仍沿原household来源及劳力重放。
  for (const auto &row : x.original.common.state().facts.rows())
    if (row.second.kind == "transport_registration" ||
        row.second.kind == "transport_release" ||
        row.second.kind == "population_transport_position")
      return common_failure("PopulationLaborUnselectedTransportSource",
                            row.first);
  return common_success();
}
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
bool same(const LaborAllocation &a, const LaborAllocation &b) {
  return a.id == b.id && a.cohort_id == b.cohort_id &&
         a.activity_or_production_id == b.activity_or_production_id &&
         a.workers == b.workers && a.from_tick == b.from_tick &&
         a.through_tick == b.through_tick && a.commitment_id == b.commitment_id;
}
const FactRecord &header(const CommonStore &c, const SourceRef &s) {
  const auto *h = c.state().facts.find(s.id);
  if (!h || h->domain != s.domain || h->kind != s.kind)
    fail("PopulationLaborOriginalHeaderRequired", s.id);
  return *h;
}
struct Replay {
  const PopulationLaborHistoryContext &x;
  OrderedTable<LaborAllocation> allocations;
  StableId requested;
  std::vector<SourceRef> dependencies;
  std::vector<const PopulationRevisionSource *> applied_changes;
  void dependency(const StableId &id, const SourceRef &s) {
    if (id == requested &&
        std::none_of(dependencies.begin(), dependencies.end(),
                     [&](const auto &v) { return same_source_ref(v, s); }))
      dependencies.push_back(s);
  }
  void insert(const LaborAllocation &a, const SourceRef &s) {
    if (a.id.empty() || a.cohort_id.empty() ||
        a.activity_or_production_id.empty() || a.workers < 0 ||
        a.from_tick < 0 || a.through_tick < a.from_tick ||
        !allocations.insert(a.id, a))
      fail("PopulationLaborOriginalOriginMismatch", a.id);
    dependency(a.id, s);
  }
  const PopulationRevisionSource &revision(const StableId &cohort,
                                           const SourceRef &s) {
    const PopulationRevisionSource *found = nullptr;
    for (const auto &r : x.original.population.revision_sources)
      if (r.cohort_id == cohort && same_source_ref(r.actual_source, s)) {
        if (found)
          fail("PopulationLaborRevisionAmbiguous", s.id);
        found = &r;
      }
    if (!found)
      fail("PopulationLaborRevisionMissing", s.id);
    if (std::find(applied_changes.begin(), applied_changes.end(), found) !=
        applied_changes.end())
      fail("DuplicatePopulationLaborSourceApplication", s.id);
    applied_changes.push_back(found);
    take(read_population_original_change_time(x.original, *found));
    return *found;
  }
  void checked_change(const PopulationLaborChange &c, const StableId &cohort,
                      const SourceRef &s) {
    auto *a = allocations.find(c.allocation_id);
    if (!a || a->cohort_id != cohort || a->workers != c.before_workers ||
        a->through_tick != c.before_through_tick)
      fail("PopulationLaborPriorHistoryMismatch", c.allocation_id);
    a->workers = c.after_workers;
    a->through_tick = c.after_through_tick;
    dependency(c.allocation_id, s);
  }
  void exact_changes(const PopulationRevisionSource &r,
                     const std::vector<PopulationLaborChange> &actual) {
    if (r.labor_changes.size() != actual.size())
      fail("PopulationLaborChangeCoverage", r.actual_source.id);
    for (std::size_t i = 0; i < actual.size(); ++i) {
      const auto &a = actual[i];
      const auto &b = r.labor_changes[i];
      if (a.allocation_id != b.allocation_id ||
          a.before_workers != b.before_workers ||
          a.after_workers != b.after_workers ||
          a.before_through_tick != b.before_through_tick ||
          a.after_through_tick != b.after_through_tick)
        fail("PopulationLaborActualReductionMismatch", a.allocation_id);
      dependency(a.allocation_id, r.actual_source);
    }
  }
  void retire(const PopulationRevisionSource &r,
              const std::vector<StableId> &ids, Integer at) {
    std::vector<PopulationLaborChange> actual;
    for (std::size_t n = 0; n < ids.size(); ++n) {
      const auto &id = ids[n];
      if (std::find(ids.begin(), ids.begin() + n, id) != ids.begin() + n)
        fail("DuplicatePopulationLaborRelease", id);
      auto *a = allocations.find(id);
      if (!a || a->cohort_id != r.cohort_id)
        fail("PopulationLaborReleaseScopeMismatch", id);
      PopulationLaborChange v{id, a->workers, a->workers, a->through_tick,
                              a->through_tick};
      if (a->from_tick < at)
        a->through_tick = std::min(a->through_tick, at - 1);
      else
        a->workers = 0;
      v.after_workers = a->workers;
      v.after_through_tick = a->through_tick;
      if (v.before_workers != v.after_workers ||
          v.before_through_tick != v.after_through_tick)
        actual.push_back(v);
    }
    exact_changes(r, actual);
  }
  PopulationMaterializationSource
  registration(const PopulationTransportBinding &b,
               const PopulationSelectionResult &result) {
    if (!x.transport_sources.original)
      fail("PopulationLaborOriginalTransportRequired", b.binding_id);
    const auto *source =
        x.original.transport.registrations.find(b.registration_source_ref.id);
    const auto *selection =
        x.original.population.selections.find(result.selection_id);
    if (!source || !selection)
      fail("PopulationLaborRegistrationRequired", b.binding_id);
    const TransportSelectionRegistration *pick = nullptr;
    for (const auto &p : source->selections)
      if (p.selection_id == selection->id) {
        if (pick)
          fail("PopulationLaborRegistrationAmbiguous", selection->id);
        pick = &p;
      }
    if (!pick)
      fail("PopulationLaborRegistrationRequired", selection->id);
    RegisterPopulationTransportFromSelection request{
        selection->id,
        source->accepted_scope.business_source,
        source->accepted_scope.accepted_scope_revision,
        b.registration_source_ref,
        b.transport_unit_id,
        pick->departure_anchor_ref};
    auto original = take(
        x.transport_sources.original->read_population_transport_registration(
            request));
    take(
        x.transport_sources.original->validate_population_transport_binding(b));
    if (original.selection_id != selection->id ||
        original.transport_unit_id != b.transport_unit_id ||
        !same_source_ref(original.actual_source, b.registration_source_ref) ||
        original.release_allocation_ids != pick->release_allocation_ids)
      fail("PopulationLaborRegistrationMismatch", b.binding_id);
    return original;
  }
  void start_transport(const PopulationTransportBinding &b,
                       const SourceRef &adoption) {
    const PopulationSelectionResult *result = nullptr;
    for (const auto &row : x.original.population.materializations.rows())
      if (row.second.result_cohort_ref.transport_binding_id ==
          std::optional<StableId>{b.binding_id}) {
        if (result)
          fail("PopulationLaborMaterializationAmbiguous", b.binding_id);
        result = &row.second;
      }
    if (!result)
      fail("PopulationLaborMaterializationRequired", b.binding_id);
    const auto original = registration(b, *result);
    const auto &changed =
        revision(result->source_cohort_ref.cohort_id, adoption);
    retire(changed, original.release_allocation_ids, original.effective_tick);
    const auto value = take(read_population_revision_source(
        x.original.economy, x.original.population, result->result_cohort_ref));
    if (value.labor_capacity < 0 ||
        b.labor_allocation_refs.size() != (value.labor_capacity > 0 ? 1u : 0u))
      fail("PopulationTransportOriginalLaborMismatch", b.binding_id);
    if (value.labor_capacity > 0)
      insert({b.labor_allocation_refs.front(), b.transport_cohort_id,
              b.transport_unit_id, value.labor_capacity,
              original.effective_tick, original.labor_until_tick,
              original.actual_source.id},
             adoption);
  }
  void transfer(const PopulationTransfer &t, const SourceRef &s) {
    const PopulationTransportBinding *transport = nullptr;
    for (const auto &row : x.original.population.transport_bindings.rows())
      if (row.second.source_population_transfer_ref ==
          std::optional<StableId>{t.id}) {
        if (transport)
          fail("PopulationLaborTransferBindingAmbiguous", t.id);
        transport = &row.second;
      }
    if (transport) {
      start_transport(*transport, s);
      return;
    }
    const auto &r = revision(t.from_cohort_id, s);
    const auto before = take(read_population_revision_source(
        x.original.economy, x.original.population,
        {t.from_cohort_id, r.revision_before, {}}));
    if (before.labor_capacity < t.labor_eligible_count)
      fail("PopulationLaborTransferCapacityMismatch", t.id);
    std::vector<StableId> releases;
    std::vector<PopulationLaborChange> actual;
    for (const auto &change : r.labor_changes) {
      auto *a = allocations.find(change.allocation_id);
      if (!a || a->cohort_id != t.from_cohort_id)
        fail("PopulationLaborReleaseScopeMismatch", change.allocation_id);
      releases.push_back(change.allocation_id);
      actual.push_back({a->id, a->workers, 0, a->through_tick, 0});
    }
    take(local_economy_detail::reduce_original_population_labor_release(
        allocations, t.from_cohort_id,
        before.labor_capacity - t.labor_eligible_count, t.effective_tick,
        releases));
    for (auto &change : actual) {
      const auto *a = allocations.find(change.allocation_id);
      change.after_workers = a->workers;
      change.after_through_tick = a->through_tick;
    }
    exact_changes(r, actual);
  }
  void training_origin(const PopulationTransfer &t, const SourceRef &s) {
    const auto *approval =
        x.original.organizations.facts.find(t.source_fact_id);
    if (!approval || approval->kind != "population_training_approved")
      return;
    const auto *a =
        x.original.organizations.activities.find(approval->activity_id);
    const auto *terms =
        a ? x.organization_catalog.activity_terms.find(a->terms_id) : nullptr;
    if (!a || !terms || a->kind != OrgActivityKind::population_training ||
        a->participation.size() != 1 || a->executors.size() != 1 ||
        a->participation.front().subject.kind != SubjectKind::cohort ||
        a->participation.front().subject.id != t.to_cohort_id ||
        a->participation.front().binding_kind !=
            ParticipationKind::cohort_allocation ||
        t.reason != "voluntary_training" || approval->amount != t.count ||
        t.labor_eligible_count <= 0 ||
        std::find(terms->consenting_cohort_ids.begin(),
                  terms->consenting_cohort_ids.end(),
                  t.from_cohort_id) == terms->consenting_cohort_ids.end())
      fail("PopulationTrainingLaborOriginMismatch", t.id);
    auto window = take(x.calendar.read_original_calendar_window(
        {"organization", approval->kind, approval->id}, terms->duration_ticks));
    insert({a->participation.front().binding_id, t.to_cohort_id, a->id,
            t.labor_eligible_count, window.approved_window.from_tick,
            window.approved_window.until_tick - 1, a->id},
           s);
  }
  void management(const FactRecord &h) {
    const auto &m = x.original.economy.production_management;
    if (!m)
      return;
    if (const auto *r = m->configurations.find(h.id)) {
      if (h.domain != "local_economy" ||
          h.kind != "production_configuration_requested" ||
          r->targets.size() != r->prior_workers.size() ||
          r->targets.size() != r->reservation_allocation_ids.size())
        fail("PopulationLaborConfigurationSourceMismatch", h.id);
      for (std::size_t i = 0; i < r->targets.size(); ++i) {
        const auto *a = allocations.find(r->targets[i].allocation_id);
        if (!a || a->workers != r->prior_workers[i] ||
            a->activity_or_production_id != r->unit_id)
          fail("PopulationLaborConfigurationPriorMismatch", h.id);
        const auto &held = r->reservation_allocation_ids[i];
        const auto increase =
            std::max(Integer(0), r->targets[i].workers - a->workers);
        if (held.empty() != (increase == 0))
          fail("PopulationLaborConfigurationReservationMismatch", h.id);
        if (!held.empty())
          insert({held, a->cohort_id, r->unit_id, increase, r->effective_tick,
                  a->through_tick, r->id},
                 {h.domain, h.kind, h.id});
      }
    }
    if (const auto *result = m->results.find(h.id)) {
      const auto *r = m->configurations.find(result->configuration_id);
      if (!r || h.domain != "local_economy" ||
          h.kind != "production_configuration_" + result->kind ||
          (result->kind != "applied" && result->kind != "cancelled" &&
           result->kind != "superseded"))
        fail("PopulationLaborConfigurationResultMismatch", h.id);
      if (result->kind == "applied")
        for (std::size_t i = 0; i < r->targets.size(); ++i) {
          auto *a = allocations.find(r->targets[i].allocation_id);
          if (!a || a->workers != r->prior_workers[i])
            fail("PopulationLaborConfigurationPriorMismatch", h.id);
          a->workers = r->targets[i].workers;
          dependency(a->id, {h.domain, h.kind, h.id});
        }
      for (const auto &id : r->reservation_allocation_ids)
        if (!id.empty()) {
          auto *a = allocations.find(id);
          if (!a || a->commitment_id != r->id)
            fail("PopulationLaborReservationOriginMismatch", id);
          a->workers = 0;
          dependency(id, {h.domain, h.kind, h.id});
        }
    }
  }
  void household(const FactRecord &h) {
    if (h.domain != "local_economy" ||
        h.kind != "household_buyer_labor_reserved")
      return;
    if (!x.household_purchases)
      fail("HouseholdLaborOriginalOwnerRequired", h.id);
    const HouseholdPurchasePlan *plan = nullptr;
    const CohortHouseholdBuyer *buyer = nullptr;
    for (const auto &row : x.household_purchases->plans.rows())
      if (const auto *b =
              std::get_if<CohortHouseholdBuyer>(&row.second.execution_choice);
          b && b->labor_allocation_id == h.id) {
        if (plan)
          fail("HouseholdLaborOriginAmbiguous", h.id);
        plan = &row.second;
        buyer = b;
      }
    if (!plan || !buyer || buyer->workers != 1)
      fail("HouseholdLaborOriginalPlanRequired", h.id);
    header(x.original.common,
           {"local_economy", "household_purchase_planned", plan->plan_id});
    const auto *s =
        x.original.population.selections.find(buyer->population_selection_ref);
    if (!s || s->source.cohort_id != buyer->cohort_id ||
        s->selected_labor_eligible != 1 || s->selected_persons != 1 ||
        s->purpose != PopulationSelectionPurpose::household_purchase ||
        !same_source_ref(
            s->owner_business_ref,
            {"local_economy", "household_purchase_planned", plan->plan_id}))
      fail("HouseholdLaborOriginalSelectionMismatch", h.id);
    insert({h.id, buyer->cohort_id, plan->plan_id, 1, s->created_tick,
            plan->delivery_deadline, plan->plan_id},
           {h.domain, h.kind, h.id});
  }
  void staffing(const FactRecord &h) {
    const auto &p = x.original.population;
    if (const auto *o = p.public_market_labor_origins.find(h.id)) {
      if (!x.staffing)
        fail("PublicStaffingLaborOriginalOwnerRequired", h.id);
      const auto *verified = take(read_public_market_staffing_labor_origin(
          *x.staffing, o->binding_ref));
      if (verified != o)
        fail("PopulationLaborSnapshotMismatch", h.id);
      header(x.original.common,
             {"local_economy", "public_market_labor_origin", o->origin_id});
      insert(o->original_allocation, {h.domain, h.kind, h.id});
    }
    for (const auto &t : p.public_market_labor_transitions)
      if (t.transition_id == h.id) {
        const auto *o = p.public_market_labor_origins.find(t.origin_ref.id);
        if (!o || !x.staffing)
          fail("PublicStaffingLaborOriginalOwnerRequired", h.id);
        take(x.staffing->sources.validate_staffing_labor_change(*o, t));
        if (const auto *change =
                std::get_if<PublicMarketAllocationAdjusted>(&t.change)) {
          const auto *a = allocations.find(o->original_allocation.id);
          if (change->after_allocation
                  ? (!a || !same(*a, *change->after_allocation))
                  : (!a || a->workers != 0 ||
                     a->cohort_id != change->before_allocation.cohort_id ||
                     a->activity_or_production_id !=
                         change->before_allocation.activity_or_production_id ||
                     a->from_tick != change->before_allocation.from_tick ||
                     a->through_tick !=
                         change->before_allocation.through_tick ||
                     a->commitment_id !=
                         change->before_allocation.commitment_id))
            fail("PublicStaffingActualLaborChangeMismatch", h.id);
        } else if (std::holds_alternative<PublicMarketLaborReleased>(
                       t.change)) {
          if (!allocations.erase(o->original_allocation.id))
            fail("PublicStaffingOriginalReleaseMismatch", h.id);
          dependency(o->original_allocation.id, {h.domain, h.kind, h.id});
        }
      }
  }
  void apply(const FactRecord &h) {
    const SourceRef source{h.domain, h.kind, h.id};
    management(h);
    household(h);
    staffing(h);
    if (h.domain == "local_economy" && h.kind == "population_transfer") {
      const auto *t = x.original.economy.population_transfers.find(h.id);
      if (!t)
        fail("PopulationLaborTransferMissing", h.id);
      transfer(*t, source);
      training_origin(*t, source);
    }
    if (h.domain == "travel" && h.kind == "transport_registration")
      for (const auto &row : x.original.population.transport_bindings.rows())
        if (!row.second.source_population_transfer_ref &&
            same_source_ref(row.second.registration_source_ref, source))
          start_transport(row.second, source);
    if (h.domain == "travel" && h.kind == "transport_release")
      for (const auto &row : x.original.population.releases)
        if (same_source_ref(row.actual_handover_source, source)) {
          const auto *b =
              x.original.population.transport_bindings.find(row.binding_id);
          if (!b)
            fail("PopulationLaborReleaseBindingMissing", h.id);
          if (!x.transport_sources.original)
            fail("PopulationLaborOriginalTransportRequired", b->binding_id);
          const auto actual = take(
              x.transport_sources.original->read_population_transport_release(
                  *b, source));
          if (!same_source_ref(actual.source, source) ||
              actual.effective_tick != actual.occurred_at.half_day_tick ||
              actual.effective_tick != row.effective_tick)
            fail("PopulationLaborReleaseSourceMismatch", h.id);
          take(x.transport_sources.original
                   ->validate_population_transport_release(*b, source));
          retire(revision(b->transport_cohort_id, source),
                 b->labor_allocation_refs, actual.effective_tick);
        }
    if (h.domain == "organization" &&
        h.kind == "population_training_completed") {
      const auto *f = x.original.organizations.facts.find(h.id);
      const auto *a =
          f ? x.original.organizations.activities.find(f->activity_id)
            : nullptr;
      if (!a || a->kind != OrgActivityKind::population_training ||
          a->participation.size() != 1 ||
          std::find(a->result_fact_ids.begin(), a->result_fact_ids.end(),
                    h.id) == a->result_fact_ids.end())
        fail("PopulationTrainingLaborTerminalMismatch", h.id);
      const auto &id = a->participation.front().binding_id;
      if (!allocations.erase(id))
        fail("PopulationTrainingLaborReleaseMismatch", id);
      dependency(id, source);
    }
  }
};
OriginalLaborAllocationRead replay(const PopulationLaborHistoryContext &x,
                                   const StableId &allocation,
                                   const SourceRef *cut, bool after) {
  take(validate_population_labor_transport_sources(x));
  if (x.staffing && (&x.staffing->common != &x.original.common ||
                     &x.staffing->economy != &x.original.economy ||
                     &x.staffing->population != &x.original.population ||
                     (x.household_purchases &&
                      x.household_purchases != &x.staffing->purchases)))
    fail("PopulationLaborSnapshotMismatch");
  if (x.original.population.version != 3)
    fail("PopulationLaborHistoryProfileRequired", allocation);
  take(validate_population_count_revisions(
      x.original.common, x.original.economy, x.original.population));
  if (cut)
    header(x.original.common, *cut);
  Replay r{x, {}, allocation, {}, {}};
  for (const auto &row : x.catalog.labor_allocations.rows()) {
    if (row.first != row.second.id)
      fail("PopulationLaborCatalogIdentityMismatch", row.first);
    r.insert(row.second,
             {"local_economy_catalog", "labor_allocation", row.first});
  }
  OriginalLaborAllocationRead captured;
  bool seen = false;
  auto capture = [&] {
    if (const auto *v = r.allocations.find(allocation))
      captured.allocation = *v;
    captured.actual_sources = r.dependencies;
    seen = true;
  };
  for (const auto &row : x.original.common.state().facts.rows()) {
    const auto &h = row.second;
    if (row.first != h.id)
      fail("PopulationLaborHeaderIdentityMismatch", row.first);
    const bool match = cut && same_source_ref(*cut, {h.domain, h.kind, h.id});
    if (match && !after)
      capture();
    r.apply(h);
    if (match && after)
      capture();
  }
  for (const auto &event : x.original.population.revision_sources)
    if (!event.labor_changes.empty() &&
        std::find(r.applied_changes.begin(), r.applied_changes.end(), &event) ==
            r.applied_changes.end())
      fail("PopulationLaborChangeSourceUnconsumed", event.actual_source.id);
  if (r.allocations.order() != x.original.economy.labor_allocations.order())
    fail("PopulationLaborOriginalOrderMismatch");
  if (r.allocations.size() != x.original.economy.labor_allocations.size())
    fail("PopulationLaborCurrentCoverageMismatch");
  for (const auto &row : r.allocations.rows()) {
    const auto *current = x.original.economy.labor_allocations.find(row.first);
    if (!current || !same(row.second, *current))
      fail("PopulationLaborCurrentHistoryMismatch", row.first);
  }
  if (cut && !seen)
    fail("PopulationLaborAdoptionSourceMissing", cut->id);
  return captured;
}
} // namespace
Outcome<OriginalLaborAllocationRead>
read_original_labor_allocation_before_source(
    const PopulationLaborHistoryContext &x, const StableId &id,
    const SourceRef &s) {
  try {
    return {true, replay(x, id, &s, false), {}};
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
Outcome<OriginalLaborAllocationRead>
read_original_labor_allocation_after_source(
    const PopulationLaborHistoryContext &x, const StableId &id,
    const SourceRef &s) {
  try {
    return {true, replay(x, id, &s, true), {}};
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
Status
validate_population_labor_history(const PopulationLaborHistoryContext &x) {
  try {
    replay(x, {}, nullptr, false);
    return common_success();
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
} // namespace hanmo::resident
