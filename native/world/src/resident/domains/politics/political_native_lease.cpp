#include "political_native_lease.hpp"
#include "resident/kernel/exact_complete_world.hpp"
#include "resident/kernel/original_catalog_identity.hpp"
#include "resident/kernel/world_bundle.hpp"

namespace hanmo::resident {
namespace {
Status same_at(const WorldMoment &expected, Outcome<WorldMoment> actual) {
  if (!actual.ok)
    return {false, {}, actual.error};
  auto same = compare_world_moments(expected, actual.value);
  if (!same.ok)
    return {false, {}, same.error};
  return same.value == 0 ? common_success()
                         : common_failure("NativePoliticalExactClockMismatch");
}
PoliticalCalendarReadInputs
with_world_clock(const PoliticalCalendarReadInputs &input,
                 const OrganizationAuthorityClock &clock) {
  const auto &m = input.market;
  return {{m.common, m.organization, m.catalog, m.runtime, m.catalog_identity,
           m.purchases, m.economy, m.population, clock},
          input.politics,
          input.timeline,
          input.economic_sources,
          input.foundation,
          input.foundation_catalog,
          input.action_registrations,
          input.action_sources,
          input.ages};
}
Status political_selection(const CatalogIdentity &identity) {
  const StableId *selected = nullptr;
  for (const auto &entry : identity.module_versions) {
    if (entry.first != "politics")
      continue;
    if (selected)
      return common_failure("DuplicatePoliticalPhysicalModule");
    selected = &entry.second;
  }
  if (!selected || *selected != "2")
    return common_failure("NativePoliticalExactTime2Required");
  return common_success();
}
} // namespace

Outcome<WorldMoment>
NativePoliticalExactSources::WorldClock::read_organization_authority_now()
    const {
  return world_.actual_phase_moment();
}

NativePoliticalExactSources::NativePoliticalExactSources(
    const WorldBundle &world, const WorldMoment &at,
    NativePoliticalExactDependencies dependencies)
    : world_(world), at_(at), dependencies_(std::move(dependencies)),
      clock_(world), versions_(world, dependencies_.formations,
                               dependencies_.returns, dependencies_.periods),
      calendar_(with_world_clock(dependencies_.calendar, clock_)),
      offices_{dependencies_.stamp, dependencies_.office_transitions,
               dependencies_.deaths},
      training_population_{dependencies_.population.common,
                           dependencies_.population.economy,
                           dependencies_.population.population,
                           dependencies_.population.organizations,
                           dependencies_.population.transport,
                           calendar_.current(),
                           dependencies_.population.military},
      training_history_{training_population_,   dependencies_.economy_catalog,
                        calendar_.opening(),    calendar_.calendar(),
                        dependencies_.carriers, dependencies_.movement,
                        dependencies_.spatial},
      training_(training_history_),
      organization_{
          dependencies_.calendar.market.runtime,
          dependencies_.lifecycle_catalog,
          dependencies_.lifecycle_sources,
          dependencies_.relation_sources,
          offices_,
          dependencies_.authority_sources,
          dependencies_.budgets,
          clock_,
          dependencies_.stamp_authority,
          versions_,
          calendar_.inputs().market,
          dependencies_.staffing,
          calendar_.calendar(),
          &training_,
          world.catalog ? world.catalog->household_purchases.get() : nullptr,
          world.catalog ? world.catalog->household_retail_settlements.get()
                        : nullptr} {}

Status NativePoliticalExactSources::validate_bindings() const {
  const auto &w = world_;
  const auto &d = dependencies_;
  const auto &c = calendar_.inputs();
  const auto &m = c.market;
  const auto &p = d.population;
  const auto *s = d.staffing;
  if (!w.catalog || !w.catalog->exact_time ||
      !exact_complete_selected(*w.catalog->exact_time) ||
      !w.catalog->complete_exact_time_runtime || !w.organization_runtime ||
      !w.actor_foundation || !w.world_timeline || !w.production_cycles ||
      !w.population_transport || !w.household_purchases ||
      !w.transport_registrations || !w.spatial_carriers || !w.movement_exact ||
      !w.read_stamps)
    return common_failure("NativePoliticalExactWorldRootsRequired", {},
                          ErrorKind::unsupported_capability);
  if (&m.common != &w.common || &m.organization != &w.organization ||
      &m.economy != &w.local_economy || &c.politics != &w.politics ||
      &m.runtime != &*w.organization_runtime ||
      &m.population != &*w.population_transport ||
      &m.purchases != &*w.household_purchases ||
      &c.foundation != &*w.actor_foundation ||
      &c.timeline != &*w.world_timeline ||
      &c.economic_sources != &*w.production_cycles ||
      &c.ages.family != &w.family || &c.ages.births != &w.family_birth ||
      &d.carriers != &*w.spatial_carriers ||
      &d.movement != &*w.movement_exact ||
      &c.action_registrations !=
          &w.catalog->complete_exact_time_runtime->action_times ||
      &d.stamp_authority != w.read_stamps.get())
    return common_failure("NativePoliticalExactWorldBindingMismatch");
  if (&m.catalog != w.catalog->organization.get() ||
      &d.lifecycle_catalog != w.catalog->organization_lifecycle.get() ||
      &d.common_catalog != w.catalog->common.get() ||
      &d.economy_catalog != w.catalog->local_economy.get() ||
      &d.politics_catalog != w.catalog->politics.get() ||
      &c.foundation_catalog != w.catalog->actor_foundation.get() ||
      !same_original_catalog_identity(m.catalog_identity, w.catalog->identity))
    return common_failure("NativePoliticalExactCatalogBindingMismatch");
  auto checked = validate_organization_runtime_codec_selection(
      m.catalog_identity, m.catalog);
  if (!checked.ok)
    return checked;
  checked = political_selection(m.catalog_identity);
  if (!checked.ok)
    return checked;
  checked = calendar_.validate_same_candidate();
  if (!checked.ok)
    return checked;
  if (!d.politics_catalog.enabled ||
      (s &&
       (&s->common != &w.common || &s->economy != &w.local_economy ||
        &s->population != &m.population || &s->purchases != &m.purchases)) ||
      &p.common != &w.common || &p.economy != &w.local_economy ||
      &p.organizations != &w.organization || &p.population != &m.population ||
      &p.transport != &*w.transport_registrations)
    return common_failure("NativePoliticalExactOriginalContextMismatch");
  if (!s) {
    if (!w.catalog->household_purchases ||
        !w.catalog->household_retail_settlements)
      return common_failure("OrganizationPublicMarketSelectionCatalogRequired",
                            {}, ErrorKind::unsupported_capability);
    checked = validate_unselected_public_market_organization(
        w.common, w.organization, *w.catalog->household_purchases,
        *w.household_purchases, *w.population_transport,
        *w.catalog->household_retail_settlements);
    if (!checked.ok)
      return checked;
  }
  const auto &stamp = d.stamp;
  if (stamp.mode() != ReadMode::phase_current &&
      stamp.mode() != ReadMode::committed_current)
    return common_failure("NativePoliticalExactCurrentStampRequired");
  checked = d.stamp_authority.validate(stamp, w.common.state().identity,
                                       stamp.mode(), w.common, versions_);
  if (!checked.ok)
    return checked;
  checked = same_at(at_, w.actual_phase_moment());
  if (!checked.ok)
    return checked;
  checked =
      same_at(at_, d.calendar.market.clock.read_organization_authority_now());
  if (!checked.ok)
    return checked;
  checked = same_at(at_, m.clock.read_organization_authority_now());
  if (!checked.ok)
    return checked;
  checked = same_at(at_, d.authority_sources.read_organization_authority_now());
  if (!checked.ok)
    return checked;
  checked = same_at(at_, p.clock.read_current_moment());
  if (!checked.ok)
    return checked;
  checked = same_at(at_, d.spatial.current_moment());
  if (!checked.ok)
    return checked;
  if (s) {
    checked = same_at(at_, {true, s->now, {}});
    if (!checked.ok)
      return checked;
  }
  // 原 stamp 时段包含两端，committed-current 则是 from == to 的单点。
  auto from = compare_world_moments(stamp.span().from, at_);
  if (!from.ok)
    return {false, {}, from.error};
  auto to = compare_world_moments(at_, stamp.span().to);
  if (!to.ok)
    return {false, {}, to.error};
  if (from.value > 0 || to.value > 0)
    return common_failure("NativePoliticalExactStampMomentMismatch");
  return common_success();
}

PoliticalExactReadBinding NativePoliticalExactSources::read_binding() const {
  const auto &d = dependencies_;
  return {organization_,
          calendar_.opening(),
          d.common_catalog,
          d.economy_catalog,
          d.calendar.politics,
          d.politics_catalog,
          d.activities,
          d.results,
          d.resource_history,
          d.prospective_grants,
          d.territories,
          d.treaty_exact,
          &calendar_.qualifications()};
}

NativePoliticalExactWriteLease::NativePoliticalExactWriteLease(
    WorldBundle &world, const WorldMoment &at,
    NativePoliticalExactWriteDependencies dependencies)
    : world_(world), dependencies_(std::move(dependencies)),
      sources_(world, at, dependencies_.read),
      calendar_(
          sources_.calendar(),
          {dependencies_.common, dependencies_.organization,
           dependencies_.runtime, dependencies_.economy,
           dependencies_.population, dependencies_.purchases,
           dependencies_.read.activities, dependencies_.read.results,
           dependencies_.read.stamp, dependencies_.membership_releases,
           dependencies_.read.staffing, dependencies_.staffing_effects,
           dependencies_.production_population, sources_.training(),
           world.catalog ? world.catalog->household_purchases.get() : nullptr,
           world.catalog ? world.catalog->household_retail_settlements.get()
                         : nullptr}) {}

Status NativePoliticalExactWriteLease::validate_bindings() const {
  auto checked = sources_.validate_bindings();
  if (!checked.ok)
    return checked;
  checked = calendar_.validate_same_candidate();
  if (!checked.ok)
    return checked;
  const auto &w = world_;
  const auto &d = dependencies_;
  if (&d.common != &w.common || &d.organization != &w.organization ||
      &d.runtime != &*w.organization_runtime ||
      &d.economy != &w.local_economy ||
      &d.population != &*w.population_transport ||
      &d.purchases != &*w.household_purchases || &d.politics != &w.politics)
    return common_failure("NativePoliticalExactWriteCandidateMismatch");
  return common_success();
}

PoliticalExactWriteBinding NativePoliticalExactWriteLease::bindings() {
  auto &d = dependencies_;
  return {calendar_.calendar(),          d.politics,
          d.read.politics_catalog,       d.read.economy_catalog,
          calendar_.political_runtime(), d.treaty_exact};
}
} // namespace hanmo::resident
