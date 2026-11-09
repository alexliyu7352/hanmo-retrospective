// 治理冷恢复逐个真来源和前后值重建；同工作/同税期不能第二次获益。
#include "politics_governance_validation.hpp"
#include "takeover/politics_occupation_fiscal_civil.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include "resident/domains/tasks/governance_task_contract.hpp"
#include <map>
#include <set>
namespace hanmo::resident {
namespace {
bool head(const CommonStore& c,const StableId& id,const char* kind){const auto* fact=c.state().facts.find(id);return fact&&fact->id==id&&fact->kind==kind&&fact->domain=="politics";}
bool instance(const CommonStore& c,const StableId& id,const char* prefix){auto value=organization_instance_sequence(id,prefix);return value&&*value<c.state().next_instance_id;}
bool same_rule(const AppeasementRule& a,const AppeasementRule& b){return a.id==b.id&&a.version==b.version&&a.name_key==b.name_key&&a.grain_item_id==b.grain_item_id&&a.duration_ticks==b.duration_ticks&&a.cash_amount==b.cash_amount&&a.grain_amount==b.grain_amount&&a.base_support_gain==b.base_support_gain&&a.support_cap==b.support_cap;}
Integer consequence(Integer before,const PoliticalCivilSource& source){return source.delta>=0?(before>=source.cap?before:std::min<Integer>(source.cap,before+source.delta)):std::max<Integer>(0,before+source.delta);}
}
Status validate_political_governance(const CommonStore& c,const CommonCatalog& cc,const PoliticsState& politics,const PoliticsGovernanceState& state,const PoliticsGovernanceCatalog& catalog,const OrganizationState& org,const OrganizationCatalog& oc,const ActivityRead& activity,const TaskGovernanceRead& tasks,const PublicReliefResultRead& resources,const PoliticalFiscalState* policies,const FiscalSettlementState* fiscal){return validate_political_governance(c,cc,politics,state,catalog,org,oc,activity,tasks,resources,policies,fiscal,nullptr);}
Status validate_political_governance(const CommonStore& c,const CommonCatalog& cc,const PoliticsState& politics,const PoliticsGovernanceState& state,const PoliticsGovernanceCatalog& catalog,const OrganizationState& org,const OrganizationCatalog& oc,const ActivityRead& activity,const TaskGovernanceRead& tasks,const PublicReliefResultRead& resources,const PoliticalFiscalState* policies,const FiscalSettlementState* fiscal,const OccupationFiscalCivilRead* occupation){return validate_political_governance(c,cc,politics,state,catalog,org,oc,activity,tasks,resources,policies,fiscal,occupation,nullptr);}
Status validate_political_governance(const CommonStore& c,const CommonCatalog& cc,const PoliticsState& politics,const PoliticsGovernanceState& state,const PoliticsGovernanceCatalog& catalog,const OrganizationState& org,const OrganizationCatalog& oc,const ActivityRead& activity,const TaskGovernanceRead& tasks,const PublicReliefResultRead& resources,const PoliticalFiscalState* policies,const FiscalSettlementState* fiscal,const OccupationFiscalCivilRead* occupation,const PoliticalGovernanceAgencyRead* agencies){
    if(!catalog.enabled)return state.authorities.empty()&&state.plans.empty()&&state.civil.empty()&&state.consequences.empty()?common_success():common_failure("UnexpectedGovernanceState");
    if(politics.jurisdictions.empty()||catalog.initial_support.size()!=politics.jurisdictions.size()||state.civil.size()!=politics.jurisdictions.size())return common_failure("PoliticalInitialSupportRequired");
    for(const auto& row:catalog.appeasement_rules.rows()){if(row.first!=row.second.id||!cc.texts.find(row.second.name_key)||!cc.items.find(row.second.grain_item_id))return common_failure("InvalidAppeasementRule",row.first);auto rule=validate_appeasement_rule(row.second);if(!rule.ok)return rule;}
    for(const auto& row:catalog.commission_policies.rows()){const auto& p=row.second;const auto* rule=catalog.appeasement_rules.find(p.rule_id);if(row.first!=p.id||!rule||p.relief_policy_id.empty()||p.offer_valid_ticks<1||p.completion_deadline_ticks<rule->duration_ticks)return common_failure("InvalidGovernanceCommissionPolicy",row.first);}
    std::set<StableId> registered;PoliticsGovernanceAccess authority(c,politics,state,catalog,org,oc,activity,agencies);
    for(const auto& row:state.authorities.rows()){
        const auto& a=row.second;auto approved=authority.governance_authority_approval(row.first);if(!approved.ok)return {false,{},approved.error};const auto* policy=catalog.commission_policies.find(a.policy_id);const auto* rule=policy?catalog.appeasement_rules.find(policy->rule_id):nullptr;
        if(!policy||!rule||!same_rule(a.rule,*rule)||a.relief_policy_id!=policy->relief_policy_id||a.completion_deadline_ticks!=policy->completion_deadline_ticks||!politics.jurisdictions.find(a.jurisdiction_id)||(!instance(c,a.id,"politics_governance_authority")&&(!agencies||!instance(c,a.id,"politics_governance_agency_authority")))||!registered.insert(a.id).second)return common_failure("InvalidGovernanceAuthorityRecord",row.first);
    }
    std::set<StableId> work_ids;
    for(const auto& row:state.plans.rows()){
        const auto& plan=row.second;const auto* a=state.authorities.find(plan.authority_fact_id);if(row.first!=plan.id||!a||!head(c,plan.id,governance_outcome_fact_kind)||!instance(c,plan.id,"politics_governance_plan")||!registered.insert(plan.id).second||!work_ids.insert(plan.work_fact_id).second)return common_failure("InvalidRecordedGovernancePlan",row.first);
        auto valid=validate_governance_outcome_plan(a->rule,plan.value);if(!valid.ok)return valid;auto read=tasks.governance_task_source(plan.task_id);if(!read.ok)return {false,{},read.error};const auto& task=read.value;const auto& acceptance=task.acceptance;const auto* acceptance_head=c.state().facts.find(acceptance.fact_id);const auto* work_head=c.state().facts.find(plan.work_fact_id);
        if(!acceptance_head||acceptance_head->domain!="tasks"||acceptance_head->kind!="governance_task_accepted"||!work_head||work_head->domain!="tasks"||work_head->kind!="governance_work_completed"||acceptance.accepted_tick<a->issued_tick||acceptance.accepted_tick>=a->offer_expires_tick||acceptance.accepted_revision<0||acceptance.accepted_revision>c.revision()||acceptance.accepted_tick>INT64_MAX-a->completion_deadline_ticks||acceptance.deadline_tick!=acceptance.accepted_tick+a->completion_deadline_ticks||!task.work||task.interruption||task.work->fact_id!=plan.work_fact_id||task.work->outcome_plan_id!=plan.id||task.work->task_id!=plan.task_id||task.work->contract_id!=plan.contract_id||task.work->action_id!=plan.action_id||task.work->acceptance_fact_id!=acceptance.fact_id||task.work->completed_tick!=plan.value.work_completed_tick||task.work->completed_tick>c.tick()||task.work->started_tick<acceptance.accepted_tick||task.work->completed_tick-task.work->started_tick<acceptance.work_ticks||acceptance.authority_source_fact_id!=a->id||acceptance.executor_actor_id!=a->executor_actor_id||acceptance.executor_actor_id!=plan.value.executor_actor_id||acceptance.organization_id!=a->organization_id||acceptance.jurisdiction_id!=a->jurisdiction_id||acceptance.site_id!=a->site_id||acceptance.rule_id!=a->rule.id||acceptance.rule_version!=a->rule.version||acceptance.work_ticks!=a->rule.duration_ticks)return common_failure("GovernancePlanWorkMismatch",row.first);
    }
    struct Expected {Integer support=0,revision=0,last_tick=0;StableId last_fact;};std::map<StableId,Expected> expected;
    for(const auto& row:politics.jurisdictions.rows()){
        const auto* value=state.civil.find(row.first);const auto* initial=catalog.initial_support.find(row.first);
        if(!value||!initial||*initial<0||*initial>100||value->id!=row.first||value->support<0||value->support>100||value->revision<0||!head(c,value->registration_fact_id,"political_civil_registered")||!instance(c,value->registration_fact_id,"politics_civil")||!registered.insert(value->registration_fact_id).second)return common_failure("InvalidPoliticalCivilState",row.first);
        expected.emplace(row.first,Expected{*initial,0,0,value->registration_fact_id});
    }
    std::set<std::pair<StableId,StableId>> consumed;std::set<StableId> consumed_plans;
    for(const auto& row:state.consequences.rows()){
        const auto& result=row.second;auto position=expected.find(result.jurisdiction_id);
        if(position==expected.end()||result.id!=row.first||!head(c,result.id,"political_civil_consequence")||!instance(c,result.id,"politics_civil_result")||!registered.insert(result.id).second||!consumed.emplace(result.source_domain,result.source_fact_id).second)return common_failure("InvalidCivilConsequence",row.first);
        Outcome<PoliticalCivilSource> source;
        if(result.source_domain=="tasks"){auto read=read_completed_governance_civil_source(c,politics,state,catalog,tasks,resources,result.source_fact_id);if(!read.ok)return {false,{},read.error};if(!read.value||result.plan_id.empty()||!consumed_plans.insert(result.plan_id).second)return common_failure("DuplicateGovernancePlanConsequence",row.first);source={true,*read.value,{}};}
        else if(result.source_domain=="local_economy"){const auto* origin=c.state().facts.find(result.source_fact_id);if(origin&&origin->kind=="occupation_fiscal_period_assessed"){if(!occupation)return common_failure("OccupationFiscalCivilSourceRequired",row.first);source=read_occupation_fiscal_civil_source(c,politics,state,fiscal,*occupation,result.source_fact_id);}else{if(!policies||!fiscal)return common_failure("FiscalCivilSourceRequired",row.first);source=read_assessed_fiscal_civil_source(c,politics,state,catalog,*policies,*fiscal,result.source_fact_id);}}
        else return common_failure("UnknownCivilConsequenceDomain",row.first);
        if(!source.ok)return {false,{},source.error};const auto& value=source.value;auto& previous=position->second;
        if(value.jurisdiction_id!=result.jurisdiction_id||value.plan_id!=result.plan_id||value.delta!=result.requested_delta||result.before!=previous.support||previous.revision==INT64_MAX||result.revision!=previous.revision+1||result.previous_fact_id!=previous.last_fact||result.tick<previous.last_tick||result.tick<value.source_tick||result.tick>c.tick()||result.after!=consequence(previous.support,value))return common_failure("CivilConsequenceHistoryMismatch",row.first);
        previous={result.after,result.revision,result.tick,result.id};
    }
    for(const auto& row:expected){const auto& actual=*state.civil.find(row.first);if(actual.support!=row.second.support||actual.revision!=row.second.revision||actual.last_fact_id!=row.second.last_fact)return common_failure("PoliticalCivilHistoryMismatch",row.first);}
    for(const auto& row:c.state().facts.rows()){
        // 最终任务、实付与政治后果属于同一发布候选；只有Work完成可以合法等待结算。
        if(row.second.domain=="tasks"&&row.second.kind=="governance_task_completed"&&!consumed.count({"tasks",row.first}))return common_failure("MissingGovernanceCivilConsequence",row.first);
        if(occupation&&row.second.domain=="local_economy"&&row.second.kind=="occupation_fiscal_period_assessed"&&!consumed.count({"local_economy",row.first}))return common_failure("MissingOccupationFiscalCivilConsequence",row.first);
        if(row.second.domain=="politics"&&(row.second.kind==governance_authority_fact_kind||row.second.kind==governance_outcome_fact_kind||row.second.kind=="political_civil_registered"||row.second.kind=="political_civil_consequence")&&!registered.count(row.first))return common_failure("UnknownGovernanceFactPayload",row.first);
    }
    return common_success();
}
}
