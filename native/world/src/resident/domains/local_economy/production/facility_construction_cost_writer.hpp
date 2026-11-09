// 原 PlanFacilityCapacity 同候选的真实开工消费，不是独立支付/授权命令。
#pragma once
#include "facility_construction_cost_capture.hpp"
#include "production_asset_construction_candidate.hpp"
#include <span>

namespace hanmo::resident {
struct FacilityConstructionCostWriteContext {
  CommonStore &common;
  FacilityConstructionState &state;
  CheckpointMemoryCharge &owned_fields;
  const FacilityConstructionAdoptionContext &read;
  ProductionAssetLifecycleWriteContext &assets;
};
// 仅内部成本行索引；尚未有真实 Action、工期采用和成功 command 的候选
// 不得发布。任一失败整 World 候选弃置，不能单独提交已扣材料或同意。
struct PendingFacilityConstructionCost {
  std::size_t row_index = 0;
};
// 首个本人施工分支只用本人实际库存/批次与私人钱包；不借初始资产 Owner
// 推断材料权，不替换未选批，也不从余额差补记已耗。调用方先取得原同意，
// 本口执行原 ConsumeSelectedLots 与付款双腿并立即反读实际 capture。
Outcome<PendingFacilityConstructionCost>
apply_facility_construction_start_cost_in_candidate(
    FacilityConstructionCostWriteContext &,
    const ProductionAssetPendingConstructionConsent &,
    const ProducedStableId &actual_construction_id,
    std::span<const CargoSelection> exact_materials);
} // namespace hanmo::resident
