#include "political_calendar_native_group.hpp"
#include "resident/domains/local_economy/production/production_population_change.hpp"
namespace hanmo::resident {
Outcome<VerifiedOfficeTransition>
NativePoliticalCalendarOfficeRead::read_office_transition(
    const SourceRef &source, const ReadStamp &stamp) const {
  if (source.domain == "politics" &&
      (source.kind == "political_appointment_arrived" ||
       source.kind == "political_office_dismissed"))
    return political_.read_office_transition(source, stamp);
  if (source.domain == "organization" &&
      source.kind == "organization_office_resigned")
    return membership_.read_office_transition(source, stamp);
  return {false,
          {},
          common_failure("PoliticalCalendarOfficeOwnerRequired", source.id,
                         ErrorKind::unsupported_capability)
              .error};
}
NativePoliticalCalendarReadGroup::NativePoliticalCalendarReadGroup(
    PoliticalCalendarReadInputs x)
    : inputs_(x), current_(inputs_.market.clock), market_(inputs_.market),
      organization_authority_(inputs_.market.common,
                              inputs_.market.organization,
                              inputs_.market.clock),
      political_authority_(inputs_.market.common, inputs_.politics,
                           inputs_.market.clock),
      activity_moments_(inputs_.market.common, inputs_.market.organization,
                        inputs_.market.catalog, market_),
      activity_history_(inputs_.market.common, inputs_.market.organization,
                        inputs_.market.catalog, activity_moments_),
      opening_{inputs_.market.common,      inputs_.market.economy,
               inputs_.economic_sources,   inputs_.foundation,
               inputs_.foundation_catalog, inputs_.market.organization,
               activity_history_,          inputs_.action_registrations,
               inputs_.action_sources,     current_},
      calendar_(inputs_.timeline, opening_),
      political_offices_(inputs_.market.common, inputs_.politics,
                         inputs_.market.organization, inputs_.market.catalog,
                         inputs_.market.runtime.office_transition_sources,
                         inputs_.market.clock),
      membership_offices_(inputs_.market.common, inputs_.market.organization,
                          inputs_.market.catalog,
                          inputs_.market.runtime.office_transition_sources,
                          inputs_.market.clock),
      offices_(political_offices_, membership_offices_),
      qualifications_(inputs_.ages, inputs_.market.clock),
      training_sources_{inputs_.market.common,
                        inputs_.market.organization,
                        inputs_.market.catalog,
                        inputs_.market.economy,
                        inputs_.market.runtime,
                        inputs_.market.clock,
                        calendar_} {}
Status NativePoliticalCalendarReadGroup::validate_same_candidate() const {
  const auto &m = inputs_.market;
  if (!m.catalog.enabled || !m.catalog.authority_lifecycle_enabled ||
      !m.catalog.exact_fact_clock_enabled ||
      &inputs_.ages.common != &m.common ||
      &inputs_.ages.foundation != &inputs_.foundation ||
      &inputs_.ages.foundation_catalog != &inputs_.foundation_catalog)
    return common_failure("PoliticalCalendarNativeReadCandidateMismatch");
  auto now = read_organization_authority_now(m.common, &m.clock);
  if (!now.ok)
    return {false, {}, now.error};
  return common_success();
}
// 将同一只读组的真实日历传给市场与原组织写者，不另造准入来源。
NativePoliticalCalendarWriteGroup::NativePoliticalCalendarWriteGroup(
    const NativePoliticalCalendarReadGroup &r, PoliticalCalendarWriteInputs x)
    : reads_(r), inputs_(x), market_{x.common,
                                     x.organization,
                                     x.runtime,
                                     r.inputs().market.catalog,
                                     r.inputs().market.catalog_identity,
                                     x.economy,
                                     x.population,
                                     x.purchases,
                                     x.activities,
                                     x.results,
                                     r.inputs().market.clock,
                                     &r.calendar()},
      market_writer_(market_), original_{x.runtime,
                                         r.inputs().market.clock,
                                         x.stamp,
                                         x.membership_releases,
                                         &r.calendar(),
                                         &x.population,
                                         &x.production_population},
      political_{x.runtime, r.inputs().market.clock, x.stamp, r.offices(),
                 &r.qualifications()},
      calendar_{market_,   r.opening(), x.staffing,          x.staffing_effects,
                original_, &x.training, x.household_catalog, x.retail_catalog} {
}
Status NativePoliticalCalendarWriteGroup::validate_same_candidate() const {
  auto status = reads_.validate_same_candidate();
  if (!status.ok)
    return status;
  const auto &m = reads_.inputs().market;
  const auto &x = inputs_;
  if (&x.common != &m.common || &x.organization != &m.organization ||
      &x.runtime != &m.runtime || &x.economy != &m.economy ||
      &x.population != &m.population || &x.purchases != &m.purchases ||
      &x.production_population.common != &m.common ||
      &x.production_population.economy != &m.economy ||
      bool(x.staffing) != bool(x.staffing_effects))
    return common_failure("PoliticalCalendarNativeWriteCandidateMismatch");
  if (x.staffing) {
    const auto &s = *x.staffing;
    if (&s.common != &m.common || &s.economy != &m.economy ||
        &s.population != &m.population || &s.purchases != &m.purchases)
      return common_failure("PoliticalCalendarNativeWriteCandidateMismatch");
  } else {
    if (!x.household_catalog || !x.retail_catalog)
      return common_failure("OrganizationPublicMarketSelectionCatalogRequired",
                            {}, ErrorKind::unsupported_capability);
    status = validate_unselected_public_market_organization(
        x.common, x.organization, *x.household_catalog, x.purchases,
        x.population, *x.retail_catalog);
    if (!status.ok)
      return status;
  }
  auto now = reads_.current().read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  if (x.staffing) {
    auto same = compare_world_moments(now.value, x.staffing->now);
    if (!same.ok)
      return {false, {}, same.error};
    if (same.value)
      return common_failure("PoliticalCalendarNativeStaffingClockMismatch");
  }
  return common_success();
}
} // namespace hanmo::resident
