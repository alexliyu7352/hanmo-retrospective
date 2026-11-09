// 唯一Task Action实际做满时才冻结政治计划；此时尚未分发资源，不增加民心。
#include "politics_governance.hpp"
#include "politics_internal.hpp"
#include "resident/domains/tasks/governance_task_contract.hpp"
namespace hanmo::resident {
Outcome<StableId> record_governance_outcome_plan_in_transaction(CommonStore& c,PoliticsGovernanceState& state,const PoliticsGovernanceCatalog& catalog,const TaskGovernanceRead& tasks,const StableId& work_id,const DynamicStepInput& phase){
    if(!catalog.enabled)return {false,{},common_failure("GovernanceNotEnabled",{},ErrorKind::unsupported_capability).error};
    auto work_read=tasks.governance_work_completion(work_id);if(!work_read.ok)return {false,{},work_read.error};const auto& work=work_read.value;
    auto task=tasks.governance_task_source(work.task_id);if(!task.ok)return {false,{},task.error};const auto& accepted=task.value.acceptance;
    const auto* authority=state.authorities.find(accepted.authority_source_fact_id);const auto* acceptance_head=c.state().facts.find(accepted.fact_id);const auto* work_head=c.state().facts.find(work_id);
    const auto bad=[&](){return Outcome<StableId>{false,{},common_failure("GovernanceWorkBindingMismatch",work_id).error};};
    if(!authority||!work_head||work_head->domain!="tasks"||work_head->kind!="governance_work_completed"||!acceptance_head||acceptance_head->domain!="tasks"||acceptance_head->kind!="governance_task_accepted"||work.fact_id!=work_id||!task.value.work||task.value.work->fact_id!=work_id||accepted.kind!=GovernanceTaskKind::appease||accepted.task_id!=work.task_id||accepted.contract_id!=work.contract_id||accepted.fact_id!=work.acceptance_fact_id||accepted.executor_actor_id!=authority->executor_actor_id||accepted.organization_id!=authority->organization_id||accepted.jurisdiction_id!=authority->jurisdiction_id||accepted.site_id!=authority->site_id||accepted.rule_id!=authority->rule.id||accepted.rule_version!=authority->rule.version||accepted.work_ticks!=authority->rule.duration_ticks||accepted.accepted_tick<authority->issued_tick||accepted.accepted_tick>=authority->offer_expires_tick||accepted.accepted_tick>INT64_MAX-authority->completion_deadline_ticks||accepted.deadline_tick!=accepted.accepted_tick+authority->completion_deadline_ticks||work.started_tick<accepted.accepted_tick||work.completed_tick<work.started_tick||work.completed_tick-work.started_tick<accepted.work_ticks)return bad();
    // 已存在计划只返回原值，等待结算后不会用新能力重算，动作也可能已经正常释放。
    for(const auto& row:state.plans.rows())if(row.second.work_fact_id==work_id){const auto& plan=row.second;const auto* head=c.state().facts.find(plan.id);if(plan.id!=row.first||!head||head->domain!="politics"||head->kind!=governance_outcome_fact_kind||plan.authority_fact_id!=authority->id||plan.task_id!=work.task_id||plan.contract_id!=work.contract_id||plan.action_id!=work.action_id||plan.value.work_completed_tick!=work.completed_tick||(!work.outcome_plan_id.empty()&&work.outcome_plan_id!=plan.id))return bad();auto valid=validate_governance_outcome_plan(authority->rule,plan.value);return valid.ok?Outcome<StableId>{true,plan.id,{}}:Outcome<StableId>{false,{},valid.error};}
    if(!work.outcome_plan_id.empty())return {false,{},common_failure("UnknownGovernanceOutcomePlan",work.outcome_plan_id).error};
    const auto* action=c.action(work.action_id);const auto* actor=c.actor(accepted.executor_actor_id);
    if(!action||!actor||actor->action_id!=work.action_id||action->kind!="governance_appease"||action->actor_id!=accepted.executor_actor_id||action->participants!=std::vector<StableId>{accepted.executor_actor_id}||action->remaining_ticks!=0||action->start_tick!=work.started_tick)return {false,{},common_failure("GovernanceActionNotCompleted",work.action_id).error};
    auto frozen=freeze_appeasement_outcome(c,authority->rule,accepted.executor_actor_id,work.completed_tick,&phase);if(!frozen.ok)return {false,{},frozen.error};
    auto next=state;auto id=c.next_id("politics_governance_plan");if(!id.ok)return id;
    RecordedGovernancePlan plan{id.value,authority->id,work_id,work.task_id,work.contract_id,work.action_id,std::move(frozen.value)};auto appended=c.append_fact({id.value,governance_outcome_fact_kind,"politics"});if(!appended.ok)return {false,{},appended.error};if(!next.plans.insert(id.value,std::move(plan)))return {false,{},common_failure("DuplicateGovernanceOutcomePlan",id.value).error};state=std::move(next);return id;
}
// 独立调用使用旧原子行为；S3 Work+Plan回调明确选in_transaction版本。
Outcome<StableId> record_governance_outcome_plan(CommonStore& c,PoliticsGovernanceState& state,const PoliticsGovernanceCatalog& catalog,const TaskGovernanceRead& tasks,const StableId& work_id,const DynamicStepInput& phase){
    local_economy_detail::PhaseSavepoint guard(c);if(!guard.token().ok)return {false,{},guard.token().error};auto next=state;auto result=record_governance_outcome_plan_in_transaction(c,next,catalog,tasks,work_id,phase);if(!result.ok)return result;auto committed=guard.commit();if(!committed.ok)return {false,{},committed.error};state=std::move(next);return result;
}

}
