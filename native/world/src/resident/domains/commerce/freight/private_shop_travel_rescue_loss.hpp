// 原finish_rescue的实际货损纯读；只有同次完成与资源执行同时成立，不能由今天缺货补旧损失。
#pragma once
#include "resident/domains/travel/travel_rescue_capture.hpp"
#include "resident/storage/resource_execution_draft.hpp"
#include <memory>
namespace hanmo::resident {
class CommonStore;struct ResourceExecutionCaptureIdentity;
// 在真实rescue减到0后、原finish之前拥有值；绑定同Common及原capture生命周期，不跨提交使用。
struct PrivateShopTravelRescueLossBoundary {
 const CommonStore* original_common=nullptr;const TravelState* original_travel=nullptr;const TravelCatalog* original_catalog=nullptr;
 std::shared_ptr<const ResourceExecutionCaptureIdentity> capture_identity;
 WorldIdentity identity;ActionCoreView action;RescueParams rescue;
 StableId origin_entry_place;Location before_location;InventoryView before_inventory;
 Integer next_instance_before=0;std::size_t recipe_begin=0;
};
// 小批原损耗为0仍保实际零腿，不合并各批后再取整，不改变任何原预约或产权。
struct PrivateShopTravelRescueLotLoss {StableId lot_id,item_id;Integer original_quantity=0,lost_quantity=0,remaining_quantity=0;};
struct PrivateShopTravelRescueLoss {
 WorldIdentity source_identity;TravelRescueCompletionDraft original_completion;
 ResourceExecutionDraft actual_recipe;std::vector<PrivateShopTravelRescueLotLoss> personal_losses;
};
// 原救助资格、减步和完整行旅源由caller负责；这个口不发起救助、扣货或判断是否到商铺。
Outcome<PrivateShopTravelRescueLossBoundary> capture_private_shop_travel_rescue_loss_boundary(
 const CommonStore&,const TravelState&,const TravelCatalog&,const StableId& actual_action_id);
// 必须紧接原finish_rescue成功且尚未release/clock_commit：实际返起点及唯一AdjustLots逐批核对。
// 失败、换捕获、夹入另一recipe、缺零腿/改他人货均拒；此结果不是退款、付款或到店资本化来源。
Outcome<PrivateShopTravelRescueLoss> read_private_shop_travel_rescue_loss(
 const CommonStore&,const TravelState&,const TravelCatalog&,
 const PrivateShopTravelRescueLossBoundary&,const TravelRescueCompletionDraft& actual_completion);
}
