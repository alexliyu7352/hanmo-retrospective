#include "production_input_lease.hpp"
#include "resident/domains/organization/office_authority.hpp"
#include "resident/kernel/world_bundle.hpp"
namespace hanmo::resident {
NativeEconomicProductionInputLease::NativeEconomicProductionInputLease(
    NativeEconomicProductionInputDependencies d)
    : dependencies_(std::move(d)),
      labor_history_{
          dependencies_.population,        dependencies_.catalog,
          dependencies_.transport,         dependencies_.training.catalog,
          dependencies_.training.calendar, dependencies_.purchases,
          dependencies_.staffing},
      supply_{dependencies_.calendar, dependencies_.catalog,
              dependencies_.population},
      population_changes_{dependencies_.population, dependencies_.training},
      adoption_context_{dependencies_.population.common,
                        dependencies_.population.economy,
                        dependencies_.catalog,
                        dependencies_.work_catalog,
                        dependencies_.calendar.sources,
                        dependencies_.conditions,
                        dependencies_.population.clock,
                        &supply_,
                        &population_changes_},
      adoptions_(adoption_context_),
      takeovers_(
          dependencies_.takeover
              ? std::optional<
                    NativePoliticalProductionTakeoverRead>{std::in_place,
                                                           *dependencies_
                                                                .takeover,
                                                           adoptions_}
              : std::nullopt),
      capacity_{adoption_context_, adoptions_,
                takeovers_ ? &*takeovers_ : nullptr,
                dependencies_.unselected_takeover_world},
      labor_{labor_history_,         supply_,
             dependencies_.training, dependencies_.carriers,
             dependencies_.movement, dependencies_.spatial},
      input_context_{capacity_, labor_, &dependencies_.tasks},
      inputs_(input_context_) {}
Status NativeEconomicProductionInputLease::validate_bindings() const {
  const auto &d = dependencies_;
  const auto &p = d.population;
  const auto &c = d.calendar;
  const auto &t = d.training;
  auto transport = validate_population_labor_transport_sources(labor_history_);
  if (!transport.ok)
    return transport;
  if (&c.common != &p.common || &c.economy != &p.economy ||
      &c.organization != &p.organizations || &c.clock != &p.clock ||
      &t.common != &p.common || &t.economy != &p.economy ||
      &t.organization != &p.organizations ||
      &d.conditions.facilities.common != &p.common ||
      &d.conditions.clock != &p.clock)
    return common_failure("NativeProductionInputSnapshotMismatch");
  if (!d.catalog.enabled || !d.catalog.production_management ||
      p.population.version != 3 ||
      &d.conditions.facilities.production_assets !=
          &*d.catalog.production_management)
    return common_failure("NativeProductionInputSelectedSourcesRequired");
  if (d.takeover) {
    const auto &takeover = *d.takeover;
    if (d.unselected_takeover_world || &takeover.common != &p.common ||
        &takeover.organizations != &p.organizations ||
        &takeover.organization_catalog != &t.catalog ||
        takeover.state.version != 2 || !takeover.actual_clock ||
        takeover.organization_runtime != &t.runtime || !takeover.politics_state)
      return common_failure("NativeProductionInputAuthoritySnapshotMismatch");
  } else {
    const auto *w = d.unselected_takeover_world;
    if (!w || !w->catalog || !w->organization_runtime ||
        !w->population_transport || !w->transport_registrations ||
        !w->spatial_carriers || !w->movement_exact ||
        &p.population != &*w->population_transport ||
        &p.transport != &*w->transport_registrations ||
        &d.carriers != &*w->spatial_carriers ||
        &d.movement != &*w->movement_exact ||
        &p.organizations != &w->organization ||
        &t.catalog != w->catalog->organization.get() ||
        &t.runtime != &*w->organization_runtime || &d.tasks != &w->tasks)
      return common_failure("NativeProductionInputAuthoritySnapshotMismatch");
    auto absent =
        validate_unselected_economic_production_takeover_sources(capacity_);
    if (!absent.ok)
      return absent;
  }
  if (d.staffing &&
      (&d.staffing->common != &p.common || &d.staffing->economy != &p.economy ||
       &d.staffing->population != &p.population ||
       (d.purchases && &d.staffing->purchases != d.purchases)))
    return common_failure("NativeProductionStaffingSnapshotMismatch");
  auto now = p.clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  auto spatial = d.spatial.current_moment();
  if (!spatial.ok)
    return {false, {}, spatial.error};
  auto same = compare_world_moments(now.value, spatial.value);
  if (!same.ok)
    return {false, {}, same.error};
  if (same.value)
    return common_failure("NativeProductionInputClockMismatch");
  auto training_now = t.clock.read_organization_authority_now();
  if (!training_now.ok)
    return {false, {}, training_now.error};
  auto training_same = compare_world_moments(now.value, training_now.value);
  if (!training_same.ok)
    return {false, {}, training_same.error};
  if (training_same.value)
    return common_failure("NativeProductionTrainingClockMismatch");
  if (d.takeover) {
    auto takeover_now =
        d.takeover->actual_clock->read_organization_authority_now();
    if (!takeover_now.ok)
      return {false, {}, takeover_now.error};
    auto takeover_same = compare_world_moments(now.value, takeover_now.value);
    if (!takeover_same.ok)
      return {false, {}, takeover_same.error};
    if (takeover_same.value)
      return common_failure("NativeProductionTakeoverClockMismatch");
  }
  auto condition =
      compare_world_moments(now.value, d.conditions.facilities.current);
  if (!condition.ok)
    return {false, {}, condition.error};
  if (condition.value)
    return common_failure("NativeProductionConditionSnapshotMismatch");
  return common_success();
}
} // namespace hanmo::resident
