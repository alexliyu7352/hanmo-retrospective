// 原 PlanFacilityCapacity 完整私有候选的开工步骤，不独立发布费用/Action。
#pragma once
#include "facility_construction_cost_writer.hpp"
#include "facility_construction_service.hpp"
#include "resident/kernel/fixed_service_runtime.hpp"

namespace hanmo::resident {
struct FacilityConstructionStartWriteContext {
  FacilityConstructionCostWriteContext &cost;
  const FacilityConstructionServiceContext &service;
  ActionElapsedStore &elapsed;
  const ActivityRead &activity;
  // 必须是中央同 job 真专业 owner；prepare/apply 会反读真实采用，不能给空实现。
  const ActionTimeOriginalSourceRead &action_sources;
};
struct PendingFacilityConstructionStart {
  std::size_t row_index = 0;
  ProductionAssetPendingConstructionConsent consent;
  PendingFacilityConstructionCost cost;
};
// 本域唯一 compiled Fixed 注册。这里只定义原 owner 身份，不启模块/发布注册。
const ActionTimeRegistration &facility_construction_action_time_registration();
// 本人空闲/身体/实际地点→真实同意→原Consume/钱腿→原Action→原Fixed采用。
// 返回仍是未封口的私有候选；失败必须丢整个 World，不能保留已收费子步骤。
Outcome<PendingFacilityConstructionStart>
start_facility_construction_in_candidate(
    FacilityConstructionStartWriteContext &, const StableId &actual_actor,
    const EconomicRuleRef &actual_terms,
    const MomentOwnerSource &expected_title,
    const FacilitySourceRef &actual_initial_usage,
    std::span<const CargoSelection> exact_materials);
// 仅把原 S0 成功结果晚绑到已实际采用的同一次工程，随后完整 owner 校验
// 仍由正式 World 发布闸执行；本口没有 install/commit 或单独成功接口。
Status bind_facility_construction_start_command_in_candidate(
    FacilityConstructionStartWriteContext &,
    const PendingFacilityConstructionStart &, const CommitResult &);
} // namespace hanmo::resident
