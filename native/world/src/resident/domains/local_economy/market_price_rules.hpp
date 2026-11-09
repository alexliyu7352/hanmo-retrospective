// S5唯一逐件整数报价；实际价格来源由Growth/S7核验，本纯叶不产生ID、预约或钱货。
#pragma once
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
enum class MarketTradeSide { buy,sell };
struct MarketRuleProfile {
 StableId id,market_id,item_id,event_source_ref;Integer revision=1,base_price=0,target_stock=0,stock_elasticity_bp=0,event_adjustment_bp=0;
 Integer buy_factor_bp=11500,sell_factor_bp=8500,maximum_discount_bp=800,event_from_tick=0,event_until_tick=0;
};
Status validate_market_rule_profile(const MarketRuleProfile&);
// 带符号floor及逐件库存顺序固定；缺钱/容量属于上层真实交易，不在报价中默缩量。
Outcome<Integer> compute_market_line_total(const MarketRuleProfile&,Integer stock_before,MarketTradeSide,Integer quantity,Integer discount_bp=0);
Outcome<Integer> compute_market_midpoint(const MarketRuleProfile&,Integer stock);
}
