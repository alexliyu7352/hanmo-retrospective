#include "resident/kernel/catalog_section_observation.hpp"
// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
#include "economic_facility_provider.hpp"
#include "economic_facility_initial_usage.hpp"
#include "economic_facility_installed_sources.hpp"
#include "resident/facility_claims/facility_physical_validation.hpp"
#include "resident/kernel/native_economic_facility_roots.hpp"
#include "resident/kernel/native_item_craft_state.hpp"
#include <limits>
#include <map>
#include <set>
#include <tuple>
#include <type_traits>
namespace hanmo::resident {
namespace facility_provider {
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
template <class T, class F> Outcome<T> guard(F f) {
  try {
    return {true, f(), {}};
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
// Fresh Installed sources borrow only the real locked originals. No runtime
// holder or applied-business provider is constructed to stand in for missing
// data.
struct InstalledSourceContext {
  const CommonStore &common;
  const EconomicFacilityCatalog &catalog;
  const EconomicFacilitySourcesState &state;
  const ProductionManagementCatalog &production_assets;
  const EstateCatalog &estate_catalog;
  const PrivateShopCatalog *shops_catalog;
  const ProductionFacilityConditionsCatalog &condition_catalog;
  const ProductionFacilityConditionsState &conditions;
  WorldMoment current;
  ExactArithmeticBudget arithmetic;
};
bool rule(const EconomicRuleRef &a, const EconomicRuleRef &b) {
  return std::tie(a.id, a.version, a.catalog_digest) ==
         std::tie(b.id, b.version, b.catalog_digest);
}
bool owner(const Owner &a, const Owner &b) {
  return a.kind == b.kind && a.id == b.id && !a.extra && !b.extra;
}
bool identity(const CatalogIdentity &a, const CatalogIdentity &b) {
  return std::tie(a.content_id, a.content_version, a.content_digest,
                  a.ruleset_id, a.schema_sha256, a.pack_set_digest,
                  a.module_versions, a.artifact_sha256) ==
         std::tie(b.content_id, b.content_version, b.content_digest,
                  b.ruleset_id, b.schema_sha256, b.pack_set_digest,
                  b.module_versions, b.artifact_sha256);
}
bool same_original_estate_seed(const EstateDefinition &a,
                               const EstateDefinition &b) {
  return std::tie(a.id, a.food_item_id, a.inventory_id, a.manager_actor_id,
                  a.output_item_id, a.owner_actor_id, a.place_id, a.wallet_id,
                  a.granary_capacity_per_level, a.output_per_worker,
                  a.palisade_defense_per_guard, a.ration_per_person,
                  a.base_capacity, a.resident_actor_ids) ==
             std::tie(b.id, b.food_item_id, b.inventory_id, b.manager_actor_id,
                      b.output_item_id, b.owner_actor_id, b.place_id,
                      b.wallet_id, b.granary_capacity_per_level,
                      b.output_per_worker, b.palisade_defense_per_guard,
                      b.ration_per_person, b.base_capacity,
                      b.resident_actor_ids) &&
         std::tie(a.initial_facilities.farm, a.initial_facilities.granary,
                  a.initial_facilities.palisade,
                  a.initial_facilities.field_order) ==
             std::tie(b.initial_facilities.farm, b.initial_facilities.granary,
                      b.initial_facilities.palisade,
                      b.initial_facilities.field_order) &&
         a.initial_duties.rows() == b.initial_duties.rows() &&
         std::tie(a.construction.build_ticks, a.construction.material_cost,
                  a.construction.max_level, a.construction.money_cost,
                  a.construction.material_item_id,
                  a.construction.payee_wallet_id) ==
             std::tie(b.construction.build_ticks, b.construction.material_cost,
                      b.construction.max_level, b.construction.money_cost,
                      b.construction.material_item_id,
                      b.construction.payee_wallet_id);
}
template <class Context>
int compare(const WorldMoment &a, const WorldMoment &b, const Context &x) {
  return take(compare_world_moments(a, b, x.arithmetic));
}
template <class Context>
bool same_initialization(const Context &x,
                         const PhysicalFacilityInitialization &a,
                         const PhysicalFacilityInitialization &b) {
  if (a.index() != b.index())
    return false;
  if (const auto *installed = std::get_if<InstalledPhysicalFacility>(&a)) {
    const auto &p = installed->installation_evidence;
    const auto &q =
        std::get<InstalledPhysicalFacility>(b).installation_evidence;
    return std::tie(p.installation_id, p.source_schema, p.world_id,
                    p.original_asset_ref, p.asset_source_key) ==
               std::tie(q.installation_id, q.source_schema, q.world_id,
                        q.original_asset_ref, q.asset_source_key) &&
           identity(p.catalog_identity, q.catalog_identity) &&
           std::tie(p.seed_row_ref.owner_domain, p.seed_row_ref.seed_kind,
                    p.seed_row_ref.seed_row_key,
                    p.seed_row_ref.seed_row_digest) ==
               std::tie(q.seed_row_ref.owner_domain, q.seed_row_ref.seed_kind,
                        q.seed_row_ref.seed_row_key,
                        q.seed_row_ref.seed_row_digest) &&
           rule(p.definition_ref, q.definition_ref) &&
           compare(p.installed_at, q.installed_at, x) == 0 &&
           std::tie(p.initialization_adoption_identity.world_id,
                    p.initialization_adoption_identity.seed_row_key,
                    p.initialization_adoption_identity.original_asset_ref,
                    p.initialization_adoption_identity.installation_id) ==
               std::tie(q.initialization_adoption_identity.world_id,
                        q.initialization_adoption_identity.seed_row_key,
                        q.initialization_adoption_identity.original_asset_ref,
                        q.initialization_adoption_identity.installation_id);
  }
  const auto &p = std::get<ActivatedPhysicalFacility>(a)
                      .original_facility_activation_receipt_ref;
  const auto &q = std::get<ActivatedPhysicalFacility>(b)
                      .original_facility_activation_receipt_ref;
  return p.receipt_id == q.receipt_id && p.source_schema == q.source_schema;
}
const char *slot_name(EconomicFacilitySlotKind kind) {
  switch (kind) {
  case EconomicFacilitySlotKind::bed_space:
    return "bed_space";
  case EconomicFacilitySlotKind::furnace_slot:
    return "furnace_slot";
  case EconomicFacilitySlotKind::workbench_slot:
    return "workbench_slot";
  case EconomicFacilitySlotKind::teaching_seat:
    return "teaching_seat";
  }
  fail("UnknownEconomicFacilitySlot");
}
template <class Context>
const EconomicFacilitySlotDefinition &definition(const Context &x,
                                                 const EconomicRuleRef &ref) {
  const EconomicFacilitySlotDefinition *found = nullptr;
  for (const auto &d : x.state.definitions)
    if (rule(d.definition_ref, ref)) {
      if (found)
        fail("DuplicateFacilityDefinition", ref.id);
      found = &d;
    }
  if (x.state.schema_version != 1 || !found || ref.id.empty() ||
      ref.version <= 0 || ref.catalog_digest.empty())
    fail("FacilityDefinitionMissing", ref.id);
  const EconomicFacilitySlotDefinition *locked = nullptr;
  for (const auto &d : x.catalog.definitions)
    if (rule(d.definition_ref, ref)) {
      if (locked)
        fail("DuplicateFacilityDefinition", ref.id);
      locked = &d;
    }
  if (!locked || !same_economic_facility_definition(*found, *locked))
    fail("EconomicFacilityDefinitionSourceMismatch", ref.id);
  return *found;
}
template <class Context>
const PhysicalServiceFacilityExtension &facility(const Context &x,
                                                 const FacilitySlotRef &slot) {
  const PhysicalServiceFacilityExtension *found = nullptr;
  for (const auto &f : x.state.facilities)
    if (f.facility_id == slot.facility_id) {
      if (found || f.asset_owner_domain != slot.provider_domain)
        fail("DuplicateFacilityIdentity", slot.facility_id);
      found = &f;
    }
  if (!found)
    fail("EconomicFacilityMissing", slot.facility_id);
  if (slot.slot_kind !=
      slot_name(definition(x, found->definition_ref).slot_kind))
    fail("WrongEconomicFacilitySlot", slot.facility_id);
  return *found;
}
FacilitySourceRef source_ref(const EconomicFacilityCapacitySource &s) {
  return {
      {s.slot_ref.provider_domain, "economic_facility_capacity", s.source_id},
      1};
}
FacilityCapacityChange event(const EconomicFacilityCapacitySource &s) {
  FacilityCapacityChange out{
      source_ref(s), s.occurred_at, s.source_revision, {}};
  if (s.previous_source_ref)
    out.previous_source = FacilitySourceRef{*s.previous_source_ref, 1};
  return out;
}
const EconomicFacilityCapacitySource &
source(const EconomicFacilityProviderContext &x, const FacilitySourceRef &ref,
       const FacilitySlotRef &slot) {
  if (ref.source_schema != 1 || ref.source_ref.domain != slot.provider_domain ||
      ref.source_ref.kind != "economic_facility_capacity")
    fail("WrongEconomicFacilitySource", ref.source_ref.id);
  const EconomicFacilityCapacitySource *found = nullptr;
  for (const auto &s : x.state.capacity_sources)
    if (s.source_id == ref.source_ref.id) {
      if (found || s.slot_ref != slot)
        fail("DuplicateFacilityCapacitySource", ref.source_ref.id);
      found = &s;
    }
  if (!found)
    fail("FacilityCapacitySourceMissing", ref.source_ref.id);
  return *found;
}
template <class Context>
std::vector<const EconomicFacilityCapacitySource *>
chain(const Context &x, const FacilitySlotRef &slot) {
  std::vector<const EconomicFacilityCapacitySource *> result;
  for (const auto &s : x.state.capacity_sources)
    if (s.slot_ref == slot) {
      if (s.source_revision != static_cast<Integer>(result.size()) + 1 ||
          s.source_schema != 1)
        fail("FacilityCapacityRevisionGap", s.source_id);
      if (result.empty()) {
        if (s.previous_source_ref ||
            !std::holds_alternative<FacilityInitializedCause>(s.change_cause))
          fail("FacilityCapacityInitialMissing", s.source_id);
      } else if (!s.previous_source_ref ||
                 !same_source_ref(*s.previous_source_ref,
                                  source_ref(*result.back()).source_ref) ||
                 compare(result.back()->occurred_at, s.occurred_at, x) > 0)
        fail("FacilityCapacityPredecessorMismatch", s.source_id);
      result.push_back(&s);
    }
  if (result.empty())
    fail("FacilityCapacityHistoryMissing", slot.facility_id);
  return result;
}
template <class Context>
const EconomicFacilityInitialSeed &
seed(const Context &x, const PhysicalFacilityInstalledEvidence &e) {
  const EconomicFacilityInitialSeed *found = nullptr;
  for (const auto &s : x.catalog.initial_rows)
    if (s.row_ref.seed_row_key == e.seed_row_ref.seed_row_key &&
        s.row_ref.owner_domain == e.seed_row_ref.owner_domain) {
      if (found)
        fail("DuplicateFacilitySeedRow", e.seed_row_ref.seed_row_key);
      found = &s;
    }
  if (!found || found->row_ref.seed_kind != e.seed_row_ref.seed_kind ||
      found->row_ref.seed_row_digest != e.seed_row_ref.seed_row_digest ||
      e.seed_row_ref.seed_row_digest.empty())
    fail("FacilityInitialSeedMissing", e.installation_id);
  return *found;
}
const std::vector<FacilityPhysicalUnitDefinition> *
units(const EconomicFacilitySlotDefinition &d) {
  if (auto *v = std::get_if<IndependentFacilityUnits>(&d.physical_layout))
    return &v->unit_definitions;
  if (auto *v = std::get_if<SharedFacilitySafetyZone>(&d.physical_layout))
    return &v->unit_definitions;
  return nullptr;
}
SourceRef installed_ref(const PhysicalServiceFacilityExtension &f) {
  if (auto *i = std::get_if<InstalledPhysicalFacility>(&f.initialization_basis))
    return {f.asset_owner_domain, "economic_facility_installed",
            i->installation_evidence.installation_id};
  fail("FacilityInstalledSourceRequired", f.facility_id);
}
FacilitySourceRef cause_source(const EconomicFacilityProviderContext &x,
                               const EconomicFacilityCapacitySource &s) {
  return std::visit(
      [&](const auto &c) -> FacilitySourceRef {
        using T = std::decay_t<decltype(c)>;
        if constexpr (std::is_same_v<T, FacilityInitializedCause>) {
          const auto &f = facility(x, s.slot_ref);
          if (std::holds_alternative<InstalledPhysicalFacility>(
                  c.initialization_basis))
            return {installed_ref(f), 1};
          const auto &r =
              std::get<ActivatedPhysicalFacility>(c.initialization_basis)
                  .original_facility_activation_receipt_ref;
          return {take(x.applied.read_applied_facility_activation(r))
                      .original_source,
                  r.source_schema};
        } else if constexpr (std::is_same_v<T, FacilityActivatedCause>)
          return {take(x.applied.read_applied_facility_activation(
                           c.activation_receipt_ref))
                      .original_source,
                  c.activation_receipt_ref.source_schema};
        else if constexpr (std::is_same_v<T, FacilityConditionChangedCause>)
          return {c.condition_change_ref, 1};
        else if constexpr (std::is_same_v<T, FacilityOpeningChangedCause>)
          return c.opening_result_ref;
        else if constexpr (std::is_same_v<T, FacilityRightChangedCause>)
          return c.right_result_ref;
        else
          return {c.retention_transition_ref, 1};
      },
      s.change_cause);
}
FacilitySourceRef cause_source(const InstalledSourceContext &x,
                               const EconomicFacilityCapacitySource &s) {
  const auto *cause = std::get_if<FacilityInitializedCause>(&s.change_cause);
  if (!cause || !std::holds_alternative<InstalledPhysicalFacility>(
                    cause->initialization_basis))
    fail("FacilityFreshInstalledCauseRequired", s.source_id);
  return {installed_ref(facility(x, s.slot_ref)), 1};
}
std::size_t original_order(const EconomicFacilityProviderContext &x,
                           const FacilitySourceRef &ref,
                           const FacilitySlotRef &slot) {
  if (ref.source_schema == 1 &&
      ref.source_ref.kind == "economic_facility_installed") {
    const auto &f = facility(x, slot);
    if (!same_source_ref(installed_ref(f), ref.source_ref))
      fail("WrongFacilityInstallationSource", ref.source_ref.id);
    return 0;
  }
  if (ref.source_schema == 1 && ref.source_ref.kind == "facility_retention") {
    const auto *t =
        take(read_facility_retention_transition(x.state, ref, slot));
    return original_order(x, {t->cause_ref, t->cause_schema}, slot);
  }
  if (const auto *f = x.common.state().facts.find(ref.source_ref.id)) {
    if (f->kind != ref.source_ref.kind || f->domain != ref.source_ref.domain)
      fail("FacilityBusinessFactMismatch", ref.source_ref.id);
    const auto &order = x.common.state().facts.order();
    return static_cast<std::size_t>(
               std::find(order.begin(), order.end(), ref.source_ref.id) -
               order.begin()) +
           1;
  }
  return take(x.applied.read_applied_facility_source_order(ref, slot));
}
template <class Context>
std::vector<MomentSpan> intersect(const std::vector<MomentSpan> &a,
                                  const std::vector<MomentSpan> &b,
                                  const Context &x) {
  std::vector<MomentSpan> out;
  for (const auto &p : a) {
    if (compare(p.from, p.to, x) >= 0)
      fail("InvalidFacilityAvailability");
    for (const auto &q : b) {
      if (compare(q.from, q.to, x) >= 0)
        fail("InvalidFacilityAvailability");
      auto from = compare(p.from, q.from, x) > 0 ? p.from : q.from;
      auto to = compare(p.to, q.to, x) < 0 ? p.to : q.to;
      if (compare(from, to, x) < 0)
        out.push_back({from, to});
    }
  }
  std::sort(out.begin(), out.end(), [&](const auto &p, const auto &q) {
    return compare(p.from, q.from, x) < 0;
  });
  for (std::size_t i = 1; i < out.size(); ++i)
    if (compare(out[i - 1].to, out[i].from, x) > 0)
      fail("OverlappingFacilityAvailability");
  return out;
}
std::vector<FacilitySourceRef>
original_dispositions(const EconomicFacilityProviderContext &,
                      const FacilitySourceRef &, const FacilitySlotRef &);
template <class Context>
Integer base_units(const Context &x, const EconomicFacilityCapacitySource &s) {
  const auto &f = facility(x, s.slot_ref);
  const auto &d = definition(x, s.definition_ref);
  return std::visit(
      [&](const auto &b) -> Integer {
        using T = std::decay_t<decltype(b)>;
        if constexpr (std::is_same_v<T, InstalledServiceUnits>) {
          if (!std::holds_alternative<InstalledPhysicalFacility>(
                  f.initialization_basis) ||
              !rule(b.installed_definition_ref, d.definition_ref) ||
              b.units <= 0)
            fail("InvalidInstalledFacilityUnits", s.source_id);
          const auto *original =
              std::get_if<InstalledServiceUnits>(&d.base_binding);
          if (!original || original->units != b.units ||
              !rule(original->installed_definition_ref,
                    b.installed_definition_ref))
            fail("InstalledFacilityBaseMismatch", s.source_id);
          return b.units;
        } else if constexpr (std::is_same_v<T, OriginalConditionBaseSlot>) {
          const auto *original =
              std::get_if<OriginalConditionBaseSlot>(&d.base_binding);
          const auto *initial = x.condition_catalog.seeds.find(b.condition_ref);
          if (!original || !initial ||
              original->condition_ref != b.condition_ref ||
              !rule(original->original_slot_definition_ref,
                    b.original_slot_definition_ref) ||
              initial->original_asset_ref != f.original_asset_ref)
            fail("ConditionFacilityBaseMismatch", s.source_id);
          for (const auto &c : initial->base_capacity_by_slot)
            if (c.slot_kind == s.slot_ref.slot_kind &&
                c.capacity_unit_id == d.capacity_unit_id)
              return c.units;
          fail("ConditionBaseSlotMissing", s.source_id);
        } else if constexpr (std::is_same_v<Context,
                                            EconomicFacilityProviderContext>) {
          auto actual = take(x.applied.read_applied_facility_activation(
              b.activation_receipt_ref));
          if (actual.original_asset_ref != f.original_asset_ref ||
              actual.capacity_unit_id != b.capacity_unit_id ||
              b.capacity_unit_id != d.capacity_unit_id ||
              !rule(actual.definition_ref, s.definition_ref) ||
              compare(actual.occurred_at, s.occurred_at, x) > 0 ||
              s.occurred_at.half_day_tick < actual.active_from_tick ||
              actual.capacity_units <= 0)
            fail("FacilityActivationBaseMismatch", s.source_id);
          return actual.capacity_units;
        } else
          fail("FacilityFreshInstalledBaseRequired", s.source_id);
      },
      s.base_basis);
}
template <class Context>
Integer capacity(const Context &x, const EconomicFacilityCapacitySource &s) {
  auto base = base_units(x, s);
  if (const auto *c =
          std::get_if<FacilityProductionCondition>(&s.condition_basis)) {
    auto actual = take(read_production_facility_condition_revision(
        x.conditions, x.condition_catalog, x.state, c->condition_ref,
        c->condition_revision));
    if (actual.facility_ref != facility(x, s.slot_ref).original_asset_ref ||
        !same_source_ref(actual.last_change_ref,
                         c->condition_change_source_ref))
      fail("FacilityConditionBasisMismatch", s.source_id);
    return take(facility_condition_capacity(
        base, actual.integrity, actual.max_integrity, x.arithmetic));
  }
  const auto &n = std::get<FacilityNotConditioned>(s.condition_basis);
  if (!rule(n.original_asset_definition_ref, s.definition_ref))
    fail("FacilityUnconditionedBasisMismatch", s.source_id);
  for (const auto &p : x.condition_catalog.seeds.rows())
    if (p.second.original_asset_ref ==
        facility(x, s.slot_ref).original_asset_ref)
      fail("FacilityConditionBasisRequired", s.source_id);
  return base;
}
// 依原始开放来源与使用权取交集；来源或策略不匹配时保留原拒绝。
std::vector<MomentSpan> availability(const EconomicFacilityProviderContext &x,
                                     const EconomicFacilityCapacitySource &s,
                                     const MomentSpan &window) {
  const auto &f = facility(x, s.slot_ref);
  const auto &d = definition(x, s.definition_ref);
  std::vector<MomentSpan> open;
  if (const auto *o =
          std::get_if<OriginalFacilityOpenRule>(&s.availability_basis)) {
    if (!rule(o->rule_ref, d.availability_policy_ref))
      fail("FacilityOpeningPolicyMismatch", s.source_id);
    const EconomicFacilityOpenRule *locked = nullptr;
    for (const auto &r : x.catalog.opening_rules)
      if (rule(r.rule_ref, o->rule_ref)) {
        if (locked)
          fail("DuplicateFacilityOpeningRule", s.source_id);
        locked = &r;
      }
    if (!locked)
      fail("FacilityOpeningRuleMissing", s.source_id);
    if (o->original_acceptance_ref)
      open = take(x.applied.read_applied_facility_opening(
          *o->original_acceptance_ref, s.slot_ref, window));
    else {
      if (!std::holds_alternative<InstalledPhysicalFacility>(
              f.initialization_basis) &&
          !std::holds_alternative<ActivatedPhysicalFacility>(
              f.initialization_basis))
        fail("FacilityOpeningAcceptanceRequired", s.source_id);
      open = locked->continuously_open
                 ? std::vector<MomentSpan>{window}
                 : intersect(locked->accepted_fixed_intervals, {window}, x);
    }
  } else {
    const auto &accepted_opening =
        std::get<AcceptedFacilityOpeningIntervals>(s.availability_basis);
    auto actual = take(x.applied.read_applied_facility_opening(
        accepted_opening.original_opening_source_ref, s.slot_ref, window));
    auto frozen = intersect(accepted_opening.intervals, {window}, x);
    if (actual.size() != frozen.size())
      fail("FacilityOpeningSourceMismatch", s.source_id);
    for (std::size_t n = 0; n < actual.size(); ++n)
      if (compare(actual[n].from, frozen[n].from, x) ||
          compare(actual[n].to, frozen[n].to, x))
        fail("FacilityOpeningSourceMismatch", s.source_id);
    open = std::move(frozen);
  }
  if (!rule(s.usage_right_basis.usage_policy_ref, d.usage_policy_ref))
    fail("FacilityUsagePolicyMismatch", s.source_id);
  std::vector<MomentSpan> rights;
  bool own = false;
  if (std::holds_alternative<InstalledPhysicalFacility>(
          f.initialization_basis) &&
      s.usage_right_basis.original_right_schema == 1 &&
      same_source_ref(s.usage_right_basis.original_right_ref,
                      installed_ref(f)) &&
      !s.usage_right_basis.original_acceptance_ref) {
    for (const auto &p : x.catalog.usage_policies)
      if (rule(p.rule_ref, d.usage_policy_ref) && p.own_initial_asset_use)
        own = true;
  }
  if (own)
    rights = {window};
  else
    rights = take(x.applied.read_applied_facility_usage_right(
        s.usage_right_basis, f, window));
  return intersect(intersect(open, rights, x), {window}, x);
}
} // namespace facility_provider
using namespace facility_provider;
Status validate_economic_facility_catalog_identity(
    const EconomicFacilityCatalog &catalog, const CatalogIdentity &actual) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 65, "EconomicFacilityCatalog", "validate_economic_facility_catalog_identity", &catalog);

