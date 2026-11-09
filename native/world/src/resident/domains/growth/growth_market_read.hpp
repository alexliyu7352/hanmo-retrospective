// 原市场中间价的当前只读投影；不授交易权，不把今天的库存倒填历史。
#pragma once
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
struct GrowthCatalog;
// 只复核已经冻结的base/stock与原算术是否一致；这些数字本身不证明旧市场确曾被观察。
Outcome<Integer> growth_market_midpoint_from_stock(Integer base_price,Integer available_stock);
struct GrowthMarketMidpoint {
    WorldIdentity source_identity;
    StableId shop_id,item_id,inventory_id,place_id;
    Integer observed_tick=0,market_revision=0,available_stock=0,base_price=0,midpoint=0;
};
// 调用者先核同一候选完整Common/Growth。库存口径与原交易相同：真实店主所有且未预约的批次。
// 返回现时原marginal，不含买卖价差/议价折让；本接口没有历史价格证明或远程接触许可。
Outcome<GrowthMarketMidpoint> read_growth_market_midpoint(const CommonStore&,const GrowthCatalog&,const CommonCatalog&,const StableId& shop_id,const StableId& item_id);
}
