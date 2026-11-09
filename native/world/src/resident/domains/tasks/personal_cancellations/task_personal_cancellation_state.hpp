// 原私人销约只增接触见证；原Task终结、返还库存与预算仍由原cancel_task唯一写者持有。
#pragma once
#include "resident/domains/tasks/personal_messages/task_personal_message_state.hpp"
namespace hanmo::resident {
inline constexpr const char* task_personal_cancelled_kind="task_personal_message_cancelled";
struct TaskPersonalCancellationRecord {
 StableId fact_id,task_id,offer_id,policy_id,carrier_actor_id,sender_actor_id;
 StableId settlement_id,asset_receipt_fact_id,previous_task_status;
 KnownPersonalDialogueProof sender_declaration;
 Integer tick=0,refunded_budget=0;
 std::optional<TaskPersonalMessageResult> actual_result;
};
struct TaskPersonalCancellationState {Integer version=1;OrderedTable<TaskPersonalCancellationRecord> cancellations;};
BoundaryValue encode_task_personal_cancellations(const TaskPersonalCancellationState&);
Outcome<TaskPersonalCancellationState> decode_task_personal_cancellations(const BoundaryValue&);
Outcome<BoundaryValue> encode_task_personal_cancellation_fact(const TaskPersonalCancellationState&,const StableId& fact_id);
Status visit_task_personal_cancellations_checkpoint(CodecVisitor&,TaskPersonalCancellationState&);
}
