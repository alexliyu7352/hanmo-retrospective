// History2冷核只借真正历史帧；原业务reader仍独立证明条款、权源与全部实际行。
#pragma once
#include "growth_market_fifo_model.hpp"
#include "resident/storage/selected_resource_execution.hpp"
#include <span>
namespace hanmo::resident {
struct MarketMovementRecord;
struct MarketOriginalStageReadView {
  const OriginalBusinessStage &stage;
  std::span<const SelectedInventoryEvidenceBorrow> inventories;
};
class MarketOriginalFifoSourceSink {
public:
  virtual ~MarketOriginalFifoSourceSink() = default;
  virtual Status visit_original_fifo_source(MarketCauseKind,
                                            const StableId &source_id,
                                            Integer source_schema) = 0;
};
// 完整业务一次反读，生产不得把inputs/outputs切开而丢失原周期、余数或产量关系。
// before由Installed永久链或已核外端来源给出；不得用当前Common库存代替历史。
class MarketOriginalFifoBusinessRead {
public:
  virtual ~MarketOriginalFifoBusinessRead() = default;
  // 从原Growth pin/LE收据独立枚举，不能扫描MarketMovement反称来源齐全。
  virtual Status visit_original_fifo_sources(
      std::span<const InventoryView *const> protected_initial,
      MarketOriginalFifoSourceSink &) const = 0;
  virtual Status validate_original_fifo_business(
      const MarketMovementRecord &,
      std::span<const MarketOriginalStageReadView>) const = 0;
};
} // namespace hanmo::resident
