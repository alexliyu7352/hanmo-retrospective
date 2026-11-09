// 新模块不放宽普通任务的作者定义/预算钱包规则；中央只对真实已登记sidecar委派此完整冷门。
#include "envoy_task_validation.hpp"
#include "envoy_task_version.hpp"
#include "resident/domains/interaction/face_to_face_contact.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
// 新旧入口共享完整事实/行动校验，只有显式v2调用提供报告历史读取权源。
Status validate_complete(const CommonStore& common,const TaskState& tasks,const EnvoyTaskState& envoy,const CommonCatalog& catalog,const DiplomaticMissionRead& missions,const DiplomacyPresentationRead& presentations,const DiplomaticEnvoyReportRead* reports){
    auto version=validate_envoy_task_version_boundary(common,envoy.version,&envoy);if(!version.ok)return version;
    auto accepted=validate_envoy_task_acceptances(common,tasks,envoy,missions);if(!accepted.ok)return accepted;EnvoyTaskRead read=reports?EnvoyTaskRead(common,tasks,envoy,missions,presentations,*reports):EnvoyTaskRead(common,tasks,envoy,missions,presentations);std::set<StableId> fact_ids,action_ids,live_actions;
    for(const auto& row:envoy.envoy_contracts.rows()){
        auto source=read.envoy_task_source(row.first);if(!source.ok)return {false,{},source.error};const auto& g=row.second;const auto& a=source.value.acceptance;const auto* task=tasks.tasks.find(row.first);auto authority=missions.diplomatic_mission_source(a.mission_authority_fact_id);if(!authority.ok)return {false,{},authority.error};const auto& original=authority.value;
        if(!fact_ids.insert(a.fact_id).second||!catalog.places.find(a.delivery_site_id)||!catalog.places.find(a.report_site_id)||g.pending_code.size()>4096||task->late)return common_failure("InvalidEnvoyContract",row.first);
        for(const auto& stopped:g.interruptions){const auto* fact=common.state().facts.find(stopped.fact_id);const bool report=stopped.action_kind=="envoy_report";const auto work=report?original.report_work_ticks:original.delivery_work_ticks;
            if(stopped.task_id!=row.first||stopped.contract_id!=a.contract_id||stopped.acceptance_fact_id!=a.fact_id||!fact||fact->id!=stopped.fact_id||fact->kind!="envoy_handover_interrupted"||fact->domain!="tasks"||!fact_ids.insert(stopped.fact_id).second||stopped.action_id.empty()||!action_ids.insert(stopped.action_id).second||common.action(stopped.action_id)||(stopped.action_kind!="envoy_deliver"&&!report)||!common.actor(stopped.provider_actor_id)||stopped.provider_actor_id==a.envoy_actor_id||stopped.place_id!=(report?a.report_site_id:a.delivery_site_id)||stopped.started_tick<a.accepted_tick||stopped.stopped_tick<stopped.started_tick||stopped.stopped_tick>common.tick()||work<1||stopped.completed_work_ticks<0||stopped.completed_work_ticks>work||stopped.stopped_tick-stopped.started_tick<stopped.completed_work_ticks||(report&&(!a.return_report_required||!g.delivery||stopped.started_tick<g.delivery->completed_tick)))return common_failure("InvalidEnvoyInterruptionHistory",row.first);
            if(stopped.cause_fact_id&&(stopped.cause_fact_id->empty()||!common.state().facts.find(*stopped.cause_fact_id)))return common_failure("MissingEnvoyInterruptionCause",row.first);
            if(stopped.reason=="CancelledByAssignee"){if(stopped.cause_fact_id||!g.completion||g.completion->result!=EnvoyTaskResult::cancelled||g.completion->completed_tick!=stopped.stopped_tick)return common_failure("InvalidEnvoyCancelledInterruption",row.first);}
            else if(stopped.reason=="RenewContact"){if(stopped.cause_fact_id||(!report&&g.delivery&&stopped.stopped_tick>g.delivery->completed_tick)||(g.completion&&stopped.stopped_tick>g.completion->completed_tick))return common_failure("InvalidEnvoyRenewedContact",row.first);}
            else if(stopped.reason=="EnvoyDead"||stopped.reason=="PrincipalDissolved"||stopped.reason=="DeliveryExpired"){if(!g.completion||(g.delivery&&(!report||envoy.version!=2||stopped.reason=="DeliveryExpired"))||g.completion->result!=EnvoyTaskResult::failed||g.completion->reason!=stopped.reason||g.completion->completed_tick!=stopped.stopped_tick||(stopped.reason=="DeliveryExpired"&&(stopped.cause_fact_id||stopped.stopped_tick<a.delivery_expires_tick)))return common_failure("InvalidEnvoyFailedInterruption",row.first);}
            else return common_failure("UnsupportedEnvoyInterruptionPolicy",row.first);
        }
        if(g.delivery){const auto& d=*g.delivery;if(d.completed_tick>common.tick()||!fact_ids.insert(d.fact_id).second||!action_ids.insert(d.action_id).second||common.action(d.action_id))return common_failure("InvalidEnvoyDeliveryHistory",row.first);}
        else if(task->objective_completed_tick)return common_failure("EnvoyObjectiveWithoutDelivery",row.first);
        if(g.completion){const auto& f=*g.completion;if(f.completed_tick>common.tick()||!fact_ids.insert(f.fact_id).second||g.action||!g.pending_code.empty())return common_failure("InvalidEnvoyFinalHistory",row.first);
            if(f.result==EnvoyTaskResult::cancelled){if(f.reason!="CancelledByAssignee"||f.report_action_id)return common_failure("InvalidEnvoyCancellation",row.first);}
            else if(f.result==EnvoyTaskResult::failed){if(f.report_action_id||(g.delivery&&(envoy.version!=2||f.reason=="DeliveryExpired"))||(f.reason!="EnvoyDead"&&f.reason!="PrincipalDissolved"&&f.reason!="DeliveryExpired")||(f.reason=="DeliveryExpired"&&f.completed_tick<a.delivery_expires_tick)||(!g.interruptions.empty()&&g.interruptions.back().stopped_tick>f.completed_tick))return common_failure("InvalidEnvoyFailedHistory",row.first);}
            else if(f.result!=EnvoyTaskResult::completed)return common_failure("UnsupportedEnvoyTerminationPolicy",row.first);
            if(f.report_action_id){if(!action_ids.insert(*f.report_action_id).second||common.action(*f.report_action_id)||!g.delivery||original.report_work_ticks<1||f.completed_tick-g.delivery->completed_tick<original.report_work_ticks)return common_failure("InvalidEnvoyReportHistory",row.first);}
        }
        else if(g.delivery&&!a.return_report_required)return common_failure("EnvoyFinalFactRequired",row.first);
        if(g.action){const auto& pending=*g.action;const auto* action=common.action(pending.action_id);const bool report=pending.kind==EnvoyTaskActionKind::report;const auto expected=report?"envoy_report":"envoy_deliver";const auto work=report?original.report_work_ticks:original.delivery_work_ticks;const auto& place=report?a.report_site_id:a.delivery_site_id;const auto& context=pending.context;const auto* actor=common.actor(a.envoy_actor_id);const auto* provider=common.actor(context.provider_actor_id);
            if((pending.kind!=EnvoyTaskActionKind::deliver&&!report)||!action_ids.insert(pending.action_id).second||!live_actions.insert(pending.action_id).second||!action||action->kind!=expected||action->actor_id!=a.envoy_actor_id||action->start_tick<a.accepted_tick||action->start_tick>common.tick()||work<1||action->remaining_ticks<1||action->remaining_ticks>work||!face_to_face_contact_context_valid(context)||context.place_id!=place||!actor||!provider||actor->id==provider->id||actor->action_id!=action->id||provider->action_id!=action->id||action->participants.size()!=2||std::count(action->participants.begin(),action->participants.end(),actor->id)!=1||std::count(action->participants.begin(),action->participants.end(),provider->id)!=1||(report?(!a.return_report_required||!g.delivery||action->start_tick<g.delivery->completed_tick):bool(g.delivery)))return common_failure("InvalidEnvoyActiveAction",row.first);
            if(!context.kind){const auto* role=catalog.roles.find(context.role_id);if(!role||role->place_id!=context.place_id)return common_failure("InvalidEnvoyActionContext",row.first);}
        }
    }
    // 反查孤立的本域Common记录，删掉sidecar不能把未完责任或已发生事实藏起来。
    for(const auto& row:common.state().actions.rows())if((row.second.kind=="envoy_deliver"||row.second.kind=="envoy_report")&&!live_actions.count(row.first))return common_failure("OrphanEnvoyAction",row.first);
    for(const auto& row:common.state().facts.rows()){const auto& kind=row.second.kind;if(row.second.domain=="tasks"&&(kind=="envoy_task_accepted"||kind=="envoy_task_delivered"||kind=="envoy_task_completed"||kind=="envoy_task_cancelled"||kind=="envoy_task_failed"||kind=="envoy_handover_interrupted")&&!fact_ids.count(row.first))return common_failure("OrphanEnvoyFact",row.first);}
    return common_success();
}
}
Status validate_envoy_tasks(const CommonStore& common,const TaskState& tasks,const EnvoyTaskState& envoy,const CommonCatalog& catalog,const DiplomaticMissionRead& missions,const DiplomacyPresentationRead& presentations){return validate_complete(common,tasks,envoy,catalog,missions,presentations,nullptr);}
Status validate_envoy_tasks_with_reports(const CommonStore& common,const TaskState& tasks,const EnvoyTaskState& envoy,const CommonCatalog& catalog,const DiplomaticMissionRead& missions,const DiplomacyPresentationRead& presentations,const DiplomaticEnvoyReportRead& reports){return validate_complete(common,tasks,envoy,catalog,missions,presentations,&reports);}
}
