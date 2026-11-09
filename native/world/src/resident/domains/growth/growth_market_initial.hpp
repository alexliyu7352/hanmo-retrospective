// 正规Installed原库存种子转换为不可变市场初态；不读取恢复或当前World的库存。
#pragma once
#include "growth_market_history.hpp"
namespace hanmo::resident {
struct BoundaryValue;
// initial_assets须来自同次effective
// Installed目录的原seed_native_inventory_assets，
// 位于人物创建/区域初始化之前。冷热prepare均重用原种子函数，不能从银行反推。
// 本口不分配ID、不创建World，只核原全局初货连续编号后复制所选市场。
Outcome<GrowthMarketHistoryCatalog> make_growth_market_initial_catalog(
    const CommonCatalog &, const GrowthCatalog &,
    const OrderedTable<InventoryView> &initial_assets,
    Integer initial_next_instance_id,
    const std::vector<StableId> &selected_shop_ids);
// 早期物理2只选明确作者version2并冻结同一Installed原件；
// final Complete轴和资源布局须在Prepared发布前由中央同包严格绑定。
Outcome<GrowthMarketHistoryCatalog> make_growth_market_initial_catalog_v2(
    const CommonCatalog &, const GrowthCatalog &,
    const OrderedTable<InventoryView> &initial_assets,
    Integer initial_next_instance_id,
    const std::vector<StableId> &selected_shop_ids);
// 作者显式块只含所选原shop；初始数量、lot编号和nextID不是作者可独立填写字段。
Outcome<std::vector<StableId>>
decode_growth_market_history_selection(const BoundaryValue &);
Outcome<std::vector<StableId>>
decode_growth_market_history_selection_v2(const BoundaryValue &);
} // namespace hanmo::resident
