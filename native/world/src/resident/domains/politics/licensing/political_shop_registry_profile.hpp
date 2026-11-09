// 接册独立opt-in；1048576只为其具名Done，原法律/私铺262144与旧账字节保持。
// 只沿原私铺有限版本 1/2 兼容；法律和接册版本仍各为 1，不放宽未知标签。
#pragma once
#include "hanmo/resident/command_contract.hpp"
#include "political_shop_registry_model.hpp"
namespace hanmo::resident {
inline constexpr std::uint32_t
    political_shop_registry_succession_checkpoint_feature = 1048576u;
inline Status validate_political_shop_registry_succession_layout(
    const CommonStore &common,
    const std::vector<std::pair<StableId, StableId>> &modules,
    const PoliticalShopRegistrySuccessionCatalog *catalog,
    const PoliticalShopRegistrySuccessionState *state) {
  Integer count = 0, law = 0, shops = 0, takeover = 0;
  bool dependency_version = true;
  for (const auto &row : modules) {
    if (row.first == political_shop_registry_succession_module) {
      if (row.second != "1")
        return common_failure(
            "UnsupportedPoliticalShopRegistrySuccessionVersion");
      if (++count != 1)
        return common_failure("DuplicatePoliticalShopRegistrySuccessionModule");
    }
    if (row.first == "political_shop_licensing") {
      ++law;
      dependency_version = dependency_version && row.second == "1";
    }
    if (row.first == "private_shops") {
      ++shops;
      dependency_version =
          dependency_version && (row.second == "1" || row.second == "2");
    }
    if (row.first == "political_takeover") {
      ++takeover;
      dependency_version = dependency_version && row.second == "1";
    }
  }
  if (bool(catalog) != bool(state) || count != (catalog ? 1 : 0) ||
      (catalog && (!catalog->enabled || catalog->version != 1)) ||
      (state && state->version != 1))
    return common_failure("PoliticalShopRegistrySuccessionProfileRequired");
  if (count && (!dependency_version || law != 1 || shops != 1 || takeover != 1))
    return common_failure(
        "PoliticalShopRegistrySuccessionDependenciesRequired");
  if (!state) {
    for (const auto &row : common.state().facts.rows())
      if (row.second.domain == political_shop_registry_succession_module ||
          row.second.kind == political_shop_registry_accept_kind)
        return common_failure("PoliticalShopRegistrySuccessionProfileRequired",
                              row.first);
    for (const auto &row : common.state().history)
      if (row.kind == political_shop_registry_accept_command)
        return common_failure("PoliticalShopRegistrySuccessionProfileRequired",
                              row.result_id);
    for (const auto &row : common.state().results.rows())
      if (row.second &&
          row.second->kind == political_shop_registry_accept_command)
        return common_failure("PoliticalShopRegistrySuccessionProfileRequired",
                              row.first);
  }
  return common_success();
}
} // namespace hanmo::resident
