// 精确市场只借实际World唯一三根；不按字段存在升级版本或创建默认来源。
#pragma once
#include "resident/domains/growth/growth_action_time_read.hpp"
namespace hanmo::resident {
struct WorldBundle;
struct PreparedCatalog;
// 模块版本只来自显式身份；两个布局互不推断或兼容猜读。
Outcome<Integer> selected_native_growth_market_version(const CatalogIdentity &);
// prepare私有可写别名在最终轴齐备后一次绑定Common布局，发布后只读。
Status bind_native_growth_market_catalog(PreparedCatalog &,
                                         GrowthMarketHistoryCatalog &);
Status validate_native_growth_market_catalog(const PreparedCatalog &);
Outcome<std::optional<GrowthMarketExactSources>>
read_native_growth_market_exact_sources(const WorldBundle &);
} // namespace hanmo::resident
