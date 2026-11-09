// E §15.3 与 S6 B1 的原私人资材报价。只借当前真实资源，不产生同意、
// 预约、贡献完成或未来组织 Owner；真正本人来源和履行另由原命令封口。
#pragma once
#include "resident/kernel/read_stamp.hpp"
#include "resident/storage/common_store.hpp"
#include <span>

namespace hanmo::resident {
// 原提案冻结前的同栈借用输入。数量和窗口均由实际输入明确给出，不取
// 开店租价等另一业务默认值；一个报价只引用一个真实库存和一个私钱包。
struct FoundationPrivateAssetRequest {
  const StableId &owner_actor_id, &founder_actor_id;
  const StableId &wallet_id;
  Integer cash_quantity;
  const StableId &inventory_id;
  std::span<const CargoSelection> selected_lots;
  const MomentSpan &commitment_window;
};
// 借值不能进入持久根。quoted_at 只来自同世界唯一 clock，common_revision
// 用于同栈 stale 检查；这里没有可供支付的信用、组织账户或长期预约余额。
struct FoundationPrivateAssetQuoteRead {
  const CommonStore *common = nullptr;
  const FoundationPrivateAssetRequest *requested = nullptr;
  const ActorCoreView *owner = nullptr, *founder = nullptr;
  const WalletView *wallet = nullptr;
  const InventoryView *inventory = nullptr;
  Integer common_revision = 0;
  WorldMoment quoted_at;
};
// 核原 Owner、可花钱与指定未预约批；独特物/性质沿原 S9 semantic reader。
// 这是报价而非授权。实际预约仍必须执行原 Common 原语，不能据成功报价
// 跳过本人同意、原 kind3 owner、执行捕获或完整世界候选的最终原子校验。
Outcome<FoundationPrivateAssetQuoteRead>
quote_private_foundation_assets(const CommonStore &, const CurrentMomentRead &,
                                const FoundationPrivateAssetRequest &);
} // namespace hanmo::resident
