#include "resident/kernel/catalog_section_observation.hpp"
// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
// 原建设条款与实际设施种子绑定；本目录核验不授予使用或建造处分权。
#include "facility_construction_catalog.hpp"
#include "resident/kernel/original_catalog_identity.hpp"
#include <algorithm>
#include <tuple>

namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
bool same_rule(const EconomicRuleRef &a, const EconomicRuleRef &b) {
  return std::tie(a.id, a.version, a.catalog_digest) ==
         std::tie(b.id, b.version, b.catalog_digest);
}
void rule(const EconomicRuleRef &value, const CatalogIdentity &identity) {
  if (value.id.empty() || value.version <= 0 ||
      value.catalog_digest != identity.content_digest)
    fail("ConstructionLockedRuleRequired", value.id);
}
const EconomicFacilitySlotDefinition &
definition(const EconomicFacilityCatalog &catalog, const EconomicRuleRef &ref) {
  const EconomicFacilitySlotDefinition *found = nullptr;
  for (const auto &value : catalog.definitions)
    if (same_rule(value.definition_ref, ref)) {
      if (found)
        fail("DuplicateConstructionFacilityDefinition", ref.id);
      found = &value;
    }
  if (!found)
    fail("ConstructionFacilityDefinitionMissing", ref.id);
  return *found;
}
// 原独立/共享名册升级必须保留旧物理单位，不能删后以同名单位洗掉占用。
const std::vector<FacilityPhysicalUnitDefinition> *
units(const EconomicFacilitySlotDefinition &definition) {
  if (const auto *value =
          std::get_if<IndependentFacilityUnits>(&definition.physical_layout))
    return &value->unit_definitions;
  if (const auto *value =
          std::get_if<SharedFacilitySafetyZone>(&definition.physical_layout))
    return &value->unit_definitions;
  return nullptr;
}
bool same_unit(const FacilityPhysicalUnitDefinition &a,
               const FacilityPhysicalUnitDefinition &b) {
  if (a.unit_key != b.unit_key || a.capacity_unit_id != b.capacity_unit_id ||
      a.physical_position_key != b.physical_position_key ||
      a.allowed_purpose_refs.size() != b.allowed_purpose_refs.size())
    return false;
  for (std::size_t i = 0; i < a.allowed_purpose_refs.size(); ++i)
    if (!same_rule(a.allowed_purpose_refs[i], b.allowed_purpose_refs[i]))
      return false;
  return true;
}
void upgrade(const ConstructionTerms &terms,
             const EconomicFacilitySlotDefinition &before,
             const EconomicFacilitySlotDefinition &after) {
  if (same_rule(before.definition_ref, after.definition_ref) ||
      before.asset_kind !=
          EconomicFacilityAssetKind::production_asset_facility ||
      after.asset_kind != before.asset_kind ||
      before.slot_kind != after.slot_kind ||
      before.capacity_unit_id != terms.capacity_unit_id ||
      after.capacity_unit_id != terms.capacity_unit_id ||
      before.physical_capacity_identity != after.physical_capacity_identity ||
      before.physical_layout.index() != after.physical_layout.index())
    fail("ConstructionOriginalTargetMismatch", terms.terms_ref.id);
  const auto *old_units = units(before);
  const auto *new_units = units(after);
  if (old_units) {
    if (!new_units || new_units->size() < old_units->size() ||
        static_cast<std::uint64_t>(terms.capacity_units) != new_units->size())
      fail("ConstructionPhysicalRosterMismatch", terms.terms_ref.id);
    for (const auto &old_unit : *old_units) {
      std::size_t matches = 0;
      for (const auto &new_unit : *new_units)
        if (new_unit.unit_key == old_unit.unit_key) {
          ++matches;
          if (!same_unit(old_unit, new_unit))
            fail("ConstructionReplacedOriginalPhysicalUnit", old_unit.unit_key);
        }
      if (matches != 1)
        fail("ConstructionOriginalPhysicalUnitMissing", old_unit.unit_key);
    }
  }
}
// site_id 沿原生产域并不固定是 settlement ID。实际施工地点必须来自
// 同资产、同物理容量身份的原设施 seed；不能猜层级或另用一个同城地点。
void original_place(const EconomicFacilityCatalog &catalog,
                    const ProductionAsset &asset,
                    const EconomicFacilitySlotDefinition &before,
                    const ConstructionTerms &terms) {
  std::size_t matches = 0;
  for (const auto &seed : catalog.initial_rows) {
    const auto *original =
        std::get_if<ProductionAsset>(&seed.original_asset_row);
    if (!original || original->id != asset.id)
      continue;
    const auto &installed = definition(catalog, seed.definition_ref);
    if (installed.physical_capacity_identity !=
        before.physical_capacity_identity)
      continue;
    if (seed.facility_id.empty() ||
        seed.row_ref.owner_domain != "local_economy" ||
        seed.row_ref.seed_kind !=
            EconomicFacilityAssetKind::production_asset_facility ||
        seed.place_ref != terms.place_id ||
        original->site_id != asset.site_id ||
        original->source_key != asset.source_key ||
        original->kind != ProductionAssetKind::facility ||
        installed.asset_kind != before.asset_kind ||
        installed.slot_kind != before.slot_kind ||
        installed.capacity_unit_id != before.capacity_unit_id)
      fail("ConstructionOriginalPlaceMismatch", terms.terms_ref.id);
    ++matches;
  }
  if (matches != 1)
    fail("ConstructionOriginalFacilitySeedRequired", terms.terms_ref.id);
}
} // namespace

