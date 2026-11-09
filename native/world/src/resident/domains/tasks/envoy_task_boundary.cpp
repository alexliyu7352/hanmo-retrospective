// 任务期限不能因本人正在旅行或没有点击交接而停止；冻结条款始终是唯一截止来源。
#include "envoy_task_boundary.hpp"
#include <limits>
namespace hanmo::resident {
Status reconcile_envoy_tasks_boundary(CommonStore& common,TaskState& tasks,EnvoyTaskState& envoy,const DiplomaticMissionRead& missions,const DiplomacyPresentationRead& presentations,const DynamicStepInput& step){
    if(step.from_tick<0||step.from_tick==std::numeric_limits<Integer>::max()||step.to_tick!=step.from_tick+1||common.tick()!=step.to_tick)return common_failure("InvalidEnvoyBoundary",{},ErrorKind::internal);
    if((envoy.version!=1&&envoy.version!=2))return common_failure("UnsupportedEnvoyTaskVersion");
    EnvoyTaskRead read(common,tasks,envoy,missions,presentations);
    for(auto& row:envoy.envoy_contracts.rows()){
        auto& contract=row.second;if(contract.action||contract.delivery||contract.completion||contract.acceptance.accepted_tick>step.from_tick)continue;
        auto source=read.envoy_task_source(row.first);if(!source.ok)return {false,{},source.error};const auto& a=source.value.acceptance;
        auto continuity=missions.diplomatic_mission_continuity(a.mission_authority_fact_id,a.envoy_actor_id);if(!continuity.ok)return {false,{},continuity.error};const auto& d=continuity.value;StableId failure;
        switch(d.kind){
            case DiplomaticMissionContinuityKind::terminate_envoy_dead:{const auto* actor=common.actor(a.envoy_actor_id);if(!actor||actor->alive)return common_failure("EnvoyDeathSourceMismatch",row.first,ErrorKind::internal);failure="EnvoyDead";break;}
            case DiplomaticMissionContinuityKind::terminate_principal_dissolved:failure="PrincipalDissolved";break;
            case DiplomaticMissionContinuityKind::continue_work:
            case DiplomaticMissionContinuityKind::pause_envoy:
            case DiplomaticMissionContinuityKind::needs_reassignment:
                if(common.tick()>=a.delivery_expires_tick)failure="DeliveryExpired";
                else if(d.kind!=DiplomaticMissionContinuityKind::continue_work)contract.pending_code=d.code.empty()?(d.kind==DiplomaticMissionContinuityKind::pause_envoy?"EnvoyUnavailable":"NeedsReassignment"):d.code;
                else contract.pending_code=contract.interruptions.empty()?StableId{}:StableId{"EnvoyContactRenewalRequired"};
                // 权威恢复只移除暂缓提示；有旧中止会面仍待本人重新接触。
                break;
            default:return common_failure("UnknownEnvoyContinuity",row.first,ErrorKind::internal);
        }
        if(failure.empty())continue;
        if(d.cause_fact_id&&(d.cause_fact_id->empty()||!common.state().facts.find(*d.cause_fact_id)))return common_failure("MissingEnvoyInterruptionCause",row.first,ErrorKind::internal);
        auto id=common.next_id("envoy_failed");if(!id.ok)return {false,{},id.error};EnvoyTaskCompletion final;final.fact_id=id.value;final.task_id=row.first;final.contract_id=a.contract_id;final.acceptance_fact_id=a.fact_id;final.result=EnvoyTaskResult::failed;final.completed_tick=common.tick();final.reason=failure;
        auto wrote=common.append_fact({id.value,"envoy_task_failed","tasks"});if(!wrote.ok)return wrote;contract.completion=final;contract.pending_code.clear();auto* task=tasks.tasks.find(row.first);task->status="failed";task->settlement_id=id.value;task->reported_tick=common.tick();
    }
    return common_success();
}
}
