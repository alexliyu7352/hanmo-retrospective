// 新独立段只记原deliver_message的接触/终结；不复制Task、信件、钱包或Known对白进度。
#pragma once
#include "resident/domains/interaction/personal_dialogues/known_personal_dialogue_contract.hpp"
namespace hanmo::resident {
inline constexpr const char* task_personal_message_started_kind="task_personal_message_started";
inline constexpr const char* task_personal_message_delivered_kind="task_personal_message_delivered";
inline constexpr const char* task_personal_message_interrupted_kind="task_personal_message_interrupted";
struct TaskPersonalMessageResult {
 StableId result_id,request_actor_id,command_kind;
 Integer request_sequence=0,result_revision=0,from_tick=0,result_tick=0;
};
struct TaskPersonalMessageStart {
 StableId fact_id,task_id,offer_id,policy_id,action_id;
 StableId carrier_actor_id,recipient_actor_id,item_id,original_lot_id,from_inventory_id,to_inventory_id;
 KnownPersonalDialogueProof recipient_declaration;
 Integer started_tick=0;
 std::optional<TaskPersonalMessageResult> actual_result;
};
struct TaskPersonalMessageEnd {
 StableId fact_id,start_fact_id,task_id,action_id,handoff_fact_id,reason;
 Integer tick=0;bool success=false,effect_task_id_present=true;
 std::optional<StableId> on_unavailable,retry;
 std::optional<TaskPersonalMessageResult> actual_result;
};
struct TaskPersonalMessageState {Integer version=1;OrderedTable<TaskPersonalMessageStart> starts;OrderedTable<TaskPersonalMessageEnd> endings;};
BoundaryValue encode_task_personal_messages(const TaskPersonalMessageState&);
Outcome<TaskPersonalMessageState> decode_task_personal_messages(const BoundaryValue&);
Outcome<BoundaryValue> encode_task_personal_message_fact(const TaskPersonalMessageState&,const StableId& fact_id);
Status visit_task_personal_messages_checkpoint(CodecVisitor&,TaskPersonalMessageState&);
}
