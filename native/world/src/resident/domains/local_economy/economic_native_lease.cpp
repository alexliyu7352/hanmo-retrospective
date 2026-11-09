#include "economic_native_lease.hpp"
#include "resident/domains/commerce/private_shop_asset_responsibilities.hpp"
#include "resident/domains/estate/employment/estate_employment.hpp"
#include "takeover/execution_phase.hpp"
namespace hanmo::resident {
NativeEconomicExactSources::NativeEconomicExactSources(
    NativeEconomicExactDependencies d)
    : dependencies_(std::move(d)), household_(dependencies_.household),
      production_(dependencies_.production),
      payments_(dependencies_.production.population.common,
                dependencies_.production.population.economy,
                &dependencies_.transport_payments, &dependencies_.work_payments,
                &dependencies_.aid_payments, &dependencies_.repair_payments),
      production_validation_{dependencies_.production.work_catalog,
                             dependencies_.production.calendar.sources,
                             production_.inputs(),
                             dependencies_.production.population.clock,
                             dependencies_.household.scale.units_per_half_day},
      validation_{dependencies_.production.calendar, payments_,
                  production_validation_, dependencies_.selected_local_economy,
                  dependencies_.born_population} {}
Status NativeEconomicExactSources::validate_bindings() const {
  const auto &d = dependencies_;
  const auto &p = d.production.population;
  const auto &h = household_.household();
  const auto &o = d.obligations;
  const auto &w = d.work_payments;
  const auto &a = d.aid_payments;
  const auto &r = d.repair_payments;
  auto checked = production_.validate_bindings();
  if (!checked.ok)
    return checked;
  checked = household_.validate_bindings();
  if (!checked.ok)
    return checked;
  checked = validate_named_birth_population_selected_layout(
      p.economy, d.selected_local_economy);
  if (!checked.ok)
    return checked;
  if (named_birth_population_selected(d.selected_local_economy)) {
    const auto *birth = d.born_population;
    if (!birth || birth->selected_local_economy != d.selected_local_economy ||
        &birth->common != &p.common || &birth->economy != &p.economy ||
        &birth->catalog != &d.production.catalog ||
        &birth->births.common != &p.common ||
        &birth->clock != &d.production.calendar.clock ||
        &birth->clock != &p.clock)
      return common_failure("NativeEconomicBornPopulationSnapshotMismatch", {},
                            ErrorKind::internal);
    checked = validate_born_population_source_bindings(*birth);
    if (!checked.ok)
      return checked;
  } else if (d.born_population)
    return common_failure("NativeEconomicBornPopulationSelectionMismatch", {},
                          ErrorKind::internal);
  if (&h.common != &p.common || &h.economy != &p.economy ||
      &h.population != &p.population || &d.household.named.clock != &p.clock ||
      d.production.purchases != &h.state ||
      d.production.staffing != d.household.staffing ||
      &d.production.catalog != &d.household.cohort.economy_catalog ||
      p.military != &d.military || &d.takeover.common != &p.common ||
      &d.takeover.economy != &p.economy ||
      &d.takeover.catalog != &d.production.catalog ||
      &d.employment.common != &p.common ||
      &d.employment.economy != &p.economy ||
      &d.employment.economy_catalog != &d.production.catalog ||
      &d.shops.common != &p.common || &d.shops.economy != &p.economy ||
      &d.shops.economy_catalog != &d.production.catalog ||
      &w.common != &p.common || &w.economy != &p.economy ||
      &a.common != &p.common || &a.economy != &p.economy ||
      &r.common != &p.common || &r.economy != &p.economy ||
      &o.common != &p.common || &o.economy != &p.economy ||
      &w.clock != &p.clock || &a.clock != &p.clock || &r.clock != &p.clock ||
      &o.clock != &p.clock ||
      &d.transport_payments.state != &h.transport_expenses ||
      d.transport_payments.sources.obligations != &o || w.obligations != &o ||
      a.obligations != &o || r.obligations != &o ||
      !o.original.bound_to(p.common, p.economy))
    return common_failure("NativeEconomicExactSnapshotMismatch", {},
                          ErrorKind::internal);
  // These source contexts must be genuine full-owner contexts, even if their
  // original tables currently contain no occurrences. Missing modules cannot
  // be hidden by returning an empty original-payment ID list.
  if (!w.budget_execution || w.budget_execution != &a.budget || !w.settlement ||
      !r.repair || !r.rules || !r.original.bound_to(p.common, r.items) ||
      !a.resources.bound_to(p.common))
    return common_failure("NativeEconomicOriginalOwnerGraphRequired", {},
                          ErrorKind::unsupported_capability);
  return common_success();
}
EconomicExactReadBinding NativeEconomicExactSources::read_binding() const {
  const auto &d = dependencies_;
  return {d.production.catalog,        d.organizations, validation_,
          household_.read_binding(),   &d.military,     &d.takeover,
          &d.takeover_execution,       &d.employment,   &d.shops,
          &production_.labor_history()};
}
Status NativeEconomicExactSources::validate_sources() const {
  auto checked = validate_bindings();
  if (!checked.ok)
    return checked;
  const auto &d = dependencies_;
  const auto &c = calendar();
  checked = validate_economic_obligation_sources(d.obligations);
  if (!checked.ok)
    return checked;
  checked = validate_local_economy(c.common, c.economy, d.production.catalog,
                                   d.organizations, &d.military, &d.takeover,
                                   &d.takeover_execution, &d.employment,
                                   &d.shops, {}, false, &validation_);
  if (!checked.ok)
    return checked;
  return household_.validate_sources();
}
NativeEconomicExactWriteLease::NativeEconomicExactWriteLease(
    NativeEconomicExactWriteDependencies d)
    : dependencies_(std::move(d)), sources_(dependencies_.read),
      production_{dependencies_.read.production.work_catalog,
                  dependencies_.production,
                  sources_.production().inputs(),
                  dependencies_.read.production.population.clock,
                  dependencies_.read.household.scale.units_per_half_day,
                  &sources_.payments()},
      work_expenses_(dependencies_.common, dependencies_.economy,
                     dependencies_.work_expenses,
                     dependencies_.read.work_payments) {}
Status NativeEconomicExactWriteLease::validate_bindings() const {
  auto checked = sources_.validate_bindings();
  if (!checked.ok)
    return checked;
  const auto &d = dependencies_;
  const auto &r = d.read;
  const auto &h = sources_.household().household();
  if (&d.common != &sources_.calendar().common ||
      &d.economy != &sources_.calendar().economy ||
      &d.production != &sources_.calendar().sources ||
      &d.work_expenses != &r.work_payments.state || &d.purchases != &h.state ||
      &d.elapsed != &h.elapsed || &d.takeover.read.common != &d.common ||
      &d.takeover.read.economy != &d.economy ||
      &d.takeover.read.catalog != &r.production.catalog ||
      &d.takeover.state != &r.takeover_execution ||
      d.takeover.read.phase != d.phase || r.takeover.phase != d.phase ||
      r.employment.phase != d.phase || r.shops.phase != d.phase)
    return common_failure("NativeEconomicExactWriteCandidateMismatch", {},
                          ErrorKind::internal);
  auto household =
      sources_.household().write_binding(d.common, d.purchases, d.elapsed);
  if (!household.ok)
    return {false, {}, household.error};
  return common_success();
}
EconomicExactWriteBinding NativeEconomicExactWriteLease::bindings() {
  auto &d = dependencies_;
  const auto &h = sources_.household();
  return {d.common,
          d.economy,
          d.read.production.catalog,
          d.read.organizations,
          d.activities,
          production_,
          sources_.calendar(),
          {d.common, d.purchases, d.elapsed, h.household(), h.market()},
          &d.household_management,
          &d.disruption,
          &d.takeover,
          &d.read.employment,
          &d.market,
          &d.read.shops,
          &sources_.read_binding().original};
}
} // namespace hanmo::resident