  if (!identity(catalog.identity, actual))
    return common_failure("EconomicFacilityFullCatalogIdentityMismatch");
  return common_success();
}
// 作者准备阶段没有World，先只核真实目录选择及锁内原资产类型。
Status validate_economic_facility_original_shop_catalog(
    const EconomicFacilityCatalog &catalog, const PrivateShopCatalog *shops) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 66, "EconomicFacilityCatalog", "validate_economic_facility_original_shop_catalog", &catalog);

  return guard<Done>([&] {
    if (shops)
      return Done{};
    for (const auto &m : catalog.identity.module_versions)
      if (m.first.starts_with("private_shop") ||
          m.first.starts_with("political_shop_") ||
          m.first.starts_with("task_shop_order"))
        fail("FacilityOriginalBusinessCatalogRequired", m.first);
    for (const auto &d : catalog.definitions)
      if (d.asset_kind == EconomicFacilityAssetKind::business_service_facility)
        fail("UnselectedFacilityBusinessSource", d.definition_ref.id);
    for (const auto &r : catalog.initial_rows)
      if (r.row_ref.owner_domain == "commerce" ||
          r.row_ref.seed_kind ==
              EconomicFacilityAssetKind::business_service_facility ||
          std::holds_alternative<PrivateShopPremises>(r.original_asset_row))
        fail("UnselectedFacilityBusinessSource", r.row_ref.seed_row_key);
    return Done{};
  });
}
namespace facility_provider {
// 此口只证明09没有借未选私铺；世界全域私铺账仍由原 owning 入口反查。
void validate_original_shop_catalog_absence(
    const EconomicFacilityInstallationReadContext &x) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 67, "EconomicFacilityInstallationReadContext", "validate_original_shop_catalog_absence", &x);

  if (x.shops_catalog)
    return;
  take(validate_economic_facility_original_shop_catalog(x.catalog, nullptr));
  auto ref = [](const SourceRef &r) {
    if (r.domain == "private_shops" || r.kind.starts_with("private_shop_") ||
        r.kind.starts_with("PrivateShop") ||
        (r.domain == "commerce" && (r.kind == "economic_facility_installed" ||
                                    r.kind == "economic_facility_capacity" ||
                                    r.kind == "facility_retention")))
      fail("UnselectedFacilityBusinessSource", r.id);
  };
  auto slot = [](const FacilitySlotRef &r) {
    if (r.provider_domain == "commerce")
      fail("UnselectedFacilityBusinessSource", r.facility_id);
  };
  auto definition = [](const EconomicFacilitySlotDefinition &d) {
    if (d.asset_kind == EconomicFacilityAssetKind::business_service_facility)
      fail("UnselectedFacilityBusinessSource", d.definition_ref.id);
  };
  auto installed = [](const PhysicalFacilityInitialization &basis) {
    if (const auto *i = std::get_if<InstalledPhysicalFacility>(&basis)) {
      const auto &e = i->installation_evidence;
      if (e.seed_row_ref.owner_domain == "commerce" ||
          e.seed_row_ref.seed_kind ==
              EconomicFacilityAssetKind::business_service_facility)
        fail("UnselectedFacilityBusinessSource", e.installation_id);
    }
  };
  for (const auto &d : x.state.definitions)
    definition(d);
  for (const auto &f : x.state.facilities) {
    if (f.asset_owner_domain == "commerce")
      fail("UnselectedFacilityBusinessSource", f.facility_id);
    installed(f.initialization_basis);
  }
  for (const auto &c : x.state.capacity_sources) {
    slot(c.slot_ref);
    if (c.previous_source_ref)
      ref(*c.previous_source_ref);
    ref(c.usage_right_basis.original_right_ref);
    if (c.usage_right_basis.original_acceptance_ref)
      ref(*c.usage_right_basis.original_acceptance_ref);
    std::visit(
        [&](const auto &b) {
          using T = std::decay_t<decltype(b)>;
          if constexpr (std::is_same_v<T, FacilityProductionCondition>)
            ref(b.condition_change_source_ref);
        },
        c.condition_basis);
    std::visit(
        [&](const auto &b) {
          using T = std::decay_t<decltype(b)>;
          if constexpr (std::is_same_v<T, OriginalFacilityOpenRule>) {
            if (b.original_acceptance_ref)
              ref(*b.original_acceptance_ref);
          } else
            ref(b.original_opening_source_ref);
        },
        c.availability_basis);
    std::visit(
        [&](const auto &b) {
          using T = std::decay_t<decltype(b)>;
          if constexpr (std::is_same_v<T, FacilityInitializedCause>)
            installed(b.initialization_basis);
          else if constexpr (std::is_same_v<T, FacilityConditionChangedCause>)
            ref(b.condition_change_ref);
          else if constexpr (std::is_same_v<T, FacilityOpeningChangedCause>)
            ref(b.opening_result_ref.source_ref);
          else if constexpr (std::is_same_v<T, FacilityRightChangedCause>)
            ref(b.right_result_ref.source_ref);
          else if constexpr (std::is_same_v<T, FacilityRetentionChangedCause>)
            ref(b.retention_transition_ref);
        },
        c.change_cause);
  }
  for (const auto &b : x.state.physical_bindings) {
    slot(b.slot_ref);
    ref(b.admission_business_source_ref);
    for (const auto &unit : b.physical_unit_refs)
      if (const auto *i = std::get_if<InstalledPhysicalUnitSource>(
              &unit.installation_source_ref))
        ref(i->source_ref);
  }
  for (const auto &t : x.state.retention_transitions) {
    slot(t.slot_ref);
    ref(t.cause_ref);
    for (const auto &r : t.normal_disposition_result_refs)
      ref(r.source_ref);
    for (const auto &entry : t.entries)
      std::visit(
          [&](const auto &e) {
            using T = std::decay_t<decltype(e)>;
            if constexpr (!std::is_same_v<T, FacilityRetentionBegin>)
              ref(e.previous_retention_source_ref);
            if constexpr (std::is_same_v<T, FacilityRetentionClear>)
              ref(e.holder_clearance_source_ref);
            else
              ref(e.holder_occupancy_source_ref);
            if constexpr (std::is_same_v<T, FacilityRetentionContinue>) {
              if (e.clearance_source_ref)
                ref(*e.clearance_source_ref);
            }
            if constexpr (std::is_same_v<T, FacilityRetentionRestoredService>)
              ref(e.actual_resume_result_ref);
            if constexpr (std::is_same_v<T, FacilityRetentionBegin>)
              slot(e.origin_admission_receipt_ref.slot_ref);
          },
          entry);
  }
}
} // namespace facility_provider
Status validate_economic_facility_installation_source(
    const EconomicFacilityInstallationReadContext &x,
    const PhysicalServiceFacilityExtension &f) {
  return guard<Done>([&] {
    validate_original_shop_catalog_absence(x);
    const auto &d = definition(x, f.definition_ref);
    if (f.facility_id.empty() || f.original_asset_ref.empty() ||
        f.place_ref.empty() || d.capacity_unit_id.empty() ||
        d.physical_capacity_identity.empty() || d.max_claim_half_days <= 0 ||
        d.retention_review_halfdays <= 0 ||
        d.retention_review_halfdays > d.max_claim_half_days)
      fail("InvalidEconomicFacilityDefinition", f.facility_id);
    if (const auto *i =
            std::get_if<InstalledPhysicalFacility>(&f.initialization_basis)) {
      const auto &e = i->installation_evidence;
      const auto &s = seed(x, e);
      if (d.asset_kind != s.row_ref.seed_kind || e.source_schema != 1 ||
          e.installation_id.empty() ||
          e.world_id != x.common.state().identity.world_id ||
          !identity(e.catalog_identity, x.catalog.identity) ||
          s.facility_id != f.facility_id ||
          s.row_ref.owner_domain != f.asset_owner_domain ||
          !rule(e.definition_ref, f.definition_ref) ||
          !rule(s.definition_ref, f.definition_ref) ||
          e.original_asset_ref != f.original_asset_ref ||
          s.place_ref != f.place_ref ||
          e.initialization_adoption_identity.world_id != e.world_id ||
          e.initialization_adoption_identity.seed_row_key !=
              e.seed_row_ref.seed_row_key ||
          e.initialization_adoption_identity.original_asset_ref !=
              e.original_asset_ref ||
          e.initialization_adoption_identity.installation_id !=
              e.installation_id ||
          compare(e.installed_at, x.current, x) > 0)
        fail("FacilityInstallationEvidenceMismatch", f.facility_id);
      std::visit(
          [&](const auto &original) {
            using T = std::decay_t<decltype(original)>;
            if constexpr (std::is_same_v<T, ProductionAsset>) {
              const auto *actual = x.production_assets.assets.find(original.id);
              if (!actual ||
                  s.row_ref.seed_kind !=
                      EconomicFacilityAssetKind::production_asset_facility ||
                  f.asset_owner_domain != "local_economy" ||
                  actual->kind != ProductionAssetKind::facility ||
                  original.id != f.original_asset_ref ||
                  actual->id != original.id ||
                  actual->site_id != original.site_id ||
                  actual->source_key != original.source_key ||
                  actual->capacity_units != original.capacity_units ||
                  actual->kind != original.kind ||
                  !owner(actual->owner, original.owner) ||
                  !owner(s.initial_title_owner, original.owner) ||
                  !e.asset_source_key ||
                  *e.asset_source_key != original.source_key)
                fail("FacilityOriginalAssetSeedMismatch", f.facility_id);
            } else if constexpr (std::is_same_v<T, EstateDefinition>) {
              const auto *actual = x.estate_catalog.estates.find(original.id);
              if (!actual ||
                  s.row_ref.seed_kind !=
                      EconomicFacilityAssetKind::estate_service_facility ||
                  f.asset_owner_domain != "estate" ||
                  original.id != f.original_asset_ref ||
                  !same_original_estate_seed(*actual, original) ||
                  f.place_ref != original.place_id ||
                  s.initial_title_owner.kind != Owner::Kind::actor ||
                  s.initial_title_owner.id != original.owner_actor_id ||
                  s.initial_title_owner.extra || e.asset_source_key)
                fail("FacilityOriginalEstateSeedMismatch", f.facility_id);
            } else {
              if (!x.shops_catalog)
                fail("FacilityOriginalBusinessCatalogRequired", f.facility_id);
              const auto *actual = x.shops_catalog->premises.find(original.id);
              if (!actual ||
                  s.row_ref.seed_kind !=
                      EconomicFacilityAssetKind::business_service_facility ||
                  f.asset_owner_domain != "commerce" ||
                  original.id != f.original_asset_ref ||
                  actual->place_id != original.place_id ||
                  f.place_ref != original.place_id ||
                  actual->lessor_actor_id != original.lessor_actor_id ||
                  actual->lessor_wallet_id != original.lessor_wallet_id ||
                  actual->lessor_role_id != original.lessor_role_id ||
                  actual->agreement_dialogue_id !=
                      original.agreement_dialogue_id ||
                  actual->agreement_node_id != original.agreement_node_id ||
                  actual->market_shop_id != original.market_shop_id ||
                  actual->opening_fee != original.opening_fee ||
                  actual->initial_capital != original.initial_capital ||
                  actual->inventory_capacity != original.inventory_capacity ||
                  actual->daily_rent != original.daily_rent ||
                  actual->rent_rate_profile != original.rent_rate_profile ||
                  s.initial_title_owner.kind != Owner::Kind::actor ||
                  s.initial_title_owner.id != original.lessor_actor_id ||
                  s.initial_title_owner.extra || e.asset_source_key)
                fail("FacilityOriginalBusinessSeedMismatch", f.facility_id);
            }
          },
          s.original_asset_row);
    } else
      fail("FacilityInstalledSourceRequired", f.facility_id);
    return Done{};
  });
}
// 初始组织基地只读原安装时已成立的自有使用权，不接空 holder 或 applied
// provider。 完整安装核验仍调用原唯一正文；此口只追加原政策和调用者所问 Owner
// 的精准匹配。
Outcome<const PhysicalServiceFacilityExtension *>
read_economic_facility_initial_owned_usage(
    const EconomicFacilityInstallationReadContext &x, const SourceRef &ref,
    const Owner &expected_owner) {
  return guard<const PhysicalServiceFacilityExtension *>([&] {
    if (ref.domain != "local_economy" ||
        ref.kind != "economic_facility_installed" || ref.id.empty() ||
        expected_owner.id.empty() || expected_owner.extra)
      fail("FacilityInitialOwnedUsageSourceRequired", ref.id);
    take(validate_adopted_economic_facility_definitions(x.common, x.state,
                                                        x.catalog));
    const PhysicalServiceFacilityExtension *found = nullptr;
    for (const auto &f : x.state.facilities) {
      const auto *installed =
          std::get_if<InstalledPhysicalFacility>(&f.initialization_basis);
      if (!installed ||
          installed->installation_evidence.installation_id != ref.id)
        continue;
      if (found || !same_source_ref(installed_ref(f), ref))
        fail("DuplicateFacilityIdentity", ref.id);
      found = &f;
    }
    if (!found)
      fail("FacilityInstalledSourceRequired", ref.id);
    take(validate_economic_facility_installation_source(x, *found));
    for (const auto &other : x.state.facilities)
      if (&other != found && other.facility_id == found->facility_id)
        fail("DuplicateFacilityIdentity", found->facility_id);
    const auto &e =
        std::get<InstalledPhysicalFacility>(found->initialization_basis)
            .installation_evidence;
    const auto &locked = seed(x, e);
    const auto *asset =
        std::get_if<ProductionAsset>(&locked.original_asset_row);
    if (!asset ||
        locked.row_ref.seed_kind !=
            EconomicFacilityAssetKind::production_asset_facility ||
        !owner(asset->owner, expected_owner) ||
        !owner(locked.initial_title_owner, expected_owner))
      fail("FacilityInitialOwnedUsageOwnerMismatch", ref.id);
    const auto &d = definition(x, found->definition_ref);
    const EconomicFacilityUsagePolicy *policy = nullptr;
    for (const auto &candidate : x.catalog.usage_policies)
      if (rule(candidate.rule_ref, d.usage_policy_ref)) {
        if (policy)
          fail("DuplicateFacilityUsagePolicy", ref.id);
        policy = &candidate;
      }
    if (!policy || !policy->own_initial_asset_use)
      fail("FacilityInitialOwnedUsagePolicyRequired", ref.id);
    const FacilitySlotRef slot{found->asset_owner_domain, found->facility_id,
                               slot_name(d.slot_kind)};
    const auto history = chain(x, slot);
    const auto &initial = *history.front();
    const auto *cause =
        std::get_if<FacilityInitializedCause>(&initial.change_cause);
    if (initial.source_schema != 1 || initial.source_id.empty() ||
        initial.source_revision != 1 || initial.previous_source_ref ||
        initial.facility_extension_ref != found->facility_id ||
        !rule(initial.definition_ref, d.definition_ref) ||
        compare(initial.occurred_at, e.installed_at, x) || !cause ||
        !same_initialization(x, cause->initialization_basis,
                             found->initialization_basis) ||
        initial.usage_right_basis.original_right_schema != 1 ||
        !same_source_ref(initial.usage_right_basis.original_right_ref, ref) ||
        initial.usage_right_basis.original_acceptance_ref ||
        !rule(initial.usage_right_basis.usage_policy_ref, d.usage_policy_ref))
      fail("FacilityInitialOwnedUsageBasisMismatch", ref.id);
    return found;
  });
}
Status NativeEconomicFacilitySourceRead::validate_installation(
    const PhysicalServiceFacilityExtension &f) const {
  return guard<Done>([&] {
    const auto &d = definition(x_, f.definition_ref);
    if (f.facility_id.empty() || f.original_asset_ref.empty() ||
        f.place_ref.empty() || d.capacity_unit_id.empty() ||
        d.physical_capacity_identity.empty() || d.max_claim_half_days <= 0 ||
        d.retention_review_halfdays <= 0 ||
        d.retention_review_halfdays > d.max_claim_half_days)
      fail("InvalidEconomicFacilityDefinition", f.facility_id);
    if (std::holds_alternative<InstalledPhysicalFacility>(
            f.initialization_basis)) {
      take(validate_economic_facility_installation_source(
          {x_.common, x_.catalog, x_.state, x_.production_assets,
           x_.estate_catalog, x_.shops_catalog, x_.current, x_.arithmetic},
          f));
    } else {
      const auto &ref =
          std::get<ActivatedPhysicalFacility>(f.initialization_basis)
              .original_facility_activation_receipt_ref;
      auto a = take(x_.applied.read_applied_facility_activation(ref));
      if (a.original_asset_ref != f.original_asset_ref ||
          !rule(a.definition_ref, f.definition_ref) ||
          a.capacity_unit_id != d.capacity_unit_id ||
          compare(a.occurred_at, x_.current, x_) > 0)
        fail("FacilityActivationEvidenceMismatch", f.facility_id);
    }
    return Done{};
  });
}
namespace facility_provider {
template <class Context>
Done validate_capacity_source(const Context &x,
                              const EconomicFacilityCapacitySource &s) {
  const auto &f = facility(x, s.slot_ref);
  if constexpr (std::is_same_v<Context, EconomicFacilityProviderContext>)
    take(NativeEconomicFacilitySourceRead(x).validate_installation(f));
  else
    take(validate_economic_facility_installation_source(
        {x.common, x.catalog, x.state, x.production_assets, x.estate_catalog,
         x.shops_catalog, x.current, x.arithmetic},
        f));
  const auto &d = definition(x, s.definition_ref);
  const auto &original = definition(x, f.definition_ref);
  if (s.source_schema != 1 || s.source_id.empty() || s.source_revision <= 0 ||
      s.facility_extension_ref != f.facility_id ||
      d.physical_capacity_identity != original.physical_capacity_identity ||
      d.capacity_unit_id != original.capacity_unit_id ||
      d.asset_kind != original.asset_kind ||
      d.slot_kind != original.slot_kind ||
      s.definition_ref.id != f.definition_ref.id ||
      compare(s.occurred_at, x.current, x) > 0)
    fail("EconomicFacilitySourceMismatch", s.source_id);
  auto history = chain(x, s.slot_ref);
  if (std::find(history.begin(), history.end(), &s) == history.end())
    fail("EconomicFacilitySourceNotOwned", s.source_id);
  if (const auto *initialized =
          std::get_if<FacilityInitializedCause>(&s.change_cause))
    if (!same_initialization(x, initialized->initialization_basis,
                             f.initialization_basis))
      fail("FacilityInitializationCauseMismatch", s.source_id);
  auto root = cause_source(x, s);
  WorldMoment at;
  bool handled = false;
  if (root.source_ref.kind == "economic_facility_installed") {
    if (s.source_revision != 1 ||
        !same_source_ref(root.source_ref, installed_ref(f)))
      fail("InvalidFacilityInitializationCause", s.source_id);
    at = std::get<InstalledPhysicalFacility>(f.initialization_basis)
             .installation_evidence.installed_at;
    handled = true;
  } else if (root.source_ref.kind == "facility_condition_change" &&
             root.source_schema == 1) {
    const auto *c = x.conditions.changes.find(root.source_ref.id);
    if (!c || c->facility_ref != f.original_asset_ref ||
        c->new_revision == c->old_revision)
      fail("FacilityConditionCauseMissing", s.source_id);
    at = c->occurred_at;
    handled = true;
  } else if (root.source_ref.kind == "facility_retention" &&
             root.source_schema == 1) {
    const auto *t =
        take(read_facility_retention_transition(x.state, root, s.slot_ref));
    at = t->occurred_at;
    handled = true;
  }
  if (!handled) {
    if constexpr (std::is_same_v<Context, EconomicFacilityProviderContext>)
      at = take(x.applied.read_applied_facility_change(root, s.slot_ref));
    else
      fail("FacilityFreshInstalledCauseRequired", s.source_id);
  }
  if (compare(at, s.occurred_at, x))
    fail("FacilityCapacityCauseTimeMismatch", s.source_id);
  auto base = base_units(x, s);
  auto cap = capacity(x, s);
  if (cap < 0 || cap > base)
    fail("FacilityCapacityOutsideBase", s.source_id);
  if (const auto *list = units(d)) {
    if (list->size() != static_cast<std::size_t>(base))
      fail("FacilityUnitRosterBaseMismatch", s.source_id);
    std::set<StableId> keys, positions;
    for (const auto &u : *list)
      if (u.unit_key.empty() || u.physical_position_key.empty() ||
          u.capacity_unit_id != d.capacity_unit_id ||
          !keys.insert(u.unit_key).second ||
          !positions.insert(u.physical_position_key).second ||
          u.allowed_purpose_refs.empty())
        fail("InvalidFacilityUnitRoster", s.source_id);
  }
  if (s.source_revision > 1) {
    const auto &before =
        *history[static_cast<std::size_t>(s.source_revision - 2)];
    const auto &prior = definition(x, before.definition_ref);
    if (!rule(prior.definition_ref, d.definition_ref)) {
      if (!std::holds_alternative<FacilityActivatedCause>(s.change_cause))
        fail("FacilityDefinitionUpgradeSourceRequired", s.source_id);
      const auto *old_units = units(prior);
      const auto *new_units = units(d);
      if (old_units) {
        if (!new_units)
          fail("FacilityLayoutUpgradeRemovedUnits", s.source_id);
        for (const auto &u : *old_units) {
          auto found = std::find_if(
              new_units->begin(), new_units->end(), [&](const auto &v) {
                return v.unit_key == u.unit_key &&
                       v.capacity_unit_id == u.capacity_unit_id &&
                       v.physical_position_key == u.physical_position_key;
              });
          if (found == new_units->end())
            fail("FacilityLayoutUpgradeRemovedUnits", s.source_id);
        }
      }
    }
  }
  return Done{};
}
} // namespace facility_provider
Status NativeEconomicFacilitySourceRead::validate_source(
    const EconomicFacilityCapacitySource &s) const {
  return guard<Done>([&] { return validate_capacity_source(x_, s); });
}
Outcome<EconomicFacilitySlotDefinition>
NativeEconomicFacilitySourceRead::read_facility_slot_definition(
    const FacilitySlotRef &slot) const {
  return guard<EconomicFacilitySlotDefinition>([&] {
    const auto history = chain(x_, slot);
    take(validate_source(*history.back()));
    return definition(x_, history.back()->definition_ref);
  });
}
Outcome<WorldMoment> NativeEconomicFacilitySourceRead::initial_source_at(
    const FacilitySlotRef &slot) const {
  return guard<WorldMoment>([&] {
    const auto history = chain(x_, slot);
    take(validate_source(*history.front()));
    return history.front()->occurred_at;
  });
}
Outcome<FacilitySourceRef> NativeEconomicFacilitySourceRead::capacity_source_at(
    const FacilitySlotRef &slot, const WorldMoment &at) const {
  return guard<FacilitySourceRef>([&] {
    const EconomicFacilityCapacitySource *result = nullptr;
    for (const auto *s : chain(x_, slot)) {
      take(validate_source(*s));
      if (compare(s->occurred_at, at, x_) <= 0)
        result = s;
      else
        break;
    }
    if (!result)
      fail("FacilityCapacityBeforeInstallation", slot.facility_id);
    return source_ref(*result);
  });
}
Outcome<std::vector<FacilityCapacityChange>>
NativeEconomicFacilitySourceRead::enumerate_capacity_changes(
    const FacilitySlotRef &slot, const WorldMoment &from,
    const WorldMoment &through) const {
  return guard<std::vector<FacilityCapacityChange>>([&] {
    if (compare(from, through, x_) > 0)
      fail("InvalidFacilityCapacityWindow", slot.facility_id);
    std::vector<FacilityCapacityChange> out;
    for (const auto *s : chain(x_, slot)) {
      take(validate_source(*s));
      if (compare(s->occurred_at, from, x_) >= 0 &&
          compare(s->occurred_at, through, x_) <= 0)
        out.push_back(event(*s));
    }
    return out;
  });
}
Outcome<std::vector<FacilityCapacityChange>>
NativeEconomicFacilitySourceRead::enumerate_economic_facility_capacity_changes(
    const FacilitySlotRef &slot, const WorldMoment &from,
    const WorldMoment &through) const {
  return enumerate_capacity_changes(slot, from, through);
}
Outcome<std::vector<VerifiedFacilityCapacitySegment>>
NativeEconomicFacilitySourceRead::read_verified_capacity(
    const FacilitySourceRef &ref, const FacilitySlotRef &slot,
    const MomentSpan &requested) const {
  return guard<std::vector<VerifiedFacilityCapacitySegment>>([&] {
    if (compare(requested.from, requested.to, x_) >= 0)
      fail("InvalidFacilityCapacityWindow", slot.facility_id);
    const auto &s = source(x_, ref, slot);
    take(validate_source(s));
    const auto history = chain(x_, slot);
    MomentSpan segment = requested;
    if (compare(segment.from, s.occurred_at, x_) < 0)
      segment.from = s.occurred_at;
    const auto index = static_cast<std::size_t>(s.source_revision);
    if (index < history.size() &&
        compare(segment.to, history[index]->occurred_at, x_) > 0)
      segment.to = history[index]->occurred_at;
    if (compare(segment.from, segment.to, x_) >= 0)
      return std::vector<VerifiedFacilityCapacitySegment>{};
    const auto &f = facility(x_, slot);
    const auto &d = definition(x_, s.definition_ref);
    FacilityCapacitySource2 result;
    result.slot_ref = slot;
    result.place_ref = f.place_ref;
    result.capacity = capacity(x_, s);
    result.capacity_unit_id = d.capacity_unit_id;
    result.capacity_revision = s.source_revision;
    result.availability_intervals = availability(x_, s, segment);
    result.usage_right_ref = s.usage_right_basis.original_right_ref;
    result.source_fact_ref = ref.source_ref;
    result.identity = x_.common.state().identity;
    result.admission_disposition = s.admission_mode;
    return std::vector<VerifiedFacilityCapacitySegment>{
        {segment, std::move(result)}};
  });
}
Outcome<std::vector<VerifiedFacilityCapacitySegment>>
NativeEconomicFacilitySourceRead::read_economic_facility_capacity_source(
    const FacilitySourceRef &ref, const FacilitySlotRef &slot,
    const MomentSpan &span) const {
  return read_verified_capacity(ref, slot, span);
}
Outcome<FacilitySourceRef>
NativeEconomicFacilitySourceRead::read_capacity_reconciliation_source(
    const FacilitySlotRef &slot, const FacilityCapacityChange &change) const {
  return guard<FacilitySourceRef>([&] {
    const auto &s = source(x_, change.source, slot);
    take(validate_source(s));
    auto cause = cause_source(x_, s);
    const FacilityRetentionTransition *retention = nullptr;
    for (const auto &r : x_.state.retention_transitions)
      if (r.slot_ref == slot &&
          compare(r.occurred_at, s.occurred_at, x_) == 0 &&
          (same_source_ref(r.cause_ref, cause.source_ref) ||
           same_source_ref(
               {slot.provider_domain, "facility_retention", r.transition_id},
               cause.source_ref))) {
        if (retention)
          fail("DuplicateFacilityReconciliationRoot", s.source_id);
        retention = &r;
      }
    if (retention)
      return FacilitySourceRef{{slot.provider_domain, "facility_retention",
                                retention->transition_id},
                               1};
    return cause;
  });
}
Outcome<FacilityMutationCapacityOrder>
NativeEconomicFacilitySourceRead::compare_mutation_to_capacity_change(
    const FacilitySlotRef &slot, const FacilityClaimMutationReceipt &receipt,
    const FacilityCapacityChange &change) const {
  return guard<FacilityMutationCapacityOrder>([&] {
    auto root = take(read_capacity_reconciliation_source(slot, change));
    if (receipt.source_schema == root.source_schema &&
        same_source_ref(receipt.source_result_ref, root.source_ref)) {
      if (compare(receipt.occurred_at, change.occurred_at, x_))
        fail("FacilityReconciliationTimeMismatch", root.source_ref.id);
      return FacilityMutationCapacityOrder::reconciles;
    }
    auto times = compare(receipt.occurred_at, change.occurred_at, x_);
    if (times)
      return times < 0 ? FacilityMutationCapacityOrder::before
                       : FacilityMutationCapacityOrder::after;
    auto mutation = original_order(
        x_, {receipt.source_result_ref, receipt.source_schema}, slot);
    auto cause = original_order(x_, root, slot);
    if (mutation == cause)
      fail("AmbiguousFacilitySourceAdoptionOrder",
           receipt.source_result_ref.id);
    return mutation < cause ? FacilityMutationCapacityOrder::before
                            : FacilityMutationCapacityOrder::after;
  });
}
Outcome<FacilityCapacityChange>
NativeEconomicFacilitySourceRead::read_capacity_history_base(
    const FacilitySlotRef &slot, const FacilityHistoryBase &base) const {
  return guard<FacilityCapacityChange>([&] {
    auto history = chain(x_, slot);
    if (std::holds_alternative<InitialEmptyFacilityClaims>(base)) {
      take(validate_source(*history.front()));
      return event(*history.front());
    }
    const auto &cut = std::get<VacantPrunedFacilityPrefix>(base);
    auto order = original_order(
        x_, {cut.last_close_result_ref, cut.last_close_source_schema}, slot);
    const EconomicFacilityCapacitySource *found = nullptr;
    for (const auto *s : history) {
      if (compare(s->occurred_at, cut.cut_at, x_) > 0)
        break;
      auto root = take(read_capacity_reconciliation_source(slot, event(*s)));
      if (original_order(x_, root, slot) <= order)
        found = s;
    }
    if (!found)
      fail("FacilityPrunedCapacityBaseMissing", slot.facility_id);
    take(validate_source(*found));
    return event(*found);
  });
}
Outcome<FacilityPhysicalBindingProof>
NativeEconomicFacilitySourceRead::read_physical_binding_proof(
    const StableId &claim) const {
  return guard<FacilityPhysicalBindingProof>([&] {
    const auto *binding = take(read_physical_claim_binding(x_.state, claim));
    const auto &f = facility(x_, binding->slot_ref);
    take(validate_installation(f));
    const auto &d = definition(x_, binding->definition_ref);
    const auto *roster = units(d);
    if (!roster || binding->physical_unit_refs.empty() ||
        binding->admission_source_schema <= 0 ||
        !valid_source_ref(binding->admission_business_source_ref))
      fail("InvalidFacilityPhysicalBinding", claim);
    StableId last;
    for (const auto &u : binding->physical_unit_refs) {
      if (u.facility_id != f.facility_id ||
          (!last.empty() && u.unit_key <= last))
        fail("FacilityPhysicalUnitOrderMismatch", claim);
      last = u.unit_key;
      auto found =
          std::find_if(roster->begin(), roster->end(), [&](const auto &v) {
            return v.unit_key == u.unit_key &&
                   v.capacity_unit_id == d.capacity_unit_id;
          });
      if (found == roster->end())
        fail("FacilityPhysicalUnitMissing", claim);
      if (const auto *i = std::get_if<InstalledPhysicalUnitSource>(
              &u.installation_source_ref)) {
        if (i->source_schema != 1 ||
            !same_source_ref(i->source_ref, installed_ref(f)))
          fail("FacilityUnitInstallationMismatch", claim);
        const auto *initial_units = units(definition(x_, f.definition_ref));
        if (!initial_units ||
            std::none_of(initial_units->begin(), initial_units->end(),
                         [&](const auto &v) {
                           return v.unit_key == u.unit_key &&
                                  v.physical_position_key ==
                                      found->physical_position_key;
                         }))
          fail("FacilityUnitNotInstalled", claim);
      } else {
        const auto &a =
            std::get<ActivatedPhysicalUnitSource>(u.installation_source_ref);
        auto actual = take(x_.applied.read_applied_facility_activation(
            a.original_facility_activation_receipt_ref));
        const auto *list = units(definition(x_, actual.definition_ref));
        if (actual.original_asset_ref != f.original_asset_ref ||
            compare(actual.occurred_at, binding->occurred_at, x_) > 0 ||
            !list ||
            std::none_of(list->begin(), list->end(), [&](const auto &v) {
              return v.unit_key == u.unit_key &&
                     v.physical_position_key == found->physical_position_key;
            }))
          fail("FacilityUnitActivationMismatch", claim);
      }
    }
    return FacilityPhysicalBindingProof{*binding, d, f};
  });
}
Outcome<FacilityRetentionProof>
NativeEconomicFacilitySourceRead::read_facility_retention_proof(
    const FacilitySourceRef &ref, const FacilitySlotRef &slot,
    const MomentSpan &span) const {
  return guard<FacilityRetentionProof>([&] {
    const auto *t =
        take(read_facility_retention_transition(x_.state, ref, slot));
    if (compare(t->occurred_at, span.to, x_) > 0 || t->cause_schema <= 0 ||
        !valid_source_ref(t->cause_ref))
      fail("InvalidFacilityRetentionSource", ref.source_ref.id);
    FacilityRetentionProof proof{*t, {}};
    std::set<StableId> previous;
    for (const auto &e : t->entries) {
      take(x_.applied.validate_original_retention_entry(*t, e, x_.claims));
      std::visit(
          [&](const auto &v) {
            if (!previous.insert(v.previous_claim_id).second)
              fail("DuplicateFacilityRetentionPredecessor", t->transition_id);
            const auto &d = definition(x_, facility(x_, slot).definition_ref);
            if constexpr (std::is_same_v<std::decay_t<decltype(v)>,
                                         FacilityRetentionBegin> ||
                          std::is_same_v<std::decay_t<decltype(v)>,
                                         FacilityRetentionContinue>) {
              if (compare(v.retention_window.from, t->occurred_at, x_) ||
                  compare(v.retention_window.from, v.retention_window.to, x_) >=
                      0 ||
                  v.retention_window.to.half_day_tick -
                          v.retention_window.from.half_day_tick >
                      d.retention_review_halfdays)
                fail("InvalidFacilityRetentionWindow", t->transition_id);
              if (units(d))
                proof.physical_bindings.push_back(
                    take(read_physical_binding_proof(v.retained_claim_id)));
            } else if constexpr (std::is_same_v<
                                     std::decay_t<decltype(v)>,
                                     FacilityRetentionRestoredService>) {
              if (units(d))
                proof.physical_bindings.push_back(
                    take(read_physical_binding_proof(v.normal_claim_id)));
            }
          },
          e);
    }
    return proof;
  });
}
Outcome<std::vector<FacilityClaimDependency>>
NativeEconomicFacilitySourceRead::enumerate_claim_dependencies(
    const FacilitySlotRef &slot) const {
  return guard<std::vector<FacilityClaimDependency>>([&] {
    auto result = take(x_.holders.enumerate_holder_claim_dependencies(slot));
    for (const auto &b : x_.state.physical_bindings)
      if (b.slot_ref == slot)
        result.push_back({b.claim_id, {}});
    for (const auto &t : x_.state.retention_transitions)
      if (t.slot_ref == slot)
        for (const auto &e : t.entries)
          std::visit(
              [&](const auto &v) {
                result.push_back({v.origin_claim_id, {}});
                result.push_back({v.previous_claim_id, {}});
                if constexpr (std::is_same_v<std::decay_t<decltype(v)>,
                                             FacilityRetentionBegin>) {
                  result.push_back(
                      {v.retained_claim_id, v.origin_admission_receipt_ref});
                } else if constexpr (std::is_same_v<std::decay_t<decltype(v)>,
                                                    FacilityRetentionContinue>)
                  result.push_back({v.retained_claim_id, {}});
                else if constexpr (std::is_same_v<
                                       std::decay_t<decltype(v)>,
                                       FacilityRetentionRestoredService>)
                  result.push_back({v.normal_claim_id, {}});
              },
              e);
    return result;
  });
}
Status NativeEconomicFacilitySourceRead::validate_claim_admission(
    const FacilityClaim2 &c, const FacilityClaimMutationReceipt &r) const {
  if (c.source_result_ref.kind == "facility_retention") {
    auto p = read_facility_retention_proof(
        {c.source_result_ref, r.source_schema}, c.slot_ref, {c.from, c.until});
    if (!p.ok)
      return {false, {}, p.error};
    return common_success();
  }
  return x_.holders.validate_holder_claim_admission(c, r);
}
Status NativeEconomicFacilitySourceRead::validate_claim_close(
    const FacilityClaim2 &c, FacilityClaimCloseReason why,
    const FacilityClaimMutationReceipt &r) const {
  return guard<Done>([&] {
    std::vector<FacilitySourceRef> originals;
    if (r.source_result_ref.kind == "facility_retention") {
      const auto *t = take(read_facility_retention_transition(
          x_.state, {r.source_result_ref, r.source_schema}, c.slot_ref));
      for (const auto &e : t->entries) {
        bool matches = std::visit(
            [&](const auto &v) { return v.previous_claim_id == c.claim_id; },
            e);
        if (matches) {
          take(x_.applied.validate_original_retention_entry(*t, e, x_.claims));
          return Done{};
        }
      }
      originals = t->normal_disposition_result_refs;
    } else if (r.source_result_ref.kind == "facility_condition_change")
      originals = original_dispositions(
          x_, {r.source_result_ref, r.source_schema}, c.slot_ref);
    else {
      auto direct = x_.holders.validate_holder_claim_close(c, why, r);
      if (direct.ok)
        return Done{};
      originals = take(x_.applied.read_original_facility_dispositions(
          {r.source_result_ref, r.source_schema}, c.slot_ref));
    }
    Integer matches = 0;
    for (const auto &original : originals) {
      auto individual = r;
      individual.source_result_ref = original.source_ref;
      individual.source_schema = original.source_schema;
      individual.closed_claims = {{c.claim_id, why}};
      individual.admitted_claims.clear();
      if (x_.holders.validate_holder_claim_close(c, why, individual).ok)
        ++matches;
    }
    if (matches != 1)
      fail("FacilityOriginalCloseDispositionMismatch", c.claim_id);
    return Done{};
  });
}
Status NativeEconomicFacilitySourceRead::validate_mutation_source(
    const FacilitySlotRef &slot, const FacilityClaimMutationReceipt &r) const {
  return guard<Done>([&] {
    if (r.source_result_ref.kind != "facility_retention" &&
        r.source_result_ref.kind != "facility_condition_change") {
      auto direct = x_.holders.validate_holder_mutation_source(slot, r);
      if (direct.ok)
        return Done{};
    }
    if (r.source_result_ref.kind == "facility_retention") {
      const auto *t = take(read_facility_retention_transition(
          x_.state, {r.source_result_ref, r.source_schema}, slot));
      if (compare(t->occurred_at, r.occurred_at, x_))
        fail("FacilityRetentionMutationTimeMismatch", t->transition_id);
      for (const auto &e : t->entries)
        take(x_.applied.validate_original_retention_entry(*t, e, x_.claims));
    } else if (r.source_result_ref.kind == "facility_condition_change") {
      const auto *change = x_.conditions.changes.find(r.source_result_ref.id);
      if (!change || r.source_schema != 1 ||
          change->facility_ref != facility(x_, slot).original_asset_ref ||
          compare(change->occurred_at, r.occurred_at, x_))
        fail("FacilityConditionMutationMismatch", r.source_result_ref.id);
    } else {
      auto at = take(x_.applied.read_applied_facility_change(
          {r.source_result_ref, r.source_schema}, slot));
      if (compare(at, r.occurred_at, x_))
        fail("FacilityChangeMutationTimeMismatch", r.source_result_ref.id);
    }
    for (const auto &closed : r.closed_claims) {
      auto episode = take(read_facility_claim_episode(
          x_.claims, closed.claim_id, x_.current, x_.arithmetic));
      take(validate_claim_close(episode.original_claim, closed.reason, r));
    }
    for (const auto &c : r.admitted_claims)
      take(validate_claim_admission(c, r));
    return Done{};
  });
}
Status NativeEconomicFacilitySourceRead::validate_pruned_prefix(
    const FacilitySlotRef &slot,
    const VacantPrunedFacilityPrefix &prefix) const {
  return guard<Done>([&] {
    if (prefix.last_removed_revision <= 0 ||
        prefix.last_close_source_schema <= 0 ||
        !valid_source_ref(prefix.last_close_result_ref))
      fail("InvalidFacilityPrunedPrefix", slot.facility_id);
    auto base = take(read_capacity_history_base(slot, prefix));
    if (compare(base.occurred_at, prefix.cut_at, x_) > 0)
      fail("FacilityPrunedSourceTimeMismatch", slot.facility_id);
    auto dependencies = take(enumerate_claim_dependencies(slot));
    for (const auto &d : dependencies)
      if (d.receipt && d.receipt->slot_ref == slot &&
          d.receipt->claim_set_revision <= prefix.last_removed_revision)
        fail("PrunedFacilitySourceStillRequired", d.claim_id);
    original_order(
        x_, {prefix.last_close_result_ref, prefix.last_close_source_schema},
        slot);
    return Done{};
  });
}
Status
NativeEconomicFacilitySourceRead::validate_retention_and_physical_occupancy(
    const FacilitySlotRef &s, const MomentSpan &w,
    const std::vector<const FacilityClaim2 *> &c,
    const FacilityCapacitySource2 &source) const {
  return validate_facility_physical_interval(x_.claims, s, w, c, source, *this,
                                             x_.arithmetic);
}
Outcome<Integer>
NativeEconomicFacilitySourceRead::read_additional_normal_capacity(
    const FacilitySlotRef &s, const MomentSpan &w,
    const std::vector<const FacilityClaim2 *> &c,
    const FacilityCapacitySource2 &source) const {
  return read_facility_additional_normal_capacity(x_.claims, s, w, c, source,
                                                  *this, x_.arithmetic);
}
Outcome<bool> NativeEconomicFacilitySourceRead::is_verified_retained_claim(
    const FacilityClaim2 &c, const MomentSpan &w,
    const FacilityCapacitySource2 &source) const {
  return read_verified_facility_retention_class(x_.claims, c, w, source, *this,
                                                x_.arithmetic);
}
Outcome<bool> NativeEconomicFacilitySourceRead::source_change_affects_claims(
    const FacilityCapacityChange &change,
    const std::vector<const FacilityClaim2 *> &claims) const {
  return guard<bool>([&] {
    for (const auto *c : claims) {
      if (!c)
        fail("NullFacilityClaim");
      if (compare(change.occurred_at, c->until, x_) < 0)
        return true;
    }
    return false;
  });
}
Outcome<bool> NativeEconomicFacilitySourceRead::conflicting_source_visible(
    const ReservationRef &requester, const FacilityClaim2 &c) const {
  return x_.applied.facility_conflict_visible(requester, c);
}
Outcome<EconomicFacilitySlotDefinition>
NativeEconomicFacilitySourceRead::read_facility_slot_definition_at_source(
    const FacilitySourceRef &ref, const FacilitySlotRef &slot) const {
  return guard<EconomicFacilitySlotDefinition>([&] {
    const auto &s = source(x_, ref, slot);
    take(validate_source(s));
    return definition(x_, s.definition_ref);
  });
}
Outcome<std::vector<FacilityPhysicalUnitRef>>
NativeEconomicFacilitySourceRead::read_physical_units_at_capacity_source(
    const FacilitySourceRef &ref, const FacilitySlotRef &slot) const {
  return guard<std::vector<FacilityPhysicalUnitRef>>([&] {
    const auto &s = source(x_, ref, slot);
    take(validate_source(s));
    const auto &f = facility(x_, slot);
    const auto *roster = units(definition(x_, s.definition_ref));
    std::vector<FacilityPhysicalUnitRef> out;
    if (!roster)
      return out;
    const auto history = chain(x_, slot);
    for (const auto &u : *roster) {
      const EconomicFacilityCapacitySource *installed = nullptr;
      for (const auto *prior : history) {
        if (prior->source_revision > s.source_revision)
          break;
        const auto *prior_units = units(definition(x_, prior->definition_ref));
        if (prior_units && std::any_of(prior_units->begin(), prior_units->end(),
                                       [&](const auto &v) {
                                         return u.unit_key == v.unit_key &&
                                                u.physical_position_key ==
                                                    v.physical_position_key;
                                       })) {
          installed = prior;
          break;
        }
      }
      if (!installed)
        fail("FacilityPhysicalUnitOriginMissing", u.unit_key);
      PhysicalUnitInstallationSource origin;
      if (installed->source_revision == 1) {
        if (std::holds_alternative<InstalledPhysicalFacility>(
                f.initialization_basis))
          origin = InstalledPhysicalUnitSource{installed_ref(f), 1};
        else
          origin = ActivatedPhysicalUnitSource{
              std::get<ActivatedPhysicalFacility>(f.initialization_basis)
                  .original_facility_activation_receipt_ref};
      } else if (const auto *a = std::get_if<FacilityActivatedCause>(
                     &installed->change_cause))
        origin = ActivatedPhysicalUnitSource{a->activation_receipt_ref};
      else
        fail("FacilityPhysicalUnitActivationMissing", u.unit_key);
      out.push_back({f.facility_id, std::move(origin), u.unit_key});
    }
    std::sort(out.begin(), out.end(), [](const auto &a, const auto &b) {
      return a.unit_key < b.unit_key;
    });
    return out;
  });
}
namespace facility_provider {
template <class Context>
StableId install_original_facility(CommonStore &common,
                                   EconomicFacilitySourcesState &state,
                                   const Context &x, const StableId &row_key) {
  if (&common != &x.common || &state != &x.state)
    fail("EconomicFacilityInstallCandidateMismatch", row_key);
  take(adopt_locked_economic_facility_definitions_in_candidate(common, state,
                                                               x.catalog));
  const EconomicFacilityInitialSeed *row = nullptr;
  for (const auto &s : x.catalog.initial_rows)
    if (s.row_ref.seed_row_key == row_key) {
      if (row)
        fail("DuplicateFacilitySeedRow", row_key);
      row = &s;
    }
  if (!row)
    fail("FacilityInitialSeedMissing", row_key);
  for (const auto &f : state.facilities)
    if (f.facility_id == row->facility_id)
      fail("FacilityAlreadyInstalled", row->facility_id);
  const auto &d = definition(x, row->definition_ref);
  PhysicalFacilityInstalledEvidence e;
  e.installation_id = take(common.next_id("economic_facility_installed"));
  e.world_id = common.state().identity.world_id;
  e.catalog_identity = x.catalog.identity;
  e.seed_row_ref = row->row_ref;
  e.definition_ref = row->definition_ref;
  e.installed_at = x.current;
  e.original_asset_ref =
      std::visit([](const auto &r) { return r.id; }, row->original_asset_row);
  if (const auto *a = std::get_if<ProductionAsset>(&row->original_asset_row))
    e.asset_source_key = a->source_key;
  e.initialization_adoption_identity = {
      e.world_id, row_key, e.original_asset_ref, e.installation_id};
  PhysicalServiceFacilityExtension f{
      row->facility_id,     row->row_ref.owner_domain,
      e.original_asset_ref, row->definition_ref,
      row->place_ref,       InstalledPhysicalFacility{e}};
  take(validate_economic_facility_installation_source(
      {x.common, x.catalog, x.state, x.production_assets, x.estate_catalog,
       x.shops_catalog, x.current, x.arithmetic},
      f));
  for (const auto &prior : state.facilities) {
    const auto &def = definition(x, prior.definition_ref);
    if (def.physical_capacity_identity == d.physical_capacity_identity)
      fail("DuplicatePhysicalCapacityIdentity", row->facility_id);
  }
  state.facilities.push_back(f);
  FacilitySlotRef slot{f.asset_owner_domain, f.facility_id,
                       slot_name(d.slot_kind)};
  EconomicFacilityCapacitySource s;
  s.source_id = take(common.next_id("economic_facility_capacity"));
  s.slot_ref = slot;
  s.source_revision = 1;
  s.occurred_at = x.current;
  s.facility_extension_ref = f.facility_id;
  s.definition_ref = f.definition_ref;
  s.base_basis = d.base_binding;
  s.condition_basis = FacilityNotConditioned{d.definition_ref};
  for (const auto &p : x.condition_catalog.seeds.rows())
    if (p.second.original_asset_ref == f.original_asset_ref)
      s.condition_basis =
          FacilityProductionCondition{p.first, 1, installed_ref(f)};
  s.availability_basis =
      OriginalFacilityOpenRule{d.availability_policy_ref, {}};
  s.usage_right_basis = {installed_ref(f), 1, d.usage_policy_ref, {}};
  s.admission_mode = AdmitWithinCapacity{};
  s.change_cause = FacilityInitializedCause{f.initialization_basis};
  state.capacity_sources.push_back(s);
  validate_capacity_source(x, state.capacity_sources.back());
  return f.facility_id;
}
} // namespace facility_provider
Outcome<StableId> install_economic_facility_in_candidate(
    CommonStore &common, EconomicFacilitySourcesState &state,
    const EconomicFacilityProviderContext &x, const StableId &row_key) {
  return guard<StableId>(
      [&] { return install_original_facility(common, state, x, row_key); });
}
Status initialize_economic_facilities_in_candidate(
    CommonStore &common, EconomicFacilitySourcesState &state,
    ProductionFacilityConditionsState &conditions,
    const EconomicFacilityInstallationReadContext &original,
    const ProductionFacilityConditionsCatalog &condition_catalog,
    const FacilityClaimsState &claims, Integer selected_version) {
  return guard<Done>([&] {
    validate_original_shop_catalog_absence(original);
    if (selected_version != 1 || state.schema_version != selected_version)
      fail("UnsupportedEconomicFacilitySourceSchema");
    if (&common != &original.common || &state != &original.state)
      fail("EconomicFacilityInstallCandidateMismatch");
    if (!state.definitions.empty() || !state.facilities.empty() ||
        !state.capacity_sources.empty() || !state.physical_bindings.empty() ||
        !state.retention_transitions.empty() ||
        !conditions.conditions.empty() || !conditions.changes.empty() ||
        !claims.claims.empty() || !claims.slot_histories.empty())
      fail("EconomicFacilityFreshInitialStateRequired");
    for (const auto &row : common.state().facts.rows())
      if ((row.second.domain == "local_economy" ||
           row.second.domain == "estate" || row.second.domain == "commerce") &&
          (row.second.kind == "economic_facility_installed" ||
           row.second.kind == "economic_facility_capacity" ||
           row.second.kind == "facility_retention" ||
           row.second.kind == "facility_condition_change"))
        fail("EconomicFacilityOriginalRestoreRequired", row.first);
    InstalledSourceContext x{common,
                             original.catalog,
                             state,
                             original.production_assets,
                             original.estate_catalog,
                             original.shops_catalog,
                             condition_catalog,
                             conditions,
                             original.current,
                             original.arithmetic};
    take(adopt_locked_economic_facility_definitions_in_candidate(common, state,
                                                                 x.catalog));
    for (const auto &row : x.catalog.initial_rows)
      install_original_facility(common, state, x, row.row_ref.seed_row_key);
    take(initialize_production_facility_conditions_in_candidate(
        conditions, condition_catalog, state));
    for (const auto &source : state.capacity_sources)
      validate_capacity_source(x, source);
    return Done{};
  });
}

