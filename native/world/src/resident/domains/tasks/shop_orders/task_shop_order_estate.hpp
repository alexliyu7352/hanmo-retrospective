// 限原未交订单的身后取消；执行人不成为Task本人，不获得新接单、交货或死者财产所有权。
#pragma once
#include "task_shop_order_execution.hpp"
namespace hanmo::resident {
struct TaskShopOrderEstateCancel {StableId shop_id,order_id,mandate_fact_id,death_notice_binding_fact_id;};
struct TaskShopOrderEstateCancellationQuote {
    WorldIdentity source_identity;
    StableId task_id;
    PrivateShopEstateCancellationRequest request;
    PrivateShopEstateCancellationSource authority;
    Integer reservation_amount=0;
};
// 本账关联结构只读不回调Shop，供原Task唯一执行读及S5逐单capture使用。
Status validate_task_shop_order_estate_links(const CommonStore&,const TaskShopOrderState&);
Status validate_task_shop_order_estate_layout(const CommonStore&,bool enabled,const std::vector<std::pair<StableId,StableId>>&,const TaskShopOrderState*);
// 完整权源晚门必须由同候选S5纯ledger Access借入，不能把新尾自身当许可。
Status validate_task_shop_order_estate_cancellations(const CommonStore&,const TaskState&,const TaskShopOrderState&,const PrivateShopEstateCancellationRead&);
Outcome<TaskShopOrderEstateCancellationQuote> preview_task_shop_order_estate_cancellation(const CommonStore&,const TaskState&,const TaskShopOrderState&,const PrivateShopOrderRead&,const PrivateShopEstateCancellationRead&,const StableId& actual_executor_actor_id,const StableId& task_id,const PrivateShopEstateCancellationRequest&);
// 原外层命令只给四个已知ID，亡者/执行人从实际合同与envelope分别取得。
Outcome<TaskShopOrderEstateCancellationQuote> preview_task_shop_order_estate_command(const CommonStore&,const TaskState&,const TaskShopOrderState&,const PrivateShopOrderRead&,const PrivateShopEstateCancellationRead&,const StableId& actual_executor_actor_id,const TaskShopOrderEstateCancel&);
// 每一单真实释放预约→原结算→本次执行来源→调用S5 capture，再开始下一单；外层负责完整候选回滚。
Outcome<PrivateShopOrderExecution> cancel_task_shop_order_estate_in_candidate(CommonStore&,TaskState&,TaskShopOrderState&,const PrivateShopOrderRead&,const PrivateShopEstateCancellationRead&,const TaskShopOrderEstateCancellationQuote&,const TaskShopOrderExecutionCapture&);
}
