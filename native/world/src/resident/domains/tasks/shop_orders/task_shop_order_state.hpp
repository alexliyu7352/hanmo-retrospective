// 商铺订单只给原Task保存冻结来源和真实结算凭据；钱货仍唯一存在Common。
#pragma once
#include "hanmo/resident/checkpoint_memory.hpp"
#include "task_shop_order_contract.hpp"
#include "task_shop_order_estate_state.hpp"
#include "resident/domains/tasks/shop_order_contacts/task_shop_order_contact_state.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/domains/interaction/interaction_codec.hpp"
namespace hanmo::resident {
inline constexpr const char* task_shop_order_module="task_shop_orders";
inline constexpr const char* task_shop_order_accepted_kind="task_shop_order_accepted";
inline constexpr const char* task_shop_order_settled_kind="task_shop_order_settled";
inline constexpr const char* task_shop_order_interrupted_kind="task_shop_order_interrupted";
struct TaskShopOrderBinding {
    StableId fact_id,task_id,order_id,order_issue_fact_id,reservation_id;
    PrivateShopOrderTerms terms;
};
// 真实批次序与TransferLots同序；退货只引用此前真实交付的原收货批次，不造等价货。
struct TaskShopOrderLotLeg {
    StableId item_id,source_lot_id,destination_lot_id;
    Owner source_owner,destination_owner;
    Integer quantity=0,source_before=0,source_after=0;
};
struct TaskShopOrderSettlement {
    StableId fact_id,task_id,order_id,source_action_id,settlement_id,outcome,termination;
    StableId from_inventory_id,to_inventory_id,from_wallet_id,to_wallet_id;
    Owner from_container_owner,to_container_owner;
    Integer tick=0,action_started_tick=-1,delivered_quantity=0,paid_price=0,refunded_price=0;
    std::vector<TaskShopOrderLotLeg> lots;
};
// 完工仍必须回原两人当面context；此来源不让任意今日会话改绑旧工时。
struct TaskShopOrderAction {StableId action_id,task_id;InteractionContext context;StableId session_id,dialogue_id,node_id;Integer started_tick=0;};
// 原交付Action实际失败后保永久来源；不释放合同价款、不把失败记为完成或成本。
struct TaskShopOrderInterruption {StableId fact_id;TaskShopOrderAction source;Integer tick=0;StableId code;};
struct TaskShopOrderState {
    Integer version=1;
    // 只独立task_shop_order_estate_cancellations/1段持久化，旧本段visitor继续忽略。
    std::optional<TaskShopOrderEstateCancellationState> estate_cancellations;
    // 仅独立contacts/1编码；原TaskShop1 visitor继续忽略。
    std::optional<TaskShopOrderContactState> contacts;
    OrderedTable<TaskShopOrderBinding> bindings;
    OrderedTable<TaskShopOrderSettlement> settlements;
    OrderedTable<TaskShopOrderAction> actions;
    OrderedTable<TaskShopOrderInterruption> interruptions;
    // 只派生本人任务索引；不持订单发行/许可/成本/第二份可支配余额。
    CheckpointMap<StableId,std::vector<StableId>> by_proprietor;
    CheckpointMap<StableId,StableId> by_order,by_task;
};
// Common Action的JSON载荷与本域同一原工时来源逐字段对应，不另造行动账。
Outcome<TaskShopOrderAction> decode_task_shop_order_action(const BoundaryValue&);
BoundaryValue encode_task_shop_order_action(const TaskShopOrderAction&);
void rebuild_task_shop_order_index(TaskShopOrderState&);
Outcome<TaskShopOrderState> decode_task_shop_orders(const BoundaryValue&);
BoundaryValue encode_task_shop_orders(const TaskShopOrderState&);
Status visit_task_shop_orders_checkpoint(CodecVisitor&,TaskShopOrderState&);
}
