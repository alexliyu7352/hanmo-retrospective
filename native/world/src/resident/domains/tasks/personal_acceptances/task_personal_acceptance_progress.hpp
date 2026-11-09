// 计划只继续本人已经实际开工的受领Action；不能把尚未开始的委托猜成普通旅行。
#pragma once
#include "task_personal_acceptance_contract.hpp"
namespace hanmo::resident {
struct TaskPersonalAcceptanceActionSource {
 WorldIdentity source_identity;StableId actor_id,task_id,action_id,start_fact_id,status,fact_id,reason,actual_result_id;
 Integer started_tick=0,remaining_ticks=0;std::optional<Integer> completed_tick;
 StableId session_id,dialogue_id,node_id;InteractionContext context;
};
// 调用前同World完整信件、人物与结果先验；未知行动空，已识别而错本人/坏源报错。
Outcome<std::optional<TaskPersonalAcceptanceActionSource>> query_task_personal_acceptance_action(const CommonStore&,const TaskState&,const TaskPersonalAcceptanceState&,const StableId& actor,const StableId& action);
bool task_personal_acceptance_continuation_meeting_matches(const TaskPersonalAcceptanceActionSource&,const InteractionSession&);
struct ReceiptDetails;
Outcome<std::optional<TaskPersonalAcceptanceActionSource>> query_task_personal_acceptance_completion(const CommonStore&,const TaskState&,const TaskPersonalAcceptanceState&,const StableId& actor,const StableId& action,const ReceiptDetails&,Integer tick,const StableId& actual_result_id);
struct CommitResult;
// 显式真实提交重载覆盖原六种外层；隐藏推进只认当前缓存中的实际对象和永久完成绑定。
Outcome<std::optional<TaskPersonalAcceptanceActionSource>> query_task_personal_acceptance_completion(const CommonStore&,const TaskState&,const TaskPersonalAcceptanceState&,const StableId& actor,const StableId& action,const CommitResult& actual_result);

}
