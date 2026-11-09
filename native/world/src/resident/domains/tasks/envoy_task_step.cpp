// 新半日先复核独立使命连续性与截止；现场/代表变化不会把全世界永远卡在同一失败半日。
#include "envoy_task_step.hpp"
#include "resident/domains/interaction/face_to_face_contact.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {
// 仅存已实际发生的劳动快照，不持有第二套remaining_ticks或政治接触真值。
Outcome<EnvoyTaskStep> interrupt_envoy_handover(CommonStore& common,TaskState& tasks,EnvoyTaskContract& contract,const ActionCoreView& action,Integer work,Integer tick,const StableId& reason,const StableId& note,bool terminate,const std::optional<StableId>& cause={}){
    const auto& a=contract.acceptance;
    if(cause&&(cause->empty()||!common.state().facts.find(*cause)))return {false,{},common_failure("MissingEnvoyInterruptionCause",a.task_id,ErrorKind::internal).error};
    auto id=common.next_id("envoy_interruption");if(!id.ok)return {false,{},id.error};
    EnvoyHandoverInterruption stopped;stopped.fact_id=id.value;stopped.task_id=a.task_id;stopped.contract_id=a.contract_id;stopped.acceptance_fact_id=a.fact_id;stopped.action_id=action.id;stopped.action_kind=action.kind;stopped.provider_actor_id=contract.action->context.provider_actor_id;stopped.place_id=contract.action->context.place_id;stopped.reason=reason;stopped.started_tick=action.start_tick;stopped.stopped_tick=tick;stopped.completed_work_ticks=work-action.remaining_ticks;stopped.cause_fact_id=cause;
    auto wrote=common.append_fact({id.value,"envoy_handover_interrupted","tasks"});if(!wrote.ok)return {false,{},wrote.error};contract.interruptions.push_back(std::move(stopped));contract.action.reset();contract.pending_code=note;
    EnvoyTaskStep result;result.task_id=a.task_id;result.action_finished=true;result.success=false;result.code=note;result.fact_id=id.value;
    if(terminate){auto final_id=common.next_id("envoy_failed");if(!final_id.ok)return {false,{},final_id.error};EnvoyTaskCompletion final;final.fact_id=final_id.value;final.task_id=a.task_id;final.contract_id=a.contract_id;final.acceptance_fact_id=a.fact_id;if(contract.delivery)final.delivery_fact_id=contract.delivery->fact_id;final.result=EnvoyTaskResult::failed;final.completed_tick=tick;final.reason=reason;wrote=common.append_fact({final.fact_id,"envoy_task_failed","tasks"});if(!wrote.ok)return {false,{},wrote.error};contract.completion=final;contract.pending_code.clear();auto* task=tasks.tasks.find(a.task_id);task->status="failed";task->settlement_id=final.fact_id;task->reported_tick=tick;result.fact_id=final.fact_id;}
    return {true,std::move(result),{}};
}
namespace {
// 完成服务只能反读本候选实际Action/在场证明，不重用开工时的报价或execution_epoch。
class StepContact final:public TaskEnvoyContactRead {
public:
    StepContact(const CommonStore& c,const TaskState& t,const EnvoyTaskState& e,const InteractionState& i,const InteractionCatalog& catalog,const InteractionAccess& access):c_(c),t_(t),e_(e),i_(i),catalog_(catalog),access_(access){}
    Outcome<EnvoyDeliveryContactProof> envoy_delivery_contact(const StableId& task,const StableId& action,Integer tick)const override{return read_envoy_delivery_contact(c_,t_,e_,i_,catalog_,access_,task,action,tick);}
private:
    const CommonStore& c_;const TaskState& t_;const EnvoyTaskState& e_;const InteractionState& i_;const InteractionCatalog& catalog_;const InteractionAccess& access_;
};
}
Outcome<EnvoyTaskStep> step_envoy_delivery_action(CommonStore& common,TaskState& tasks,EnvoyTaskState& envoy,const InteractionState& interaction,const InteractionCatalog& catalog,const InteractionAccess& access,const DiplomaticMissionRead& missions,const DiplomacyPresentationRead& presentations,const StableId& action_id,const DynamicStepInput& step,const EnvoyPresentationAttempt& present){
    if(common.tick()==std::numeric_limits<Integer>::max()||step.from_tick!=common.tick()||step.to_tick!=step.from_tick+1||std::count(step.existing_action_ids.begin(),step.existing_action_ids.end(),action_id)!=1)return {false,{},common_failure("InvalidEnvoyStepBoundary",action_id,ErrorKind::internal).error};
    EnvoyTaskContract* contract=nullptr;for(auto& row:envoy.envoy_contracts.rows())if(row.second.action&&row.second.action->action_id==action_id){if(contract)return {false,{},common_failure("DuplicateEnvoyAction",action_id,ErrorKind::internal).error};contract=&row.second;}
    if(!contract)return {false,{},common_failure("UnknownEnvoyAction",action_id,ErrorKind::internal).error};const auto& a=contract->acceptance;EnvoyTaskRead read(common,tasks,envoy,missions,presentations);auto source=read.envoy_task_source(a.task_id);if(!source.ok)return {false,{},source.error};auto original=missions.diplomatic_mission_source(a.mission_authority_fact_id);if(!original.ok)return {false,{},original.error};const auto work=original.value.delivery_work_ticks;const auto* current=common.action(action_id);const auto& ctx=contract->action->context;const auto* actor=common.actor(a.envoy_actor_id);const auto* provider=common.actor(ctx.provider_actor_id);
    if(source.value.task_status!="active"||source.value.delivery||source.value.completion||contract->action->kind!=EnvoyTaskActionKind::deliver||!current||current->kind!="envoy_deliver"||current->actor_id!=a.envoy_actor_id||work<1||current->remaining_ticks<1||current->remaining_ticks>work||current->start_tick<a.accepted_tick||current->start_tick>common.tick()||common.tick()-current->start_tick<work-current->remaining_ticks||!face_to_face_contact_context_valid(ctx)||ctx.place_id!=a.delivery_site_id||!actor||!provider||actor->id==provider->id||actor->action_id!=action_id||provider->action_id!=action_id||current->participants.size()!=2||std::count(current->participants.begin(),current->participants.end(),actor->id)!=1||std::count(current->participants.begin(),current->participants.end(),provider->id)!=1)return {false,{},common_failure("InvalidEnvoyDeliveryStep",action_id,ErrorKind::internal).error};
    const auto action=*current;auto continuity=missions.diplomatic_mission_continuity(a.mission_authority_fact_id,a.envoy_actor_id);if(!continuity.ok)return {false,{},continuity.error};
    // 所有分型来自政治typed读口；未知枚举/不实死亡不能被正常等待掩盖。
    auto decision=[&](const DiplomaticMissionContinuity& d)->Outcome<EnvoyTaskStep>{EnvoyTaskStep result;result.task_id=a.task_id;
        if(d.kind==DiplomaticMissionContinuityKind::terminate_envoy_dead){if(actor->alive)return {false,{},common_failure("EnvoyDeathSourceMismatch",action_id,ErrorKind::internal).error};return interrupt_envoy_handover(common,tasks,*contract,action,work,step.to_tick,"EnvoyDead",d.code.empty()?"EnvoyDead":d.code,true,d.cause_fact_id);}
        if(d.kind==DiplomaticMissionContinuityKind::terminate_principal_dissolved)return interrupt_envoy_handover(common,tasks,*contract,action,work,step.to_tick,"PrincipalDissolved",d.code.empty()?"PrincipalDissolved":d.code,true,d.cause_fact_id);
        if(d.kind!=DiplomaticMissionContinuityKind::continue_work&&d.kind!=DiplomaticMissionContinuityKind::pause_envoy&&d.kind!=DiplomaticMissionContinuityKind::needs_reassignment)return {false,{},common_failure("UnknownEnvoyContinuity",action_id,ErrorKind::internal).error};
        if(step.to_tick>=a.delivery_expires_tick)return interrupt_envoy_handover(common,tasks,*contract,action,work,step.to_tick,"DeliveryExpired","EnvoyDeliveryExpired",true);
        if(d.kind!=DiplomaticMissionContinuityKind::continue_work){result.suspended=true;result.code=d.code.empty()?(d.kind==DiplomaticMissionContinuityKind::pause_envoy?"EnvoyUnavailable":"NeedsReassignment"):d.code;contract->pending_code=result.code;}
        return {true,std::move(result),{}};};
    auto permitted=decision(continuity.value);if(!permitted.ok||permitted.value.action_finished||permitted.value.suspended)return permitted;
    if(!actor->alive||!actor->available)return {false,{},common_failure("EnvoyContinuityMismatch",action_id,ErrorKind::internal).error};
    if(!access.resolve_location||!access.role||!access.battle_occupied)return {false,{},common_failure("MissingEnvoyContactAccess",action_id,ErrorKind::internal).error};
    if(access.battle_occupied(actor->id)||access.battle_occupied(provider->id)){contract->pending_code="BattlePending";return {true,{true,false,true,a.task_id,"BattlePending",{}},{}};}
    auto at_site=[&](const StableId& id){auto where=access.resolve_location(id);const auto* place=std::get_if<SettlementLocation>(&where);return place&&place->place_id==ctx.place_id;};
    if(!provider->alive||!provider->available||!at_site(actor->id)||!at_site(provider->id))return interrupt_envoy_handover(common,tasks,*contract,action,work,step.to_tick,"RenewContact","EnvoyContactUnavailable",false);
    if(!ctx.kind){const auto* role=catalog.common?catalog.common->roles.find(ctx.role_id):nullptr;if(!role||role->place_id!=ctx.place_id)return {false,{},common_failure("InvalidEnvoyAuthoredContact",action_id,ErrorKind::internal).error};auto assignment=access.role(ctx.role_id);if(!assignment||!assignment->enabled||assignment->actor_id!=provider->id)return interrupt_envoy_handover(common,tasks,*contract,action,work,step.to_tick,"RenewContact","EnvoyRoleChanged",false);}
    auto contact=validate_interaction(common,interaction,catalog,access,actor->id,ctx,{},action_id);if(!contact.ok)return {false,{},contact.error};
    contract->pending_code.clear();ActionMutation decrement;decrement.action=action;decrement.remaining_ticks=action.remaining_ticks-1;auto changed=common.apply_action(decrement);if(!changed.ok)return {false,{},changed.error};
    if(action.remaining_ticks>1)return {true,{false,false,true,a.task_id,{},{}},{}};
    if(!present)return {false,{},common_failure("MissingEnvoyPresentationService",a.task_id,ErrorKind::internal).error};StepContact completing(common,tasks,envoy,interaction,catalog,access);auto attempted=present(completing,a.task_id,action_id,step);if(!attempted.ok)return {false,{},attempted.error};const auto& attempt=attempted.value;
    if(attempt.kind==DiplomaticTaskPresentationKind::presented){if(attempt.fact_id.empty()||!attempt.code.empty()||attempt.continuity)return {false,{},common_failure("InvalidEnvoyPresentationAttempt",action_id,ErrorKind::internal).error};EnvoyPresentationCommit committed=[&](const auto&,const auto&,const auto&,Integer)->Outcome<StableId>{return {true,attempt.fact_id,{}};};auto finished=finish_envoy_delivery_in_candidate(common,tasks,envoy,interaction,catalog,access,missions,presentations,a.task_id,action_id,step.to_tick,committed);if(!finished.ok)return {false,{},finished.error};return {true,{false,true,true,a.task_id,{},finished.value.completion?finished.value.completion->fact_id:finished.value.delivery->fact_id},{}};}
    if(!attempt.fact_id.empty()||attempt.code.empty()||(attempt.kind!=DiplomaticTaskPresentationKind::mission_blocked&&attempt.continuity))return {false,{},common_failure("InvalidEnvoyPresentationAttempt",action_id,ErrorKind::internal).error};
    if(attempt.kind==DiplomaticTaskPresentationKind::renew_contact)return interrupt_envoy_handover(common,tasks,*contract,*common.action(action_id),work,step.to_tick,"RenewContact",attempt.code,false);
    if(attempt.kind==DiplomaticTaskPresentationKind::expired){if(step.to_tick<a.delivery_expires_tick)return {false,{},common_failure("EnvoyExpirySourceMismatch",action_id,ErrorKind::internal).error};return interrupt_envoy_handover(common,tasks,*contract,action,work,step.to_tick,"DeliveryExpired",attempt.code,true);}
    if(attempt.kind!=DiplomaticTaskPresentationKind::mission_blocked||!attempt.continuity||attempt.continuity->kind==DiplomaticMissionContinuityKind::continue_work)return {false,{},common_failure("InvalidEnvoyPresentationAttempt",action_id,ErrorKind::internal).error};
    // 政治复核决定本半日未能劳动时，恢复原唯一行动余量；不生成第二个计时器。
    ActionMutation restore;restore.action=action;restore.remaining_ticks=action.remaining_ticks;changed=common.apply_action(restore);if(!changed.ok)return {false,{},changed.error};return decision(*attempt.continuity);
}
}
