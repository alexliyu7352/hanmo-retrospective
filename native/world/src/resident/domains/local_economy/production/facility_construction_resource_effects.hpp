// 首次施工成本中的真实原语证据；不持 hot draft、今日余额或第二份材料账。
#pragma once
#include "resident/storage/resource_execution_draft.hpp"
namespace hanmo::resident {
struct ConstructionSelectedConsumptionEffect {
  ConsumeSelectedLots request;
  std::vector<ResourceInventoryExecutionDraft> inventories;
  std::vector<ResourceLotFlowDraft> flows;
  Integer next_instance_before = 0, next_instance_after = 0;
};
struct ConstructionWalletDeltaEffect {
  WalletDelta request;
  WalletView before, after;
};
struct ConstructionMoneyCostEffects {
  ConstructionWalletDeltaEffect debit, credit;
};
struct ConstructionStartCostEffects {
  std::vector<ConstructionSelectedConsumptionEffect> materials;
  std::optional<ConstructionMoneyCostEffects> payment;
};
struct ConstructionTerms;
struct ConstructionStartCost;
class CommonStore;
class CheckpointMemoryCharge;
// 热/冷唯一消费校验正文；只证明原消费，不授产权、场所或施工权限。
Status validate_facility_construction_start_cost_effects(
    const ConstructionTerms &, const ConstructionStartCost &);
// 只从同候选活动捕获取实际源；复制费用归真实 World candidate 持活。
Outcome<ConstructionStartCostEffects>
copy_facility_construction_start_cost_capture(
    const CommonStore &, std::size_t actual_capture,
    CheckpointMemoryCharge &actual_candidate_fields);
} // namespace hanmo::resident
