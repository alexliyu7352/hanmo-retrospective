// 使者回到原报告场所完成劳动后才能提供接触来源；已过递交期限不抹去此前真实交付。
#include "envoy_task_report_contact.hpp"
#include "task_model.hpp"
#include "resident/domains/interaction/face_to_face_contact.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {
// 仅证明当前原Action与当面状态；政治接收权和最终报告来源须由调用方同候选复核。
Outcome<EnvoyReportContactProof> read_envoy_report_contact(const CommonStore& common,const TaskState& tasks,const EnvoyTaskState& envoy,const InteractionState& interaction,const InteractionCatalog& catalog,const InteractionAccess& access,const StableId& task_id,const StableId& action_id,Integer completed_tick){
    if(envoy.version!=2)return {false,{},common_failure("UnsupportedEnvoyReportVersion").error};
    const auto* contract=envoy.envoy_contracts.find(task_id);const auto* task=tasks.tasks.find(task_id);const auto* action=common.action(action_id);
    if(!contract||!task||!action||!contract->action||!contract->delivery||contract->completion)return {false,{},common_failure("EnvoyReportActionRequired").error};
    const auto& a=contract->acceptance;const auto& d=*contract->delivery;const auto& handover=*contract->action;
    const auto* accepted=common.state().facts.find(a.fact_id);const auto* delivered=common.state().facts.find(d.fact_id);
    if(!a.return_report_required||a.task_id!=task_id||task->id!=task_id||task->assignee!=a.envoy_actor_id||task->offer_id!=a.proposal_id||task->status!="ready"||task->contract_id||!task->budget_wallet_id.empty()||!task->settlement_id.empty()||task->reported_tick||task->accepted_tick!=a.accepted_tick||task->deadline_tick!=a.delivery_expires_tick||a.contract_id.empty()||a.mission_authority_fact_id.empty()||a.proposal_fact_id.empty()||a.report_office_id.empty()||a.report_site_id.empty()||!accepted||accepted->kind!="envoy_task_accepted"||accepted->domain!="tasks")return {false,{},common_failure("InvalidEnvoyReportTaskSource",task_id).error};
    if(d.task_id!=task_id||d.contract_id!=a.contract_id||d.acceptance_fact_id!=a.fact_id||d.action_id.empty()||d.action_id==action_id||d.diplomatic_presentation_fact_id.empty()||d.completed_tick<a.accepted_tick||d.completed_tick>=a.delivery_expires_tick||d.completed_tick>common.tick()||task->objective_completed_tick!=d.completed_tick||!delivered||delivered->kind!="envoy_task_delivered"||delivered->domain!="tasks")return {false,{},common_failure("InvalidEnvoyReportDeliverySource",task_id).error};
    if(handover.action_id!=action_id||handover.kind!=EnvoyTaskActionKind::report||action->id!=action_id||action->kind!="envoy_report"||action->actor_id!=a.envoy_actor_id||action->remaining_ticks!=0||action->start_tick<d.completed_tick||action->start_tick>common.tick()||common.tick()==std::numeric_limits<Integer>::max()||completed_tick!=common.tick()+1)return {false,{},common_failure("InvalidEnvoyReportBoundary",action_id).error};
    const auto& context=handover.context;const auto* actor=common.actor(a.envoy_actor_id);const auto* provider=common.actor(context.provider_actor_id);
    if(!face_to_face_contact_context_valid(context)||context.place_id!=a.report_site_id||!actor||!provider||actor->id==provider->id||actor->action_id!=action_id||provider->action_id!=action_id||action->participants.size()!=2||std::find(action->participants.begin(),action->participants.end(),actor->id)==action->participants.end()||std::find(action->participants.begin(),action->participants.end(),provider->id)==action->participants.end())return {false,{},common_failure("InvalidEnvoyReportParticipants",action_id).error};
    auto checked=validate_interaction(common,interaction,catalog,access,a.envoy_actor_id,context,{},action_id);if(!checked.ok)return {false,{},checked.error};
    return {true,{common.state().identity,a,d,action_id,checked.value,completed_tick},{}};
}
}