Outcome<StableId> record_economic_facility_capacity_in_candidate(
    CommonStore &common, EconomicFacilitySourcesState &state,
    const EconomicFacilityProviderContext &x, const FacilitySlotRef &slot,
    const EconomicFacilityChangeCause &cause,
    const FacilityAdmissionDisposition &disposition) {
  return guard<StableId>([&] {
    auto history = chain(x, slot);
    const auto prior = *history.back();
    if (prior.source_revision == std::numeric_limits<Integer>::max())
      fail("FacilityCapacityRevisionOverflow", slot.facility_id);
    EconomicFacilityCapacitySource next = prior;
    next.source_id = take(common.next_id("economic_facility_capacity"));
    next.source_revision = prior.source_revision + 1;
    next.previous_source_ref = source_ref(prior).source_ref;
    next.occurred_at = x.current;
    next.change_cause = cause;
    next.admission_mode = disposition;
    if (auto *c =
            std::get_if<FacilityConditionChangedCause>(&next.change_cause)) {
      const auto *change =
          x.conditions.changes.find(c->condition_change_ref.id);
      if (!change || change->new_revision == change->old_revision ||
          change->facility_ref != facility(x, slot).original_asset_ref)
        fail("FacilityConditionCauseMissing", slot.facility_id);
      const auto *old =
          std::get_if<FacilityProductionCondition>(&prior.condition_basis);
      if (!old || old->condition_revision != change->old_revision)
        fail("FacilityConditionCapacityPredecessorMismatch", slot.facility_id);
      next.condition_basis = FacilityProductionCondition{
          old->condition_ref, change->new_revision, c->condition_change_ref};
      next.occurred_at = change->occurred_at;
    } else if (auto *a =
                   std::get_if<FacilityActivatedCause>(&next.change_cause)) {
      auto actual = take(x.applied.read_applied_facility_activation(
          a->activation_receipt_ref));
      next.definition_ref = actual.definition_ref;
      next.base_basis = ActivatedServiceUnits{a->activation_receipt_ref,
                                              actual.capacity_unit_id};
      next.occurred_at = actual.occurred_at;
      if (next.occurred_at.half_day_tick < actual.active_from_tick)
        fail("FacilityActivationNotYetEffective", actual.original_asset_ref);
    } else if (auto *o = std::get_if<FacilityOpeningChangedCause>(
                   &next.change_cause)) {
      next.availability_basis = AcceptedFacilityOpeningIntervals{
          o->opening_result_ref.source_ref,
          take(x.applied.read_complete_applied_facility_opening(
              o->opening_result_ref.source_ref, slot))};
    } else if (auto *r =
                   std::get_if<FacilityRightChangedCause>(&next.change_cause)) {
      next.usage_right_basis = take(x.applied.read_applied_facility_right_basis(
          r->right_result_ref, slot));
    } else if (std::holds_alternative<FacilityInitializedCause>(
                   next.change_cause))
      fail("FacilityAlreadyInstalled", slot.facility_id);
    state.capacity_sources.push_back(next);
    NativeEconomicFacilitySourceRead provider(x);
    take(provider.validate_source(state.capacity_sources.back()));
    return next.source_id;
  });
}
Status record_facility_physical_binding_in_candidate(
    EconomicFacilitySourcesState &state,
    const EconomicFacilityProviderContext &x, const FacilityClaim2 &claim,
    const FacilityClaimMutationReceipt &receipt,
    const std::vector<FacilityPhysicalUnitRef> &chosen) {
  return guard<Done>([&] {
    NativeEconomicFacilitySourceRead provider(x);
    const EconomicFacilityCapacitySource *at = nullptr;
    for (const auto *s : chain(x, claim.slot_ref))
      if (s->source_revision == claim.capacity_revision)
        at = s;
    if (!at)
      fail("FacilityClaimCapacitySourceMissing", claim.claim_id);
    const auto &d = definition(x, at->definition_ref);
    if (!units(d)) {
      if (!chosen.empty())
        fail("AggregateFacilityHasPhysicalBinding", claim.claim_id);
      return Done{};
    }
    if (claim.units <= 0 ||
        chosen.size() != static_cast<std::size_t>(claim.units) ||
        !same_source_ref(claim.source_result_ref, receipt.source_result_ref))
      fail("FacilityPhysicalAdoptionMismatch", claim.claim_id);
    auto roster = take(provider.read_physical_units_at_capacity_source(
        source_ref(*at), claim.slot_ref));
    StableId last;
    for (const auto &u : chosen) {
      if (!last.empty() && u.unit_key <= last)
        fail("FacilityPhysicalUnitOrderMismatch", claim.claim_id);
      last = u.unit_key;
      const auto match =
          std::find_if(roster.begin(), roster.end(), [&](const auto &r) {
            if (r.facility_id != u.facility_id || r.unit_key != u.unit_key ||
                r.installation_source_ref.index() !=
                    u.installation_source_ref.index())
              return false;
            if (auto *a = std::get_if<InstalledPhysicalUnitSource>(
                    &r.installation_source_ref)) {
              const auto &b = std::get<InstalledPhysicalUnitSource>(
                  u.installation_source_ref);
              return a->source_schema == b.source_schema &&
                     same_source_ref(a->source_ref, b.source_ref);
            }
            const auto &a =
                std::get<ActivatedPhysicalUnitSource>(r.installation_source_ref)
                    .original_facility_activation_receipt_ref;
            const auto &b =
                std::get<ActivatedPhysicalUnitSource>(u.installation_source_ref)
                    .original_facility_activation_receipt_ref;
            return a.receipt_id == b.receipt_id &&
                   a.source_schema == b.source_schema;
          });
      if (match == roster.end())
        fail("FacilityPhysicalUnitOriginMismatch", claim.claim_id);
    }
    for (const auto &b : state.physical_bindings)
      if (b.claim_id == claim.claim_id)
        fail("DuplicatePhysicalClaimBinding", claim.claim_id);
    state.physical_bindings.push_back(
        {claim.claim_id, claim.slot_ref, d.definition_ref, chosen,
         receipt.source_result_ref,
         static_cast<std::uint32_t>(receipt.source_schema),
         receipt.occurred_at});
    take(provider.read_physical_binding_proof(claim.claim_id));
    return Done{};
  });
}
Outcome<StableId> record_facility_retention_in_candidate(
    CommonStore &common, EconomicFacilitySourcesState &state,
    const EconomicFacilityProviderContext &x, const FacilitySlotRef &slot,
    const FacilitySourceRef &cause,
    const std::vector<FacilityRetentionEntry> &entries,
    const std::vector<FacilitySourceRef> &normal) {
  return guard<StableId>([&] {
    if (entries.empty() || cause.source_schema <= 0 ||
        !valid_source_ref(cause.source_ref))
      fail("EmptyFacilityRetentionTransition", slot.facility_id);
    for (const auto &t : state.retention_transitions)
      if (t.slot_ref == slot && t.cause_schema == cause.source_schema &&
          same_source_ref(t.cause_ref, cause.source_ref) &&
          compare(t.occurred_at, x.current, x) == 0)
        fail("DuplicateFacilityRetentionCause", slot.facility_id);
    FacilityRetentionTransition t;
    t.transition_id = take(common.next_id("facility_retention"));
    t.slot_ref = slot;
    t.occurred_at = x.current;
    t.cause_ref = cause.source_ref;
    t.cause_schema = static_cast<std::uint32_t>(cause.source_schema);
    t.entries = entries;
    t.normal_disposition_result_refs = normal;
    StableId last;
    for (const auto &e : entries) {
      auto previous =
          std::visit([](const auto &v) { return v.previous_claim_id; }, e);
      if (!last.empty() && previous <= last)
        fail("FacilityRetentionEntryOrderMismatch", slot.facility_id);
      last = previous;
      take(x.applied.validate_original_retention_entry(t, e, x.claims));
    }
    state.retention_transitions.push_back(t);
    return t.transition_id;
  });
}
Status
validate_economic_facility_sources(const EconomicFacilityProviderContext &x) {
  return guard<Done>([&] {
    take(validate_adopted_economic_facility_definitions(x.common, x.state,
                                                        x.catalog));
    NativeEconomicFacilitySourceRead provider(x);
    std::set<StableId> ids, physical, installations;
    std::set<std::tuple<StableId, StableId, StableId>> seed_keys;
    for (const auto &f : x.state.facilities) {
      take(provider.validate_installation(f));
      if (!ids.insert(f.facility_id).second ||
          !physical
               .insert(
                   definition(x, f.definition_ref).physical_capacity_identity)
               .second)
        fail("DuplicatePhysicalCapacityIdentity", f.facility_id);
      if (const auto *i =
              std::get_if<InstalledPhysicalFacility>(&f.initialization_basis)) {
        const auto &e = i->installation_evidence;
        if (!installations.insert(e.installation_id).second ||
            !seed_keys
                 .emplace(e.seed_row_ref.owner_domain,
                          e.seed_row_ref.seed_row_key, e.original_asset_ref)
                 .second)
          fail("DuplicateFacilityInstallation", f.facility_id);
      }
      FacilitySlotRef slot{
          f.asset_owner_domain, f.facility_id,
          slot_name(definition(x, f.definition_ref).slot_kind)};
      chain(x, slot);
    }
    ids.clear();
    for (const auto &s : x.state.capacity_sources) {
      if (!ids.insert(s.source_id).second)
        fail("DuplicateFacilityCapacitySource", s.source_id);
      take(provider.validate_source(s));
    }
    ids.clear();
    for (const auto &b : x.state.physical_bindings) {
      if (!ids.insert(b.claim_id).second)
        fail("DuplicatePhysicalClaimBinding", b.claim_id);
      take(provider.read_physical_binding_proof(b.claim_id));
      auto e = take(read_facility_claim_episode(x.claims, b.claim_id, x.current,
                                                x.arithmetic));
      auto r =
          take(read_facility_claim_receipt(x.claims, e.admission_receipt_ref));
      if (e.original_claim.slot_ref != b.slot_ref ||
          e.original_claim.units !=
              static_cast<Integer>(b.physical_unit_refs.size()) ||
          !same_source_ref(r.source_result_ref,
                           b.admission_business_source_ref) ||
          r.source_schema != b.admission_source_schema ||
          compare(r.occurred_at, b.occurred_at, x))
        fail("FacilityPhysicalBindingHistoryMismatch", b.claim_id);
    }
    ids.clear();
    for (const auto &t : x.state.retention_transitions) {
      if (!ids.insert(t.transition_id).second)
        fail("DuplicateFacilityRetentionSource", t.transition_id);
      for (const auto &e : t.entries)
        take(x.applied.validate_original_retention_entry(t, e, x.claims));
      bool mutation = false;
      for (const auto &h : x.claims.slot_histories)
        if (h.slot_ref == t.slot_ref)
          for (const auto &r : h.mutations)
            if (r.source_schema == 1 &&
                same_source_ref(r.source_result_ref,
                                {t.slot_ref.provider_domain,
                                 "facility_retention", t.transition_id})) {
              if (mutation || compare(r.occurred_at, t.occurred_at, x))
                fail("FacilityRetentionMutationMismatch", t.transition_id);
              mutation = true;
            }
      if (!mutation)
        fail("FacilityRetentionMutationMissing", t.transition_id);
    }
    take(validate_production_facility_capacity_adoptions(x));
    return Done{};
  });
}
namespace facility_provider {
EconomicFacilityChangeCause
actual_change_cause(const EconomicFacilityProviderContext &x,
                    const FacilitySlotRef &slot, const FacilitySourceRef &ref) {
  if (ref.source_ref.domain == slot.provider_domain &&
      ref.source_ref.kind == "facility_condition_change" &&
      ref.source_schema == 1) {
    const auto *c = x.conditions.changes.find(ref.source_ref.id);
    if (!c || c->facility_ref != facility(x, slot).original_asset_ref ||
        c->new_revision == c->old_revision)
      fail("FacilityConditionCauseMissing", ref.source_ref.id);
    return FacilityConditionChangedCause{ref.source_ref};
  }
  if (ref.source_ref.domain == slot.provider_domain &&
      ref.source_ref.kind == "facility_retention" && ref.source_schema == 1) {
    take(read_facility_retention_transition(x.state, ref, slot));
    return FacilityRetentionChangedCause{ref.source_ref};
  }
  return take(x.applied.read_applied_capacity_change_basis(ref, slot));
}
EconomicFacilityCapacitySource
after_original_source(const EconomicFacilityProviderContext &x,
                      const FacilitySlotRef &slot, const FacilitySourceRef &ref,
                      const EconomicFacilityCapacitySource *old) {
  if (old->source_revision == std::numeric_limits<Integer>::max())
    fail("FacilityCapacityRevisionOverflow", slot.facility_id);
  auto s = *old;
  s.source_id.clear();
  s.source_revision++;
  s.previous_source_ref = source_ref(*old).source_ref;
  s.change_cause = actual_change_cause(x, slot, ref);
  s.admission_mode = AdmitWithinCapacity{};
  s.occurred_at = x.current;
  if (const auto *c =
          std::get_if<FacilityConditionChangedCause>(&s.change_cause)) {
    const auto *change = x.conditions.changes.find(c->condition_change_ref.id);
    const auto *prior =
        std::get_if<FacilityProductionCondition>(&old->condition_basis);
    if (!change || !prior || prior->condition_revision != change->old_revision)
      fail("FacilityConditionCapacityPredecessorMismatch", slot.facility_id);
    s.condition_basis = FacilityProductionCondition{
        prior->condition_ref, change->new_revision, c->condition_change_ref};
    s.occurred_at = change->occurred_at;
  } else if (const auto *a =
                 std::get_if<FacilityActivatedCause>(&s.change_cause)) {
    auto actual = take(
        x.applied.read_applied_facility_activation(a->activation_receipt_ref));
    s.definition_ref = actual.definition_ref;
    s.base_basis = ActivatedServiceUnits{a->activation_receipt_ref,
                                         actual.capacity_unit_id};
    s.occurred_at = actual.occurred_at;
  } else if (const auto *o =
                 std::get_if<FacilityOpeningChangedCause>(&s.change_cause)) {
    s.availability_basis = AcceptedFacilityOpeningIntervals{
        o->opening_result_ref.source_ref,
        take(x.applied.read_complete_applied_facility_opening(
            o->opening_result_ref.source_ref, slot))};
    s.occurred_at = take(x.applied.read_applied_facility_change(ref, slot));
  } else if (const auto *r =
                 std::get_if<FacilityRightChangedCause>(&s.change_cause)) {
    s.usage_right_basis = take(
        x.applied.read_applied_facility_right_basis(r->right_result_ref, slot));
    s.occurred_at = take(x.applied.read_applied_facility_change(ref, slot));
  } else if (const auto *t =
                 std::get_if<FacilityRetentionChangedCause>(&s.change_cause))
    s.occurred_at = take(read_facility_retention_transition(
                             x.state, {t->retention_transition_ref, 1}, slot))
                        ->occurred_at;
  else
    fail("FacilityReinitializationForbidden", slot.facility_id);
  if (compare(s.occurred_at, x.current, x) > 0)
    fail("FutureFacilityOriginalChange", ref.source_ref.id);
  return s;
}
EconomicFacilityCapacitySource
prospective_source(const EconomicFacilityProviderContext &x,
                   const FacilitySlotRef &slot, const FacilitySourceRef &ref) {
  auto s = after_original_source(x, slot, ref, chain(x, slot).back());
  if (compare(s.occurred_at, x.current, x))
    fail("FacilityChangeNotCurrent", ref.source_ref.id);
  return s;
}
EconomicFacilityCapacitySource
historical_prospective_source(const EconomicFacilityProviderContext &x,
                              const FacilitySlotRef &slot,
                              const FacilitySourceRef &ref) {
  const auto cut = original_order(x, ref, slot);
  if (!cut)
    fail("ActualFacilityChangeOrderRequired", ref.source_ref.id);
  const EconomicFacilityCapacitySource *before = nullptr;
  for (const auto *row : chain(x, slot))
    if (original_order(x, cause_source(x, *row), slot) < cut)
      before = row;
  if (!before)
    fail("FacilityOriginalCapacityPredecessorRequired", ref.source_ref.id);
  NativeEconomicFacilitySourceRead provider(x);
  take(provider.validate_source(*before));
  auto value = after_original_source(x, slot, ref, before);
  if (compare(before->occurred_at, value.occurred_at, x) > 0)
    fail("FacilityCapacityOriginalOrderMismatch", ref.source_ref.id);
  return value;
}
std::vector<FacilitySourceRef>
original_dispositions(const EconomicFacilityProviderContext &x,
                      const FacilitySourceRef &root,
                      const FacilitySlotRef &slot) {
  if (root.source_schema == 1 &&
      root.source_ref.kind == "facility_condition_change") {
    const auto *c = x.conditions.changes.find(root.source_ref.id);
    if (!c || c->facility_ref != facility(x, slot).original_asset_ref)
      fail("FacilityConditionCauseMissing", root.source_ref.id);
    return c->interrupted_activity_results;
  }
  return take(x.applied.read_original_facility_dispositions(root, slot));
}
} // namespace facility_provider
Outcome<EconomicFacilitySlotDefinition>
read_economic_facility_definition_after_original_change(
    const EconomicFacilityProviderContext &x, const FacilitySlotRef &slot,
    const FacilitySourceRef &ref) {
  return guard<EconomicFacilitySlotDefinition>([&] {
    const auto change = actual_change_cause(x, slot, ref);
    if (std::holds_alternative<FacilityInitializedCause>(change) ||
        std::holds_alternative<FacilityRetentionChangedCause>(change))
      fail("IndependentFacilityChangeRequired", ref.source_ref.id);
    const auto cut = original_order(x, ref, slot);
    if (!cut)
      fail("ActualFacilityChangeOrderRequired", ref.source_ref.id);
    WorldMoment at;
    if (const auto *c = std::get_if<FacilityConditionChangedCause>(&change)) {
      const auto *row = x.conditions.changes.find(c->condition_change_ref.id);
      if (!row)
        fail("FacilityConditionCauseMissing", ref.source_ref.id);
      at = row->occurred_at;
    } else
      at = take(x.applied.read_applied_facility_change(ref, slot));
    if (compare(at, x.current, x) > 0)
      fail("FutureFacilityOriginalChange", ref.source_ref.id);
    const EconomicFacilityCapacitySource *before = nullptr;
    for (const auto *row : chain(x, slot)) {
      const auto position = original_order(x, cause_source(x, *row), slot);
      if (position >= cut)
        continue;
      if (compare(row->occurred_at, at, x) > 0)
        fail("FacilityCapacityOriginalOrderMismatch", row->source_id);
      before = row;
    }
    if (!before)
      fail("FacilityOriginalCapacityPredecessorRequired", ref.source_ref.id);
    NativeEconomicFacilitySourceRead provider(x);
    take(provider.validate_source(*before));
    if (const auto *a = std::get_if<FacilityActivatedCause>(&change)) {
      const auto actual = take(x.applied.read_applied_facility_activation(
          a->activation_receipt_ref));
      if (actual.original_asset_ref != facility(x, slot).original_asset_ref ||
          compare(actual.occurred_at, at, x))
        fail("FacilityActivationOriginalScopeMismatch", ref.source_ref.id);
      return definition(x, actual.definition_ref);
    }
    if (const auto *c = std::get_if<FacilityConditionChangedCause>(&change)) {
      const auto *row = x.conditions.changes.find(c->condition_change_ref.id);
      const auto *old =
          std::get_if<FacilityProductionCondition>(&before->condition_basis);
      if (!old || row->old_revision != old->condition_revision)
        fail("FacilityConditionOriginalPredecessorMismatch", ref.source_ref.id);
      take(read_production_facility_condition_revision(
          x.conditions, x.condition_catalog, x.state, old->condition_ref,
          row->new_revision));
    }
    return definition(x, before->definition_ref);
  });
}
namespace facility_provider {
EconomicFacilityProspectiveCapacity project_original_capacity(
    const EconomicFacilityProviderContext &x, const FacilitySlotRef &slot,
    const FacilitySourceRef &cause, const MomentSpan &window,
    const EconomicFacilityCapacitySource &s) {
  if (compare(window.from, s.occurred_at, x) < 0 ||
      compare(window.from, window.to, x) >= 0)
    fail("InvalidProspectiveFacilityWindow", slot.facility_id);
  const auto &d = definition(x, s.definition_ref);
  EconomicFacilityProspectiveCapacity out;
  out.slot_ref = slot;
  out.original_change = cause;
  out.previous_capacity_source = {*s.previous_source_ref, 1};
  out.occurred_at = s.occurred_at;
  out.definition_ref = s.definition_ref;
  out.place_ref = facility(x, slot).place_ref;
  out.capacity_unit_id = d.capacity_unit_id;
  out.capacity_revision = s.source_revision;
  out.capacity = capacity(x, s);
  out.availability_intervals = availability(x, s, window);
  out.usage_right_ref = s.usage_right_basis.original_right_ref;
  out.identity = x.common.state().identity;
  NativeEconomicFacilitySourceRead provider(x);
  out.physical_units = take(provider.read_physical_units_at_capacity_source(
      out.previous_capacity_source, slot));
  if (const auto *list = units(d)) {
    if (const auto *activation =
            std::get_if<FacilityActivatedCause>(&s.change_cause))
      for (const auto &unit : *list)
        if (std::none_of(
                out.physical_units.begin(), out.physical_units.end(),
                [&](const auto &old) { return old.unit_key == unit.unit_key; }))
          out.physical_units.push_back(
              {slot.facility_id,
               ActivatedPhysicalUnitSource{activation->activation_receipt_ref},
               unit.unit_key});
    if (out.physical_units.size() != list->size())
      fail("ProspectiveFacilityRosterMismatch", slot.facility_id);
    std::sort(
        out.physical_units.begin(), out.physical_units.end(),
        [](const auto &a, const auto &b) { return a.unit_key < b.unit_key; });
  } else if (!out.physical_units.empty())
    fail("FacilityUpgradeErasedPhysicalLayout", slot.facility_id);
  return out;
}
} // namespace facility_provider
Outcome<EconomicFacilityProspectiveCapacity>
read_prospective_economic_facility_capacity(
    const EconomicFacilityProviderContext &x, const FacilitySlotRef &slot,
    const FacilitySourceRef &cause, const MomentSpan &window) {
  return guard<EconomicFacilityProspectiveCapacity>([&] {
    return project_original_capacity(x, slot, cause, window,
                                     prospective_source(x, slot, cause));
  });
}
Outcome<EconomicFacilityProspectiveCapacity>
read_economic_facility_capacity_at_original_change(
    const EconomicFacilityProviderContext &x, const FacilitySlotRef &slot,
    const FacilitySourceRef &cause, const MomentSpan &window) {
  return guard<EconomicFacilityProspectiveCapacity>([&] {
    return project_original_capacity(
        x, slot, cause, window, historical_prospective_source(x, slot, cause));
  });
}
Outcome<EconomicFacilityExistingCapacity>
read_economic_facility_existing_capacity(
    const EconomicFacilityProviderContext &x, const FacilitySlotRef &slot,
    const FacilitySourceRef &ref, const MomentSpan &window) {
  return guard<EconomicFacilityExistingCapacity>([&] {
    const auto &s = source(x, ref, slot);
    NativeEconomicFacilitySourceRead provider(x);
    take(provider.validate_source(s));
    if (compare(window.from, s.occurred_at, x) < 0 ||
        compare(window.from, window.to, x) >= 0)
      fail("InvalidExistingFacilityWindow", ref.source_ref.id);
    const auto &d = definition(x, s.definition_ref);
    EconomicFacilityExistingCapacity out;
    out.slot_ref = slot;
    out.occurred_at = s.occurred_at;
    out.definition_ref = s.definition_ref;
    out.place_ref = facility(x, slot).place_ref;
    out.capacity_unit_id = d.capacity_unit_id;
    out.capacity_revision = s.source_revision;
    out.capacity = capacity(x, s);
    out.availability_intervals = availability(x, s, window);
    out.usage_right_ref = s.usage_right_basis.original_right_ref;
    out.physical_units =
        take(provider.read_physical_units_at_capacity_source(ref, slot));
    out.identity = x.common.state().identity;
    out.original_capacity_source = ref;
    return out;
  });
}
Outcome<EconomicFacilityExistingCapacity>
read_economic_facility_capacity_before_owner_source(
    const EconomicFacilityProviderContext &x, const FacilitySlotRef &slot,
    const SourceRef &owner, const MomentSpan &window) {
  return guard<EconomicFacilityExistingCapacity>([&] {
    const auto *head = x.common.state().facts.find(owner.id);
    if (!head || head->domain != owner.domain || head->kind != owner.kind)
      fail("FacilityActualOwnerHeadRequired", owner.id);
    const auto cut = original_order(x, {owner, 1}, slot);
    const EconomicFacilityCapacitySource *before = nullptr;
    for (const auto *row : chain(x, slot))
      if (original_order(x, cause_source(x, *row), slot) < cut)
        before = row;
    if (!before)
      fail("FacilityOwnerCapacityPredecessorRequired", owner.id);
    return take(read_economic_facility_existing_capacity(
        x, slot, source_ref(*before), window));
  });
}
namespace facility_provider {
// 两类真实原因共用原半开窗总量算法；准备型不携带未产生的容量版本。
struct PlannedFacilityOccupancy {
  MomentSpan window;
  Integer units = 0;
  bool retained = false;
};
FacilityAdmissionDisposition
planned_admission_mode(const EconomicFacilityProviderContext &x,
                       const FacilitySlotRef &slot,
                       const EconomicFacilitySlotDefinition &def, Integer cap,
                       const FacilitySourceRef &root,
                       const std::optional<FacilitySourceRef> &restriction,
                       const std::vector<PlannedFacilityOccupancy> &future,
                       const std::vector<FacilitySourceRef> &retained_refs) {
  std::vector<WorldMoment> cuts{x.current};
  for (const auto &row : future) {
    if (row.units <= 0 || compare(row.window.from, row.window.to, x) >= 0)
      fail("InvalidFacilityDispositionWindow", slot.facility_id);
    if (compare(row.window.to, x.current, x) <= 0)
      continue;
    if (compare(row.window.from, x.current, x) > 0)
      cuts.push_back(row.window.from);
    cuts.push_back(row.window.to);
  }
  std::sort(cuts.begin(), cuts.end(),
            [&](const auto &a, const auto &b) { return compare(a, b, x) < 0; });
  cuts.erase(std::unique(cuts.begin(), cuts.end(),
                         [&](const auto &a, const auto &b) {
                           return !compare(a, b, x);
                         }),
             cuts.end());
  Integer total = 0, working = 0;
  for (std::size_t n = 0; n + 1 < cuts.size(); ++n) {
    Integer all = 0, normal = 0;
    for (const auto &row : future)
      if (compare(row.window.from, cuts[n], x) <= 0 &&
          compare(cuts[n], row.window.to, x) < 0) {
        if (all > std::numeric_limits<Integer>::max() - row.units)
          fail("FacilityPhysicalUnitsOverflow", slot.facility_id);
        all += row.units;
        if (!row.retained) {
          if (normal > std::numeric_limits<Integer>::max() - row.units)
            fail("FacilityPhysicalUnitsOverflow", slot.facility_id);
          normal += row.units;
        }
      }
    total = std::max(total, all);
    working = std::max(working, normal);
  }
  if (restriction) {
    if (working)
      fail("ClosedFacilityHasNormalService", slot.facility_id);
    return EntireSlotNoNewAdmission{retained_refs, restriction->source_ref,
                                    restriction->source_schema};
  }
  if (!retained_refs.empty()) {
    if (std::holds_alternative<IndependentFacilityUnits>(def.physical_layout)) {
      if (working > cap)
        fail("FacilityNormalCapacityExceeded", slot.facility_id);
      return NormalWithRetentions{retained_refs};
    }
    if (total <= cap)
      return NormalWithRetentions{retained_refs};
    if (working || root.source_ref.kind != "facility_retention")
      fail("FacilityWholeSlotDispositionRequired", slot.facility_id);
    return EntireSlotNoNewAdmission{retained_refs, root.source_ref,
                                    root.source_schema};
  }
  if (total > cap)
    fail("FacilityNormalCapacityExceeded", slot.facility_id);
  return AdmitWithinCapacity{};
}
} // namespace facility_provider
Outcome<FacilityClaimMutationReceipt>
finalize_economic_facility_reconciliation_in_candidate(
    CommonStore &common, EconomicFacilitySourcesState &state,
    EconomicFacilityReconciliationWriteContext &write,
    const FacilitySlotRef &slot, const FacilitySourceRef &original_change,
    const std::vector<FacilityClaimMutationReceipt> &results) {
  return guard<FacilityClaimMutationReceipt>([&] {
    const auto &x = write.read;
    if (&common != &x.common || &state != &x.state ||
        &write.conditions != &x.conditions)
      fail("FacilityCandidateIdentityMismatch", slot.facility_id);
    auto prospective = prospective_source(x, slot, original_change);
    const auto &def = definition(x, prospective.definition_ref);
    std::vector<FacilityRetentionEntry> entries;
    std::vector<FacilitySourceRef> normal, all_results;
    std::set<StableId> closed_ids, admitted_ids;
    std::map<StableId, std::vector<FacilityPhysicalUnitRef>> physical;
    FacilityClaimMutationReceipt merged;
    merged.occurred_at = x.current;
    Integer revision = 0;
    for (const auto &h : x.claims.slot_histories)
      if (h.slot_ref == slot) {
        if (const auto *p = std::get_if<VacantPrunedFacilityPrefix>(&h.base))
          revision = p->last_removed_revision;
        for (const auto &m : h.mutations)
          revision = m.claim_set_revision;
      }
    if (revision == std::numeric_limits<Integer>::max())
      fail("FacilityClaimRevisionOverflow", slot.facility_id);
    merged.claim_set_revision = revision + 1;
    for (const auto &result : results) {
      if (result.source_schema <= 0 ||
          !valid_source_ref(result.source_result_ref) ||
          compare(result.occurred_at, x.current, x))
        fail("FacilityOwnerDispositionSourceMismatch",
             result.source_result_ref.id);
      take(x.holders.validate_holder_mutation_source(slot, result));
      FacilitySourceRef actual{result.source_result_ref, result.source_schema};
      all_results.push_back(actual);
      bool has_retention = false;
      for (const auto &closed : result.closed_claims) {
        if (!closed_ids.insert(closed.claim_id).second)
          fail("DuplicateFacilityDispositionClose", closed.claim_id);
        auto episode = take(read_facility_claim_episode(
            x.claims, closed.claim_id, x.current, x.arithmetic));
        if (episode.original_claim.slot_ref != slot ||
            episode.close_receipt_ref)
          fail("FacilityDispositionClaimNotActive", closed.claim_id);
        take(x.holders.validate_holder_claim_close(episode.original_claim,
                                                   closed.reason, result));
        auto retention =
            take(x.applied.read_original_facility_retention_disposition(
                episode.original_claim, result));
        if (retention) {
          if (std::visit([](const auto &e) { return e.previous_claim_id; },
                         *retention) != closed.claim_id)
            fail("FacilityRetentionDispositionMismatch", closed.claim_id);
          entries.push_back(*retention);
          has_retention = true;
        }
        merged.closed_claims.push_back(closed);
      }
      if (!has_retention)
        normal.push_back(actual);
      for (const auto &admitted : result.admitted_claims) {
        if (admitted.slot_ref != slot ||
            admitted.capacity_revision != prospective.source_revision ||
            !admitted_ids.insert(admitted.claim_id).second)
          fail("FacilityDispositionAdmissionMismatch", admitted.claim_id);
        auto chosen = take(
            x.applied.read_original_facility_admission_units(admitted, result));
        if (units(def) &&
            chosen.size() != static_cast<std::size_t>(admitted.units))
          fail("FacilityDispositionPhysicalUnitsMismatch", admitted.claim_id);
        physical.emplace(admitted.claim_id, std::move(chosen));
        merged.admitted_claims.push_back(admitted);
      }
    }
    std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) {
      return std::visit([](const auto &e) { return e.previous_claim_id; }, a) <
             std::visit([](const auto &e) { return e.previous_claim_id; }, b);
    });
    FacilitySourceRef root = original_change;
    if (!entries.empty()) {
      auto id = take(record_facility_retention_in_candidate(
          common, state, x, slot, original_change, entries, normal));
      root = {{slot.provider_domain, "facility_retention", id}, 1};
    }
    merged.source_result_ref = root.source_ref;
    merged.source_schema = root.source_schema;
    for (auto &c : merged.admitted_claims)
      c.source_result_ref = root.source_ref;
    struct FutureOccupancy {
      const FacilityClaim2 *claim;
      bool retained;
    };
    std::vector<FutureOccupancy> future;
    std::vector<FacilitySourceRef> retained_refs;
    auto account = [&](const FacilityClaim2 &c, bool newly_admitted) {
      if (c.units <= 0 || compare(c.from, c.until, x) >= 0)
        fail("InvalidFacilityDispositionWindow", c.claim_id);
      if (compare(c.until, x.current, x) <= 0)
        return;
      bool retained = false;
      if (newly_admitted) {
        for (const auto &e : entries)
          std::visit(
              [&](const auto &v) {
                if constexpr (std::is_same_v<std::decay_t<decltype(v)>,
                                             FacilityRetentionBegin> ||
                              std::is_same_v<std::decay_t<decltype(v)>,
                                             FacilityRetentionContinue>)
                  if (v.retained_claim_id == c.claim_id)
                    retained = true;
              },
              e);
      } else if (c.source_result_ref.kind == "facility_retention") {
        const auto *t = take(read_facility_retention_transition(
            state, {c.source_result_ref, 1}, slot));
        for (const auto &e : t->entries)
          std::visit(
              [&](const auto &v) {
                if constexpr (std::is_same_v<std::decay_t<decltype(v)>,
                                             FacilityRetentionBegin> ||
                              std::is_same_v<std::decay_t<decltype(v)>,
                                             FacilityRetentionContinue>)
                  if (v.retained_claim_id == c.claim_id)
                    retained = true;
              },
              e);
      }
      if (retained) {
        FacilitySourceRef ref{c.source_result_ref, 1};
        if (std::none_of(retained_refs.begin(), retained_refs.end(),
                         [&](const auto &old) {
                           return same_source_ref(old.source_ref,
                                                  ref.source_ref) &&
                                  old.source_schema == ref.source_schema;
                         }))
          retained_refs.push_back(ref);
      }
      future.push_back({&c, retained});
    };
    for (const auto &c : x.claims.claims)
      if (c.slot_ref == slot && !closed_ids.count(c.claim_id))
        account(c, false);
    for (const auto &c : merged.admitted_claims)
      account(c, true);
    // 同一个原半开窗算法也用于真实 until 准备型。
    std::vector<PlannedFacilityOccupancy> planned;
    for (const auto &row : future)
      planned.push_back({{row.claim->from, row.claim->until},
                         row.claim->units,
                         row.retained});
    const auto cap = capacity(x, prospective);
    const auto restriction = take(
        x.applied.read_applied_whole_slot_restriction(original_change, slot));
    const auto disposition = planned_admission_mode(
        x, slot, def, cap, root, restriction, planned, retained_refs);
    const auto source_id = take(record_economic_facility_capacity_in_candidate(
        common, state, x, slot, prospective.change_cause, disposition));
    FacilitySourceRef capacity_ref{
        {slot.provider_domain, "economic_facility_capacity", source_id}, 1};
    merged.capacity_source_refs = {capacity_ref};
    FacilityClaimReceiptRef receipt{slot, merged.claim_set_revision};
    if (original_change.source_schema == 1 &&
        original_change.source_ref.kind == "facility_condition_change") {
      auto *original =
          write.conditions.changes.find(original_change.source_ref.id);
      if (!original)
        fail("FacilityConditionCauseMissing", original_change.source_ref.id);
      for (const auto &r : all_results)
        if (std::none_of(original->interrupted_activity_results.begin(),
                         original->interrupted_activity_results.end(),
                         [&](const auto &existing) {
                           return same_source_ref(existing.source_ref,
                                                  r.source_ref) &&
                                  existing.source_schema == r.source_schema;
                         }))
          original->interrupted_activity_results.push_back(r);
      original->claim_release_results.push_back(receipt);
    } else
      take(write.applied.bind_original_facility_dispositions(
          original_change, slot, all_results, receipt));
    for (const auto &c : merged.admitted_claims)
      take(record_facility_physical_binding_in_candidate(
          state, x, c, merged, physical.at(c.claim_id)));
    return merged;
  });
}
Outcome<FacilityClaimMutationReceipt>
finalize_economic_facility_until_in_candidate(
    CommonStore &common, EconomicFacilitySourcesState &state,
    EconomicFacilityReconciliationWriteContext &write,
    const FacilityClaimUntilRequest &q,
    const std::vector<PreparedFacilityClaimMutation> &results,
    const EconomicFacilityPreparedDispositionRead &prepared) {
  return guard<FacilityClaimMutationReceipt>([&] {
    const auto &x = write.read;
    const auto &slot = q.due_claim.slot_ref;
    if (&common != &x.common || &state != &x.state ||
        &write.conditions != &x.conditions ||
        !prepared.bound_to(common, x.claims) ||
        compare(q.occurred_at, x.current, x) ||
        compare(q.due_claim.until, x.current, x))
      fail("FacilityUntilCandidateMismatch", q.due_claim.claim_id);
    const auto episode = take(read_facility_claim_episode(
        x.claims, q.due_claim.claim_id, x.current, x.arithmetic));
    if (episode.close_receipt_ref ||
        episode.admission_receipt_ref.slot_ref !=
            q.admission_receipt_ref.slot_ref ||
        episode.admission_receipt_ref.claim_set_revision !=
            q.admission_receipt_ref.claim_set_revision ||
        episode.original_claim.slot_ref != slot ||
        compare(episode.original_claim.until, x.current, x))
      fail("FacilityUntilActualEpisodeRequired", q.due_claim.claim_id);
    const auto prior_capacity = *chain(x, slot).back();
    NativeEconomicFacilitySourceRead reader(x);
    take(reader.validate_source(prior_capacity));
    const auto &def = definition(x, prior_capacity.definition_ref);
    Integer revision = 0;
    for (const auto &h : x.claims.slot_histories)
      if (h.slot_ref == slot) {
        if (const auto *b = std::get_if<VacantPrunedFacilityPrefix>(&h.base))
          revision = b->last_removed_revision;
        for (const auto &m : h.mutations)
          revision = m.claim_set_revision;
      }
    if (revision == std::numeric_limits<Integer>::max())
      fail("FacilityClaimRevisionOverflow", slot.facility_id);
    FacilityClaimMutationReceipt merged;
    merged.claim_set_revision = revision + 1;
    merged.occurred_at = x.current;
    std::set<StableId> closed_ids, admitted_ids;
    std::vector<FacilityRetentionEntry> entries;
    std::vector<FacilitySourceRef> normal, all_results;
    std::map<StableId, std::vector<FacilityPhysicalUnitRef>> physical;
    std::optional<FacilitySourceRef> actual_until;
    for (const auto &r : results) {
      if (r.claim_set_revision != revision + 1 ||
          compare(r.occurred_at, x.current, x))
        fail("FacilityUntilPreparedRevisionMismatch", r.source_result_ref.id);
      take(prepared.validate_prepared_facility_disposition(slot, r));
      const FacilitySourceRef ref{r.source_result_ref, r.source_schema};
      all_results.push_back(ref);
      bool has_retention = false;
      for (const auto &closed : r.closed_claims) {
        if (!closed_ids.insert(closed.claim_id).second)
          fail("DuplicateFacilityDispositionClose", closed.claim_id);
        const auto old = take(read_facility_claim_episode(
            x.claims, closed.claim_id, x.current, x.arithmetic));
        if (old.close_receipt_ref || old.original_claim.slot_ref != slot)
          fail("FacilityUntilClaimNotActive", closed.claim_id);
        if (closed.claim_id == q.due_claim.claim_id) {
          if (actual_until)
            fail("DuplicateFacilityUntilDisposition", closed.claim_id);
          actual_until = ref;
        }
        const auto entry = take(
            prepared.read_prepared_facility_retention(old.original_claim, r));
        if (entry) {
          if (std::visit([](const auto &e) { return e.previous_claim_id; },
                         *entry) != closed.claim_id)
            fail("FacilityRetentionDispositionMismatch", closed.claim_id);
          entries.push_back(*entry);
          has_retention = true;
        }
        merged.closed_claims.push_back(closed);
      }
      if (!has_retention)
        normal.push_back(ref);
      for (const auto &c : r.admitted_claims) {
        if (c.slot_ref != slot || !admitted_ids.insert(c.claim_id).second)
          fail("FacilityUntilAdmissionMismatch", c.claim_id);
        auto units_selected =
            take(prepared.read_prepared_facility_admission_units(c, r));
        if (units(def)
                ? units_selected.size() != static_cast<std::size_t>(c.units)
                : !units_selected.empty())
          fail("FacilityUntilPhysicalSelectionMismatch", c.claim_id);
        physical.emplace(c.claim_id, std::move(units_selected));
      }
    }
    if (!actual_until)
      fail("FacilityUntilTriggerNotClosed", q.due_claim.claim_id);
    std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) {
      return std::visit([](const auto &e) { return e.previous_claim_id; }, a) <
             std::visit([](const auto &e) { return e.previous_claim_id; }, b);
    });
    FacilitySourceRef root = *actual_until;
    if (!entries.empty()) {
      const auto id = take(record_facility_retention_in_candidate(
          common, state, x, slot, *actual_until, entries, normal));
      root = {{slot.provider_domain, "facility_retention", id}, 1};
    }
    std::vector<PlannedFacilityOccupancy> future;
    std::vector<FacilitySourceRef> retained_refs;
    auto remember = [&](const FacilitySourceRef &ref) {
      if (std::none_of(retained_refs.begin(), retained_refs.end(),
                       [&](const auto &s) {
                         return s.source_schema == ref.source_schema &&
                                same_source_ref(s.source_ref, ref.source_ref);
                       }))
        retained_refs.push_back(ref);
    };
    for (const auto &c : x.claims.claims)
      if (c.slot_ref == slot && !closed_ids.count(c.claim_id) &&
          compare(c.until, x.current, x) > 0) {
        bool kept = false;
        if (c.source_result_ref.kind == "facility_retention") {
          const auto *t = take(read_facility_retention_transition(
              state, {c.source_result_ref, 1}, slot));
          for (const auto &e : t->entries)
            std::visit(
                [&](const auto &v) {
                  using T = std::decay_t<decltype(v)>;
                  if constexpr (std::is_same_v<T, FacilityRetentionBegin> ||
                                std::is_same_v<T, FacilityRetentionContinue>)
                    if (v.retained_claim_id == c.claim_id)
                      kept = true;
                },
                e);
        }
        if (kept)
          remember({c.source_result_ref, 1});
        future.push_back({{c.from, c.until}, c.units, kept});
      }
    for (const auto &r : results)
      for (const auto &c : r.admitted_claims) {
        bool kept = false;
        for (const auto &e : entries)
          std::visit(
              [&](const auto &v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, FacilityRetentionBegin> ||
                              std::is_same_v<T, FacilityRetentionContinue>)
                  if (v.retained_claim_id == c.claim_id)
                    kept = true;
              },
              e);
        if (kept)
          remember(root);
        future.push_back({{c.from, c.until}, c.units, kept});
      }
    std::optional<FacilitySourceRef> restriction;
    if (const auto *closed = std::get_if<EntireSlotNoNewAdmission>(
            &prior_capacity.admission_mode))
      restriction = take(x.applied.read_applied_whole_slot_restriction(
          {closed->whole_slot_restriction_source_ref,
           closed->whole_slot_restriction_source_schema},
          slot));
    const auto mode =
        planned_admission_mode(x, slot, def, capacity(x, prior_capacity), root,
                               restriction, future, retained_refs);
    FacilitySourceRef capacity_ref = source_ref(prior_capacity);
    Integer capacity_revision = prior_capacity.source_revision;
    if (!entries.empty()) {
      const auto id = take(record_economic_facility_capacity_in_candidate(
          common, state, x, slot,
          FacilityRetentionChangedCause{root.source_ref}, mode));
      capacity_ref = {{slot.provider_domain, "economic_facility_capacity", id},
                      1};
      capacity_revision = source(x, capacity_ref, slot).source_revision;
    } else if (mode.index() != prior_capacity.admission_mode.index())
      fail("FacilityUntilUnprovenCapacityModeChange", slot.facility_id);
    // 只有原容量已经真实存在后，才从准备型物化正式 Claim2。
    merged.source_result_ref = root.source_ref;
    merged.source_schema = root.source_schema;
    merged.capacity_source_refs = {capacity_ref};
    for (const auto &r : results)
      for (const auto &c : r.admitted_claims)
        merged.admitted_claims.push_back(
            {c.claim_id, c.slot_ref, c.holder, c.from, c.until, c.units,
             capacity_revision, c.usage_right_ref, root.source_ref});
    const FacilityClaimReceiptRef receipt{slot, merged.claim_set_revision};
    take(write.applied.bind_original_facility_dispositions(
        *actual_until, slot, all_results, receipt));
    for (const auto &c : merged.admitted_claims)
      take(record_facility_physical_binding_in_candidate(
          state, x, c, merged, physical.at(c.claim_id)));
    return merged;
  });
}

} // namespace hanmo::resident

