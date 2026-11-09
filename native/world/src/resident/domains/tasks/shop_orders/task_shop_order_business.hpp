// 店单三命令共用真实Task预览/采用；S5绑定及成本capture是同候选必需步骤，不允许半边提交。
#pragma once
#include "task_shop_order_view.hpp"
namespace hanmo::resident {
struct TaskShopOrderQuote {
    WorldIdentity source_identity;StableId actor_id;TaskShopOrderCommand command;
    PrivateShopOrderTerms terms;
    std::optional<TaskShopOrderAcceptQuote> acceptance;
    std::optional<TaskShopOrderDeliveryQuote> delivery;
};
struct TaskShopOrderCommit {StableId task_id,order_id,shop_id,source_fact_id,action_id;};
struct TaskShopOrderBusinessEffects {
    std::function<Status(const TaskShopOrderAcceptanceSource&)> bind;
    TaskShopOrderExecutionCapture capture;
};
// 接头所有引用仅活在这次调用栈；价格/账户/批次不接受外部命令输入。
Outcome<TaskShopOrderQuote> preview_task_shop_order_business(const CommonStore&,const TaskState&,const TaskShopOrderState&,
    const TaskShopOrderViewAccess&,const StableId& actor,const TaskShopOrderCommand&);
// 外层必须共同保存Common/完整Task/Shop；本口任何失败不得发布部分已写结果。
Outcome<TaskShopOrderCommit> commit_task_shop_order_business_in_candidate(CommonStore&,TaskState&,TaskShopOrderState&,
    const TaskShopOrderViewAccess&,const TaskShopOrderQuote&,const TaskShopOrderBusinessEffects&);
// 原半日交付成功后必须立即由S5认实际成本；中断不清责任，也不虚构成交capture。
Outcome<TaskEffect> finish_task_shop_order_business_in_candidate(CommonStore&,TaskState&,TaskShopOrderState&,
    const TaskShopOrderViewAccess&,const StableId& action_id,const DynamicStepInput&,const TaskShopOrderExecutionCapture&);
BoundaryValue encode_task_shop_order_quote(const TaskShopOrderQuote&);
}
