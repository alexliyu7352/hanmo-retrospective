// 原claim成功且原Action参数已入账后才可冻结，任何后继失败由原整World候选一起撤回。
#include "task_personal_advance_admission.hpp"
#include "task_personal_advance_rules.hpp"
namespace hanmo::resident {
Outcome<bool> authorize_task_personal_advance_in_candidate(const CommonStore& common,TaskState& tasks,const TaskCatalog& task_catalog,const TaskPersonalAdvanceCatalog& catalog,TaskPersonalAdvanceState& advances,const StableId& action_id){
    if(advances.version!=1)return {false,false,common_failure("UnsupportedTaskAdvanceVersion").error};
    const auto* action=common.action(action_id);const auto* parameters=tasks.actions.find(action_id);
    auto* task=parameters?tasks.tasks.find(parameters->task_id):nullptr;auto* contract=task&&task->contract_id?tasks.contracts.find(*task->contract_id):nullptr;
    const auto* actor=action?common.actor(action->actor_id):nullptr;
    if(!action||!parameters||!actor||!task||!contract||action->kind!="accept_offer"||action->start_tick!=common.tick()||action->remaining_ticks<1||actor->action_id!=action_id||task->assignee!=actor->id||task->status!="accepting"||contract->status!=ContractStatus::accepted||contract->execution_stopped||contract->assignee.kind!=SubjectKind::actor||contract->assignee.id!=actor->id||contract->executor_actor_id!=actor->id||contract->current_task_id!=task->id||parameters->offer_id!=task->offer_id||!task->offer_ref||task->offer_ref->kind!=OfferRef::Kind::runtime||task->offer_ref->id!=task->offer_id||std::count(action->participants.begin(),action->participants.end(),actor->id)!=1)return {false,false,common_failure("TaskAdvanceAcceptanceRequired",action_id).error};
    if(advances.terms.find(contract->id)||contract->frozen_terms.personal_advance_allowed)return {false,false,common_failure("TaskAdvanceAlreadyAuthorized",contract->id).error};
    auto policy=task_personal_advance_policy_for_offer(catalog,task_catalog,tasks,task->offer_id);if(!policy.ok)return {false,false,policy.error};
    if(!policy.value)return {true,false,{}};
    contract->frozen_terms.personal_advance_allowed=true;
    auto terms=freeze_task_personal_advance_terms(common,tasks,contract->id,*policy.value);
    if(!terms.ok){contract->frozen_terms.personal_advance_allowed=false;return {false,false,terms.error};}
    advances.terms.insert(contract->id,std::move(terms.value));return {true,true,{}};
}
}
