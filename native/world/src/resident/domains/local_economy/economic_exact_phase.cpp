#include "economic_exact_phase.hpp"
#include "population_labor_history.hpp"
#include "resident/kernel/native_production_cycles.hpp"
#include "resident/kernel/world_bundle.hpp"
#include "resources/resource_site_absence.hpp"
#include <limits>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const StableId &code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> r) {
  if (!r.ok)
    throw Failure{r.error};
  return std::move(r.value);
}
void take(Status r) {
  if (!r.ok)
    throw Failure{r.error};
}
template <class T, class F> Outcome<T> guard(F &&f) {
  try {
    return {true, f(), {}};
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
int cmp(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b));
}
void read_binding(const EconomicExactReadBinding &r, const WorldMoment &at) {
  const auto &c = r.original.calendar;
  const auto &p = r.original.production;
  if (!r.catalog.enabled || !c.sources.calendar_origin ||
      &c.sources != &p.sources || &c.clock != &p.clock ||
      &c.common != &r.household.household.common ||
      &c.economy != &r.household.household.economy ||
      p.units_per_half_day != r.household.household.scale.units_per_half_day ||
      cmp(at, take(c.clock.read_current_moment())) ||
      cmp(at, r.household.household.now) || cmp(at, r.household.market.now))
    fail("EconomicExactReadBindingMismatch");
}
void write_binding(const EconomicExactWriteBinding &w, const WorldMoment &at) {
  if (!w.catalog.enabled || &w.common != &w.calendar.common ||
      &w.economy != &w.calendar.economy ||
      &w.production.state != &w.calendar.sources ||
      &w.production.clock != &w.calendar.clock ||
      &w.common != &w.household.common ||
      &w.economy != &w.household.household.economy ||
      w.production.units_per_half_day !=
          w.household.household.scale.units_per_half_day ||
      !w.production.original_payments ||
      cmp(at, take(w.calendar.clock.read_current_moment())) ||
      cmp(at, w.household.household.now) || cmp(at, w.household.market.now))
    fail("EconomicExactWriteBindingMismatch");
}
std::shared_ptr<const EconomicExactReadLease>
read(EconomicExactWorldFactory f, const WorldBundle &w, const WorldMoment &at) {
  if (!f.read || !f.write)
    fail("EconomicExactWorldFactoryRequired");
  auto lease = take(f.read(w, at));
  if (!lease)
    fail("EconomicExactReadLeaseRequired");
  read_binding(lease->bindings(), at);
  return lease;
}
std::shared_ptr<EconomicExactWriteLease>
write(EconomicExactWorldFactory f, WorldBundle &w, const WorldMoment &at,
      const DynamicStepInput *p = nullptr) {
  if (!f.read || !f.write)
    fail("EconomicExactWorldFactoryRequired");
  auto lease = take(f.write(w, at, p));
  if (!lease)
    fail("EconomicExactWriteLeaseRequired");
  auto b = lease->bindings();
  write_binding(b, at);
  return lease;
}
// 原完整候選與冷恢復共用此終端；不得以當前空allocation省略P02/3來源反讀。
Status validate(const WorldBundle &world, const EconomicExactReadBinding &r,
                const WorldMoment &at) {
  return guard<Done>([&] {
    read_binding(r, at);
    const auto &c = r.original.calendar;
    if (!world.catalog || !world.population_transport ||
        !world.household_purchases || !world.transport_registrations ||
        !r.population_history)
      fail("PopulationLaborOriginalHistoryRequired");
    const auto selected = take(
        selected_native_population_transport_version(world.catalog->identity));
    const auto &h = *r.population_history;
    if (selected != 3 || world.population_transport->version != selected ||
        &h.original.common != &world.common ||
        &h.original.common != &c.common ||
        &h.original.economy != &world.local_economy ||
        &h.original.economy != &c.economy ||
        &h.original.organizations != &world.organization ||
        &h.original.organizations != &c.organization ||
        &h.original.clock != &c.clock ||
        &h.original.population != &*world.population_transport ||
        &h.original.population != &r.household.household.population ||
        &h.original.transport != &*world.transport_registrations ||
        &h.catalog != world.catalog->local_economy.get() ||
        &h.catalog != &r.catalog ||
        &h.organization_catalog != world.catalog->organization.get() ||
        h.household_purchases != &*world.household_purchases ||
        h.household_purchases != &r.household.household.state ||
        h.original.military != r.military)
      fail("PopulationLaborOriginalHistorySnapshotMismatch");
    take(validate_population_labor_history(h));
    take(validate_local_economy(c.common, c.economy, r.catalog, r.organizations,
                                r.military, r.takeover, r.takeover_execution,
                                r.employment, r.shops, {}, false, &r.original));
    take(validate_household_service_runtime_sources(r.household, at));
    return Done{};
  });
}
} // namespace
Status
initialize_economic_exact_sources_in_candidate(EconomicExactWriteBinding &w) {
  return guard<Done>([&] {
    auto now = take(w.calendar.clock.read_current_moment());
    write_binding(w, now);
    take(validate_economic_work_catalog(w.production.catalog, w.catalog));
    take(initialize_economic_calendar_origin_in_candidate(w.production.state,
                                                          w.calendar));
    for (const auto &row : w.production.catalog.production_profiles.rows())
      take(open_production_cycle_in_candidate(
          w.common, w.economy, w.catalog, w.production.catalog,
          w.production.state, w.production.inputs, row.first, now,
          w.production.clock));
    return Done{};
  });
}
ExactPhaseRegistration
make_economic_exact_phase_registration(EconomicExactWorldFactory factory) {
  ExactPhaseRegistration r;
  r.provider = ExactProviderId::local_economy;
  r.profile_id = "economic_original_calendar_and_household";
  r.version = 3;
  r.stages = {ExactStage::resource_recovery, ExactStage::estate_production,
              ExactStage::action_completions};
  r.calendar_stages = {ExactStage::resource_recovery,
                       ExactStage::estate_production};
  r.interval_writer = true;
  r.period_source_owner = true;
  r.period_closure_kind = ExactPeriodClosureKind::stage_barrier;
  r.effect_writer_keys = {"local_economy/calendar_step",
                          "local_economy/household_purchase_service_credit",
                          "local_economy/household_interrupted_period"};
  r.causal_edges = {
      {ExactStage::resource_recovery, ExactStage::estate_production},
      {ExactStage::estate_production, ExactStage::action_completions},
      {ExactStage::estate_production, ExactStage::work_inspection},
      {ExactStage::estate_production, ExactStage::economic_supply_affairs}};
  r.validate_author_selection = [factory](const WorldBundle &w) {
    return guard<Done>([&] {
      take(validate_unselected_resource_sites_world(w));
      auto at = take(w.actual_phase_moment());
      auto lease = read(factory, w, at);
      const auto b = lease->bindings();
      if (!b.catalog.enabled || b.original.production.units_per_half_day <= 0)
        fail("EconomicExactAuthorSelectionRequired");
      take(validate_economic_work_catalog(b.original.production.catalog,
                                          b.catalog));
      return Done{};
    });
  };
  r.validate_world_sources = [factory](const WorldBundle &w,
                                       const WorldMoment &at) {
    return guard<Done>([&] {
      take(validate_unselected_resource_sites_world(w));
      auto lease = read(factory, w, at);
      take(validate(w, lease->bindings(), at));
      return Done{};
    });
  };
  r.predict = [factory](const WorldBundle &w, const WorldMoment &from,
                        const WorldMoment &until) {
    return guard<std::vector<ExactEventPrediction>>([&] {
      auto lease = read(factory, w, from);
      return take(predict_household_failed_period_closures(
          lease->bindings().household, from, until));
    });
  };
  r.apply_due = [factory](WorldBundle &w, const ExactEventPrediction &p,
                          ExactPhaseWriteContext &) {
    return guard<ExactEventApplied>([&] {
      auto lease = write(factory, w, p.at);
      auto b = lease->bindings();
      return take(apply_household_failed_period_closure(b.household, p));
    });
  };
  // Input accumulators close only on an actual input change, consumer window or
  // cycle settlement. An unrelated S1 slice does not manufacture a production
  // segment.
  r.provide_interval = [factory](WorldBundle &w, const MomentSpan &span,
                                 ExactPhaseWriteContext &writes) {
    return guard<Done>([&] {
      auto lease = write(factory, w, span.to);
      auto b = lease->bindings();
      take(provide_household_service_interval(b.household, span, writes));
      return Done{};
    });
  };
  r.apply_calendar_stage = [factory](WorldBundle &w, ExactStage stage,
                                     const ExactPeriodContext &p,
                                     ExactPhaseWriteContext &writes) {
    return guard<Done>([&] {
      // 当前作者确未选资源点才允许这个槽无写入；已选或任何原来源仍显式拒绝。
      if (stage == ExactStage::resource_recovery) {
        take(validate_unselected_resource_sites_world(w));
        const WorldMoment at{p.through_tick, {}};
        auto actual = take(w.actual_phase_moment());
        if (!w.world_timeline || p.period_id.empty() ||
            w.world_timeline->boundary_state != PeriodBoundaryState::open ||
            w.world_timeline->period_id != p.period_id ||
            w.active_exact_write_context() != &writes ||
            !w.common.actor_condition_capture_identity() || cmp(actual, at) ||
            p.through_tick <= 0 ||
            p.opening_at.half_day_tick != w.common.tick() ||
            p.opening_at.half_day_tick != p.through_tick - 1 ||
            take(compare_fractions(p.opening_at.sub_half_day, {})))
          fail("ResourceRecoveryActualPeriodMismatch", p.period_id);
        auto lease = read(factory, w, at);
        (void)lease;
        return Done{};
      }
      if (stage != ExactStage::estate_production)
        fail("EconomicCalendarStageMismatch");
      WorldMoment at{p.through_tick, {}};
      auto original = read(factory, w, at);
      auto input = take(read_economic_calendar_input(
          original->bindings().original.calendar, p));
      auto lease = write(factory, w, at, &input);
      auto b = lease->bindings();
      take(step_local_economy_exact_in_candidate(
          b.common, b.economy, b.catalog, b.organizations, b.activities, input,
          b.household_management, b.disruption, b.takeover, b.employment,
          b.market, b.shops, b.production));
      return Done{};
    });
  };
  r.open_period = [factory](WorldBundle &w, const ExactPeriodContext &p,
                            ExactPhaseWriteContext &) {
    return guard<ExactPeriodOwnerSources>([&] {
      auto lease = write(factory, w, p.opening_at);
      auto b = lease->bindings();
      return take(open_economic_production_calendar_period_in_candidate(
          b.common, b.catalog, b.production, b.calendar, p));
    });
  };
  r.close_period = [factory](WorldBundle &w, const ExactPeriodContext &p,
                             ExactPhaseWriteContext &) {
    return guard<ExactPeriodOwnerSources>([&] {
      auto lease = read(factory, w, {p.through_tick, {}});
      return take(close_economic_calendar_period(
          lease->bindings().original.calendar, p));
    });
  };
  r.read_period_sources = [factory](const WorldBundle &w,
                                    const ExactPeriodContext &p) {
    return guard<ExactPeriodOwnerSources>([&] {
      auto now = take(w.actual_phase_moment());
      auto lease = read(factory, w, now);
      return take(read_economic_calendar_period_sources(
          lease->bindings().original.calendar, p));
    });
  };
  r.validate_applied_source = [factory](const WorldBundle &w,
                                        const ExactEventApplied &applied) {
    return guard<Done>([&] {
      auto at = take(w.actual_phase_moment());
      auto lease = read(factory, w, at);
      auto b = lease->bindings();
      if (applied.source_schema != 1 ||
          applied.applied_result.domain != "local_economy" ||
          applied.applied_result.kind !=
              "household_purchase_period_interrupted")
        fail("EconomicAppliedSourceMismatch", applied.applied_result.id);
      const auto *result = b.household.household.state.service_results.find(
          applied.applied_result.id);
      const auto *period = b.household.household.state.work_periods.find(
          applied.applied_result.id);
      if (!result || !period ||
          period->disposition != WorkPeriodDisposition::interrupted ||
          !same_source_ref(result->coverage_source, applied.applied_result) ||
          cmp(result->completed_at, at) > 0)
        fail("EconomicAppliedSourceMissing", applied.applied_result.id);
      take(validate_household_service_runtime_sources(b.household, at));
      return Done{};
    });
  };
  return r;
}
} // namespace hanmo::resident
