#include "production_capacity_input.hpp"
#include "resident/domains/politics/takeover/politics_takeover_absence.hpp"
#include "resident/kernel/world_bundle.hpp"
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
bool same_owner(const Owner &a, const Owner &b) {
  return a.kind == b.kind && a.id == b.id && !a.extra && !b.extra;
}
bool same_sources(const std::vector<SourceRef> &a,
                  const std::vector<SourceRef> &b) {
  if (a.size() != b.size())
    return false;
  for (std::size_t i = 0; i < a.size(); ++i)
    if (!same_source_ref(a[i], b[i]))
      return false;
  return true;
}
bool same_sources(const std::vector<MomentOwnerSource> &a,
                  const std::vector<MomentOwnerSource> &b) {
  if (a.size() != b.size())
    return false;
  for (std::size_t i = 0; i < a.size(); ++i)
    if (a[i].source_schema != b[i].source_schema ||
        !same_source_ref(a[i].source_ref, b[i].source_ref))
      return false;
  return true;
}
void add(std::vector<SourceRef> &out, const SourceRef &s) {
  if (!valid_source_ref(s))
    fail("ProductionAssetInputSourceShape", s.id);
  if (std::none_of(out.begin(), out.end(),
                   [&](const auto &v) { return same_source_ref(v, s); }))
    out.push_back(s);
}
PoliticalResponsibilityRef unit_ref(const StableId &id) {
  return {PoliticalResponsibilityKind::production_unit, id, {}, {}};
}
PoliticalResponsibilityRef asset_ref(const StableId &unit,
                                     const StableId &asset) {
  return {PoliticalResponsibilityKind::production_asset_use, asset, unit,
          asset};
}
bool political_change(const MomentOwnerSource &s) {
  return s.source_schema == 2 && s.source_ref.domain == "politics" &&
         (s.source_ref.kind == political_takeover_pending_fact_kind ||
          s.source_ref.kind == political_takeover_fact_kind);
}
struct Original {
  const ProductionUnit *unit = nullptr;
  const ProductionCapacityPlan *plan = nullptr;
  std::vector<const ProductionAsset *> assets;
  EconomicProductionAdoption adoption;
};
Original original(const OriginalProductionCapacityContext &x,
                  const StableId &id, const MomentOwnerSource &source) {
  Original o;
  if (political_change(source)) {
    if (!x.takeovers)
      fail("ProductionTakeoverOriginalSourceRequired", source.source_ref.id);
    auto scope = take(x.adoptions.read_original_production_asset_scope(id));
    o.adoption = {
        scope.unit_id, source,
        take(x.takeovers->read_production_responsibility_change_moment(id,
                                                                       source)),
        std::move(scope.production_asset_ids)};
  } else
    o.adoption = take(x.adoptions.read_production_adoption(id, source));
  const auto &c = x.original.catalog;
  o.unit = c.production_units.find(id);
  if (!o.unit || !c.production_management || o.adoption.unit_id != id ||
      o.adoption.actual_source.source_schema != source.source_schema ||
      !same_source_ref(o.adoption.actual_source.source_ref, source.source_ref))
    fail("ProductionAssetAdoptionMismatch", id);
  o.plan = c.production_management->plans.find(id);
  if (!o.plan ||
      o.plan->asset_uses.size() != o.adoption.production_asset_ids.size())
    fail("ProductionAssetAdoptionPlanMismatch", id);
  for (std::size_t i = 0; i < o.plan->asset_uses.size(); ++i) {
    const auto &use = o.plan->asset_uses[i];
    const auto *a = c.production_management->assets.find(use.asset_id);
    if (!a || o.adoption.production_asset_ids[i] != a->id)
      fail("ProductionAssetAdoptionPlanMismatch", use.asset_id);
    o.assets.push_back(a);
  }
  return o;
}
Integer capacity(const Original &o, const std::vector<Integer> &usable) {
  Integer land = 0, facility = 0;
  bool has_land = false, has_facility = false;
  for (std::size_t i = 0; i < o.assets.size(); ++i) {
    const auto &use = o.plan->asset_uses[i];
    Integer amount = 0;
    if (usable[i] < 0 || __builtin_mul_overflow(std::min(use.units, usable[i]),
                                                use.batches_per_unit, &amount))
      fail("ProductionAssetCapacityOverflow", use.asset_id);
    auto &sum =
        o.assets[i]->kind == ProductionAssetKind::land ? land : facility;
    if (__builtin_add_overflow(sum, amount, &sum))
      fail("ProductionAssetCapacityOverflow", use.asset_id);
    if (o.assets[i]->kind == ProductionAssetKind::land)
      has_land = true;
    else
      has_facility = true;
  }
  Integer result = o.unit->capacity;
  if (result < 0)
    fail("ProductionAssetCapacityNegative", o.unit->id);
  if (has_land)
    result = std::min(result, land);
  if (has_facility)
    result = std::min(result, facility);
  return result;
}
void right_sources(OriginalProductionCapacityInput &out,
                   const PoliticalResponsibilityMomentRead &r) {
  for (const auto &s : r.actual_sources) {
    if (s.source_schema != 2)
      fail("ProductionAssetResponsibilitySchema", s.source_ref.id);
    add(out.asset_use_source_refs, s.source_ref);
  }
}
std::optional<Owner> right(const OriginalProductionCapacityContext &x,
                           const Original &o,
                           const PoliticalResponsibilityRef &ref,
                           const Owner &owner,
                           const std::vector<StableId> &assets,
                           OriginalProductionCapacityInput &out) {
  if (!x.takeovers)
    return owner;
  auto actual =
      take(political_change(o.adoption.actual_source)
               ? x.takeovers->read_after_production_responsibility_change(
                     ref, o.unit->id, o.adoption.actual_source)
               : x.takeovers->read_before_production_adoption(
                     ref, o.unit->id, o.adoption.actual_source));
  right_sources(out, actual);
  auto executing = take(read_production_responsibility_execution_owner(
      actual, owner, ref, assets));
  if (!executing) {
    if (actual.actual_sources.empty())
      fail("ProductionAssetBlockingSourceRequired", ref.responsibility_id);
    if (!out.actual_blocking_source)
      out.actual_blocking_source = actual.actual_sources.back();
  }
  return executing;
}
} // namespace
Status validate_unselected_economic_production_takeover_sources(
    const OriginalProductionCapacityContext &x) {
  try {
    if (x.takeovers || !x.unselected_takeover_world)
      fail("ProductionTakeoverSelectionProofRequired");
    const auto &w = *x.unselected_takeover_world;
    const auto &o = x.original;
    if (!w.catalog || !w.catalog->local_economy ||
        !w.catalog->production_cycles || !w.production_cycles ||
        !w.catalog->economic_facilities ||
        !w.economic_facility_capacity_sources ||
        !w.catalog->production_facility_conditions ||
        !w.production_facility_conditions || &o.common != &w.common ||
        &o.economy != &w.local_economy ||
        &o.catalog != w.catalog->local_economy.get() ||
        &o.work_catalog != w.catalog->production_cycles.get() ||
        &o.sources != &*w.production_cycles ||
        &o.conditions.facilities.common != &w.common ||
        &o.conditions.facilities.catalog !=
            w.catalog->economic_facilities.get() ||
        &o.conditions.facilities.state !=
            &*w.economic_facility_capacity_sources ||
        &o.conditions.condition_catalog !=
            w.catalog->production_facility_conditions.get() ||
        &o.conditions.conditions != &*w.production_facility_conditions)
      fail("ProductionTakeoverAbsenceWorldMismatch");
    take(validate_unselected_political_takeover_sources(w));
    if (compare(take(o.clock.read_current_moment()),
                take(w.actual_phase_moment())))
      fail("ProductionTakeoverAbsenceClockMismatch");
    // 无接管账时，原付款不能暗换已锁义务的权源。
    for (const auto &row : o.economy.payments.rows()) {
      const auto *obligation =
          o.economy.obligations.find(row.second.obligation_id);
      if (!obligation || row.second.authority_ref != obligation->authority_ref)
        fail("UnexpectedEconomicTakenPaymentSource", row.first);
    }
    // 真配置仍由原权限reader核；这里只拒缺接管记录却改用另一机构的执行主体。
    if (o.economy.production_management)
      for (const auto &row :
           o.economy.production_management->configurations.rows()) {
        const auto *unit = o.catalog.production_units.find(row.second.unit_id);
        if (!unit)
          fail("ProductionAssetAdoptionMismatch", row.second.unit_id);
        if (row.second.authority &&
            (unit->owner.kind != Owner::Kind::organization ||
             row.second.authority->organization_id != unit->owner.id))
          fail("EconomicConfigurationCommandPrincipalMismatch", row.first);
      }
    const auto source = [](const SourceRef &r) {
      if (r.kind == political_takeover_pending_fact_kind ||
          r.kind == political_takeover_fact_kind ||
          r.kind == political_takeover_decision_fact_kind)
        fail("UnselectedProductionTakeoverSource", r.id);
    };
    const auto refs = [&](const std::vector<SourceRef> &rows) {
      for (const auto &r : rows)
        source(r);
    };
    const auto input = [&](const ProductionSegmentSource &s) {
      refs(s.condition_source_refs);
      refs(s.asset_use_source_refs);
      refs(s.participation_refs);
      refs(s.actual_supply_refs);
    };
    for (const auto &row : o.sources.cycles.rows()) {
      refs(row.second.active_input_source_refs);
      input(row.second.active_input);
    }
    for (const auto &row : o.sources.segments.rows()) {
      input(row.second.source);
      source(row.second.close_cause);
    }
    return common_success();
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
Outcome<std::optional<Owner>> read_production_responsibility_execution_owner(
    const PoliticalResponsibilityMomentRead &r, const Owner &original,
    const PoliticalResponsibilityRef &ref,
    const std::vector<StableId> &assets) {
  try {
    if (!r.execution)
      return {true, original, {}};
    const auto &e = *r.execution;
    if (e.responsibility.kind != ref.kind ||
        e.responsibility.responsibility_id != ref.responsibility_id ||
        e.responsibility.unit_id != ref.unit_id ||
        e.responsibility.asset_id != ref.asset_id || r.actual_sources.empty())
      fail("ProductionResponsibilityOriginalMismatch", ref.responsibility_id);
    if (e.status == PoliticalResponsibilityExecutionStatus::pending ||
        e.status == PoliticalResponsibilityExecutionStatus::suspended)
      return {true, {}, {}};
    if (e.status != PoliticalResponsibilityExecutionStatus::approved ||
        !e.decision)
      fail("ProductionResponsibilityDecisionRequired", ref.responsibility_id);
    const auto &d = *e.decision;
    if (d.disposition ==
        PoliticalResponsibilityDisposition::continue_under_original) {
      if (d.execution_principal || !d.production_asset_ids.empty())
        fail("ProductionResponsibilityOriginalExecutionMismatch",
             ref.responsibility_id);
      return {true, original, {}};
    }
    if (d.disposition !=
            PoliticalResponsibilityDisposition::assume_by_successor ||
        !d.execution_principal ||
        d.execution_principal->kind != SubjectKind::organization ||
        d.execution_principal->id != e.registration.successor_organization_id)
      fail("ProductionResponsibilitySuccessorMismatch", ref.responsibility_id);
    auto expected = assets, actual = d.production_asset_ids;
    std::sort(expected.begin(), expected.end());
    std::sort(actual.begin(), actual.end());
    if (actual != expected)
      fail("ProductionResponsibilityAssetUseCoverage", ref.responsibility_id);
    return {true,
            Owner{Owner::Kind::organization, d.execution_principal->id, {}},
            {}};
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
Outcome<OriginalProductionCapacityInput>
read_original_production_capacity_input(
    const OriginalProductionCapacityContext &x, const StableId &id,
    const MomentOwnerSource &source) {
  try {
    if (!x.takeovers)
      take(validate_unselected_economic_production_takeover_sources(x));
    const auto o = original(x, id, source);
    OriginalProductionCapacityInput out;
    out.unit_id = id;
    out.adoption_source = source;
    out.adopted_at = o.adoption.occurred_at;
    add(out.asset_use_source_refs,
        {"local_economy_catalog", "production_capacity_plan", id});
    add(out.asset_use_source_refs, source.source_ref);
    const auto executing = right(x, o, unit_ref(id), o.unit->owner,
                                 o.adoption.production_asset_ids, out);
    bool allowed = bool(executing);
    std::vector<Integer> usable;
    for (const auto *a : o.assets) {
      add(out.asset_use_source_refs,
          {"local_economy_catalog", "production_asset", a->id});
      const auto ref = asset_ref(id, a->id);
      auto asset_owner = right(x, o, ref, a->owner, {a->id}, out);
      if (!asset_owner || !executing || !same_owner(*asset_owner, *executing)) {
        allowed = false;
        if (!out.actual_blocking_source) {
          if (!x.takeovers)
            fail("ProductionAssetUseOwnerMismatch", a->id);
          auto actual = take(
              political_change(source)
                  ? x.takeovers->read_after_production_responsibility_change(
                        ref, id, source)
                  : x.takeovers->read_before_production_adoption(ref, id,
                                                                 source));
          if (actual.actual_sources.empty())
            fail("ProductionAssetUseOwnerMismatch", a->id);
          out.actual_blocking_source = actual.actual_sources.back();
        }
      }
      if (a->kind == ProductionAssetKind::land)
        usable.push_back(a->capacity_units);
      else {
        auto c = take(read_production_facility_capacity_after_adoption(
            x.original.conditions, a->id, out.adopted_at, source.source_ref));
        usable.push_back(c.usable_asset_units);
        add(out.condition_source_refs, c.condition_change_source_ref);
      }
    }
    out.batch_capacity = allowed ? capacity(o, usable) : 0;
    return {true, std::move(out), {}};
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
Status validate_original_production_capacity_window(
    const OriginalProductionCapacityContext &x,
    const OriginalProductionCapacityInput &claimed, const MomentSpan &window) {
  try {
    const auto actual = take(read_original_production_capacity_input(
        x, claimed.unit_id, claimed.adoption_source));
    if (compare(actual.adopted_at, claimed.adopted_at) ||
        actual.batch_capacity != claimed.batch_capacity ||
        !same_sources(actual.condition_source_refs,
                      claimed.condition_source_refs) ||
        !same_sources(actual.asset_use_source_refs,
                      claimed.asset_use_source_refs) ||
        bool(actual.actual_blocking_source) !=
            bool(claimed.actual_blocking_source) ||
        (actual.actual_blocking_source &&
         (actual.actual_blocking_source->source_schema !=
              claimed.actual_blocking_source->source_schema ||
          !same_source_ref(actual.actual_blocking_source->source_ref,
                           claimed.actual_blocking_source->source_ref))))
      fail("ProductionCapacityFrozenSourceMismatch", claimed.unit_id);
    if (compare(window.from, window.to) >= 0 ||
        compare(window.from, actual.adopted_at) < 0 ||
        compare(window.to, take(x.original.clock.read_current_moment())) > 0)
      fail("ProductionCapacityActualWindowMismatch", claimed.unit_id);
    const auto o = original(x, claimed.unit_id, claimed.adoption_source);
    auto unchanged = [&](const PoliticalResponsibilityRef &ref) {
      if (!x.takeovers)
        return;
      auto initial =
          take(political_change(claimed.adoption_source)
                   ? x.takeovers->read_after_production_responsibility_change(
                         ref, o.unit->id, claimed.adoption_source)
                   : x.takeovers->read_before_production_adoption(
                         ref, o.unit->id, claimed.adoption_source));
      auto history = take(
          political_change(claimed.adoption_source)
              ? x.takeovers->read_production_responsibility_window_after_change(
                    ref, o.unit->id, claimed.adoption_source, window)
              : x.takeovers->read_production_responsibility_window(
                    ref, o.unit->id, claimed.adoption_source, window));
      if (!same_sources(initial.actual_sources,
                        history.initial.actual_sources) ||
          !history.changes.empty())
        fail("ProductionInputCrossesResponsibilityChange",
             ref.responsibility_id);
    };
    unchanged(unit_ref(o.unit->id));
    for (const auto *a : o.assets) {
      unchanged(asset_ref(o.unit->id, a->id));
      if (a->kind == ProductionAssetKind::land)
        continue;
      auto adopted = take(read_production_facility_capacity_after_adoption(
          x.original.conditions, a->id, claimed.adopted_at,
          claimed.adoption_source.source_ref));
      auto history = take(read_production_facility_capacity_window(
          x.original.conditions, a->id, window));
      if (history.empty())
        fail("ProductionCapacityConditionWindowMissing", a->id);
      for (const auto &part : history)
        if (part.capacity.condition_revision != adopted.condition_revision ||
            part.capacity.usable_asset_units != adopted.usable_asset_units ||
            !same_source_ref(part.capacity.condition_change_source_ref,
                             adopted.condition_change_source_ref))
          fail("ProductionInputCrossesConditionChange", a->id);
    }
    return common_success();
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
} // namespace hanmo::resident
