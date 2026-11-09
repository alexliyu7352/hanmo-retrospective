// 市场历史由原Growth作者初态及永久钱货来源重建；商铺只消费该只读口，不能用自己的零售账证明报价。
#pragma once
#include "resident/domains/growth/growth_market_read.hpp"
namespace hanmo::resident {
class PrivateShopMarketHistoryRead {
public:
    virtual ~PrivateShopMarketHistoryRead()=default;
    virtual Status private_shop_immediate_market_trade(const StableId& trade_id)const {return common_failure("PrivateShopImmediateTradeRequired",trade_id);}
    // 当前预检须确认该原市场有完整可重建来源；仅今天余额/中间价可读不等于历史资格。
    virtual Outcome<GrowthMarketMidpoint> private_shop_market_history_current(const StableId& market_shop_id,const StableId& item_id)const=0;
    // 调用者先核本域真实完成载荷；提供者另核确切事实头、顺序及原市场所有写入来源。
    virtual Outcome<GrowthMarketMidpoint> private_shop_market_before_result(const StableId& market_shop_id,const StableId& item_id,Integer observed_tick,const StableId& actual_day_fact_id)const=0;
};
}
