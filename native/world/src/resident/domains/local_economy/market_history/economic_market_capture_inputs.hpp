// 原生产/口粮的完整读取足迹；包括没有实际写入的输入库，不从资源recipe猜读取集合。
#pragma once
#include "economic_market_cause.hpp"

namespace hanmo::resident {
struct EconomicMarketCaptureInputsBorrow {
  std::span<const StableId> production_input_inventory_ids;
  const StableId *production_output_inventory_id = nullptr;
  const StableId *ration_inventory_id = nullptr;
};
// 只借原冻结条款的容器身份与顺序。数量、FIFO、权限和容量仍归各自唯一原正文。
Outcome<EconomicMarketCaptureInputsBorrow> read_economic_market_capture_inputs(
    const CommonStore &, const LocalEconomyState &, const LocalEconomyCatalog &,
    MarketCauseKind, const StableId &original_business_key);
} // namespace hanmo::resident
