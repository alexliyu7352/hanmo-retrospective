// 原私铺命令只组合真实 S7 写入与 S0 整槽处置，不另造容量或终局来源。
#pragma once
#include "household_foundation_lease.hpp"
#include "resident/domains/commerce/private_shop_exact_service.hpp"

namespace hanmo::resident {
struct NativePrivateShopMarketCommandContext {
  WorldBundle &world;
  const NativeHouseholdFoundationLease &household;
  const std::vector<FacilityHolderRegistration> &holder_handlers;
  ExactPhaseWriteContext &phase_writes;
};

// 原开工、终局或已经由 S7 采用的 S4 变化，只协调其真实 P05 后继。
Status reconcile_native_private_shop_market_operation_change_in_candidate(
    NativePrivateShopMarketCommandContext &, const StableId &operation,
    const MomentOwnerSource &actual_change);

// 外层必须持完整 World 候选与同一次真实 capture；失败即丢弃共同写集。
Outcome<StableId> start_native_private_shop_market_operation_in_candidate(
    NativePrivateShopMarketCommandContext &, const PrivateShopOperationQuote &);

// 此返回值就是原 S7 opaque token。中央须保活同一个 household owner 和 capture，
// 立即用原 command result 封口，再调用下方 bind；不得重建 context 或写入存档。
Outcome<PendingPrivateShopCancellation>
cancel_native_private_shop_market_operation_in_candidate(
    NativePrivateShopMarketCommandContext &, const StableId &actor,
    const StableId &operation);
Status bind_native_private_shop_market_cancellation_in_candidate(
    NativePrivateShopMarketCommandContext &, PendingPrivateShopCancellation &&,
    const MomentOwnerSource &actual_command_seal);
} // namespace hanmo::resident
