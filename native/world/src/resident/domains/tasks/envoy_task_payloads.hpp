// 公共冷边界仅持使者Task的具名引用，完整责任继续由唯一typed sidecar持有。
#pragma once
#include "envoy_task_validation.hpp"
namespace hanmo::resident {
class ProjectionWriter;
struct EnvoyTaskActionPayload {StableId action_id,kind,task_id,contract_id,acceptance_fact_id;};
struct EnvoyTaskPayloadRegistry {std::vector<FactDraft> facts;std::vector<EnvoyTaskActionPayload> actions;};
Outcome<EnvoyTaskPayloadRegistry> envoy_task_payload_registry(const CommonStore&,const TaskState&,const EnvoyTaskState&,const CommonCatalog&,const DiplomaticMissionRead&,const DiplomacyPresentationRead&,const DiplomaticEnvoyReportRead* reports=nullptr);
// 同原字段表借读已有数据；源注册和领域验证仍由原调用点完成。
Status emit_envoy_task_payloads_fields(ProjectionWriter&,const EnvoyTaskPayloadRegistry&);
BoundaryValue encode_envoy_task_payloads(const EnvoyTaskPayloadRegistry&);
Status validate_envoy_task_cold_payloads(const BoundaryValue& source,const EnvoyTaskPayloadRegistry&);
}
