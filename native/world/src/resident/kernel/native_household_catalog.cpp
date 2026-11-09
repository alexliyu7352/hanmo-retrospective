#include "native_household_catalog.hpp"
#include "exact_time_profile.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/checkpoint/checkpoint_json_decode.hpp"
#include "resident/checkpoint/checkpoint_catalog_json_kind.hpp"
#include "resident/domains/commerce/household_retail.hpp"
#include "resident/domains/local_economy/household_purchase_contract.hpp"
#include "resident/storage/common_codec.hpp"
#include <optional>
namespace hanmo::resident {
namespace {
// 身份准入和冷目录都借同一候选任务；独立解码不另造任务或放宽原拒绝。
Outcome<HouseholdPurchaseCatalog>
decode_original_purchase_catalog(const BoundaryValue &input) {
  std::optional<ScopedCheckpointJsonDecode> scope;
  if (ScopedCheckpointBoundaryDecode::current())
    scope.emplace(CheckpointCatalogJsonKind::household_purchases);
  return decode_household_purchase_catalog(input);
}
// 零售目录使用自己的有限集合登记，不能借用购买或世界状态的额度。
Outcome<HouseholdRetailCatalog>
decode_original_retail_catalog(const BoundaryValue &input) {
  std::optional<ScopedCheckpointJsonDecode> scope;
  if (ScopedCheckpointBoundaryDecode::current())
    scope.emplace(CheckpointCatalogJsonKind::household_retail_settlements);
  return decode_household_retail_catalog(input);
}
struct HouseholdCatalogInput {
  Integer purchases_version = 0;
  const BoundaryValue *purchases = nullptr;
  const BoundaryValue *retail = nullptr;
};
Outcome<HouseholdCatalogInput> read(const BoundaryValue &scenario) {
  try {
    using namespace boundary;
    HouseholdCatalogInput out;
    const auto *dynamic = scenario.find("dynamic_world");
    if (!dynamic)
      return {true, out, {}};
    std::size_t purchases_count = 0, retail_count = 0;
    for (const auto &row : object(*dynamic, "scenario/dynamic_world")) {
      if (row.first == "household_purchases") {
        ++purchases_count;
        out.purchases = &row.second;
      }
      if (row.first == "household_retail_settlements") {
        ++retail_count;
        out.retail = &row.second;
      }
    }
    if (purchases_count > 1 || retail_count > 1)
      fail("DuplicateHouseholdCatalogSelection", "scenario/dynamic_world");
    if (!out.purchases && !out.retail)
      return {true, out, {}};
    auto exact = decode_exact_time_profile(scenario);
    if (!exact.ok)
      return {false, {}, std::move(exact.error)};
    if (!exact.value || !exact_complete_selected(*exact.value))
      unsupported("HouseholdExactProfileRequired", "scenario/dynamic_world");
    if (!out.purchases || !out.retail)
      fail("HouseholdPurchaseRetailCatalogPairRequired",
           "scenario/dynamic_world");
    const auto envelope = [](const BoundaryValue &v, const StableId &path) {
      fields(v, {"version", "catalog"}, {}, path);
      if (object(v, path).size() != 2)
        fail("DuplicateHouseholdCatalogField", path);
      return get_int(v, "version", path);
    };
    out.purchases_version =
        envelope(*out.purchases, "scenario/dynamic_world/household_purchases");
    if (out.purchases_version != 2 && out.purchases_version != 3)
      unsupported("UnsupportedHouseholdPurchasesVersion",
                  "scenario/dynamic_world/household_purchases/version");
    if (envelope(*out.retail,
                 "scenario/dynamic_world/household_retail_settlements") != 1)
      unsupported(
          "UnsupportedHouseholdRetailVersion",
          "scenario/dynamic_world/household_retail_settlements/version");
    out.purchases = &field(*out.purchases, "catalog", "household_purchases");
    out.retail = &field(*out.retail, "catalog", "household_retail_settlements");
    return {true, out, {}};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
Status append(CatalogIdentity &identity, const HouseholdCatalogInput &input) {
  for (const auto &row : identity.module_versions)
    if (row.first == "household_purchases" ||
        row.first == "household_retail_settlements")
      return common_failure("DuplicateHouseholdModuleSelection", row.first);
  if (!input.purchases)
    return common_success();
  identity.module_versions.emplace_back(
      "household_purchases", std::to_string(input.purchases_version));
  identity.module_versions.emplace_back("household_retail_settlements", "1");
  return common_success();
}
} // namespace
Status append_native_household_modules(const BoundaryValue &scenario,
                                       CatalogIdentity &identity) {
  auto input = read(scenario);
  if (!input.ok)
    return {false, {}, std::move(input.error)};
  // Native identity admission and the owned cold directory consume the same two
  // decoders.
  if (input.value.purchases) {
    auto purchases = decode_original_purchase_catalog(*input.value.purchases);
    if (!purchases.ok)
      return {false, {}, std::move(purchases.error)};
    auto retail = decode_original_retail_catalog(*input.value.retail);
    if (!retail.ok)
      return {false, {}, std::move(retail.error)};
    if (retail.value.version != 1)
      return common_failure("HouseholdRetailCatalogVersionMismatch");
  }
  return append(identity, input.value);
}
Status prepare_native_household_catalogs(const BoundaryValue &scenario,
                                         PreparedCatalog &catalog) {
  if (catalog.household_purchases || catalog.household_retail_settlements ||
      catalog.selected_household_version)
    return common_failure("HouseholdCatalogAlreadyPrepared");
  auto input = read(scenario);
  if (!input.ok)
    return {false, {}, std::move(input.error)};
  if (!input.value.purchases)
    return append(catalog.identity, input.value);
  auto purchases = decode_original_purchase_catalog(*input.value.purchases);
  if (!purchases.ok)
    return {false, {}, std::move(purchases.error)};
  auto retail = decode_original_retail_catalog(*input.value.retail);
  if (!retail.ok)
    return {false, {}, std::move(retail.error)};
  if (retail.value.version != 1)
    return common_failure("HouseholdRetailCatalogVersionMismatch");
  auto selected = append(catalog.identity, input.value);
  if (!selected.ok)
    return selected;
  catalog.household_purchases =
      checkpoint_make_shared<const HouseholdPurchaseCatalog>(
          std::move(purchases.value));
  catalog.household_retail_settlements =
      checkpoint_make_shared<const HouseholdRetailCatalog>(
          std::move(retail.value));
  catalog.selected_household_version = input.value.purchases_version;
  return common_success();
}
} // namespace hanmo::resident
