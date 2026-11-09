// 私产处分前的Task责任纯读；死亡、零预留或零余额都不等于原责任已经解除。
#pragma once
#include "resident/domains/tasks/task_model.hpp"
namespace hanmo::resident {
struct TaskActorPendingResponsibilities {WorldIdentity source_identity;StableId actor_id;std::vector<StableId> pending_source_ids;};
// 只借已完整验证的同World Task/sidecar。相关Shipment尚无可靠终结证明时保守列入待清单。
Outcome<TaskActorPendingResponsibilities> read_task_actor_pending_responsibilities(const CommonStore&,const TaskState&,const TaskCatalog&,const StableId& actor_id);
}
