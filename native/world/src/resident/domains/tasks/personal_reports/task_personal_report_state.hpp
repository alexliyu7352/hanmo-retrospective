// 只保存原report_offer的真实接触及完成来源；原Task结酬、余额、关系和handoff报告仍只有原账。
#pragma once
#include "resident/domains/tasks/personal_messages/task_personal_message_state.hpp"
namespace hanmo::resident {
inline constexpr const char* task_personal_report_started_kind="task_personal_report_started";
inline constexpr const char* task_personal_report_completed_kind="task_personal_report_completed";
inline constexpr const char* task_personal_report_interrupted_kind="task_personal_report_interrupted";
struct TaskPersonalReportStart {
 StableId fact_id,task_id,offer_id,policy_id,action_id,handoff_fact_id;
 StableId carrier_actor_id,sender_actor_id;
 KnownPersonalDialogueProof sender_declaration;
 Integer started_tick=0,reward=0;
 std::optional<TaskPersonalMessageResult> actual_result;
};
struct TaskPersonalReportEnd {
 StableId fact_id,start_fact_id,task_id,action_id,report_fact_id,settlement_id,reason;
 Integer tick=0;bool success=false,effect_task_id_present=true;
 std::optional<StableId> on_unavailable,retry;
 std::optional<TaskPersonalMessageResult> actual_result;
};
struct TaskPersonalReportState {Integer version=1;OrderedTable<TaskPersonalReportStart> starts;OrderedTable<TaskPersonalReportEnd> endings;};
BoundaryValue encode_task_personal_reports(const TaskPersonalReportState&);
Outcome<TaskPersonalReportState> decode_task_personal_reports(const BoundaryValue&);
Outcome<BoundaryValue> encode_task_personal_report_fact(const TaskPersonalReportState&,const StableId& fact_id);
Status visit_task_personal_reports_checkpoint(CodecVisitor&,TaskPersonalReportState&);
}
