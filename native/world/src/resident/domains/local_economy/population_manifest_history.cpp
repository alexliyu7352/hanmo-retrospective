#include "population_manifest_history.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
template <class T> Outcome<T> failure(const char *code, const StableId &id) {
  return {false, {}, common_failure(code, id).error};
}
const PopulationTransportBinding *binding(const PopulationTransportState &state,
                                          const SourceRef &ref) {
  return ref.domain == "local_economy" &&
                 ref.kind == "population_transport_binding"
             ? state.transport_bindings.find(ref.id)
             : nullptr;
}
Status fact(const CommonStore &common, const SourceRef &ref) {
  const auto *head = common.state().facts.find(ref.id);
  if (!head || head->domain != ref.domain || head->kind != ref.kind)
    return common_failure("PopulationManifestActualSourceRequired", ref.id);
  return common_success();
}
} // namespace
Outcome<std::uint64_t>
read_population_transport_domain_revision(const LocalEconomyState &economy,
                                          const PopulationTransportState &state,
                                          const DomainReadKey &key) {
  if (key.domain != "local_economy")
    return failure<std::uint64_t>("PopulationDomainRevisionNotOwned",
                                  key.domain);
  StableId cohort;
  if (key.kind == "population_source_revision")
    cohort = key.id;
  else if (key.kind == "population_transport_binding") {
    const auto *b = state.transport_bindings.find(key.id);
    if (!b || b->binding_id != key.id)
      return failure<std::uint64_t>("PopulationTransportBindingRequired",
                                    key.id);
    cohort = b->transport_cohort_id;
  } else if (key.kind == "labor_allocation") {
    // Only allocations actually created by this transport writer are covered
    // here.
    const auto *a = economy.labor_allocations.find(key.id);
    const PopulationTransportBinding *owner = nullptr;
    for (const auto &row : state.transport_bindings.rows()) {
      const auto &b = row.second;
      if (std::find(b.labor_allocation_refs.begin(),
                    b.labor_allocation_refs.end(),
                    key.id) == b.labor_allocation_refs.end())
        continue;
      if (owner)
        return failure<std::uint64_t>("DuplicateTransportLaborOwner", key.id);
      owner = &b;
    }
    if (!a || !owner || a->cohort_id != owner->transport_cohort_id ||
        a->activity_or_production_id != owner->transport_unit_id ||
        a->commitment_id != owner->registration_source_ref.id)
      return failure<std::uint64_t>("TransportLaborRevisionSourceRequired",
                                    key.id);
    cohort = owner->transport_cohort_id;
  } else
    return failure<std::uint64_t>("UnsupportedPopulationDomainRevision",
                                  key.kind);
  const auto *source = state.source_revisions.find(cohort);
  if (!source || source->revision < 1 || !economy.cohorts.find(cohort))
    return failure<std::uint64_t>("PopulationRevisionUnavailable", cohort);
  return {true, static_cast<std::uint64_t>(source->revision), {}};
}
Outcome<Integer>
read_population_manifest_revision(const LocalEconomyState &economy,
                                  const PopulationTransportState &state,
                                  const SourceRef &ref) {
  const auto *b = binding(state, ref);
  if (!b || b->state == PopulationTransportPhase::released)
    return failure<Integer>("ActivePopulationTransportBindingRequired", ref.id);
  auto current =
      read_civilian_population_ref(economy, state, b->transport_cohort_id);
  if (!current.ok)
    return {false, {}, current.error};
  if (current.value.transport_binding_id !=
      std::optional<StableId>{b->binding_id})
    return failure<Integer>("PopulationTransportRevisionBacklinkMismatch",
                            ref.id);
  auto original =
      read_population_revision_source(economy, state, current.value);
  if (!original.ok)
    return {false, {}, original.error};
  return {true, current.value.population_revision, {}};
}
Outcome<TransportPopulationSource> read_population_transport_manifest_history(
    const CommonStore &common, const LocalEconomyState &economy,
    const PopulationTransportState &state,
    const TransportRegistrationState &registrations,
    const PopulationTransportSourceRead &sources,
    const SourceRef &population_ref, Integer population_revision,
    const WorldMoment &at) {
  auto valid_moment = encode_world_moment(at);
  if (!valid_moment.ok)
    return {false, {}, valid_moment.error};
  const auto *b = binding(state, population_ref);
  if (!b || b->binding_id != population_ref.id || population_revision < 1)
    return failure<TransportPopulationSource>(
        "PopulationManifestBindingMismatch", population_ref.id);
  auto valid = sources.validate_population_transport_binding(*b);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto *registration =
      registrations.registrations.find(b->registration_source_ref.id);
  if (!registration || b->registration_source_ref.domain != "travel" ||
      b->registration_source_ref.kind != "transport_registration" ||
      registration->id != b->registration_source_ref.id ||
      registration->transport_unit_id != b->transport_unit_id)
    return failure<TransportPopulationSource>(
        "PopulationManifestRegistrationRequired", b->binding_id);
  valid = fact(common, b->registration_source_ref);
  if (!valid.ok)
    return {false, {}, valid.error};
  auto started = compare_world_moments(registration->occurred_at, at);
  if (!started.ok)
    return {false, {}, started.error};
  if (started.value > 0)
    return failure<TransportPopulationSource>(
        "PopulationManifestBeforeRegistration", b->binding_id);
  const PopulationSelectionResult *materialization = nullptr;
  const PopulationSelection *selection = nullptr;
  for (const auto &row : state.materializations.rows()) {
    if (row.second.result_cohort_ref.transport_binding_id !=
        std::optional<StableId>{b->binding_id})
      continue;
    if (materialization)
      return failure<TransportPopulationSource>(
          "DuplicatePopulationBindingMaterialization", b->binding_id);
    materialization = &row.second;
    selection = state.selections.find(row.first);
  }
  if (!materialization || !selection ||
      materialization->selection_id != selection->id ||
      materialization->result_cohort_ref.cohort_id != b->transport_cohort_id ||
      selection->state == PopulationSelectionPhase::reserved ||
      std::count(registration->accepted_scope.accepted_selection_ids.begin(),
                 registration->accepted_scope.accepted_selection_ids.end(),
                 selection->id) != 1)
    return failure<TransportPopulationSource>(
        "PopulationManifestAcceptedSelectionRequired", b->binding_id);
  const TransportSelectionRegistration *accepted = nullptr;
  for (const auto &s : registration->selections)
    if (s.selection_id == selection->id) {
      if (accepted)
        return failure<TransportPopulationSource>("DuplicateTransportSelection",
                                                  selection->id);
      accepted = &s;
    }
  if (!accepted)
    return failure<TransportPopulationSource>(
        "PopulationManifestSelectionRegistrationRequired", selection->id);
  RegisterPopulationTransportFromSelection request{
      selection->id,
      registration->accepted_scope.business_source,
      registration->accepted_scope.accepted_scope_revision,
      b->registration_source_ref,
      b->transport_unit_id,
      accepted->departure_anchor_ref};
  auto original_registration =
      sources.read_population_transport_registration(request);
  if (!original_registration.ok)
    return {false, {}, original_registration.error};
  if (!same_source_ref(original_registration.value.actual_source,
                       b->registration_source_ref) ||
      original_registration.value.labor_until_tick !=
          registration->accepted_scope.labor_until_tick)
    return failure<TransportPopulationSource>(
        "PopulationManifestLaborSourceMismatch", b->binding_id);
  CivilianPopulationRef historical{b->transport_cohort_id, population_revision,
                                   b->binding_id};
  auto original = read_population_revision_source(economy, state, historical);
  if (!original.ok)
    return {false, {}, original.error};
  if (original.value.location_binding_ref.domain != "travel" ||
      original.value.location_binding_ref.kind !=
          "population_transport_position" ||
      original.value.location_binding_ref.id != b->binding_id)
    return failure<TransportPopulationSource>(
        "PopulationManifestOriginalCountMismatch", b->binding_id);
  // The registration writer allocated the materialized cohort's actual labor
  // capacity once. A later release truncates today's allocation. Historical
  // coverage uses the accepted source.
  auto at_registration = read_population_revision_source(
      economy, state, materialization->result_cohort_ref);
  if (!at_registration.ok)
    return {false, {}, at_registration.error};
  Integer workers =
      at.half_day_tick <= original_registration.value.labor_until_tick
          ? at_registration.value.labor_capacity
          : 0;
  if (at_registration.value.headcount != selection->selected_persons ||
      at_registration.value.labor_capacity !=
          selection->selected_labor_eligible ||
      workers > original.value.labor_capacity)
    return failure<TransportPopulationSource>(
        "PopulationManifestOriginalLaborMismatch", b->binding_id);
  const PopulationTransportRelease *released = nullptr;
  for (const auto &release : state.releases)
    if (release.binding_id == b->binding_id) {
      if (released)
        return failure<TransportPopulationSource>(
            "DuplicatePopulationTransportRelease", b->binding_id);
      released = &release;
    }
  if (released) {
    auto original_release = sources.read_population_transport_release(
        *b, released->actual_handover_source);
    if (!original_release.ok)
      return {false, {}, original_release.error};
    auto before = compare_world_moments(at, original_release.value.occurred_at);
    if (!before.ok)
      return {false, {}, before.error};
    // Equal-M entry is ordered by the S1 original entry/registration proof. Its
    // population revision above must still be the pre-release revision; a later
    // revision cannot pass.
    if (before.value > 0)
      return failure<TransportPopulationSource>(
          "PopulationManifestAfterRelease", b->binding_id);
  }
  return {true,
          {population_ref,
           b->transport_cohort_id,
           original.value.headcount,
           workers,
           {}},
          {}};
}
Status validate_population_transport_manifest_history(
    const CommonStore &common, const LocalEconomyState &economy,
    const PopulationTransportState &state,
    const TransportRegistrationState &registrations,
    const PopulationTransportSourceRead &sources,
    const TransportPopulationManifest &manifest, const WorldMoment &at) {
  auto original = read_population_transport_manifest_history(
      common, economy, state, registrations, sources, manifest.population_ref,
      manifest.population_revision, at);
  if (!original.ok)
    return {false, {}, original.error};
  const auto &a = original.value;
  const auto &b = manifest.actual;
  if (!same_source_ref(a.source_ref, b.source_ref) ||
      a.cohort_id != b.cohort_id || a.persons != b.persons ||
      a.working_crew != b.working_crew ||
      a.named_actor_ids != b.named_actor_ids)
    return common_failure("PopulationManifestOriginalValueMismatch",
                          manifest.population_ref.id);
  return common_success();
}
Outcome<Integer>
NativeEconomicTransportManifestHistoryRead::population_manifest_revision(
    const SourceRef &ref) const {
  if (!economy_ || !population_ || !sources_)
    return failure<Integer>("UnselectedCivilianPopulationManifest", ref.id);
  return read_population_manifest_revision(*economy_, *population_, ref);
}
Outcome<TransportPopulationSource>
NativeEconomicTransportManifestHistoryRead::read_population_manifest_history(
    const SourceRef &ref, Integer revision, const WorldMoment &at) const {
  if (!economy_ || !population_ || !sources_)
    return failure<TransportPopulationSource>(
        "UnselectedCivilianPopulationManifest", ref.id);
  return read_population_transport_manifest_history(
      common_, *economy_, *population_, registrations_, *sources_, ref,
      revision, at);
}
Status
NativeEconomicTransportManifestHistoryRead::validate_inventory_manifest_history(
    const TransportInventoryManifest &row) const {
  return validate_inventory_entry_evidence(
      common_, catalog_, row.entry_evidence, entries_, items_);
}
Status NativeEconomicTransportManifestHistoryRead::
    validate_population_manifest_history(const TransportPopulationManifest &row,
                                         const WorldMoment &at) const {
  if (!economy_ || !population_ || !sources_)
    return common_failure("UnselectedCivilianPopulationManifest",
                          row.population_ref.id);
  return validate_population_transport_manifest_history(
      common_, *economy_, *population_, registrations_, *sources_, row, at);
}
Outcome<TransportManifestDerivedTotals>
NativeEconomicTransportManifestHistoryRead::read_military_manifest_derived(
    const TransportEntryManifestSource &entry) const {
  if (!military_)
    return {false,
            {},
            common_failure("MilitaryTransportHistoryProviderRequired", "",
                           ErrorKind::unsupported_capability)
                .error};
  return military_->read_military_manifest_derived(entry);
}
Status
NativeEconomicTransportManifestHistoryRead::validate_military_manifest_history(
    const TransportEntryManifestSource &entry) const {
  if (!military_)
    return common_failure("MilitaryTransportHistoryProviderRequired", "",
                          ErrorKind::unsupported_capability);
  return military_->validate_military_manifest_history(entry);
}
} // namespace hanmo::resident
