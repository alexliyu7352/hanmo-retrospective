// History2显式冷链入口；旧History1不会因字段存在而进入此路径。
#pragma once
#include "growth_market_history.hpp"
namespace hanmo::resident {
Status validate_growth_market_history_v2(const CommonStore &,
                                         const CommonCatalog &,
                                         const GrowthCatalog &,
                                         const GrowthMarketHistoryCatalog &,
                                         const GrowthMarketHistoryState &,
                                         const GrowthMarketHistoryAccess &);
Outcome<GrowthMarketMidpoint> read_growth_market_history_v2_midpoint(
    const CommonStore &, const CommonCatalog &, const GrowthCatalog &,
    const GrowthMarketHistoryCatalog &, const GrowthMarketHistoryState &,
    const GrowthMarketHistoryAccess &, const StableId &shop_id,
    const StableId &item_id, Integer observed_tick,
    const StableId &original_shop_day = {});
// wrapper持有真正capture；本口只借记录并让原表取得其自身copy租约，不先按值复制。
Outcome<StableId> record_growth_market_movement_v2_in_candidate(
    CommonStore &, const CommonCatalog &, const GrowthCatalog &,
    const GrowthMarketHistoryCatalog &, GrowthMarketHistoryState &,
    const GrowthMarketHistoryAccess &, const MarketMovementRecord &);
} // namespace hanmo::resident
