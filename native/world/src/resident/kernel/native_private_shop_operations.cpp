#include "resident/kernel/catalog_section_observation.hpp"
#include "native_private_shop_operations.hpp"
#include "exact_complete_world.hpp"
#include "resident/domains/actor_foundation/character_capability.hpp"
#include "resident/domains/commerce/private_shop_capability.hpp"
#include "resident/storage/common_codec.hpp"
#include "world_bundle.hpp"
namespace hanmo::resident {
namespace {
const BoundaryValue *selected_block(const BoundaryValue &scenario,
                                    const char *key, const char *profile) {
  const BoundaryValue *selected = nullptr;
  for (const auto &field : boundary::object(scenario, "scenario"))
    if (field.first == key) {
      if (selected)
        boundary::fail("DuplicateShopOperationSelection", key);
      selected = &field.second;
    }
  if (selected) {
    boundary::fields(*selected, {"profile"}, {}, key);
    if (boundary::object(*selected, key).size() != 1 ||
        boundary::get_string(*selected, "profile", key) != profile)
      boundary::unsupported("UnsupportedShopOperationSelection", key);
  }
  return selected;
}
} // namespace
Outcome<Integer> selected_native_political_shop_licensing_version(
    const CatalogIdentity &identity) {
  Integer selected = 0;
  for (const auto &module : identity.module_versions) {
    if (module.first != "political_shop_licensing")
      continue;
    if (selected || (module.second != "1" && module.second != "2"))
      return {false,
              {},
              common_failure("UnsupportedPoliticalShopPairVersion").error};
    selected = module.second == "2" ? 2 : 1;
  }
  return {true, selected, {}};
}
Outcome<NativePrivateShopOperationVersions>
selected_native_private_shop_operation_versions(
    const CatalogIdentity &identity) {
  NativePrivateShopOperationVersions selected;
  for (const auto &row : identity.module_versions) {
    Integer *target = nullptr;
    if (row.first == "private_shops")
      target = &selected.shops;
    else if (row.first == "private_shop_operation_sources")
      target = &selected.sources;
    else if (row.first == "private_shop_operation_capabilities")
      target = &selected.capabilities;
    if (!target)
      continue;
    if (*target ||
        (row.second != "1" && (target != &selected.shops ||
                               (row.second != "2" && row.second != "3"))))
      return {false,
              {},
              common_failure("ShopOperationModuleMismatch", row.first).error};
    *target = row.second == "3" ? 3 : row.second == "2" ? 2 : 1;
  }
  if (bool(selected.sources) != bool(selected.capabilities) ||
      (selected.sources && selected.shops != 2 && selected.shops != 3))
    return {false,
            {},
            common_failure("ShopOperationOriginalModulePairRequired").error};
  return {true, selected, {}};
}
Status
append_native_private_shop_operation_modules(const BoundaryValue &scenario,
                                             CatalogIdentity &identity) {
  try {
    for (const auto &row : identity.module_versions)
      if (row.first == "private_shop_operation_sources" ||
          row.first == "private_shop_operation_capabilities")
        return common_failure("DuplicateShopOperationModule", row.first);
    const bool sources =
        selected_block(scenario, "private_shop_operation_sources",
                       "original_private_shop_operation_sources_v1");
    const bool capabilities =
        selected_block(scenario, "private_shop_operation_capabilities",
                       "purpose_capability_v1");
    if (sources != capabilities)
      return common_failure("ShopOperationOriginalModulePairRequired");
    if (sources) {
      identity.module_versions.emplace_back("private_shop_operation_sources",
                                            "1");
      identity.module_versions.emplace_back(
          "private_shop_operation_capabilities", "1");
    }
    auto selected = selected_native_private_shop_operation_versions(identity);
    return selected.ok ? common_success()
                       : Status{false, {}, std::move(selected.error)};
  } catch (const boundary::DecodeFailure &error) {
    return {false, {}, error.error};
  }
}
Status
validate_native_private_shop_operation_catalog(const PreparedCatalog &catalog) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 152, "PreparedCatalog", "validate_native_private_shop_operation_catalog", &catalog);

  auto selected =
      selected_native_private_shop_operation_versions(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (!selected.value.exact_operations())
    return common_success();
  if (!catalog.exact_time || !exact_complete_selected(*catalog.exact_time) ||
      !catalog.private_shops ||
      catalog.private_shops->version != selected.value.shops ||
      !catalog.actor_foundation || !catalog.household_purchases ||
      !catalog.common)
    return common_failure("ShopOperationOriginalCatalogsRequired", {},
                          ErrorKind::unsupported_capability);
  // The original S4 owner registers and validates its one existing normal-work
  // reducer. This selector neither copies the formula nor chooses a new rule.
  auto valid = validate_character_capability_catalog(*catalog.actor_foundation);
  if (!valid.ok)
    return valid;
  if (!catalog.actor_foundation->capability_purpose_bindings.find(
          "private_shop_operation"))
    return common_failure("ShopOperationOriginalPurposeRequired", {},
                          ErrorKind::unsupported_capability);
  return common_success();
}
Status validate_native_private_shop_operation_layout(const WorldBundle &world) {
  if (!world.catalog)
    return common_failure("ShopOperationCatalogRequired", {},
                          ErrorKind::internal);
  auto valid = validate_native_private_shop_operation_catalog(*world.catalog);
  if (!valid.ok)
    return valid;
  auto selected =
      selected_native_private_shop_operation_versions(world.catalog->identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (bool(world.private_shop_operation_sources) !=
          bool(selected.value.sources) ||
      bool(world.private_shop_operation_capabilities) !=
          bool(selected.value.capabilities) ||
      (world.private_shop_operation_sources &&
       world.private_shop_operation_sources->schema_version != 1) ||
      (world.private_shop_operation_capabilities &&
       world.private_shop_operation_capabilities->schema_version != 1) ||
      (selected.value.exact_operations() &&
       world.private_shops.version != selected.value.shops))
    return common_failure("ShopOperationSelectedRootMismatch");
  return common_success();
}
Status validate_native_private_shop_operation_world(const WorldBundle &world) {
  auto valid = validate_native_private_shop_operation_layout(world);
  if (!valid.ok)
    return valid;
  if (!world.private_shop_operation_sources)
    return common_success();
  auto owner = world.complete_exact_time_bindings();
  if (!owner.ok)
    return {false, {}, std::move(owner.error)};
  const auto *source = owner.value->reads().private_shop_operations;
  if (!source || &source->shops.common != &world.common ||
      &source->shops.state != &world.private_shops ||
      &source->shops.catalog != world.catalog->private_shops.get() ||
      &source->markets != world.catalog->household_purchases.get() ||
      &source->sources != &*world.private_shop_operation_sources ||
      &source->capabilities != &*world.private_shop_operation_capabilities ||
      !world.action_elapsed || &source->elapsed != &*world.action_elapsed)
    return common_failure("ShopOperationSameWorldSourcesRequired", {},
                          ErrorKind::internal);
  valid = validate_private_shop_operation_sources(*source);
  if (!valid.ok)
    return valid;
  return validate_private_shop_operation_capabilities(*source);
}
} // namespace hanmo::resident
