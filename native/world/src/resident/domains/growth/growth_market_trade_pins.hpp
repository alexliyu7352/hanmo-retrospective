// 显式市场历史选择从新局起保留所有参与者的原Trade；只保引用，不建立第二份钱货/价格账。
#pragma once
#include "resident/domains/growth/growth_refs.hpp"
namespace hanmo::resident {
struct GrowthMarketHistoryCatalog;
class MarketOriginalFifoSourceSink;
// 必须在原Growth完整校验过的同候选调用；protected_shop_ids来自安装时固定的独立市场历史profile。
// 此profile不得热加到已经采用16条裁剪的旧World；为空保持旧策略。返回错误不可静默跳过。
// 这里仅保全所有真实Trade原因，不能证明其他领域市场库存写者或已经遗失的旧来源。
Outcome<std::vector<StableId>>
read_growth_market_trade_pins(const CommonStore &, const GrowthState &,
                              const GrowthCatalog &,
                              const std::vector<StableId> &protected_shop_ids);
// 物理2沿原校验和Common事实顺序借引用；原LifeActivity仍是schema1。
Status visit_growth_market_trade_pins(const CommonStore &, const GrowthState &,
                                      const GrowthCatalog &,
                                      const GrowthMarketHistoryCatalog &,
                                      MarketOriginalFifoSourceSink &);
} // namespace hanmo::resident
