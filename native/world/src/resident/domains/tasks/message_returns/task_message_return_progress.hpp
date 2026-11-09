// 计划只继续本人已经实际开工的接回书Action；不能把尚未取回书的任务猜成普通旅行。
#pragma once
#include "task_message_return_contract.hpp"
namespace hanmo::resident {
struct TaskMessageReturnActionSource {
 WorldIdentity source_identity;StableId actor_id,task_id,action_id,start_fact_id,status,fact_id,reason,actual_result_id;
 Integer started_tick=0,remaining_ticks=0;std::optional<Integer> completed_tick;
 StableId session_id,dialogue_id,node_id;InteractionContext context;
};
// 调用前同World完整回书、人物与结果先验；未知行动空，已识别而错本人/坏源报错。
Outcome<std::optional<TaskMessageReturnActionSource>> query_task_message_return_action(const CommonStore&,const TaskMessageReturnState&,const StableId& actor,const StableId& action);
bool task_message_return_continuation_meeting_matches(const TaskMessageReturnActionSource&,const InteractionSession&);
struct ReceiptDetails;
Outcome<std::optional<TaskMessageReturnActionSource>> query_task_message_return_completion(const CommonStore&,const TaskMessageReturnState&,const StableId& actor,const StableId& action,const ReceiptDetails&,Integer tick,const StableId& actual_result_id);
}
