// 原 PlanFacilityCapacity 本人建造同意的实际源；不把产权或安装 shape 当许可。
#pragma once
#include "facility_construction_adoption.hpp"
#include "production_asset_usage_read.hpp"

namespace hanmo::resident {
// 只读已实际发生的原 construction_started 排他 cut。首个有限分支要求
// 原产权人本人施工、原初始用途仍有效，并由同一真实成功命令封口；
// 后购、租约、机构/代工分支不从这里取得缺失的独立许可。
// 借用结果寿命不超过两个 context 的同 job 根，不允许持久化该指针。
Outcome<const ProductionAssetConstructionConsent *>
read_production_asset_original_construction_consent(
    const ProductionAssetLifecycleReadContext &,
    const FacilityConstructionAdoptionContext &,
    const MomentOwnerSource &actual_construction_started);
// 原开工 writer 与稳定冷读共用同一用途窗口核；先独立核真实 /2 期来源。
// 本口只验证该工程的原许可覆盖窗口，不能单凭自由窗口产生工量或费用。
Status validate_production_asset_construction_period_usage(
    const ProductionAssetLifecycleReadContext &,
    const FacilityConstructionAdoptionContext &, const FacilityConstruction &,
    const WorkPeriodRef2 &actual_period);
} // namespace hanmo::resident
