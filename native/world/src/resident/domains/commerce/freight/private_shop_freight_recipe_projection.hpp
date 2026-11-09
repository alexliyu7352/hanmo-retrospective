// 将一份成功Common临时执行草稿投影到指定原lot；不是库存历史、携运授权或保货结论。
#pragma once
#include "private_shop_freight_resource_scope.hpp"
#include "resident/storage/resource_execution_draft.hpp"
namespace hanmo::resident {
// 只带真实所处容器的约束和该批完整值；没有按品种补配或合并别批的口。
struct PrivateShopFreightLotFrame {
    StableId inventory_id;
    Owner inventory_owner;
    Integer capacity=0,revision=0;
    LotView lot;
};
// 保原语种类和原次序，Owner/预约/容量及净零调整也能被上层明确识别。
struct PrivateShopFreightOperationProjection {
    std::size_t operation_index=0;
    ResourceOperation original_operation;
    std::vector<PrivateShopFreightLotFrame> before_lots,after_lots;
    std::vector<ResourceLotFlowDraft> original_flows;
};
struct PrivateShopFreightRecipeProjection {
    WorldIdentity source_identity;
    std::vector<PrivateShopFreightOperationProjection> operations;
    std::vector<PrivateShopFreightCargoSelection> remaining_lots;
};
// selected是调用前真实整批ID。原库存/来源完整性由调用者先验；这里仅验证一次草稿内的连续值。
// 只沿原loop实际split/transfer/integration映射扩展ID；耗尽移除，同品AddLot不会认作补回。
// 返回操作投影并不表示这些变更可用于资本化，直接state写者/跨recipe/任意before-fact史尚未证明。
Outcome<PrivateShopFreightRecipeProjection> project_private_shop_freight_recipe(
    const ResourceExecutionDraft&,const std::vector<PrivateShopFreightCargoSelection>& selected);
}
