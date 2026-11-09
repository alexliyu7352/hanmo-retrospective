// 同候选原送信规则的准备/记账借口；没有任意通知命令，不越过原人物接触、任务和货权检查。
#pragma once
#include "task_message_handoff_state.hpp"
#include "resident/domains/tasks/task_model.hpp"
namespace hanmo::resident {
struct TaskMessageHandoffPreparation {TaskMessageHandoff receipt;Integer recipient_quantity_before=0;std::vector<StableId> recipient_lots_before;};
// 原finish已完成现场准入后调用；此读口再核真实Common Action和精确一份预留任务信件。
Outcome<TaskMessageHandoffPreparation> prepare_task_message_handoff(const CommonStore&,const TaskState&,const TaskCatalog&,const CommonCatalog&,const TaskMessageHandoffState&,const ActionCoreView&,Integer tick,const DynamicStepInput* phase=nullptr);
// 必须在原TransferLots成功之后同候选调用，只登记事实，不再搬货或改Task状态。
Status record_task_message_handoff_after_resources(CommonStore&,TaskMessageHandoffState&,const TaskMessageHandoffPreparation&);
// 独立段的事实头、原人物/Task/Action/目录/时间和派生索引缺一不可；当前持信位置不反造历史。
Status validate_task_message_handoffs(const CommonStore&,const TaskState&,const TaskCatalog&,const CommonCatalog&,const TaskMessageHandoffState&,const DynamicStepInput* phase=nullptr);
Status validate_task_message_handoff_layout(const CommonStore&,bool enabled,const std::vector<std::pair<StableId,StableId>>& modules,const TaskMessageHandoffState*,Integer expected_version=1);
// 只证明观察者亲自交付或接收此原信件；不证明信中任意消息为真，也不自动授予第三方知情。
Outcome<std::optional<TaskMessageHandoff>> read_known_task_message_handoff(const CommonStore&,const TaskState&,const TaskCatalog&,const CommonCatalog&,const TaskMessageHandoffState&,const StableId& observer,const StableId& fact_id,const DynamicStepInput* phase=nullptr);
// 页从本人两种参与角色的派生索引读取，带实际World身份；查询无RNG、编号或任何业务写入。
Outcome<BoundaryValue> query_task_message_handoffs(const CommonStore&,const TaskState&,const TaskCatalog&,const CommonCatalog&,const TaskMessageHandoffState&,Integer cursor=0);
}
