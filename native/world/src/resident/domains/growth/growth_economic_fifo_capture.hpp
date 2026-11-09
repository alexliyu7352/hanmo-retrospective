// LE 原执行的短寿命捕获；完整扫描库在 apply 前冻结，实际钱货仍只由 Common
// 执行一次。
#pragma once
#include "growth_market_capture.hpp"
#include "resident/domains/local_economy/market_history/economic_material_read.hpp"
namespace hanmo::resident {
class NativeEconomicFifoRead;
class GrowthEconomicFifoMovementCapture;
class GrowthEconomicFifoStageCapture {
public:
  GrowthEconomicFifoStageCapture() = default;
  // 只问已实际捕获的写，不从 recipe 非空或保护足迹推断成交。
  bool has_protected_write() const noexcept { return bool(protected_step_); }
  GrowthEconomicFifoStageCapture(const GrowthEconomicFifoStageCapture &) =
      delete;
  GrowthEconomicFifoStageCapture &
  operator=(const GrowthEconomicFifoStageCapture &) = delete;
  GrowthEconomicFifoStageCapture(GrowthEconomicFifoStageCapture &&) = default;
  GrowthEconomicFifoStageCapture &
  operator=(GrowthEconomicFifoStageCapture &&) = delete;

private:
  explicit GrowthEconomicFifoStageCapture(CheckpointMemoryCharge);
  // 普通拥有字段先预留；其后的 DTO 全销毁后才释放同 job 租约。
  CheckpointMemoryCharge copied_fields_;
  std::vector<InventoryView> before_;
  StableId original_key_;
  CheckpointVector<EconomicMarketItemOperationBorrow> metadata_;
  CheckpointVector<EconomicMaterialNeedRead> needs_;
  std::optional<OriginalBusinessStage> stage_;
  std::optional<MarketCaptureStep> protected_step_;
  const CommonStore *common_ = nullptr;
  const ProtectedMarketAccounts *policy_ = nullptr;
  const NativeEconomicMaterialRead *materials_ = nullptr;
  std::shared_ptr<const ResourceExecutionCaptureIdentity> identity_;
  std::size_t recipe_index_ = 0;
  Integer observed_tick_ = 0, due_tick_ = 0, next_before_ = 1;
  MarketCauseKind kind_ = MarketCauseKind::local_production;
  MarketCaptureStage stage_kind_ = MarketCaptureStage::production_inputs;
  friend Outcome<GrowthEconomicFifoStageCapture>
  prepare_growth_economic_fifo_stage(
      const CommonStore &, const GrowthMarketHistoryCatalog &,
      const ProtectedMarketAccounts &, const NativeEconomicMaterialRead &,
      MarketCauseKind, const StableId &, Integer, MarketCaptureStage,
      std::span<const InventoryView *const>, const ResourceRecipe &,
      std::span<const EconomicMarketItemOperationBorrow>);
  friend Status
  capture_growth_economic_fifo_stage(GrowthEconomicFifoStageCapture &,
                                     const CommonStore &);
  friend Outcome<GrowthEconomicFifoMovementCapture>
  bind_growth_economic_fifo_movement(
      const CommonStore &, const NativeEconomicFifoRead &, const StableId &,
      std::span<const GrowthEconomicFifoStageCapture *const>);
};
// full_before 必须由原 LE key 的完整足迹读口给出，含未写和只读库，保持原顺序。
Outcome<GrowthEconomicFifoStageCapture> prepare_growth_economic_fifo_stage(
    const CommonStore &, const GrowthMarketHistoryCatalog &,
    const ProtectedMarketAccounts &, const NativeEconomicMaterialRead &,
    MarketCauseKind, const StableId &, Integer due_tick, MarketCaptureStage,
    std::span<const InventoryView *const> full_before, const ResourceRecipe &,
    std::span<const EconomicMarketItemOperationBorrow>);
// 必须紧接同候选唯一一次实际 apply；不重新运行资源操作。
Status capture_growth_economic_fifo_stage(GrowthEconomicFifoStageCapture &,
                                          const CommonStore &);
class GrowthEconomicFifoMovementCapture {
public:
  GrowthEconomicFifoMovementCapture() = default;
  GrowthEconomicFifoMovementCapture(const GrowthEconomicFifoMovementCapture &) =
      delete;
  GrowthEconomicFifoMovementCapture &
  operator=(const GrowthEconomicFifoMovementCapture &) = delete;
  GrowthEconomicFifoMovementCapture(GrowthEconomicFifoMovementCapture &&) =
      default;
  GrowthEconomicFifoMovementCapture &
  operator=(GrowthEconomicFifoMovementCapture &&) = delete;
  Outcome<const MarketMovementRecord *>
  original_record(const CommonStore &) const;

private:
  explicit GrowthEconomicFifoMovementCapture(CheckpointMemoryCharge);
  CheckpointMemoryCharge copied_fields_;
  std::optional<MarketMovementRecord> record_;
  const CommonStore *common_ = nullptr;
  std::shared_ptr<const ResourceExecutionCaptureIdentity> identity_;
  friend Outcome<GrowthEconomicFifoMovementCapture>
  bind_growth_economic_fifo_movement(
      const CommonStore &, const NativeEconomicFifoRead &, const StableId &,
      std::span<const GrowthEconomicFifoStageCapture *const>);
};
// 原收据头和 payload 已成功后，借其真实 reader 反核来源/周期/物料与原数量数学。
// 返回对象一直活到原 movements.insert 完成，表仍取得自己的原行租约。
Outcome<GrowthEconomicFifoMovementCapture> bind_growth_economic_fifo_movement(
    const CommonStore &, const NativeEconomicFifoRead &,
    const StableId &exact_receipt,
    std::span<const GrowthEconomicFifoStageCapture *const>);
} // namespace hanmo::resident
