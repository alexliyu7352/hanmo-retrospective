// 真实scalar开始源与完整历史帧的有限冷核；不执行recipe或复制影子Common。
#pragma once
#include "growth_market_fifo_read.hpp"
#include "growth_market_start_read.hpp"
namespace hanmo::resident {
// caller须先由原start
// reader获得actual，且永久链另核Installed、前驱、价格和逆覆盖。
Status validate_growth_scalar_fifo_business(
    const GrowthScalarMarketStartView &actual, const CommonCatalog &,
    const GrowthCatalog &, const CommonResourceCodecContext &,
    std::span<const MarketOriginalStageReadView> stages);
} // namespace hanmo::resident
