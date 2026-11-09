// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
#include "facility_condition_history.hpp"
#include "../economic_facility_installed_sources.hpp"
#include <algorithm>
namespace hanmo::resident {
Status validate_production_facility_history_sources(
    const ProductionFacilityHistoryContext &x) {
  const auto &f = x.facilities;
  if (x.effects)
    return validate_production_facility_conditions(f.common, x.conditions,
                                                   x.condition_catalog, f.state,
                                                   *x.effects, f.arithmetic);
  if (!x.installed_sources || &x.installed_sources->installation() != &f ||
      !x.installed_sources->bound_to_conditions(x.condition_catalog,
                                                x.conditions))
    return common_failure("ProductionFacilityOriginalEffectsRequired", {},
                          ErrorKind::unsupported_capability);
  auto now = x.clock.read_current_moment();
  if (!now.ok)
    return {false, {}, std::move(now.error)};
  auto same = compare_world_moments(now.value, f.current, f.arithmetic);
  if (!same.ok)
    return {false, {}, std::move(same.error)};
  if (same.value)
    return common_failure("FacilityHistorySnapshotClockMismatch");
  // 原初装reader逐selector、完整condition头、Common与物理历史反核；不能仅看changes空。
  return x.installed_sources->validate_bindings();
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
int cmp(const ProductionFacilityHistoryContext &x, const WorldMoment &a,
        const WorldMoment &b) {
  return take(compare_world_moments(a, b, x.facilities.arithmetic));
}
std::size_t order(const CommonStore &common, const SourceRef &source) {
  std::size_t n = 0;
  for (const auto &row : common.state().facts.rows()) {
    if (row.first == source.id) {
      if (row.second.id != source.id || row.second.domain != source.domain ||
          row.second.kind != source.kind)
        fail("FacilityHistoryOriginalHeaderMismatch", source.id);
      return n;
    }
    ++n;
  }
  fail("FacilityHistoryOriginalHeaderMissing", source.id);
}
struct Original {
  const ProductionFacilityConditionSeed *seed = nullptr;
  const PhysicalServiceFacilityExtension *facility = nullptr;
  const PhysicalFacilityInstalledEvidence *installation = nullptr;
  SourceRef installation_source;
  std::vector<const FacilityConditionChange *> changes;
};
Original original(const ProductionFacilityHistoryContext &x,
                  const StableId &asset) {
  const auto &f = x.facilities;
  const auto now = take(x.clock.read_current_moment());
  if (cmp(x, now, f.current))
    fail("FacilityHistorySnapshotClockMismatch", asset);
  take(validate_production_facility_history_sources(x));
  Original out;
  for (const auto &row : x.condition_catalog.seeds.rows())
    if (row.second.original_asset_ref == asset) {
      if (out.seed || row.first != row.second.condition_ref)
        fail("DuplicateFacilityConditionAsset", asset);
      out.seed = &row.second;
    }
  if (!out.seed)
    fail("FacilityConditionSourceMissing", asset);
  out.facility =
      take(read_production_facility_condition_installation(*out.seed, f.state));
  out.installation =
      &std::get<InstalledPhysicalFacility>(out.facility->initialization_basis)
           .installation_evidence;
  take(validate_economic_facility_installation_source(f, *out.facility));
  out.installation_source = {out.facility->asset_owner_domain,
                             "economic_facility_installed",
                             out.installation->installation_id};
  std::optional<std::size_t> previous_order;
  WorldMoment previous_at = out.installation->installed_at;
  for (const auto &row : x.conditions.changes.rows())
    if (row.second.facility_ref == asset) {
      const auto &c = row.second;
      SourceRef source{out.facility->asset_owner_domain,
                       "facility_condition_change", row.first};
      const auto n = order(f.common, source);
      if (c.change_id != row.first ||
          (previous_order && n <= *previous_order) ||
          cmp(x, c.occurred_at, previous_at) < 0 ||
          cmp(x, c.occurred_at, now) > 0)
        fail("FacilityConditionOriginalHistoryOrder", row.first);
      out.changes.push_back(&c);
      previous_order = n;
      previous_at = c.occurred_at;
    }
  // A retained actual condition header may not lose its only typed payload.
  for (const auto &row : f.common.state().facts.rows())
    if (row.second.kind == "facility_condition_change") {
      const auto *c = x.conditions.changes.find(row.first);
      if (!c)
        fail("FacilityConditionOriginalPayloadMissing", row.first);
      const ProductionFacilityConditionSeed *seed = nullptr;
      for (const auto &v : x.condition_catalog.seeds.rows())
        if (v.second.original_asset_ref == c->facility_ref) {
          if (seed)
            fail("DuplicateFacilityConditionAsset", c->facility_ref);
          seed = &v.second;
        }
      if (!seed || seed->original_installation_seed_ref.owner_domain !=
                       row.second.domain)
        fail("FacilityConditionOriginalHeaderMismatch", row.first);
    }
  return out;
}
ConditionBoundAssetCapacity initial(const ProductionFacilityHistoryContext &x,
                                    const Original &o) {
  const auto &s = *o.seed;
  return {s.original_asset_ref,
          s.condition_ref,
          1,
          s.initial_integrity,
          s.max_integrity,
          take(facility_condition_capacity(s.base_asset_capacity_units,
                                           s.initial_integrity, s.max_integrity,
                                           x.facilities.arithmetic)),
          o.installation_source};
}
void apply(const ProductionFacilityHistoryContext &x, const Original &o,
           ConditionBoundAssetCapacity &state,
           const FacilityConditionChange &c) {
  if (c.old_revision != state.condition_revision ||
      c.old_integrity != state.integrity)
    fail("FacilityConditionHistoryGap", c.change_id);
  state.condition_revision = c.new_revision;
  state.integrity = c.new_integrity;
  state.usable_asset_units = take(facility_condition_capacity(
      o.seed->base_asset_capacity_units, state.integrity, state.max_integrity,
      x.facilities.arithmetic));
  // Real no-change receipts still exist but do not replace the original head.
  if (c.new_revision != c.old_revision)
    state.condition_change_source_ref = {o.facility->asset_owner_domain,
                                         "facility_condition_change",
                                         c.change_id};
}
ConditionBoundAssetCapacity at(const ProductionFacilityHistoryContext &x,
                               const Original &o, const WorldMoment &moment,
                               const SourceRef *cut, bool inclusive = false) {
  if (cmp(x, moment, o.installation->installed_at) < 0 ||
      cmp(x, moment, take(x.clock.read_current_moment())) > 0)
    fail("FacilityHistoryOutsideOriginalLifetime", o.seed->original_asset_ref);
  const auto stop =
      cut ? std::optional<std::size_t>{order(x.facilities.common, *cut)}
          : std::nullopt;
  auto value = initial(x, o);
  for (const auto *c : o.changes) {
    const auto relation = cmp(x, c->occurred_at, moment);
    const auto n =
        order(x.facilities.common, {o.facility->asset_owner_domain,
                                    "facility_condition_change", c->change_id});
    if (stop && ((relation < 0 && (inclusive ? n > *stop : n >= *stop)) ||
                 (relation > 0 && (inclusive ? n <= *stop : n < *stop))))
      fail("FacilityConditionAdoptionCausalityMismatch", c->change_id);
    if (relation > 0 || (stop && (inclusive ? n > *stop : n >= *stop)))
      continue;
    apply(x, o, value, *c);
  }
  return value;
}
} // namespace
Outcome<ConditionBoundAssetCapacity>
read_production_facility_capacity_at_adoption(
    const ProductionFacilityHistoryContext &x, const StableId &asset,
    const WorldMoment &moment, const SourceRef *cut) {
  try {
    take(encode_world_moment(moment, x.facilities.arithmetic));
    if (!cut && cmp(x, moment, take(x.clock.read_current_moment())))
      fail("FacilityHistoryActualCurrentMomentRequired", asset);
    const auto o = original(x, asset);
    return {true, at(x, o, moment, cut), {}};
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
Outcome<ConditionBoundAssetCapacity>
read_production_facility_capacity_after_adoption(
    const ProductionFacilityHistoryContext &x, const StableId &asset,
    const WorldMoment &moment, const SourceRef &cut) {
  try {
    take(encode_world_moment(moment, x.facilities.arithmetic));
    const auto o = original(x, asset);
    return {true, at(x, o, moment, &cut, true), {}};
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
Outcome<std::vector<ConditionBoundAssetCapacitySegment>>
read_production_facility_capacity_window(
    const ProductionFacilityHistoryContext &x, const StableId &asset,
    const MomentSpan &window) {
  try {
    take(encode_world_moment(window.from, x.facilities.arithmetic));
    take(encode_world_moment(window.to, x.facilities.arithmetic));
    if (cmp(x, window.from, window.to) >= 0 ||
        cmp(x, window.to, take(x.clock.read_current_moment())) > 0)
      fail("FacilityHistoryWindowMismatch", asset);
    const auto o = original(x, asset);
    auto value = at(x, o, window.from, nullptr);
    WorldMoment cursor = window.from;
    std::vector<ConditionBoundAssetCapacitySegment> out;
    for (const auto *change : o.changes) {
      if (cmp(x, change->occurred_at, window.from) <= 0 ||
          cmp(x, change->occurred_at, window.to) >= 0)
        continue;
      if (cmp(x, cursor, change->occurred_at) < 0)
        out.push_back({{cursor, change->occurred_at}, value});
      apply(x, o, value, *change);
      cursor = change->occurred_at;
    }
    if (cmp(x, cursor, window.to) < 0)
      out.push_back({{cursor, window.to}, value});
    return {true, std::move(out), {}};
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
} // namespace hanmo::resident
