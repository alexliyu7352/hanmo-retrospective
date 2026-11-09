// B1 已发生资源原语的有限冷证据，不保存 enclosing hot capture、world
// 身份、recipe 或操作游标。钱/批前后帧只证明这次执行，不是可变账本。
#pragma once
#include "resident/storage/resource_execution_draft.hpp"

namespace hanmo::resident {
struct FoundationAssetWalletReservationEffect {
  SetReservation request;
  WalletView before, after;
};
struct FoundationAssetWalletDeltaEffect {
  WalletDelta request;
  WalletView before, after;
};
struct FoundationAssetSelectedReservationEffect {
  ReserveSelectedLots request;
  std::vector<ResourceInventoryExecutionDraft> inventories;
  std::vector<ResourceLotFlowDraft> flows;
  Integer next_instance_before = 0, next_instance_after = 0;
};
struct FoundationAssetSelectedTransferEffect {
  TransferSelectedLots request;
  std::vector<ResourceInventoryExecutionDraft> inventories;
  std::vector<ResourceLotFlowDraft> flows;
  Integer next_instance_before = 0, next_instance_after = 0;
};
struct FoundationAssetPreparationEffects {
  std::optional<FoundationAssetWalletReservationEffect> cash;
  std::optional<FoundationAssetSelectedReservationEffect> goods;
};
// 原 release_then_spendable 的严格三腿；同一 payer 的前后帧必须逐腿相接，
// 受领者另为同候选已实际安装组织。不是预付款、临时私人中转或承接原 donor 债。
struct FoundationAssetReservedCashTransferEffects {
  FoundationAssetWalletReservationEffect release;
  FoundationAssetWalletDeltaEffect debit, credit;
};
struct FoundationAssetFulfillmentEffects {
  std::optional<FoundationAssetReservedCashTransferEffects> cash;
  std::optional<FoundationAssetSelectedTransferEffect> goods;
};
struct FoundationAssetOffer;
struct FoundationAssetPreparation;
struct FoundationAssetFulfillment;
// 冷/热都复用原 Common 唯一原语正文；本域只核被冻结条款/实际结果绑定。
Status validate_foundation_asset_preparation_effects(
    const FoundationAssetOffer &, const FoundationAssetPreparation &);
Status validate_foundation_asset_fulfillment_effects(
    const FoundationAssetOffer &, const FoundationAssetPreparation &,
    const FoundationAssetFulfillment &);
} // namespace hanmo::resident
