// 只消费原初始自用及其真实首次交割终止，不为买方生成新场所/建设权。
#pragma once
#include "production_asset_usage_read.hpp"
namespace hanmo::resident {
// 借原完整产权历史与唯一安装政策；返回 [opened_at, ended_at) 的真实根值。
// 原 capacity consumer 仍须把窗口与自身容量版本区间相交，不以空窗口冒源存在。
Outcome<ProductionAssetInitialUsageRead>
read_production_asset_facility_usage_history(
    const ProductionAssetLifecycleReadContext &,
    const EconomicFacilityUsageRightBasis &,
    const PhysicalServiceFacilityExtension &);
// 只认第一次真实交割作为该条初始用途的终止源。后续回购不能复活此源，
// 任意 title/acceptance shape、今日 Owner 或他人资产不成为终止结果。
Outcome<const ProductionAssetTitleTransfer *>
read_production_asset_facility_usage_termination(
    const ProductionAssetLifecycleReadContext &, const FacilitySourceRef &,
    const PhysicalServiceFacilityExtension &);
} // namespace hanmo::resident
