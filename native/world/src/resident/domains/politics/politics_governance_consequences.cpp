// 真实任务终态或每辖区每期评税才产生一次后果；改政策按钮和仅完成劳动都不给民心。
#include "politics_governance_consequences.hpp"
#include "takeover/politics_occupation_fiscal_civil.hpp"
#include "politics_governance_sources.hpp"
#include "resident/domains/tasks/governance_task_contract.hpp"
#include "resident/domains/local_economy/relief/public_relief.hpp"
#include "resident/domains/local_economy/fiscal/fiscal_settlement.hpp"
#include <set>
namespace hanmo::resident {
namespace {
bool fact_is(const CommonStore& c,const StableId& id,const StableId& kind,const StableId& domain){const auto* head=c.state().facts.find(id);return head&&head->id==id&&head->kind==kind&&head->domain==domain;}
Integer changed_support(Integer before,Integer delta,Integer cap){if(delta>=0)return before>=cap?before:std::min<Integer>(cap,before+delta);return std::max<Integer>(0,before+delta);}
// 只在领域已经核准来源后进入此内部primitive，外部没有提交任意delta的入口。
Outcome<StableId> apply_source(CommonStore& c,PoliticsGovernanceState& state,const StableId& jurisdiction,const StableId& domain,const StableId& source,const StableId& plan,Integer delta,Integer cap){
    auto* civil=state.civil.find(jurisdiction);if(!civil||civil->id!=jurisdiction||civil->support<0||civil->support>100||civil->revision<0||cap<1||cap>100||delta< -100||delta>104)return {false,{},common_failure("InvalidPoliticalCivilState",jurisdiction).error};
    if(!plan.empty())for(const auto& row:state.consequences.rows())if(row.second.plan_id==plan&&(row.second.source_domain!=domain||row.second.source_fact_id!=source))return {false,{},common_failure("GovernancePlanAlreadyConsumed",plan).error};
    const CivilConsequence* existing=nullptr;for(const auto& row:state.consequences.rows())if(row.second.source_domain==domain&&row.second.source_fact_id==source){if(existing||row.first!=row.second.id)return {false,{},common_failure("DuplicateCivilConsequence",source).error};existing=&row.second;}
    if(existing){if(existing->jurisdiction_id!=jurisdiction||existing->plan_id!=plan||existing->requested_delta!=delta||existing->after!=changed_support(existing->before,delta,cap)||!fact_is(c,existing->id,"political_civil_consequence","politics"))return {false,{},common_failure("CivilConsequenceSourceMismatch",source).error};return {true,existing->id,{}};}
    if(civil->revision==INT64_MAX||!c.state().facts.find(source)||!c.state().facts.find(civil->last_fact_id))return {false,{},common_failure("InvalidCivilConsequenceSource",source).error};
    auto id=c.next_id("politics_civil_result");if(!id.ok)return id;CivilConsequence record{id.value,jurisdiction,domain,source,plan,civil->last_fact_id,c.tick(),civil->support,changed_support(civil->support,delta,cap),delta,civil->revision+1};
    auto head=c.append_fact({record.id,"political_civil_consequence","politics"});if(!head.ok)return {false,{},head.error};if(!state.consequences.insert(record.id,record))return {false,{},common_failure("DuplicateCivilConsequence",record.id).error};civil->support=record.after;civil->revision=record.revision;civil->last_fact_id=record.id;return id;
}
}
Status initialize_political_civil_state_in_transaction(CommonStore& c,const PoliticsState& politics,PoliticsGovernanceState& state,const PoliticsGovernanceCatalog& catalog){
    if(!catalog.enabled)return common_failure("GovernanceNotEnabled",{},ErrorKind::unsupported_capability);
    if(c.tick()!=0||c.revision()!=0||!state.civil.empty()||!state.consequences.empty()||!state.authorities.empty()||!state.plans.empty())return common_failure("PoliticalCivilAlreadyInitialized");
    if(politics.jurisdictions.empty()||catalog.initial_support.size()!=politics.jurisdictions.size())return common_failure("PoliticalInitialSupportRequired");
    for(const auto& row:politics.jurisdictions.rows()){const auto* initial=catalog.initial_support.find(row.first);if(!initial||*initial<0||*initial>100)return common_failure("InvalidPoliticalInitialSupport",row.first);}
    for(const auto& row:politics.jurisdictions.rows()){auto id=c.next_id("politics_civil");if(!id.ok)return {false,{},id.error};auto head=c.append_fact({id.value,"political_civil_registered","politics"});if(!head.ok)return head;state.civil.insert(row.first,{row.first,id.value,id.value,*catalog.initial_support.find(row.first),0});}
    return common_success();
}
Outcome<std::optional<PoliticalCivilSource>> read_completed_governance_civil_source(const CommonStore& c,const PoliticsState& politics,const PoliticsGovernanceState& state,const PoliticsGovernanceCatalog& catalog,const TaskGovernanceRead& tasks,const PublicReliefResultRead& resources,const StableId& final_id){
    const auto bad=[&](const char* code){return Outcome<std::optional<PoliticalCivilSource>>{false,{},common_failure(code,final_id).error};};if(!catalog.enabled)return bad("GovernanceNotEnabled");
    auto final_read=tasks.governance_task_completion(final_id);if(!final_read.ok)return {false,{},final_read.error};const auto& final=final_read.value;
    auto source=tasks.governance_task_source(final.task_id);if(!source.ok)return {false,{},source.error};const auto& task=source.value;const auto& acceptance=task.acceptance;
    if(final.fact_id!=final_id||!task.completion||task.completion->fact_id!=final_id||final.completed_tick<0||final.completed_tick>c.tick()||final.task_id!=acceptance.task_id||final.contract_id!=acceptance.contract_id||final.acceptance_fact_id!=acceptance.fact_id)return bad("GovernanceFinalSourceMismatch");
    if(final.result!=GovernanceTaskResult::completed){if(!fact_is(c,final_id,final.result==GovernanceTaskResult::cancelled?"governance_task_cancelled":"governance_task_failed","tasks"))return bad("GovernanceFinalSourceMismatch");return {true,std::nullopt,{}};}
    if(task.task_status!="success"||task.interruption||!task.work||!fact_is(c,final_id,"governance_task_completed","tasks")||final.resource_receipt_ids.size()!=1||!fact_is(c,acceptance.fact_id,"governance_task_accepted","tasks"))return bad("GovernanceFinalSourceMismatch");
    const auto& work=*task.work;const auto* plan=state.plans.find(final.outcome_plan_id);const auto* authority=state.authorities.find(acceptance.authority_source_fact_id);const auto* jurisdiction=politics.jurisdictions.find(acceptance.jurisdiction_id);
    if(!plan||!authority||!jurisdiction||plan->id!=final.outcome_plan_id||plan->authority_fact_id!=authority->id||plan->task_id!=final.task_id||plan->contract_id!=final.contract_id||plan->action_id!=final.action_id||plan->work_fact_id!=final.work_completion_fact_id||work.fact_id!=plan->work_fact_id||work.task_id!=acceptance.task_id||work.contract_id!=acceptance.contract_id||work.acceptance_fact_id!=acceptance.fact_id||plan->value.executor_actor_id!=acceptance.executor_actor_id||acceptance.work_ticks!=authority->rule.duration_ticks||work.outcome_plan_id!=plan->id||work.action_id!=plan->action_id||work.completed_tick!=plan->value.work_completed_tick||final.completed_tick<work.completed_tick||acceptance.executor_actor_id!=authority->executor_actor_id||acceptance.organization_id!=authority->organization_id||acceptance.site_id!=authority->site_id||acceptance.jurisdiction_id!=authority->jurisdiction_id||acceptance.rule_id!=authority->rule.id||acceptance.rule_version!=authority->rule.version||!fact_is(c,work.fact_id,"governance_work_completed","tasks")||!fact_is(c,plan->id,governance_outcome_fact_kind,"politics")||!fact_is(c,authority->id,governance_authority_fact_kind,"politics"))return bad("GovernanceOutcomeSourceMismatch");
    auto valid=validate_governance_outcome_plan(authority->rule,plan->value);if(!valid.ok)return {false,{},valid.error};
    auto read=resources.public_relief_receipt(final.resource_receipt_ids.front());if(!read.ok)return {false,{},read.error};if(!read.value)return bad("GovernanceResourceReceiptMissing");const auto& receipt=*read.value;
    if(receipt.id!=final.resource_receipt_ids.front()||receipt.task_id!=final.task_id||receipt.contract_id!=final.contract_id||receipt.action_id!=final.action_id||receipt.acceptance_fact_id!=acceptance.fact_id||receipt.authority_source_fact_id!=authority->id||receipt.terms_id!=acceptance.relief_terms_id||receipt.work_fact_id!=work.fact_id||receipt.tick!=final.completed_tick||receipt.cash_transferred!=authority->rule.cash_amount||receipt.grain_transferred!=authority->rule.grain_amount||!fact_is(c,receipt.id,"public_relief_distributed","local_economy"))return bad("GovernanceResourceReceiptMismatch");
    return {true,PoliticalCivilSource{jurisdiction->id,"tasks",final_id,plan->id,final.completed_tick,plan->value.support_delta,plan->value.support_cap},{}};
}
Outcome<PoliticalCivilSource> read_assessed_fiscal_civil_source(const CommonStore& c,const PoliticsState& politics,const PoliticsGovernanceState& state,const PoliticsGovernanceCatalog& catalog,const PoliticalFiscalState& policies,const FiscalSettlementState& fiscal,const StableId& period){
    if(!catalog.enabled)return {false,{},common_failure("GovernanceNotEnabled").error};auto assessment=fiscal_assessment_result(c,fiscal,period);if(!assessment||!assessment->assessment_completed||assessment->fact_id!=period||assessment->due_tick>c.tick()||!fact_is(c,period,"fiscal_period_assessed","local_economy"))return {false,{},common_failure("FiscalAssessmentNotCompleted",period).error};
    const auto* jurisdiction=politics.jurisdictions.find(assessment->jurisdiction_id);const auto* policy=policies.policies.find(assessment->policy_fact_id);
    if(!jurisdiction||!policy||policy->snapshot.jurisdiction_id!=assessment->jurisdiction_id||policy->snapshot.site_id!=assessment->site_id||policy->snapshot.organization_id!=assessment->organization_id||policy->snapshot.mode!=assessment->mode||policy->snapshot.effective_from_tick>assessment->due_tick||!fact_is(c,policy->id,"political_fiscal_policy_changed","politics"))return {false,{},common_failure("FiscalCivilPolicyMismatch",period).error};
    for(const auto& row:state.consequences.rows())if(row.second.source_domain=="local_economy"&&row.second.source_fact_id!=period){auto prior=fiscal_assessment_result(c,fiscal,row.second.source_fact_id);if(prior&&prior->jurisdiction_id==assessment->jurisdiction_id&&prior->due_tick==assessment->due_tick)return {false,{},common_failure("FiscalCivilPeriodAlreadyConsumed",period).error};}
    const Integer delta=assessment->mode==FiscalTaxMode::lenient?3:assessment->mode==FiscalTaxMode::surcharge?-5:0;
    return {true,{jurisdiction->id,"local_economy",period,{},assessment->due_tick,delta,100},{}};
}
Outcome<std::optional<StableId>> apply_governance_civil_consequence_in_transaction(CommonStore& c,const PoliticsState& politics,PoliticsGovernanceState& state,const PoliticsGovernanceCatalog& catalog,const TaskGovernanceRead& tasks,const PublicReliefResultRead& resources,const StableId& final_id){
    auto source=read_completed_governance_civil_source(c,politics,state,catalog,tasks,resources,final_id);if(!source.ok)return {false,{},source.error};if(!source.value)return {true,std::nullopt,{}};const auto& value=*source.value;auto applied=apply_source(c,state,value.jurisdiction_id,value.source_domain,value.source_fact_id,value.plan_id,value.delta,value.cap);return applied.ok?Outcome<std::optional<StableId>>{true,applied.value,{}}:Outcome<std::optional<StableId>>{false,{},applied.error};
}
Outcome<StableId> apply_fiscal_civil_consequence_in_transaction(CommonStore& c,const PoliticsState& politics,PoliticsGovernanceState& state,const PoliticsGovernanceCatalog& catalog,const PoliticalFiscalState& policies,const FiscalSettlementState& fiscal,const StableId& period){
    auto source=read_assessed_fiscal_civil_source(c,politics,state,catalog,policies,fiscal,period);if(!source.ok)return {false,{},source.error};const auto& value=source.value;return apply_source(c,state,value.jurisdiction_id,value.source_domain,value.source_fact_id,value.plan_id,value.delta,value.cap);
}
Outcome<StableId> apply_occupation_fiscal_civil_in_transaction(CommonStore& c,const PoliticsState& politics,PoliticsGovernanceState& state,const PoliticsGovernanceCatalog& catalog,const FiscalSettlementState* legacy,const OccupationFiscalCivilRead& read,const StableId& id){
    if(!catalog.enabled)return {false,{},common_failure("GovernanceNotEnabled").error};
    auto source=read_occupation_fiscal_civil_source(c,politics,state,legacy,read,id);if(!source.ok)return {false,{},source.error};const auto& value=source.value;
    return apply_source(c,state,value.jurisdiction_id,value.source_domain,value.source_fact_id,value.plan_id,value.delta,value.cap);
}

}
