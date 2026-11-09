// 治理公共动作/事实只投影本域完整typed来源；冷JSON不能成为另一套可执行任务参数。
#pragma once
#include "governance_task_rules.hpp"
namespace hanmo::resident {
class ProjectionWriter;
// 原Action只需链接唯一任务/契约/接受事实；工时和占用不复制进payload。
struct GovernanceTaskActionPayload { StableId action_id,task_id,contract_id,acceptance_fact_id; };
struct GovernanceTaskPayloadRegistry { std::vector<FactDraft> facts;std::vector<GovernanceTaskActionPayload> actions; };
// 完整交叉验证后才枚举本域事实和仍在场的原Action，并拒绝无sidecar的孤立公共记录。
Outcome<GovernanceTaskPayloadRegistry> governance_task_payload_registry(const CommonStore&,const TaskState&,const GovernanceTaskState&);
// 仅冷导出使用：facts是原具名kind/domain头，完整责任仍在governance_tasks独立域。
// 同原字段表借读已有数据；源注册和领域验证仍由原调用点完成。
Status emit_governance_task_payloads_fields(ProjectionWriter&,const GovernanceTaskPayloadRegistry&);
BoundaryValue encode_governance_task_payloads(const GovernanceTaskPayloadRegistry&);
// 冷恢复逐一核共同params/fact头与sidecar派生值相同；不执行这些参数，也不填缺失块。
Status validate_governance_task_cold_payloads(const BoundaryValue& source,const GovernanceTaskPayloadRegistry&);
}
