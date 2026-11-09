// 原半日真实来源供计划/本人责任读，资金、库存与业务决定仍走原店单writer。
#include "task_shop_order_progress.hpp"
#include "resident/kernel/command_types.hpp"
namespace hanmo::resident {
Outcome<std::optional<TaskShopOrderActionSource>> query_task_shop_order_action(const CommonStore& common,const TaskState& tasks,const TaskShopOrderState& state,const StableId& actor,const StableId& action){
    auto valid=validate_task_shop_order_ledger(common,tasks,state);if(!valid.ok)return {false,{},valid.error};if(!common.actor(actor))return {false,{},common_failure("UnknownActor",actor).error};TaskShopOrderActionSource result;result.source_identity=common.state().identity;result.actor_id=actor;result.action_id=action;
    if(const auto* active=state.actions.find(action)){result.task_id=active->task_id;result.started_tick=active->started_tick;result.status="active";result.active_contact=*active;}
    else {
        for(const auto& row:state.settlements.rows())if(row.second.outcome=="delivered"&&row.second.source_action_id==action){result.task_id=row.second.task_id;result.started_tick=row.second.action_started_tick;result.status="delivered";result.reason="TaskShopOrderDelivered";result.fact_id=row.first;result.settlement_id=row.second.settlement_id;result.completed_tick=row.second.tick;}
        for(const auto& row:state.interruptions.rows())if(row.second.source.action_id==action){result.task_id=row.second.source.task_id;result.started_tick=row.second.source.started_tick;result.status="interrupted";result.fact_id=row.first;result.reason=row.second.code;result.completed_tick=row.second.tick;}
    }
    if(result.task_id.empty())return {true,std::nullopt,{}};const auto at=state.by_task.find(result.task_id);const auto* bound=at==state.by_task.end()?nullptr:state.bindings.find(at->second);if(!bound)return {false,{},common_failure("TaskShopOrderActionSourceMissing",action).error};if(bound->terms.proprietor_actor_id!=actor)return {false,{},common_failure("TaskShopOrderActionNotOwned",action).error};result.order_id=bound->order_id;result.acceptance_fact_id=bound->fact_id;result.accepted_tick=bound->terms.accepted_tick;result.deadline_tick=bound->terms.deadline_tick;return {true,std::move(result),{}};
}
bool task_shop_order_continuation_meeting_matches(const TaskShopOrderActionSource& source,const InteractionSession& session){
    if(source.status!="active"||!source.active_contact||source.completed_tick||session.actor_id!=source.actor_id||session.phase!="say"||!session.pending_action_id.empty()||session.pending_command)return false;const auto& original=*source.active_contact;const auto& a=original.context;const auto& b=session.context;return original.action_id==source.action_id&&original.task_id==source.task_id&&original.started_tick==source.started_tick&&session.id==original.session_id&&session.dialogue_id==original.dialogue_id&&session.node_id==original.node_id&&a.place_id==b.place_id&&a.provider_actor_id==b.provider_actor_id&&a.role_id==b.role_id&&a.kind==b.kind&&a.encounter_id==b.encounter_id&&a.input_error==b.input_error;
}
Status validate_task_shop_order_action_effect(const TaskShopOrderActionSource& source,const TaskEffect& effect,Integer tick){
    if((source.status!="delivered"&&source.status!="interrupted")||source.fact_id.empty()||!source.completed_tick||*source.completed_tick!=tick||effect.task_id!=std::optional<StableId>{source.task_id}||effect.success!=(source.status=="delivered")||effect.code!=(source.status=="delivered"?std::optional<StableId>{}:std::optional<StableId>{source.reason})||effect.on_unavailable||effect.retry)return common_failure("TaskShopOrderCompletionSourceMismatch",source.action_id);return common_success();
}
Outcome<std::optional<TaskShopOrderActionSource>> query_task_shop_order_completion(const CommonStore& common,const TaskState& tasks,const TaskShopOrderState& state,const StableId& actor,const StableId& action,const ReceiptDetails& receipt,Integer tick){
    auto source=query_task_shop_order_action(common,tasks,state,actor,action);if(!source.ok||!source.value)return source;const auto* advance=std::get_if<AdvanceDetails>(&receipt.data);const auto* completed=advance?advance->completed_actions.find(action):nullptr;if(!completed)return {true,std::nullopt,{}};
    if(tick<0||tick>common.tick()||!completed->ok||completed->completed||completed->success||completed->already_completed||completed->code||completed->military||completed->family||completed->growth||!completed->task)return {false,{},common_failure("TaskShopOrderCompletionSourceMismatch",action).error};auto valid=validate_task_shop_order_action_effect(*source.value,*completed->task,tick);if(!valid.ok)return {false,{},valid.error};return source;
}

}
