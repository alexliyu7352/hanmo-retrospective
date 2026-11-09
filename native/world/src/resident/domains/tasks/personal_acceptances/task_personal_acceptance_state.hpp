// 独立受领接触史只绑定原Task开工与资金/信件发放；旧Task及handoff字节不增加空字段。
#pragma once
#include "resident/domains/tasks/personal_messages/task_personal_message_state.hpp"
namespace hanmo::resident {
inline constexpr const char* task_personal_acceptance_started_kind="task_personal_acceptance_started";
inline constexpr const char* task_personal_acceptance_completed_kind="task_personal_acceptance_completed";
inline constexpr const char* task_personal_acceptance_interrupted_kind="task_personal_acceptance_interrupted";
// 只冻结原knowledge已有条目的值；它不是新发现事实，也不能授权新的资金来源。
struct TaskPersonalAcceptanceKnowledge {Integer discovered_tick=0;StableId source_kind,source_id;};
struct TaskPersonalAcceptanceStart {
 StableId fact_id,task_id,offer_id,policy_id,action_id,carrier_actor_id,sender_actor_id;
 StableId funding_wallet_id,budget_wallet_id,carrier_inventory_id,message_item_id;
 KnownPersonalDialogueProof sender_declaration;
 TaskPersonalAcceptanceKnowledge known_offer;
 Integer started_tick=0,budget=0;
 std::optional<TaskPersonalMessageResult> actual_result;
};
// 成功沿原预算转移与唯一信件lot，失败保原未拨款Task和原Action释放结果；不新发第二封信。
struct TaskPersonalAcceptanceEnd {
 StableId fact_id,start_fact_id,task_id,action_id,original_lot_id,reason;
 Integer tick=0;bool success=false,effect_task_id_present=true;
 std::optional<StableId> on_unavailable,retry;
 std::optional<TaskPersonalMessageResult> actual_result;
};
struct TaskPersonalAcceptanceState {Integer version=1;OrderedTable<TaskPersonalAcceptanceStart> starts;OrderedTable<TaskPersonalAcceptanceEnd> endings;};
BoundaryValue encode_task_personal_acceptances(const TaskPersonalAcceptanceState&);
Outcome<TaskPersonalAcceptanceState> decode_task_personal_acceptances(const BoundaryValue&);
Outcome<BoundaryValue> encode_task_personal_acceptance_fact(const TaskPersonalAcceptanceState&,const StableId& fact_id);
Status visit_task_personal_acceptances_checkpoint(CodecVisitor&,TaskPersonalAcceptanceState&);
}
