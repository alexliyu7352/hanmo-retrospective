// 政治递交在动作完工时再次核准；这里只按冻结工时占用当前两人，不预写成功或移动位置。
#include "envoy_task_actions.hpp"
namespace hanmo::resident {
Outcome<EnvoyDeliveryQuote> preview_envoy_delivery(const CommonStore& common,const TaskState& tasks,const EnvoyTaskState& envoy,const InteractionState& interaction,const InteractionCatalog& catalog,const InteractionAccess& access,const DiplomaticMissionRead& missions,const DiplomacyPresentationRead& presentations,const StableId& actor_id,const StableId& task_id,const StableId& provider,Integer revision,const EnvoyContactAuthorization& authorize){
    if(revision!=common.revision())return {false,{},common_failure("StaleWorldRevision").error};const auto* task=tasks.tasks.find(task_id);if(!task||task->assignee!=actor_id)return {false,{},common_failure("TaskNotKnown").error};if(task->status!="active")return {false,{},common_failure("EnvoyDeliveryNotAvailable",task_id).error};EnvoyTaskRead read(common,tasks,envoy,missions,presentations);auto source=read.envoy_task_source(task_id);if(!source.ok)return {false,{},source.error};const auto* contract=envoy.envoy_contracts.find(task_id);const auto& a=source.value.acceptance;
    if(task->status!="active"||source.value.delivery||source.value.completion||contract->action)return {false,{},common_failure("EnvoyDeliveryNotAvailable",task_id).error};if(!authorize)return {false,{},common_failure("MissingEnvoyPoliticalContactService",task_id,ErrorKind::internal).error};
    auto terms=missions.diplomatic_mission_source(a.mission_authority_fact_id);if(!terms.ok)return {false,{},terms.error};const auto duration=terms.value.delivery_work_ticks;if(duration<1||common.tick()>=a.delivery_expires_tick||duration>=a.delivery_expires_tick-common.tick())return {false,{},common_failure("EnvoyDeliveryExpired",task_id).error};
    auto contact=preview_face_to_face_contact(common,interaction,catalog,access,actor_id,provider);if(!contact.ok)return {false,{},contact.error};
    // 行动的actor/provider必须沿原本人会话朝向；不能把反向旁观会话变成任意岗位服务。
    if(contact.value.session_actor_id!=actor_id||contact.value.context.provider_actor_id!=provider||contact.value.context.place_id!=a.delivery_site_id)return {false,{},common_failure("EnvoyDeliveryMeetingRequired",task_id).error};auto permission=authorize(a,contact.value);if(!permission.ok)return {false,{},permission.error};
    return {true,{common.state().identity,task_id,duration,std::move(contact.value)},{}};
}
Outcome<StableId> begin_envoy_delivery_in_candidate(CommonStore& common,const TaskState& tasks,EnvoyTaskState& envoy,const InteractionState& interaction,const InteractionCatalog& catalog,const InteractionAccess& access,const DiplomaticMissionRead& missions,const DiplomacyPresentationRead& presentations,const StableId& actor_id,const StableId& task_id,const StableId& provider,Integer revision,const EnvoyContactAuthorization& authorize){
    auto quote=preview_envoy_delivery(common,tasks,envoy,interaction,catalog,access,missions,presentations,actor_id,task_id,provider,revision,authorize);if(!quote.ok)return {false,{},quote.error};auto id=common.next_id("action");if(!id.ok)return id;
    ActionMutation start;start.kind=ActionMutation::Kind::insert;start.action={id.value,"envoy_deliver",actor_id,{actor_id,provider},common.tick(),quote.value.duration_ticks};auto changed=common.apply_action(start);if(!changed.ok)return {false,{},changed.error};
    for(const auto& participant:start.action.participants){ActorMutation occupied;occupied.actor_id=participant;occupied.action_id=id.value;changed=common.apply_actor(occupied);if(!changed.ok)return {false,{},changed.error};}
    auto* contract=envoy.envoy_contracts.find(task_id);contract->action=EnvoyTaskAction{id.value,EnvoyTaskActionKind::deliver,quote.value.contact.context};contract->pending_code.clear();return id;
}
}
