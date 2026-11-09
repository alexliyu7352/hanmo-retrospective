// 本人普通委托完成证据只读原任务账；人物卡消费来源，不持另一份任务计数器。
#pragma once
#include "task_model.hpp"
#include <array>
namespace hanmo::resident {
struct TaskCompletionEvidence {StableId task_id,template_id,settlement_id;Integer reported_tick=0;};
// 输入须属于同一已完整验证的世界候选；只返回原三类委托的本人成功，治理/未知类型不提供证据。
// 迟到但原合同判为success仍属完成；本接口不能证明全生涯无侵占，也不授予卡片。
Outcome<std::vector<TaskCompletionEvidence>> task_completion_evidence_for_actor(const TaskState&,const TaskCatalog&,const StableId& actor_id);
Outcome<std::array<std::optional<TaskCompletionEvidence>,2>> read_first_two_procurement_completion_evidence(const TaskState&,const TaskCatalog&,const StableId& actor_id);
}
