// 原 PlanFacilityCapacity 的私有候选同意采用，不创建额外玩家命令。
#pragma once
#include "production_asset_construction_authority.hpp"
#include "production_asset_transfer.hpp"

namespace hanmo::resident {
// 只在该完整 World 候选内使用的行索引，不是持久权限或可外传授权票据。
struct ProductionAssetPendingConstructionConsent {
  std::size_t row_index = 0;
};
// 首个本人施工分支：当时 title 与真实初始用途都仍有效才写未封口同意。
// 后续必须在同候选完成真实材料、Action、S0 工期采用并 seal 原成功命令；
// 任何失败弃置整个候选，不许把本口作为独立的授予许可命令发布。
Outcome<ProductionAssetPendingConstructionConsent>
adopt_production_asset_construction_consent_in_candidate(
    ProductionAssetLifecycleWriteContext &,
    const FacilityConstructionAdoptionContext &, const StableId &actual_actor,
    const EconomicRuleRef &terms, const MomentOwnerSource &expected_title,
    const FacilitySourceRef &actual_initial_usage);
// 借尚未封口的真实候选行，仅供本次开工绑定 authority_sources 与回执。
Outcome<const ProductionAssetConstructionConsent *>
read_pending_production_asset_construction_consent(
    const ProductionAssetLifecycleWriteContext &,
    const ProductionAssetPendingConstructionConsent &);
// 同候选真实成功 Common 结果晚绑，再走唯一原同意 reader；没有实际
// construction_started/cost 和精确本人回执不能封口。仍由外层决定发布。
Status bind_production_asset_construction_command_result_in_candidate(
    ProductionAssetLifecycleWriteContext &,
    const FacilityConstructionAdoptionContext &,
    const ProductionAssetPendingConstructionConsent &,
    const MomentOwnerSource &actual_construction_started, const CommitResult &);
} // namespace hanmo::resident
