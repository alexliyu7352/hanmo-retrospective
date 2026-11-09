// 原scalar真实capture的短寿命拥有候选；先计实际复制，再交原有序表采用自己的行租约。
#pragma once
#include "growth_market_history.hpp"
#include "growth_scalar_fifo.hpp"
#include "study_service_sources.hpp"
namespace hanmo::resident {
// 不提供可写DTO或自由来源构造；它不是持久段、资源许可或新的业务事实。
class GrowthScalarFifoMovementCapture {
public:
  GrowthScalarFifoMovementCapture() = default;
  GrowthScalarFifoMovementCapture(const GrowthScalarFifoMovementCapture &) =
      delete;
  GrowthScalarFifoMovementCapture &
  operator=(const GrowthScalarFifoMovementCapture &) = delete;
  GrowthScalarFifoMovementCapture(GrowthScalarFifoMovementCapture &&) = default;
  GrowthScalarFifoMovementCapture &
  operator=(GrowthScalarFifoMovementCapture &&) = delete;
  // 原LifeActivity头/载荷成功后，只借同候选不可变草稿。调用方保本对象至insert返回。
  Outcome<const MarketMovementRecord *>
  original_record(const CommonStore &, const GrowthState &) const;

private:
  explicit GrowthScalarFifoMovementCapture(CheckpointMemoryCharge);
  // 第一成员先预留；析构顺序保证所有真实普通拥有字段先销毁，最后才退预算。
  CheckpointMemoryCharge copied_fields_;
  std::optional<MarketMovementRecord> record_;
  const CommonStore *common_ = nullptr;
  std::shared_ptr<const ResourceExecutionCaptureIdentity> identity_;
  std::size_t recipe_index_ = 0;
  friend Outcome<GrowthScalarFifoMovementCapture>
  capture_growth_scalar_fifo_movement(
      const CommonStore &, const GrowthCatalog &,
      const GrowthMarketHistoryCatalog &, const ProtectedMarketAccounts &,
      const ActionCoreView &, const GrowthQuote &,
      const GrowthExactStartContext &, std::size_t);
};
// 只能读Common此候选最后一份actual
// draft；不执行recipe、分ID、补事实或用Tick造Moment。
Outcome<GrowthScalarFifoMovementCapture> capture_growth_scalar_fifo_movement(
    const CommonStore &, const GrowthCatalog &,
    const GrowthMarketHistoryCatalog &, const ProtectedMarketAccounts &,
    const ActionCoreView &, const GrowthQuote &,
    const GrowthExactStartContext &, std::size_t actual_recipe_index);
} // namespace hanmo::resident
