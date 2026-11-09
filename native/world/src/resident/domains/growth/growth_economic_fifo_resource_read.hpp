// 原业务纯核通过后，沿共享唯一资源变换证明完整阶段末值；不执行任何世界writer。
#pragma once
#include "growth_market_fifo_read.hpp"
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
// 调用者先核真实LE收据、权源、需求与FIFO；本口只核全frame、原flow、revision和allocator。
Status validate_growth_economic_fifo_resource_stages(
    const CommonCatalog &, const CommonResourceCodecContext &,
    std::span<const MarketOriginalStageReadView>);
} // namespace hanmo::resident
