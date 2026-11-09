// Same original record/effect fields for JSON and bounded export shape.
#pragma once
#include "task_model.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
struct FactRecord;
// Emit fields into the caller-owned original row; successful false never touches the sink.
Outcome<bool> emit_task_action_payload_fields(ProjectionWriter&,const TaskState&,const ActionCoreView&);
Outcome<bool> emit_task_fact_payload_fields(ProjectionWriter&,const TaskState&,const FactRecord&);
Status emit_task_root_fields(ProjectionWriter&,const TaskState&);
Status emit_task_command_fields(ProjectionWriter&,const TaskCommand&);
Status emit_task_effect_fields(ProjectionWriter&,const TaskEffect&);
Status emit_task_state_fields(ProjectionWriter&,const TaskState&);
Status emit_task_record_fields(ProjectionWriter&,const TaskRecord&);
Status emit_task_action_fields(ProjectionWriter&,const TaskAction&);
Status emit_task_facts_fields(ProjectionWriter&,const TaskState&);
// 原 dynamic_tasks 段及 TaskCargo 全局事实与 JSON 同字段借读。
Status emit_dynamic_task_state_fields(ProjectionWriter&,const TaskState&);
Outcome<bool> emit_task_cargo_fact_payload_fields(ProjectionWriter&,const TaskState&,const FactRecord&);
Status emit_task_cargo_facts_fields(ProjectionWriter&,const TaskState&);
// 原三动作、采购/护送/货损事实逐头双向覆盖。只核 payload 所有权；
// 原 Task/数量合同/货损完整来源 validator 仍须单独执行。
Status validate_task_original_payload_headers(const CommonStore&,const TaskState&);

// 原附属段 whole 出口；版本门沿各段唯一字段表。
Status emit_task_extensions_fields(ProjectionWriter&,const TaskExtensionState&);
Status emit_task_personal_advances_fields(ProjectionWriter&,const TaskPersonalAdvanceState&);
Status emit_task_static_assets_fields(ProjectionWriter&,const TaskStaticAssetState&);
Status emit_task_public_account_execution_fields(ProjectionWriter&,const TaskPublicAccountExecutionState&);
Status emit_task_message_handoffs_fields(ProjectionWriter&,const TaskMessageHandoffState&);

struct RuntimeTaskOfferResponses;
Status emit_task_offer_response_fields(ProjectionWriter&,const TaskOfferResponseState&);
Status emit_runtime_task_offer_responses(ProjectionWriter&,std::string_view,const RuntimeTaskOfferResponses&);

Status emit_task_message_notices_fields(ProjectionWriter&,const TaskMessageNoticeState&);
}
