// S0 唯一计量真实 Fixed 正片；S7 只消费已供整期并采用原工资/终局。
#pragma once
#include "private_shop_market_commands.hpp"
#include "resident/kernel/exact_phase_graph.hpp"
namespace hanmo::resident {
Outcome<std::vector<ExactEventPrediction>>
predict_native_private_shop_service_periods(
    const NativeHouseholdFoundationLease &, const WorldMoment &current,
    const WorldMoment &until);
// 调用前由原 S0 正片 writer 结清至 prediction.at，不在这里重复累工时。
Outcome<ExactEventApplied>
apply_native_private_shop_service_period_in_candidate(
    NativePrivateShopMarketCommandContext &, const ExactEventPrediction &);
} // namespace hanmo::resident
