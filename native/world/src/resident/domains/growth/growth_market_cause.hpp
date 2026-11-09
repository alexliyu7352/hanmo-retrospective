// 市场重放只消费原成交事实，不从自己的库存回执或今天价格倒推出成交原因。
#pragma once
#include "growth_market_history_contract.hpp"
#include "resident/domains/growth/growth_refs.hpp"
namespace hanmo::resident {
// 只用于新局即选定且完整保护史已核的固定Owner市场；不支持旧市场/产权迁转，不能当通用交易产权史。
// 同候选原Growth全史须先验；商店Owner只取Installed原required_inventory。
Outcome<MarketBusinessCause> read_growth_market_cause(const CommonStore&,const CommonCatalog&,const GrowthCatalog&,const GrowthState&,const StableId& exact_trade_fact_id);
// 只复用原trade_price内核，验真正历史库存/revision对应的实付价格；原议价挑战仍须先验。
Status validate_growth_market_trade_before(const CommonStore&,const CommonCatalog&,const GrowthCatalog&,const GrowthState&,const StableId& exact_trade_fact_id,const InventoryView& market_before);
}
