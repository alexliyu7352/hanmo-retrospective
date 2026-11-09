// 原History2
// scalar开始时刻的纯来源读；不依赖ActionElapsed/FSL，避免时钟相互自证。
#pragma once
#include "growth_market_history.hpp"
#include "resident/domains/growth/growth_refs.hpp"
#include "resident/kernel/source_ref.hpp"
namespace hanmo::resident {
struct GrowthScalarMarketStartView {
  const MarketMovementRecord *movement = nullptr;
  const LifeActivityRecord *activity = nullptr;
  const WorldMoment *started_at = nullptr;
  const StableId *movement_id = nullptr;
};
// 完整世界冷验仍须先核Installed/FIFO/业务反覆盖；此口只严格定位其原持久来源。
Outcome<GrowthScalarMarketStartView> read_growth_scalar_market_start_v2(
    const CommonStore &, const CommonCatalog &, const GrowthCatalog &,
    const GrowthState &, const ActorFoundationCatalog &,
    const GrowthMarketHistoryCatalog &, const GrowthMarketHistoryState &,
    const SourceRef &, Integer source_schema, const WorldMoment &actual_now,
    ExactArithmeticBudget budget = {});
// 按真正原Action定位唯一市场持值，仍委托上面的同一源校验。
Outcome<GrowthScalarMarketStartView>
read_growth_scalar_market_start_for_action_v2(
    const CommonStore &, const CommonCatalog &, const GrowthCatalog &,
    const GrowthState &, const ActorFoundationCatalog &,
    const GrowthMarketHistoryCatalog &, const GrowthMarketHistoryState &,
    const StableId &action_id, const WorldMoment &actual_now,
    ExactArithmeticBudget budget = {});
} // namespace hanmo::resident
