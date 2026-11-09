// History2冷链的只读派生值；不是另一活跃库存，也不能作为资源写入许可。
#pragma once
#include "growth_market_resource_contract.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
class GrowthMarketInventoryProjection {
public:
  GrowthMarketInventoryProjection() = default;
  GrowthMarketInventoryProjection(const GrowthMarketInventoryProjection &) =
      delete;
  GrowthMarketInventoryProjection &
  operator=(const GrowthMarketInventoryProjection &) = delete;
  GrowthMarketInventoryProjection(GrowthMarketInventoryProjection &&) = default;
  GrowthMarketInventoryProjection &
  operator=(GrowthMarketInventoryProjection &&) = delete;
  const InventoryView *value() const noexcept {
    return value_ ? &*value_ : nullptr;
  }

private:
  explicit GrowthMarketInventoryProjection(CheckpointMemoryCharge memory)
      : copied_fields_(std::move(memory)), value_(std::in_place) {}
  CheckpointMemoryCharge copied_fields_;
  std::optional<InventoryView> value_;
  friend Outcome<GrowthMarketInventoryProjection>
  derive_growth_market_inventory_after(const InventoryView &,
                                       const MarketInventoryChange &,
                                       const CommonResourceCodecContext &);
};
// 完整原before匹配稀疏before；保未动批及相对顺序，新批按真实delta序追加。
// 来源/FIFO/allocator由原业务独立核；本口不会执行Common原语或猜新lot身份。
Outcome<GrowthMarketInventoryProjection> derive_growth_market_inventory_after(
    const InventoryView &original_before,
    const MarketInventoryChange &actual_delta,
    const CommonResourceCodecContext &actual_resources);
} // namespace hanmo::resident
