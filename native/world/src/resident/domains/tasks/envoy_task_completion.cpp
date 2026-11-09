// 只消费当前Action实际完工和政治唯一presentation；无返报要求才可当步生成任务最终事实。
#include "envoy_task_completion.hpp"
namespace hanmo::resident {
namespace {
// 借用同候选真实Task/Common/会面状态，S6每次取Proof都走原完工来源核验。
class CompletingEnvoyContact final:public TaskEnvoyContactRead {
public:
    CompletingEnvoyContact(const CommonStore& c,const TaskState& t,const EnvoyTaskState& e,const InteractionState& i,const InteractionCatalog& catalog,const InteractionAccess& access):common_(c),tasks_(t),envoy_(e),interaction_(i),catalog_(catalog),access_(access){}
    Outcome<EnvoyDeliveryContactProof> envoy_delivery_contact(const StableId& task,const StableId& action,Integer tick)const override{return read_envoy_delivery_contact(common_,tasks_,envoy_,interaction_,catalog_,access_,task,action,tick);}
private:
    const CommonStore& common_;const TaskState& tasks_;const EnvoyTaskState& envoy_;const InteractionState& interaction_;const InteractionCatalog& catalog_;const InteractionAccess& access_;
};
}
Outcome<EnvoyTaskSource> finish_envoy_delivery_in_candidate(CommonStore& common,TaskState& tasks,EnvoyTaskState& envoy,const InteractionState& interaction,const InteractionCatalog& catalog,const InteractionAccess& access,const DiplomaticMissionRead& missions,const DiplomacyPresentationRead& presentations,const StableId& task_id,const StableId& action_id,Integer completed_tick,const EnvoyPresentationCommit& present){
    EnvoyTaskRead read(common,tasks,envoy,missions,presentations);auto source=read.envoy_task_source(task_id);if(!source.ok)return source;if(source.value.delivery){if(source.value.delivery->action_id!=action_id||source.value.delivery->completed_tick!=completed_tick)return {false,{},common_failure("EnvoyAlreadyDelivered",task_id).error};return source;}
    if(!present)return {false,{},common_failure("MissingEnvoyPresentationService",task_id,ErrorKind::internal).error};CompletingEnvoyContact contact(common,tasks,envoy,interaction,catalog,access);auto proof=contact.envoy_delivery_contact(task_id,action_id,completed_tick);if(!proof.ok)return {false,{},proof.error};auto terms=missions.diplomatic_mission_source(source.value.acceptance.mission_authority_fact_id);if(!terms.ok)return {false,{},terms.error};const auto* action=common.action(action_id);
    if(terms.value.delivery_work_ticks<1||completed_tick-action->start_tick<terms.value.delivery_work_ticks)return {false,{},common_failure("EnvoyDeliveryWorkIncomplete",task_id,ErrorKind::internal).error};
    auto political=present(contact,task_id,action_id,completed_tick);if(!political.ok)return {false,{},political.error};if(political.value.empty())return {false,{},common_failure("MissingEnvoyPresentationFact",task_id,ErrorKind::internal).error};auto id=common.next_id("envoy_delivery");if(!id.ok)return {false,{},id.error};const auto& a=source.value.acceptance;EnvoyTaskDelivery delivered{id.value,task_id,a.contract_id,action_id,a.fact_id,political.value,completed_tick};auto appended=common.append_fact({delivered.fact_id,"envoy_task_delivered","tasks"});if(!appended.ok)return {false,{},appended.error};auto* contract=envoy.envoy_contracts.find(task_id);auto* task=tasks.tasks.find(task_id);contract->delivery=delivered;contract->action.reset();contract->pending_code.clear();task->objective_completed_tick=completed_tick;task->status="ready";
    // 反读政治typed记录的完整五ID绑定；写入任何错来源时必须整体撤回政治与Task候选。
    auto verified=read.envoy_task_source(task_id);if(!verified.ok)return verified;if(a.return_report_required)return verified;
    auto final_id=common.next_id("envoy_complete");if(!final_id.ok)return {false,{},final_id.error};EnvoyTaskCompletion final;final.fact_id=final_id.value;final.task_id=task_id;final.contract_id=a.contract_id;final.acceptance_fact_id=a.fact_id;final.delivery_fact_id=delivered.fact_id;final.completed_tick=completed_tick;appended=common.append_fact({final.fact_id,"envoy_task_completed","tasks"});if(!appended.ok)return {false,{},appended.error};contract->completion=final;task->status="success";task->settlement_id=final.fact_id;task->reported_tick=completed_tick;return read.envoy_task_source(task_id);
}
}
