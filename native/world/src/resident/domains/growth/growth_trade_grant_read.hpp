// 原交易成长授予只读来源；只借同一行动的真实终局，不反调能力链或FSL。
#pragma once
#include "resident/domains/growth/growth_refs.hpp"
#include "growth_trade_grant.hpp"
namespace hanmo::resident {
struct GrowthTradeGrantView {
  const GrowthTradeGrant *grant = nullptr;
  const TradeTerminalTime *terminal = nullptr;
};
// 允许原能力writer尚未填application；完整链存在性由原stable validator另核。
Outcome<GrowthTradeGrantView> read_growth_trade_grant_v1(
    const CommonStore &, const GrowthState &, const GrowthCatalog &,
    const CommonCatalog &, const GrowthTradeGrantRef &, const WorldMoment &);
// 正反覆盖包含净增为零的真实授予；取消、活动中与未选型残留均明确拒绝。
Status validate_growth_trade_grants_v1(
    const CommonStore &, const GrowthState &, const GrowthCatalog &,
    const CommonCatalog &, const WorldMoment &);
} // namespace hanmo::resident
