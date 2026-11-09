// 原施工四表的稳定出口；源依赖读口不得回入本整根校验形成循环自证。
#pragma once
#include "facility_construction_completion_read.hpp"
#include "production_asset_construction_authority.hpp"

namespace hanmo::resident {
// 同锁、同 Common/clock 的实际四表与原头、成本、许可、期间、动作双向覆盖。
// 只在完整私有候选最终出口/冷读运行；不在成本、开工或终止的中间步骤调用。
// 不替代全 World 的 S0 注册闭包、物理 holder 处置或实际设施应用准入。
Status validate_facility_construction_sources(
    const FacilityConstructionServiceContext &,
    const ProductionAssetLifecycleReadContext &);
} // namespace hanmo::resident
