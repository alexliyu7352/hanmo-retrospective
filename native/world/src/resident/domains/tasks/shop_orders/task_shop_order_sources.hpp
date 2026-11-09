// 原Task侧账消费者按具名店单来源分流；不将店单伪装成普通Offer，也不删原责任。
#pragma once
#include "task_shop_order_execution.hpp"
#include "resident/domains/tasks/task_fulfilled_clients.hpp"
namespace hanmo::resident {
// 仅当前真实资金/履约未结者进入本人责任；原客户和店主各保自己的主体身份。
Outcome<std::vector<StableId>> read_task_shop_order_actor_pending(const CommonStore&,const TaskState&,const TaskShopOrderState&,const StableId& actor);
// 只接受当前边界或原family_inheritance事实；晚终结不能倒填早交割。
Status validate_task_shop_order_actor_history(const CommonStore&,const TaskState&,const TaskShopOrderState&,const StableId& actor,const StableId& inheritance_fact={},Integer tick=-1);
// 原本人手记追加已知冻结条款和真实结果；没有普通Offer定义不阻断全手记。
Outcome<std::vector<StableId>> task_shop_order_journal_lines(const CommonStore&,const TaskState&,const TaskShopOrderState&,const TaskCatalog&,const StableId& actor,const StableId& task_id);
// 只把真正delivered全价两腿计客户；初店前/当前口继续使用同一永久来源。
Outcome<TaskFulfilledClients> read_task_shop_order_fulfilled_clients(const CommonStore&,const TaskState&,const TaskShopOrderState&,const StableId& actor,const TaskFulfilledClientBoundary* before=nullptr,const DynamicStepInput* phase=nullptr);
// Common预约登记只认本表真实active Task的一次合同，不按ID前缀自动放行。
bool task_shop_order_reservation_exists(const TaskState&,const TaskShopOrderState&,const StableId& reservation_id);
}
