// 独立开工接触来源；不改TaskShopOrders1、原Action或唯一钱货/结算账的旧visitor。
#pragma once
#include "resident/storage/common_codec.hpp"
#include "resident/domains/interaction/interaction_model.hpp"
namespace hanmo::resident {
inline constexpr const char* task_shop_order_contact_module="task_shop_order_contacts";
inline constexpr const char* task_shop_order_contact_started_kind="task_shop_order_contact_started";
struct TaskShopOrderContactResult {
 StableId result_id,request_actor_id;
 Integer request_sequence=0,result_revision=0;
};
// 原work来自真实begin writer，接触头紧接同一次实际Action分配；不是今日会面补历史。
struct TaskShopOrderContactStart {
 StableId fact_id,task_id,order_id,acceptance_fact_id,order_issue_fact_id;
 StableId proprietor_actor_id,client_actor_id,action_id,session_id,dialogue_id,node_id;
 InteractionContext context;
 Integer started_tick=0;
 std::optional<TaskShopOrderContactResult> actual_result;
};
struct TaskShopOrderContactState {
 Integer version=1;
 OrderedTable<TaskShopOrderContactStart> starts;
};
BoundaryValue encode_task_shop_order_contacts(const TaskShopOrderContactState&);
Outcome<TaskShopOrderContactState> decode_task_shop_order_contacts(const BoundaryValue&);
Status visit_task_shop_order_contacts_checkpoint(CodecVisitor&,TaskShopOrderContactState&);
Outcome<BoundaryValue> encode_task_shop_order_contact_fact(const TaskShopOrderContactState&,const StableId&);
}
