// 原本人初始用途政策的真实历史区间；产权易主只终止此原源，不替新主发许可。
#pragma once
#include "production_asset_title_read.hpp"

namespace hanmo::resident {
// 借同 job 原安装/交割记录；区间为 [opened_at, ended_at)，未终止时后界空。
// 它既不是新的权利账，也不是第三方用途或建设处分证明。
struct ProductionAssetInitialUsageRead {
  const PhysicalServiceFacilityExtension *facility = nullptr;
  const WorldMoment *opened_at = nullptr;
  const WorldMoment *ended_at = nullptr;
  const ProductionAssetTitleTransfer *actual_termination = nullptr;
};
Outcome<ProductionAssetInitialUsageRead>
read_production_asset_initial_usage_history(
    const ProductionAssetLifecycleReadContext &, const FacilitySourceRef &,
    const Owner &original_owner);
} // namespace hanmo::resident
