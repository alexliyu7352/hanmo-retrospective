#include "private_shop_initial_state.hpp"
#include "private_shop.hpp"
namespace hanmo::resident {
Status
validate_original_private_shop_initial_state(const CommonStore &common,
                                             const PrivateShopCatalog &catalog,
                                             const PrivateShopState &state) {
  // 原工厂只设置所选版与两个当前tick；不从默认1或当前表有无猜选择。
  if ((catalog.version != 1 && catalog.version != 2 && catalog.version != 3) ||
      state.version != catalog.version)
    return common_failure("PrivateShopInitialVersionMismatch");
  if (state.initialized_tick != common.tick() ||
      state.last_step_tick != common.tick())
    return common_failure("PrivateShopInitialClockMismatch");
  if (!state.staff.contracts.empty() || !state.staff.decisions.empty() ||
      !state.staff.services.empty() || !state.orders.orders.empty() ||
      !state.orders.bindings.empty() || !state.shops.empty() ||
      !state.closings.empty() || !state.order_settlements.empty() ||
      !state.operations.empty() || !state.operating_results.empty() ||
      !state.capital.empty() || !state.markups.empty() ||
      !state.purchases.empty())
    return common_failure("PrivateShopInitialHistoryExists");
  // 空表不可掩盖已经发生而被清掉的原业务头；不读取/删除其他域正常初册。
  for (const auto &row : common.state().facts.rows())
    if (row.second.domain == private_shop_module)
      return common_failure("PrivateShopInitialSourceHistoryExists", row.first);
  return common_success();
}
} // namespace hanmo::resident
