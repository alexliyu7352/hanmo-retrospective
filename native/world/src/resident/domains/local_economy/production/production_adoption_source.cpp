#include "production_adoption_source.hpp"
#include "production_population_source.hpp"
#include "production_supply_input.hpp"
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
void take(Status v) {
  if (!v.ok)
    throw Failure{v.error};
}
int compare(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b));
}
std::size_t head(const CommonStore &common, const SourceRef &ref) {
  const auto *h = common.state().facts.find(ref.id);
  if (!h || h->id != ref.id || h->domain != ref.domain || h->kind != ref.kind)
    fail("ProductionAdoptionOriginalHeaderRequired", ref.id);
  const auto &order = common.state().facts.order();
  const auto i = std::find(order.begin(), order.end(), ref.id);
  if (i == order.end())
    fail("ProductionAdoptionOriginalHeaderRequired", ref.id);
  return static_cast<std::size_t>(i - order.begin());
}
bool owner(const Owner &a, const Owner &b) {
  return a.kind == b.kind && a.id == b.id && !a.extra && !b.extra;
}
const ProductionUnit &unit(const EconomicProductionAdoptionContext &x,
                           const StableId &id, std::vector<StableId> &assets) {
  const auto *original = x.catalog.production_units.find(id);
  const auto *actual = x.economy.production_units.find(id);
  const auto *profile = x.work_catalog.production_profiles.find(id);
  if (!x.catalog.enabled || !original || !actual || original->id != id ||
      actual->id != id || !profile || profile->unit_id != id ||
      profile->work_kind == EconomicWorkKind::facility_construction ||
      profile->rule_version <= 0 || profile->labor_denominator <= 0 ||
      profile->worker_halfdays_per_batch <= 0 ||
      actual->site_id != original->site_id ||
      !owner(actual->owner, original->owner) ||
      actual->recipe_id != original->recipe_id ||
      actual->capacity != original->capacity ||
      actual->labor_allocation_ids != original->labor_allocation_ids ||
      actual->input_inventory_ids != original->input_inventory_ids ||
      actual->output_inventory_id != original->output_inventory_id)
    fail("ProductionAdoptionOriginalUnitMismatch", id);
  if (!x.catalog.production_management ||
      x.catalog.production_management->version != 1)
    fail("ProductionAdoptionOriginalAssetPlanRequired", id);
  const auto &management = *x.catalog.production_management;
  const auto *plan = management.plans.find(id);
  if (!plan || plan->unit_id != id || plan->asset_uses.empty() ||
      plan->asset_uses.size() > 32)
    fail("ProductionAdoptionOriginalAssetPlanRequired", id);
  for (const auto &use : plan->asset_uses) {
    const auto *a = management.assets.find(use.asset_id);
    Integer quantity = 0;
    if (!a || a->id != use.asset_id || a->source_key.empty() ||
        a->site_id != original->site_id || !owner(a->owner, original->owner) ||
        use.units <= 0 || use.batches_per_unit <= 0 ||
        a->capacity_units < use.units ||
        __builtin_mul_overflow(use.units, use.batches_per_unit, &quantity) ||
        std::find(assets.begin(), assets.end(), a->id) != assets.end())
      fail("ProductionAdoptionOriginalAssetMismatch", use.asset_id);
    assets.push_back(a->id);
  }
  return *original;
}
const AccumulatedProductionState2 &
cycle(const EconomicProductionAdoptionContext &x, const ProductionUnit &u,
      const StableId &id) {
  const auto *c = x.sources.cycles.find(id);
  const auto *recipe = x.catalog.recipes.find(u.recipe_id);
  const auto *profile = x.work_catalog.production_profiles.find(u.id);
  if (!c || c->cycle_source_ref != id || c->unit_id != u.id || !recipe ||
      recipe->period_ticks <= 0 || c->start_tick < 0 ||
      c->due_tick <= c->start_tick ||
      c->due_tick - c->start_tick != recipe->period_ticks || !profile ||
      c->labor_denominator != profile->labor_denominator ||
      c->original_labor_remainder_ref.empty())
    fail("ProductionAdoptionOriginalCycleMismatch", id);
  head(x.common, {"local_economy", "production_cycle_started", id});
  const auto *remainder =
      x.sources.labor_remainders.find(c->original_labor_remainder_ref);
  if (!remainder || remainder->id != c->original_labor_remainder_ref ||
      remainder->unit_id != u.id)
    fail("ProductionAdoptionOriginalRemainderRequired", id);
  if (compare({c->start_tick, {}}, c->last_accounted_at) > 0 ||
      compare(c->last_accounted_at, {c->due_tick, {}}) > 0 ||
      compare(c->last_accounted_at, take(x.clock.read_current_moment())) > 0)
    fail("ProductionAdoptionOriginalCycleTimeMismatch", id);
  return *c;
}
} // namespace
Outcome<EconomicProductionAssetScope>
NativeEconomicProductionAdoptionRead::read_original_production_asset_scope(
    const StableId &id) const {
  try {
    EconomicProductionAssetScope out;
    out.unit_id = id;
    unit(x_, id, out.production_asset_ids);
    return {true, std::move(out), {}};
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
Outcome<EconomicProductionAdoption>
NativeEconomicProductionAdoptionRead::read_production_adoption(
    const StableId &id, const MomentOwnerSource &requested) const {
  try {
    EconomicProductionAdoption out;
    out.unit_id = id;
    out.actual_source = requested;
    const auto &u = unit(x_, id, out.production_asset_ids);
    if (&x_.conditions.facilities.common != &x_.common ||
        &x_.conditions.clock != &x_.clock ||
        &x_.conditions.facilities.production_assets !=
            &*x_.catalog.production_management)
      fail("ProductionAdoptionSnapshotMismatch", id);
    const auto &ref = requested.source_ref;
    const auto cut = head(x_.common, ref);
    if (ref.domain == "local_economy" &&
        ref.kind == "production_cycle_started" &&
        requested.source_schema == 2) {
      const auto &c = cycle(x_, u, ref.id);
      out.occurred_at = {c.start_tick, {}};
    } else if (ref.domain == "local_economy" &&
               ref.kind == "production_input_segment" &&
               requested.source_schema == 2) {
      const auto *s = x_.sources.segments.find(ref.id);
      if (!s || s->id != ref.id || s->source.unit_id != id)
        fail("ProductionAdoptionOriginalSegmentRequired", ref.id);
      const auto &c = cycle(x_, u, s->source.cycle_source_ref);
      if (head(x_.common, {"local_economy", "production_cycle_started",
                           c.cycle_source_ref}) >= cut ||
          compare(s->source.from, s->source.to) >= 0 ||
          compare(s->source.from, {c.start_tick, {}}) < 0 ||
          compare(s->source.to, c.last_accounted_at) > 0 ||
          !valid_source_ref(s->close_cause))
        fail("ProductionAdoptionOriginalSegmentMismatch", ref.id);
      // The owning full validator checks original inputs, close cause and
      // integral. This narrow boundary projection does not call it back through
      // S6.
      out.occurred_at = s->source.to;
    } else if (ref.domain == "local_economy" &&
               ref.kind == "facility_condition_change" &&
               requested.source_schema == 1) {
      const auto *change = x_.conditions.conditions.changes.find(ref.id);
      if (!change || change->change_id != ref.id ||
          std::find(out.production_asset_ids.begin(),
                    out.production_asset_ids.end(),
                    change->facility_ref) == out.production_asset_ids.end())
        fail("ProductionAdoptionConditionAssetMismatch", ref.id);
      take(validate_production_facility_history_sources(x_.conditions));
      out.occurred_at = change->occurred_at;
    } else if (ref.domain == "local_economy" && ref.kind == "local_ration" &&
               requested.source_schema == 1) {
      if (!x_.supply || &x_.supply->population.common != &x_.common ||
          &x_.supply->catalog != &x_.catalog ||
          &x_.supply->calendar.sources != &x_.sources)
        fail("ProductionRationAdoptionReaderRequired", ref.id);
      const auto actual = take(read_original_production_ration_source(
          *x_.supply, ref, requested.source_schema));
      bool relevant = false;
      for (const auto &allocation : u.labor_allocation_ids) {
        const auto *a = x_.catalog.labor_allocations.find(allocation);
        relevant |= a &&
                    actual.receipt->beneficiary.kind == SubjectKind::cohort &&
                    a->cohort_id == actual.receipt->beneficiary.id;
      }
      if (!relevant)
        fail("ProductionRationAdoptionScopeMismatch", ref.id);
      out.occurred_at = actual.applied_at;
    } else if (ref.domain == "local_economy" &&
               ref.kind == "economic_period_opened" &&
               requested.source_schema == 1) {
      if (!x_.supply || &x_.supply->calendar.common != &x_.common ||
          &x_.supply->calendar.economy != &x_.economy ||
          &x_.supply->calendar.sources != &x_.sources ||
          &x_.supply->calendar.clock != &x_.clock)
        fail("ProductionCalendarAdoptionReaderRequired", ref.id);
      const auto *opening = take(read_economic_period_opening(
          x_.supply->calendar, ref, requested.source_schema));
      out.occurred_at = opening->opening_at;
    } else if (is_original_production_population_change(requested)) {
      if (!x_.population_changes ||
          &x_.population_changes->population.common != &x_.common ||
          &x_.population_changes->population.economy != &x_.economy ||
          &x_.population_changes->population.clock != &x_.clock)
        fail("ProductionPopulationAdoptionReaderRequired", ref.id);
      const auto actual = take(read_original_production_population_change(
          *x_.population_changes, requested));
      bool relevant = false;
      for (const auto &allocation : u.labor_allocation_ids) {
        const auto *a = x_.catalog.labor_allocations.find(allocation);
        relevant |=
            a && std::find(actual.cohort_ids.begin(), actual.cohort_ids.end(),
                           a->cohort_id) != actual.cohort_ids.end();
      }
      if (!relevant)
        fail("ProductionPopulationAdoptionScopeMismatch", ref.id);
      out.occurred_at = actual.occurred_at;
    } else
      fail("ProductionAdoptionSourceKindRequired", ref.id);
    take(encode_world_moment(out.occurred_at));
    if (compare(out.occurred_at, take(x_.clock.read_current_moment())) > 0)
      fail("ProductionAdoptionSourceFromFuture", ref.id);
    return {true, std::move(out), {}};
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
} // namespace hanmo::resident
