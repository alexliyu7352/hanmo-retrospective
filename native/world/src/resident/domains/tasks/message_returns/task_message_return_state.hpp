// 回书来源独立于原Task、去程handoff与返报v2；不保存第二份余额、截止或成功状态。
#pragma once
#include "resident/storage/common_codec.hpp"
#include "resident/domains/interaction/interaction_model.hpp"
namespace hanmo::resident {
inline constexpr const char* task_message_return_module="task_message_returns";
inline constexpr const char* task_message_return_started_kind="task_message_return_started";
inline constexpr const char* task_message_return_collected_kind="task_message_return_collected";
inline constexpr const char* task_message_return_interrupted_kind="task_message_return_interrupted";
inline constexpr const char* task_message_return_report_started_kind="task_message_return_report_started";
inline constexpr const char* task_message_return_report_interrupted_kind="task_message_return_report_interrupted";
inline constexpr const char* task_message_return_delivered_kind="task_message_return_delivered";
// 外层真实提交之后才绑定；推进可完成多名原承运人的行动，不把request_actor冒业务参与者。
struct TaskMessageReturnResult {
 StableId result_id,request_actor_id,command_kind;
 Integer request_sequence=0,result_revision=0,from_tick=0,result_tick=0;
};
// 原收件者的当前SAY在真实开工时冻结；身份与原去程handoff逐项对应，不读远方现状。
struct TaskMessageReturnStart {
 StableId fact_id,task_id,offer_id,policy_id,handoff_fact_id,action_id;
 StableId carrier_actor_id,recipient_actor_id,sender_actor_id,reply_item_id,carrier_inventory_id;
 StableId session_id,dialogue_id,node_id;InteractionContext context;
 Integer started_tick=0;
 std::optional<TaskMessageReturnResult> actual_result;
};
// 成功只保存实际新增批次；失败保存同一原Action与原因，不分配信件或改Task责任。
struct TaskMessageReturnCollection {
 StableId fact_id,start_fact_id,task_id,action_id,reply_lot_id,reason;
 Integer tick=0;
 bool success=false;
 std::optional<TaskMessageReturnResult> actual_result;
};
// 原report Action的真实预约接触；该段不复制原报酬或预算，只固定本次必须交回的批次。
struct TaskMessageReturnReportStart {
 StableId fact_id,task_id,collection_fact_id,action_id,original_lot_id,from_inventory_id,to_inventory_id;
 StableId carrier_actor_id,sender_actor_id,session_id,dialogue_id,node_id;InteractionContext context;
 Integer started_tick=0;
 std::optional<TaskMessageReturnResult> actual_result;
};
// 原report失败仍待履责，保原TaskEffect可见形状与同一Action，不假装已经完成回书实转。
struct TaskMessageReturnReportInterruption {
 StableId fact_id,report_start_fact_id,task_id,action_id,reason;
 Integer tick=0;bool effect_task_id_present=false;
 std::optional<StableId> on_unavailable,retry;
 std::optional<TaskMessageReturnResult> actual_result;
};
// 本人返报仍用原Task唯一结酬；此凭据仅证明那次原report实际转移了原回书。
struct TaskMessageReturnDelivery {
 StableId fact_id,task_id,collection_fact_id,report_start_fact_id,source_action_id,report_fact_id,settlement_id;
 StableId original_lot_id,recipient_lot_id,from_inventory_id,to_inventory_id;
 Integer tick=0;
 std::optional<TaskMessageReturnResult> actual_result;
};
struct TaskMessageReturnState {
 Integer version=1;
 OrderedTable<TaskMessageReturnStart> starts;
 OrderedTable<TaskMessageReturnCollection> collections;
 OrderedTable<TaskMessageReturnDelivery> deliveries;
 OrderedTable<TaskMessageReturnReportStart> report_starts;
 OrderedTable<TaskMessageReturnReportInterruption> report_interruptions;
};
BoundaryValue encode_task_message_returns(const TaskMessageReturnState&);
Outcome<TaskMessageReturnState> decode_task_message_returns(const BoundaryValue&);
Outcome<BoundaryValue> encode_task_message_return_fact(const TaskMessageReturnState&,const StableId& fact_id);
Status visit_task_message_returns_checkpoint(CodecVisitor&,TaskMessageReturnState&);
}
