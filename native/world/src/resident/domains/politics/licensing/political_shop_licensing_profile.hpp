// 法律与首店显式成对启用；新Done位只给具名四命令，不继承宽泛旧政治回执权限。
// 许可1保原字节；许可2显式保存实际M。接册1不冒精确来源，未知标签拒绝。
#pragma once
#include "hanmo/resident/command_contract.hpp"
#include "political_shop_licensing.hpp"
namespace hanmo::resident {
inline constexpr std::uint32_t political_shop_licensing_checkpoint_feature =
    262144u;
inline bool political_shop_licensing_command_kind(const StableId &kind) {
  return kind == "political_request_shop_permission" ||
         kind == "political_register_wanted" ||
         kind == "political_release_wanted" ||
         kind == "political_revoke_shop_permit";
}
inline Status validate_political_shop_licensing_layout(
    const CommonStore &common,
    const std::vector<std::pair<StableId, StableId>> &modules,
    const PoliticalShopLicensingCatalog *catalog,
    const PoliticalShopLicensingState *state) {
  Integer political = 0, commerce = 0, selected = 0, shop_selected = 0;
  for (const auto &row : modules) {
    if (row.first != political_shop_licensing_module &&
        row.first != "private_shops")
      continue;
    if ((row.first == political_shop_licensing_module && row.second != "1" &&
         row.second != "2") ||
        (row.first == "private_shops" && row.second != "1" &&
         row.second != "2" && row.second != "3"))
      return common_failure("UnsupportedPoliticalShopPairVersion");
    if (row.first == "private_shops")
      shop_selected = row.second == "3" ? 3 : row.second == "2" ? 2 : 1;
    if (row.first == political_shop_licensing_module)
      selected = row.second == "2" ? 2 : 1;
    if (++(row.first == political_shop_licensing_module ? political
                                                        : commerce) != 1)
      return common_failure("DuplicatePoliticalShopPairModule");
  }
  if (selected == 2 && shop_selected != 3)
    return common_failure("PoliticalShopExactPairRequired");
  if (selected == 1 && shop_selected == 3)
    return common_failure("PoliticalShopExactPairRequired");
  if (bool(catalog) != bool(state) || political != commerce ||
      political != (catalog ? 1 : 0) || (catalog && catalog->version != 1) ||
      (state && state->version != selected))
    return common_failure("PoliticalShopLicensingPairRequired");
  if (!state) {
    for (const auto &row : common.state().facts.rows())
      if (row.second.domain == political_shop_licensing_module ||
          row.second.kind == "political_shop_registry_initialized" ||
          row.second.kind == "political_wanted_registered" ||
          row.second.kind == "political_wanted_released" ||
          row.second.kind == political_shop_clearance_kind ||
          row.second.kind == political_shop_permit_kind ||
          row.second.kind == "political_shop_permit_revoked")
        return common_failure("PoliticalShopLicensingProfileRequired",
                              row.first);
    for (const auto &entry : common.state().history)
      if (political_shop_licensing_command_kind(entry.kind))
        return common_failure("PoliticalShopLicensingProfileRequired",
                              entry.result_id);
    for (const auto &row : common.state().results.rows())
      if (row.second && political_shop_licensing_command_kind(row.second->kind))
        return common_failure("PoliticalShopLicensingProfileRequired",
                              row.first);
  }
  return common_success();
}
} // namespace hanmo::resident
