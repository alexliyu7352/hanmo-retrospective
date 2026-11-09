// 原私人受领的计划适配只读永久来源和实际回执；权限与钱物仍由原Task执行。
#include "plan_binding.hpp"
#include "resident/model/procurement_timing.hpp"
#include "resident/kernel/world_bundle.hpp"
namespace hanmo::resident {
// 按永久Action识别已完成的受领，删除失败理由不能回退为旅行。
bool player_plan_personal_acceptance_action(const WorldBundle& w,const StableId& action){
    if(!w.catalog->task_personal_acceptances||!w.tasks.personal_acceptances)return false;
    for(const auto& row:w.tasks.personal_acceptances->starts.rows())if(row.second.action_id==action)return true;
    return false;
}
// 原Common头和永久开工的本人、开始时点与唯一半日必须一致。
bool player_plan_personal_acceptance_continuation(const CommonStore& common,const TaskState& tasks,const TaskPersonalAcceptanceState& state,const ActionCoreView& action){
    if(action.kind!="accept_offer")return false;
    auto source=query_task_personal_acceptance_action(common,tasks,state,action.actor_id,action.id);
    return source.ok&&source.value&&source.value->status=="active"&&source.value->actor_id==action.actor_id&&source.value->action_id==action.id&&source.value->started_tick==action.start_tick&&source.value->remaining_ticks==action.remaining_ticks&&action.remaining_ticks==task_handover_ticks;
}
// 旧任务受领没有新来源，不因同kind自动获准Continue。
bool player_plan_personal_acceptance_continuation(const WorldBundle& w,const ActionCoreView& action){
    return w.catalog->task_personal_acceptances&&w.tasks.personal_acceptances&&player_plan_personal_acceptance_continuation(w.common,w.tasks,*w.tasks.personal_acceptances,action);
}
// 原SAY必须真实存在，不能把空会话集合或通用waiting当作本人许可。
bool player_plan_personal_acceptance_meetings_match(const TaskPersonalAcceptanceActionSource& source,const std::vector<InteractionSession>& sessions){
    bool found=false;for(const auto& session:sessions)if(session.actor_id==source.actor_id){if(found||!task_personal_acceptance_continuation_meeting_matches(source,session))return false;found=true;}return found;
}
// 实际结果在World提交尾先绑定；这里不递归世界或补造来源。
Outcome<std::optional<TaskPersonalAcceptanceActionSource>> player_plan_personal_acceptance_completion(const WorldBundle& w,const StableId& actor,const StableId& action,const CommitResult& result){
    if(!w.catalog->task_personal_acceptances)return {true,std::nullopt,{}};
    if(!w.tasks.personal_acceptances)return {false,{},common_failure("TaskPersonalAcceptanceProfileRequired").error};
    return query_task_personal_acceptance_completion(w.common,w.tasks,*w.tasks.personal_acceptances,actor,action,result);
}
// 原计划只允许Continue指向这个既存Action，成功不添业务code，失败保原Task原因。
Status validate_player_plan_personal_acceptance_completion(const CommonStore& common,const TaskState& tasks,const TaskPersonalAcceptanceState& messages,const StableId& actor,const PlayerPlanStep& step,const CommitResult& result){
    const auto* continued=std::get_if<PlayerPlanContinue>(&step.data);
    if(!step.completed_tick||!step.action_id||!continued||continued->action_id!=*step.action_id||!result.details||step.completion_result_id!=std::optional<StableId>{result.result_id}||result.tick!=*step.completed_tick||step.source.kind!=PlayerPlanSourceKind::existing_action||step.source.source_id!=*step.action_id)return common_failure("PlayerPlanPersonalAcceptanceMismatch",step.id);
    auto source=query_task_personal_acceptance_completion(common,tasks,messages,actor,*step.action_id,result);if(!source.ok)return {false,{},source.error};
    if(!source.value||source.value->actor_id!=actor||source.value->started_tick>step.source.observed_tick||(step.phase==PlayerPlanStepPhase::completed)!=(source.value->status=="accepted")||step.stop_reason!=(source.value->status=="accepted"?StableId{}:source.value->reason))return common_failure("PlayerPlanPersonalAcceptanceMismatch",step.id);
    return common_success();
}
}
