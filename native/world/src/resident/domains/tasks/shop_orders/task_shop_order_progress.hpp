// 本人计划只延续已实际开工的店单Action；完成读不因后来的取消改写早先失败结果。
#pragma once
#include "task_shop_order_execution.hpp"
namespace hanmo::resident {
struct TaskShopOrderActionSource {
    WorldIdentity source_identity;
    StableId actor_id,task_id,order_id,action_id,acceptance_fact_id;
    Integer accepted_tick=0,started_tick=0,deadline_tick=0;
    StableId status,fact_id,settlement_id,reason;
    std::optional<Integer> completed_tick;
    std::optional<TaskShopOrderAction> active_contact;
};
// 未知行动返回空；已识别但不是本人或坏账报错，防计划退回旅行猜成功。
// 调用前先完成同World完整Task/Shop冷验；此内部来源口不直接暴露给任意UI查询。
Outcome<std::optional<TaskShopOrderActionSource>> query_task_shop_order_action(const CommonStore&,const TaskState&,const TaskShopOrderState&,const StableId& actor_id,const StableId& action_id);
// 只放行同一原say会面等待已开工交付；不接管另一会话、不把say改成action节点waiting。
bool task_shop_order_continuation_meeting_matches(const TaskShopOrderActionSource&,const InteractionSession&);
// 只核这次真实完成的TaskEffect；外层仍须验证实际advance/plan回执及result分配区间。
Status validate_task_shop_order_action_effect(const TaskShopOrderActionSource&,const TaskEffect&,Integer completed_tick);
struct ReceiptDetails;
// 仅原Advance中的同Action完成条目可生成计划终态；无实际条目返回空，伪字段或错tick拒绝。
Outcome<std::optional<TaskShopOrderActionSource>> query_task_shop_order_completion(const CommonStore&,const TaskState&,const TaskShopOrderState&,const StableId& actor_id,const StableId& action_id,const ReceiptDetails&,Integer completed_tick);
}