namespace hanmo::resident {
namespace facility_provider {
InstalledSourceContext
installed_context(const EconomicFacilityInstallationReadContext &x,
                  const ProductionFacilityConditionsCatalog &catalog,
                  const ProductionFacilityConditionsState &state) {
  return {x.common,         x.catalog,       x.state, x.production_assets,
          x.estate_catalog, x.shops_catalog, catalog, state,
          x.current,        x.arithmetic};
}
bool physical_slot(const FacilitySlotRef &slot) {
  return slot.slot_kind == "bed_space" || slot.slot_kind == "furnace_slot" ||
         slot.slot_kind == "workbench_slot" ||
         slot.slot_kind == "teaching_seat";
}
const EconomicFacilityOpenRule &
installed_open_rule(const EconomicFacilityInstallationReadContext &x,
                    const EconomicFacilitySlotDefinition &d) {
  const EconomicFacilityOpenRule *result = nullptr;
  for (const auto &r : x.catalog.opening_rules)
    if (rule(r.rule_ref, d.availability_policy_ref)) {
      if (result)
        fail("DuplicateFacilityOpeningRule", d.definition_ref.id);
      result = &r;
    }
  if (!result)
    fail("FacilityOpeningRuleMissing", d.definition_ref.id);
  for (std::size_t i = 0; i < result->accepted_fixed_intervals.size(); ++i) {
    const auto &v = result->accepted_fixed_intervals[i];
    if (compare(v.from, v.to, x) >= 0 ||
        (i &&
         compare(result->accepted_fixed_intervals[i - 1].to, v.from, x) > 0))
      fail("InvalidFacilityAvailability", d.definition_ref.id);
  }
  return *result;
}
const EconomicFacilityCapacitySource &
installed_original(const NativeEconomicInstalledFacilityAppliedRead &read,
                   const FacilitySourceRef &ref, const FacilitySlotRef &slot) {
  take(read.validate_bindings());
  const auto &x = read.installation();
  const auto &f = facility(x, slot);
  if (ref.source_schema != 1 ||
      !same_source_ref(ref.source_ref, installed_ref(f)))
    fail("EconomicFacilityDynamicOriginalOwnerRequired", ref.source_ref.id);
  return *chain(x, slot).front();
}
} // namespace facility_provider
NativeEconomicInstalledFacilityAppliedRead::
    NativeEconomicInstalledFacilityAppliedRead(
        EconomicFacilityInstallationReadContext installation,
        const ProductionFacilityConditionsCatalog &catalog,
        const ProductionFacilityConditionsState &conditions,
        const FacilityClaimsState &claims)
    : installation_(installation), condition_catalog_(catalog),
      conditions_(conditions), claims_(claims) {}
Status NativeEconomicInstalledFacilityAppliedRead::validate_bindings() const {
  return guard<Done>([&] {
    const auto &x = installation_;
    validate_original_shop_catalog_absence(x);
    const auto versions =
        take(selected_native_economic_facility_versions(x.catalog.identity));
    if (versions.facilities != 1 || versions.conditions != 1)
      fail("EconomicFacilityInstalledSelectionRequired");
    const auto items =
        take(selected_native_item_craft_versions(x.catalog.identity));
    if (items.craft_jobs || items.item_instances == 3)
      fail("EconomicFacilityCraftAppliedOwnerRequired");
    for (const auto &m : x.catalog.identity.module_versions)
      if (m.first == "production_asset_lifecycle" ||
          m.first == "production_work_applications")
        fail("EconomicFacilityDynamicOriginalOwnerRequired", m.first);
    take(validate_adopted_economic_facility_definitions(x.common, x.state,
                                                        x.catalog));
    if (!x.state.physical_bindings.empty() ||
        !x.state.retention_transitions.empty() || !conditions_.changes.empty())
      fail("EconomicFacilityDynamicOriginalOwnerRequired");
    // 原有限 branch 与反向覆盖分别成立；空表本身从不证明专业模块未选。
    for (const auto &c : claims_.claims)
      if (physical_slot(c.slot_ref))
        fail("EconomicFacilityHolderAppliedOwnerRequired", c.claim_id);
    for (const auto &h : claims_.slot_histories)
      if (physical_slot(h.slot_ref) &&
          (!std::holds_alternative<InitialEmptyFacilityClaims>(h.base) ||
           !h.mutations.empty()))
        fail("EconomicFacilityHolderAppliedOwnerRequired",
             h.slot_ref.facility_id);
    for (const auto &p : x.common.state().facts.rows()) {
      const auto &h = p.second;
      if (h.kind == "economic_facility_installed" ||
          h.kind == "economic_facility_capacity" ||
          h.kind == "facility_retention")
        fail("EconomicFacilityOwnedSourceCannotBeCommonHeader", p.first);
      if (h.kind == "facility_condition_change" ||
          h.kind == "facility_activation" ||
          h.kind == "production_asset_transfer_offered" ||
          h.kind == "production_asset_transfer_accepted" ||
          h.kind == "production_asset_title_transferred" ||
          h.kind == "production_asset_construction_consented")
        fail("EconomicFacilityDynamicOriginalOwnerRequired", p.first);
    }
    if (x.state.facilities.size() != x.catalog.initial_rows.size() ||
        x.state.capacity_sources.size() != x.state.facilities.size())
      fail("EconomicFacilityInstalledCoverageMismatch");
    using BorrowedIds = std::set<std::string_view, std::less<>,
                                 CheckpointAllocator<std::string_view>>;
    using BorrowedSeed = std::pair<std::string_view, std::string_view>;
    BorrowedIds ids, original_ids, installation_ids, sources, physical;
    std::set<BorrowedSeed, std::less<>, CheckpointAllocator<BorrowedSeed>>
        seeds;
    const auto original = installed_context(x, condition_catalog_, conditions_);
    for (const auto &d : x.catalog.definitions)
      if (d.asset_kind !=
              EconomicFacilityAssetKind::production_asset_facility ||
          std::holds_alternative<ActivatedServiceUnits>(d.base_binding))
        fail("EconomicFacilitySpecializedAppliedOwnerRequired",
             d.definition_ref.id);
    for (const auto &f : x.state.facilities) {
      take(validate_economic_facility_installation_source(x, f));
      const auto &d = definition(x, f.definition_ref);
      const auto &e =
          std::get<InstalledPhysicalFacility>(f.initialization_basis)
              .installation_evidence;
      if (!ids.insert(f.facility_id).second ||
          !original_ids.insert(f.original_asset_ref).second ||
          !installation_ids.insert(e.installation_id).second ||
          !physical.insert(d.physical_capacity_identity).second ||
          !seeds
               .emplace(e.seed_row_ref.owner_domain,
                        e.seed_row_ref.seed_row_key)
               .second)
        fail("DuplicateFacilityInstallation", f.facility_id);
      const FacilitySlotRef slot{f.asset_owner_domain, f.facility_id,
                                 slot_name(d.slot_kind)};
      const auto history = chain(x, slot);
      if (history.size() != 1)
        fail("EconomicFacilityDynamicOriginalOwnerRequired", f.facility_id);
      const auto &s = *history.front();
      take(guard<Done>([&] { return validate_capacity_source(original, s); }));
      const auto *open =
          std::get_if<OriginalFacilityOpenRule>(&s.availability_basis);
      if (!sources.insert(s.source_id).second || !open ||
          open->original_acceptance_ref ||
          !rule(open->rule_ref, d.availability_policy_ref) ||
          s.usage_right_basis.original_right_schema != 1 ||
          !same_source_ref(s.usage_right_basis.original_right_ref,
                           installed_ref(f)) ||
          s.usage_right_basis.original_acceptance_ref ||
          !rule(s.usage_right_basis.usage_policy_ref, d.usage_policy_ref) ||
          !std::holds_alternative<AdmitWithinCapacity>(s.admission_mode))
        fail("EconomicFacilityInstalledBasisRequired", s.source_id);
      installed_open_rule(x, d);
      const EconomicFacilityUsagePolicy *policy = nullptr;
      for (const auto &candidate : x.catalog.usage_policies)
        if (rule(candidate.rule_ref, d.usage_policy_ref)) {
          if (policy)
            fail("DuplicateFacilityUsagePolicy", f.facility_id);
          policy = &candidate;
        }
      if (!policy || !policy->own_initial_asset_use)
        fail("EconomicFacilityOriginalUsageOwnerRequired", f.facility_id);
    }
    // 状态头必须等于原唯一 revision reader 导出的完整初装值，不另抄损坏数学。
    if (conditions_.conditions.size() != condition_catalog_.seeds.size())
      fail("FacilityConditionCoverageMismatch");
    BorrowedIds condition_assets;
    for (const auto &p : condition_catalog_.seeds.rows()) {
      if (p.first != p.second.condition_ref ||
          !condition_assets.insert(p.second.original_asset_ref).second)
        fail("DuplicateFacilityConditionAsset", p.first);
      const auto expected = take(read_production_facility_condition_revision(
          conditions_, condition_catalog_, x.state, p.first, 1));
      const auto *actual = conditions_.conditions.find(p.first);
      if (!actual || actual->facility_ref != expected.facility_ref ||
          actual->condition_revision != expected.condition_revision ||
          actual->integrity != expected.integrity ||
          actual->max_integrity != expected.max_integrity ||
          actual->base_asset_capacity_units !=
              expected.base_asset_capacity_units ||
          !rule(actual->damage_policy_ref, expected.damage_policy_ref) ||
          !rule(actual->repair_policy_ref, expected.repair_policy_ref) ||
          !same_source_ref(actual->last_change_ref, expected.last_change_ref) ||
          actual->base_capacity_by_slot.size() !=
              expected.base_capacity_by_slot.size())
        fail("FacilityConditionHeadMismatch", p.first);
      for (std::size_t i = 0; i < actual->base_capacity_by_slot.size(); ++i) {
        const auto &a = actual->base_capacity_by_slot[i];
        const auto &b = expected.base_capacity_by_slot[i];
        if (a.slot_kind != b.slot_kind ||
            a.capacity_unit_id != b.capacity_unit_id || a.units != b.units)
          fail("FacilityConditionBaseMismatch", p.first);
      }
    }
    return Done{};
  });
}
Outcome<EconomicFacilityActivationRead>
NativeEconomicInstalledFacilityAppliedRead::read_applied_facility_activation(
    const FacilityActivationReceiptRef &r) const {
  return {false,
          {},
          common_failure("EconomicFacilityActivationOriginalOwnerRequired",
                         r.receipt_id)
              .error};
}
Outcome<std::vector<MomentSpan>> NativeEconomicInstalledFacilityAppliedRead::
    read_complete_applied_facility_opening(const SourceRef &r,
                                           const FacilitySlotRef &slot) const {
  return guard<std::vector<MomentSpan>>([&] {
    installed_original(*this, {r, 1}, slot);
    const auto &x = installation_;
    const auto &open =
        installed_open_rule(x, definition(x, facility(x, slot).definition_ref));
    if (open.continuously_open)
      fail("EconomicFacilityInstalledOpeningWindowRequired", r.id);
    return open.accepted_fixed_intervals;
  });
}
Outcome<EconomicFacilityUsageRightBasis>
NativeEconomicInstalledFacilityAppliedRead::read_applied_facility_right_basis(
    const FacilitySourceRef &r, const FacilitySlotRef &slot) const {
  return guard<EconomicFacilityUsageRightBasis>(
      [&] { return installed_original(*this, r, slot).usage_right_basis; });
}
Outcome<std::vector<MomentSpan>>
NativeEconomicInstalledFacilityAppliedRead::read_applied_facility_opening(
    const SourceRef &r, const FacilitySlotRef &slot,
    const MomentSpan &window) const {
  return guard<std::vector<MomentSpan>>([&] {
    installed_original(*this, {r, 1}, slot);
    const auto &x = installation_;
    const auto &f = facility(x, slot);
    if (compare(window.from, window.to, x) >= 0 ||
        compare(window.from,
                std::get<InstalledPhysicalFacility>(f.initialization_basis)
                    .installation_evidence.installed_at,
                x) < 0)
      fail("InvalidFacilityAvailability", r.id);
    const auto &open = installed_open_rule(x, definition(x, f.definition_ref));
    if (open.continuously_open)
      return std::vector<MomentSpan>{window};
    return intersect(open.accepted_fixed_intervals, {window}, x);
  });
}
Outcome<std::vector<MomentSpan>>
NativeEconomicInstalledFacilityAppliedRead::read_applied_facility_usage_right(
    const EconomicFacilityUsageRightBasis &basis,
    const PhysicalServiceFacilityExtension &f, const MomentSpan &window) const {
  return guard<std::vector<MomentSpan>>([&] {
    const auto &x = installation_;
    const FacilitySlotRef slot{
        f.asset_owner_domain, f.facility_id,
        slot_name(definition(x, f.definition_ref).slot_kind)};
    const auto &s = installed_original(
        *this, {basis.original_right_ref, basis.original_right_schema}, slot);
    const auto &actual = facility(x, slot);
    if (actual.original_asset_ref != f.original_asset_ref ||
        actual.place_ref != f.place_ref ||
        !rule(actual.definition_ref, f.definition_ref) ||
        !same_initialization(x, actual.initialization_basis,
                             f.initialization_basis) ||
        !rule(basis.usage_policy_ref, s.usage_right_basis.usage_policy_ref) ||
        basis.original_acceptance_ref ||
        compare(window.from, window.to, x) >= 0 ||
        compare(window.from, s.occurred_at, x) < 0)
      fail("EconomicFacilityInstalledUsageMismatch", f.facility_id);
    return std::vector<MomentSpan>{window};
  });
}
Outcome<WorldMoment>
NativeEconomicInstalledFacilityAppliedRead::read_applied_facility_change(
    const FacilitySourceRef &r, const FacilitySlotRef &slot) const {
  return guard<WorldMoment>(
      [&] { return installed_original(*this, r, slot).occurred_at; });
}
Outcome<std::size_t>
NativeEconomicInstalledFacilityAppliedRead::read_applied_facility_source_order(
    const FacilitySourceRef &r, const FacilitySlotRef &slot) const {
  return guard<std::size_t>([&] {
    installed_original(*this, r, slot);
    return std::size_t{0};
  });
}
Status
NativeEconomicInstalledFacilityAppliedRead::validate_original_retention_entry(
    const FacilityRetentionTransition &t, const FacilityRetentionEntry &,
    const FacilityClaimsState &) const {
  return common_failure("EconomicFacilityRetentionOriginalOwnerRequired",
                        t.transition_id);
}
Outcome<EconomicFacilityChangeCause>
NativeEconomicInstalledFacilityAppliedRead::read_applied_capacity_change_basis(
    const FacilitySourceRef &r, const FacilitySlotRef &slot) const {
  return guard<EconomicFacilityChangeCause>(
      [&] { return installed_original(*this, r, slot).change_cause; });
}
Outcome<std::optional<FacilityRetentionEntry>>
NativeEconomicInstalledFacilityAppliedRead::
    read_original_facility_retention_disposition(
        const FacilityClaim2 &c, const FacilityClaimMutationReceipt &) const {
  return {false,
          {},
          common_failure("EconomicFacilityRetentionOriginalOwnerRequired",
                         c.claim_id)
              .error};
}
Outcome<std::vector<FacilityPhysicalUnitRef>>
NativeEconomicInstalledFacilityAppliedRead::
    read_original_facility_admission_units(
        const FacilityClaim2 &c, const FacilityClaimMutationReceipt &) const {
  return {false,
          {},
          common_failure("EconomicFacilityAdmissionOriginalOwnerRequired",
                         c.claim_id)
              .error};
}
Outcome<std::vector<FacilitySourceRef>>
NativeEconomicInstalledFacilityAppliedRead::read_original_facility_dispositions(
    const FacilitySourceRef &r, const FacilitySlotRef &slot) const {
  return guard<std::vector<FacilitySourceRef>>([&] {
    installed_original(*this, r, slot);
    // 真初装没有旧占位或原处置，全部 physical claim/history 已逐根反核。
    return std::vector<FacilitySourceRef>{};
  });
}
Outcome<std::optional<FacilitySourceRef>>
NativeEconomicInstalledFacilityAppliedRead::read_applied_whole_slot_restriction(
    const FacilitySourceRef &r, const FacilitySlotRef &slot) const {
  return guard<std::optional<FacilitySourceRef>>([&] {
    installed_original(*this, r, slot);
    return std::optional<FacilitySourceRef>{};
  });
}
Outcome<bool>
NativeEconomicInstalledFacilityAppliedRead::facility_conflict_visible(
    const ReservationRef &, const FacilityClaim2 &c) const {
  return {false,
          {},
          common_failure("EconomicFacilityConflictOriginalOwnerRequired",
                         c.claim_id)
              .error};
}
} // namespace hanmo::resident
