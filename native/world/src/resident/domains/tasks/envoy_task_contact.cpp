// 任务域只证明真正交接动作完成时仍在场；提议有效性与目标Office继任资格始终由政治域核准。
#include "envoy_task_contact.hpp"
#include "task_model.hpp"
#include "resident/domains/interaction/face_to_face_contact.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {
// 该读取不推进时间、不补行动，也不把过去报价的epoch或一段对白当成已递交事实。
Outcome<EnvoyDeliveryContactProof> read_envoy_delivery_contact(const CommonStore& common,const TaskState& tasks,const EnvoyTaskState& envoy,const InteractionState& interaction,const InteractionCatalog& catalog,const InteractionAccess& access,const StableId& task_id,const StableId& action_id,Integer completed_tick){
    if((envoy.version!=1&&envoy.version!=2))return {false,{},common_failure("UnsupportedEnvoyTaskVersion").error};
    const auto* contract=envoy.envoy_contracts.find(task_id);const auto* task=tasks.tasks.find(task_id);const auto* action=common.action(action_id);
    if(!contract||!task||!action||!contract->action||contract->delivery||contract->completion)return {false,{},common_failure("EnvoyDeliveryActionRequired").error};
    const auto& accepted=contract->acceptance;const auto& handover=*contract->action;const auto* fact=common.state().facts.find(accepted.fact_id);
    if(accepted.task_id!=task_id||task->id!=task_id||task->assignee!=accepted.envoy_actor_id||task->offer_id!=accepted.proposal_id||task->status!="active"||task->contract_id||!task->budget_wallet_id.empty()||task->accepted_tick!=accepted.accepted_tick||task->deadline_tick!=accepted.delivery_expires_tick||accepted.contract_id.empty()||accepted.mission_authority_fact_id.empty()||accepted.proposal_fact_id.empty()||accepted.target_office_id.empty()||!fact||fact->kind!="envoy_task_accepted"||fact->domain!="tasks")return {false,{},common_failure("InvalidEnvoyTaskSource",task_id).error};
    if(handover.action_id!=action_id||handover.kind!=EnvoyTaskActionKind::deliver||action->id!=action_id||action->kind!="envoy_deliver"||action->actor_id!=accepted.envoy_actor_id||action->remaining_ticks!=0||action->start_tick<accepted.accepted_tick||action->start_tick>common.tick()||common.tick()==std::numeric_limits<Integer>::max()||completed_tick!=common.tick()+1)return {false,{},common_failure("InvalidEnvoyDeliveryBoundary",action_id).error};
    if(completed_tick>=accepted.delivery_expires_tick)return {false,{},common_failure("EnvoyDeliveryExpired",task_id).error};
    const auto& context=handover.context;const auto* actor=common.actor(accepted.envoy_actor_id);const auto* provider=common.actor(context.provider_actor_id);
    if(!face_to_face_contact_context_valid(context)||context.place_id!=accepted.delivery_site_id||!actor||!provider||actor->action_id!=action_id||provider->action_id!=action_id||action->participants.size()!=2||std::find(action->participants.begin(),action->participants.end(),actor->id)==action->participants.end()||std::find(action->participants.begin(),action->participants.end(),provider->id)==action->participants.end())return {false,{},common_failure("InvalidEnvoyDeliveryParticipants",action_id).error};
    auto checked=validate_interaction(common,interaction,catalog,access,accepted.envoy_actor_id,context,{},action_id);if(!checked.ok)return {false,{},checked.error};
    return {true,{common.state().identity,accepted,action_id,checked.value,completed_tick},{}};
}
}