SourceRef facility_construction_started_source(const StableId &id) {
  return {"local_economy", "facility_construction_started", id};
}
SourceRef facility_construction_cost_source(const StableId &id) {
  return {"local_economy", "facility_construction_cost", id};
}
SourceRef facility_construction_period_source(const StableId &id) {
  return {"local_economy", "facility_construction_period", id};
}
SourceRef facility_construction_stage_source(const StableId &id) {
  return {"local_economy", "facility_construction_stage", id};
}

Outcome<const ConstructionTerms *>
read_facility_construction_terms(const FacilityConstructionCatalog &catalog,
                                 const EconomicRuleRef &ref) {
  try {
    rule(ref, catalog.identity);
    const ConstructionTerms *value = nullptr;
    for (const auto &candidate : catalog.terms)
      if (candidate.terms_ref.id == ref.id) {
        if (value)
          fail("DuplicateConstructionTerms", ref.id);
        value = &candidate;
      }
    if (catalog.schema_version != 1 || !value ||
        !same_rule(value->terms_ref, ref))
      fail("ConstructionOriginalTermsMissing", ref.id);
    return {true, value, {}};
  } catch (const Failure &failure) {
    return {false, nullptr, failure.error};
  }
}

Status validate_facility_construction_catalog(
    const FacilityConstructionCatalog &catalog, const CatalogIdentity &identity,
    const CommonCatalog &common, const ProductionManagementCatalog &assets,
    const EconomicFacilityCatalog &facilities) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 74, "FacilityConstructionCatalog", "validate_facility_construction_catalog", &catalog);

  try {
    if (catalog.schema_version != 1 || identity.content_digest.empty() ||
        !same_original_catalog_identity(catalog.identity, identity) ||
        !same_original_catalog_identity(facilities.identity, identity))
      fail("ConstructionCatalogIdentityMismatch");
    std::size_t selected = 0, actor_start = 0;
    for (const auto &module : identity.module_versions) {
      if (module.first == "actor_start_context") {
        ++actor_start;
        if (module.second != "3")
          fail("ConstructionOriginalBodyProfileRequired");
      }
      if (module.first == "facility_construction") {
        ++selected;
        if (module.second != "1")
          fail("ConstructionSourceVersionMismatch");
      }
    }
    if (selected != 1 || actor_start != 1)
      fail("ConstructionSourceSelectionRequired");
    for (std::size_t term_index = 0; term_index < catalog.terms.size();
         ++term_index) {
      const auto &terms = catalog.terms[term_index];
      for (std::size_t prior = 0; prior < term_index; ++prior)
        if (catalog.terms[prior].terms_ref.id == terms.terms_ref.id)
          fail("DuplicateConstructionTerms", terms.terms_ref.id);
      rule(terms.terms_ref, identity);
      rule(terms.previous_definition_ref, identity);
      rule(terms.adopted_definition_ref, identity);
      const auto *asset = assets.assets.find(terms.asset_ref);
      const auto *place = common.places.find(terms.place_id);
      if (!asset || asset->kind != ProductionAssetKind::facility ||
          asset->source_key.empty() || !place || terms.duration_halfdays < 1 ||
          terms.materials.empty() || terms.money_cost < 0 ||
          terms.capacity_units < 0 || terms.capacity_unit_id.empty())
        fail("InvalidConstructionTerms", terms.terms_ref.id);
      if ((terms.money_cost > 0 &&
           !common.required_wallets.find(terms.payee_wallet_id)) ||
          (terms.money_cost == 0 && !terms.payee_wallet_id.empty()))
        fail("ConstructionOriginalPayeeRequired", terms.terms_ref.id);
      for (std::size_t i = 0; i < terms.materials.size(); ++i) {
        const auto &material = terms.materials[i];
        if (material.quantity <= 0 || !common.items.find(material.item_id))
          fail("ConstructionOriginalMaterialRequired", material.item_id);
        for (std::size_t prior = 0; prior < i; ++prior)
          if (terms.materials[prior].item_id == material.item_id)
            fail("DuplicateConstructionMaterialRequirement", material.item_id);
      }
      const auto &before =
          definition(facilities, terms.previous_definition_ref);
      upgrade(terms, before,
              definition(facilities, terms.adopted_definition_ref));
      original_place(facilities, *asset, before, terms);
    }
    return common_success();
  } catch (const Failure &failure) {
    return {false, {}, failure.error};
  }
}
} // namespace hanmo::resident
