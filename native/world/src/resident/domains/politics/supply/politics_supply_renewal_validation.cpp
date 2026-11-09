// 冷计划保原签发人和原登记，不从今天的任职或最新草案反造旧采用来源。
#include "politics_supply_renewal_validation.hpp"
namespace hanmo::resident {
Status validate_political_supply_renewal_modules(bool enabled,const std::vector<std::pair<StableId,StableId>>& modules){
    Integer count=0,economic=0,political=0;for(const auto& m:modules){
        if(m.first==political_supply_renewal_module){++count;if(m.second!="1")return common_failure("UnsupportedPoliticalSupplyRenewalVersion");}
        if(m.first=="economic_supply_policies"&&m.second=="1")++economic;
        if(m.first=="political_supply_adoptions"&&m.second=="1")++political;
    }
    if(count!=(enabled?1:0)||(enabled&&(economic!=1||political!=1)))return common_failure("PoliticalSupplyRenewalProfileMismatch");
    return common_success();
}
Status validate_political_supply_renewal_director(const DirectorCatalog& catalog,const DirectorState& state,
    const PoliticalTakeoverContext* political,const EconomicSupplyPolicyContext* economic){
    const auto* intents=catalog.supply_renewals.get();
    if(intents){if(!political||!economic||&political->common!=&economic->ledger.common||political->phase||economic->ledger.phase||!political->supply_adoptions||!political->supply_policies)return common_failure("PoliticalSupplyRenewalPairRequired");
        auto valid=validate_political_supply_renewals(*intents,political->catalog,political->organization_catalog,economic->ledger.catalog);if(!valid.ok)return valid;}
    const auto& policies=catalog.dynamic_subjects?catalog.policies:catalog.subjects;
    for(const auto& row:policies.rows()){
        const auto& p=row.second;const bool selected=std::find(p.templates.begin(),p.templates.end(),DirectorTemplate::political_supply_renewal)!=p.templates.end();
        if(selected!=p.supply_renewal_intent_id.has_value())return common_failure("InvalidPoliticalSupplyRenewalDirectorPolicy",row.first);
        if(!selected)continue;if(!intents||!catalog.dynamic_subjects)return common_failure("PoliticalSupplyRenewalDirectorProfileRequired",row.first);
        const auto* intent=intents->intents.find(*p.supply_renewal_intent_id);const auto* definition=intent?economic->ledger.catalog.policies.find(intent->author_policy_id):nullptr;
        if(!definition||definition->organization_id!=p.organization_id)return common_failure("PoliticalSupplyRenewalDirectorOrganizationMismatch",row.first);
    }
    const auto check=[&](const auto& plans)->Status{for(const auto& row:plans.rows()){
        const auto& plan=row.second;if(plan.template_id!="political_supply_renewal")continue;
        const auto* policy=catalog.dynamic_subjects?policies.find(plan.policy_id):policies.find(plan.subject.id);
        if(!intents||!political||!economic||!policy||!policy->supply_renewal_intent_id||plan.subject.kind!=SubjectKind::actor
            ||std::find(policy->templates.begin(),policy->templates.end(),DirectorTemplate::political_supply_renewal)==policy->templates.end())return common_failure("PoliticalSupplyRenewalPlanMismatch",row.first);
        const auto* intent=intents->intents.find(*policy->supply_renewal_intent_id);const auto* registration=political->state.registrations.find(plan.goal_ref);
        if(!intent||!registration||registration->rule_id!=intent->takeover_rule_id||registration->successor_organization_id!=policy->organization_id
            ||plan.current_action_id||!plan.commitment_ids.empty()||(plan.phase!=PlanPhase::waiting&&plan.phase!=PlanPhase::completed))return common_failure("PoliticalSupplyRenewalPlanMismatch",row.first);
        const auto* source=economic->ledger.catalog.policies.find(intent->author_policy_id);
        if(!source||std::none_of(registration->responsibilities.begin(),registration->responsibilities.end(),[&](const auto& original){return original.responsibility.kind==PoliticalResponsibilityKind::supply_source&&original.responsibility.responsibility_id==source->original_source_key;}))return common_failure("PoliticalSupplyRenewalPlanSourceMissing",row.first);
        if(plan.phase!=PlanPhase::completed)continue;bool real=false;
        for(const auto& p:economic->ledger.state.policies.rows())if(p.second.registration_fact_id==plan.goal_ref&&p.second.author_policy_id==intent->author_policy_id
            &&p.second.authority.actor_id==plan.subject.id&&p.second.authority.organization_id==policy->organization_id)real=true;
        for(const auto& a:political->state.approvals.rows())if(a.second.registration_fact_id==plan.goal_ref&&a.second.authority.actor_id==plan.subject.id){
            for(const auto& id:a.second.decision_fact_ids){const auto* binding=political->supply_adoptions?political->supply_adoptions->bindings.find(id):nullptr;
                const auto* draft=binding?economic->ledger.state.policies.find(binding->policy_issue_fact_id):nullptr;
                if(draft&&draft->author_policy_id==intent->author_policy_id&&draft->authority.organization_id==policy->organization_id)real=true;}
        }
        if(!real)return common_failure("PoliticalSupplyRenewalResultMissing",row.first);
    }return common_success();};auto valid=check(state.plans);return valid.ok?check(state.archived_plans):valid;
}
}
