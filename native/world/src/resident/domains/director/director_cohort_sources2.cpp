#include "director_cohort_sources2.hpp"
#include "resident/domains/local_economy/household_buyer_execution.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {namespace {
template<class T>Outcome<T> fail(const char*c,const StableId&id={}){return {false,{},common_failure(c,id).error};}
bool same(const SourceRef&a,const SourceRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
bool terminal(HouseholdPurchasePhase p){return p==HouseholdPurchasePhase::received||p==HouseholdPurchasePhase::cancelled||p==HouseholdPurchasePhase::lost;}
bool proof_equal(const HouseholdProvisionControlProof&a,const HouseholdProvisionControlProof&b){return a.actor_id==b.actor_id&&a.household_group_id==b.household_group_id&&a.fact_id==b.fact_id&&a.source_role_id==b.source_role_id&&a.operation==b.operation&&a.source_revision==b.source_revision&&a.effective_from_tick==b.effective_from_tick&&a.through_tick==b.through_tick;}
bool belongs(const HouseholdPurchasePlan&p,const SubjectRef&s){const auto*b=std::get_if<CohortHouseholdBuyer>(&p.execution_choice);return b&&s.kind==SubjectKind::cohort&&b->cohort_id==s.id;}
Outcome<bool> active_policy(const HouseholdPurchaseContext&x,const HouseholdPurchasePolicy&p){
 if(p.closed_source_ref||!p.allow_cohort_buyer||x.now.half_day_tick<p.valid_from_tick||x.now.half_day_tick>=p.valid_until_tick)return {true,false,{}};
 auto control=x.controls.current(p.controller_actor_id,p.household_id,household_purchase_operation,x.now.half_day_tick);if(!control.ok)return {false,false,control.error};return {true,control.value&&proof_equal(*control.value,p.controller_authority_ref),{}};
}
const HouseholdPurchasePolicy* policy(const HouseholdPurchaseContext&x,const SourceRef&r){const auto*p=x.state.policies.find(r.id);return p&&same(p->acceptance_source_ref,r)?p:nullptr;}
bool has_purchase_responsibility(const DirectorCohortSourceContext2&owner,const SubjectRef&s,const SourceRef*required_policy){
 const auto&x=owner.purchase;
 for(const auto&row:x.state.purchases.rows()){if(terminal(row.second.state))continue;const auto*p=x.state.plans.find(row.second.plan_ref);if(p&&belongs(*p,s)&&(!required_policy||same(p->accepted_policy_ref,*required_policy)))return true;}
 // Objective cargo closure does not tell the original Plan cohort. Keep its
 // already-adopted finite responsibility until its own real result handling
 // closes the plan. Household ownership is not a knowledge receipt.
 if(owner.director.subject2)for(const auto&row:owner.director.plans.rows()){
  const auto&p=row.second;if(p.subject.kind!=s.kind||p.subject.id!=s.id||p.phase==PlanPhase::completed)continue;
  const auto*b=owner.director.subject2->subject_plan_bindings.find(p.id);const auto*purchase=b?std::get_if<CohortPurchaseBinding>(&b->binding):nullptr;
  if(!purchase||!purchase->execution_ref)continue;
  const auto*plan=x.state.plans.find(purchase->purchase_plan_ref.id);
  if(plan&&belongs(*plan,s)&&(!required_policy||same(plan->accepted_policy_ref,*required_policy)))return true;
 }
 return false;
}
}
bool NativeDirectorCohortSources2::bound_to(const CommonStore&c,const DirectorState&d)const noexcept{return &c==&x_.purchase.common&&&d==&x_.director;}
Outcome<std::optional<SubjectAdmissionRead2>> NativeDirectorCohortSources2::admission(const SubjectRef&subject)const{
 const auto&x=x_.purchase;if(subject.kind!=SubjectKind::cohort||!x_.director.subject2||!x_.catalog.subjects2)return fail<std::optional<SubjectAdmissionRead2>>("DirectorCohortSourceRequired",subject.id);
 auto valid=validate_household_purchases(x);if(!valid.ok)return {false,{},valid.error};const auto*cohort=x.economy.cohorts.find(subject.id);if(!cohort)return fail<std::optional<SubjectAdmissionRead2>>("DirectorCohortSourceMissing",subject.id);
 const auto*prior=x_.director.subject2->registrations.find(director_subject_key2(subject));const auto*prior_source=prior?std::get_if<CohortDirectorAdmission>(&prior->admission_source):nullptr;
 const HouseholdPurchasePolicy*selected=nullptr;
 // A policy's allow_cohort_buyer flag does not select a person. Only the real
 // accepted Plan names this cohort and a single accepted policy revision.
 for(const auto&row:x.state.plans.rows()){const auto&p=row.second;if(!belongs(p,subject))continue;const auto*terms=policy(x,p.accepted_policy_ref);if(!terms||terms->revision!=p.accepted_policy_revision||p.household_id!=cohort->household_group_id)return fail<std::optional<SubjectAdmissionRead2>>("DirectorCohortPlanPolicyMismatch",row.first);auto active=active_policy(x,*terms);if(!active.ok)return {false,{},active.error};if(!active.value)continue;if(selected&&!same(selected->acceptance_source_ref,terms->acceptance_source_ref))return fail<std::optional<SubjectAdmissionRead2>>("AmbiguousCohortAcceptedPolicy",subject.id);selected=terms;}
 const bool prior_responsibility=prior_source&&has_purchase_responsibility(x_,subject,&prior_source->accepted_household_policy_ref);
 const bool keep_prior=prior_responsibility&&(!selected||!same(selected->acceptance_source_ref,prior_source->accepted_household_policy_ref));
 const HouseholdPurchasePolicy*accepted=keep_prior?policy(x,prior_source->accepted_household_policy_ref):selected;
 if(!accepted&&prior_source)accepted=policy(x,prior_source->accepted_household_policy_ref);
 if(!accepted)return prior?fail<std::optional<SubjectAdmissionRead2>>("DirectorCohortAcceptedPolicyMissing",subject.id):Outcome<std::optional<SubjectAdmissionRead2>>{true,std::nullopt,{}};
 const std::pair<StableId,DirectorAdmission>*authored=nullptr;
 if(keep_prior){for(const auto&row:x_.catalog.admissions.rows())if(row.first==prior->admission_id)authored=&row;}
 else for(const auto&row:x_.catalog.admissions.rows()){if(row.second.kind!=DirectorAdmissionKind::household_cohorts)continue;if(!row.second.organization_id.empty())return fail<std::optional<SubjectAdmissionRead2>>("CohortDirectorOrganizationFilterRequired",row.first);const auto*p=x_.catalog.policies.find(row.second.policy_id);if(!p)return fail<std::optional<SubjectAdmissionRead2>>("UnknownDirectorPolicy",row.first);if(std::find(p->templates.begin(),p->templates.end(),DirectorTemplate::economic_household)==p->templates.end())return fail<std::optional<SubjectAdmissionRead2>>("CohortOriginalTemplateProviderRequired",row.first);if(authored)return fail<std::optional<SubjectAdmissionRead2>>("AmbiguousCohortDirectorAdmission",subject.id);authored=&row;}
 if(!authored&&prior)for(const auto&row:x_.catalog.admissions.rows())if(row.first==prior->admission_id)authored=&row;
 if(!authored)return {true,std::nullopt,{}};
 auto population=read_civilian_population_ref(x.economy,x.population,subject.id);if(!population.ok)return {false,{},population.error};auto original_population=read_population_revision_source(x.economy,x.population,population.value);if(!original_population.ok)return {false,{},original_population.error};
 SubjectAdmissionRead2 out;auto&r=out.registration;if(prior)r=*prior;else{r.epoch=1;r.entered_tick=r.changed_tick=x.now.half_day_tick;}r.subject=subject;r.policy_id=authored->second.policy_id;r.admission_id=authored->first;r.admission_source=CohortDirectorAdmission{accepted->acceptance_source_ref,population.value.population_revision};
 out.route=DirectorDecisionRoute2::cohort_purchase;out.template_id="economic_household/1";out.source_active=selected&&!keep_prior;out.has_frozen_responsibility=has_purchase_responsibility(x_,subject,&accepted->acceptance_source_ref);r.obligations_only=out.has_frozen_responsibility&&!out.source_active;
 r.lifecycle=out.source_active||out.has_frozen_responsibility?DirectorLifecycle::active:DirectorLifecycle::retired;r.reason=r.obligations_only?"ExistingResponsibilityOnly":out.source_active?"AcceptedHouseholdPlan":"HouseholdSourceClosed";
 if(out.source_active&&!out.has_frozen_responsibility&&(cohort->status!=PopulationStatus::civilian||cohort->headcount<1||cohort->labor_capacity<1)){r.lifecycle=DirectorLifecycle::paused;r.reason="CohortUnavailable";}
 return {true,std::optional<SubjectAdmissionRead2>{std::move(out)},{}};
}
Status NativeDirectorCohortSources2::visit_admissions(const std::function<Status(const SubjectAdmissionRead2&)>&visit)const{
 if(!visit)return common_failure("DirectorAdmissionVisitorRequired");for(const auto&row:x_.purchase.economy.cohorts.rows()){auto a=admission({SubjectKind::cohort,row.first});if(!a.ok)return {false,{},a.error};if(a.value){auto v=visit(*a.value);if(!v.ok)return v;}}return common_success();
}
Outcome<CohortKnownSubjectView> NativeDirectorCohortSources2::known(const SubjectRef&subject)const{
 auto admitted=admission(subject);if(!admitted.ok)return {false,{},admitted.error};if(!admitted.value)return fail<CohortKnownSubjectView>("DirectorCohortNotAdmitted",subject.id);const auto*source=std::get_if<CohortDirectorAdmission>(&admitted.value->registration.admission_source);if(!source)return fail<CohortKnownSubjectView>("DirectorCohortSourceRequired",subject.id);
 const auto&x=x_.purchase;const auto*cohort=x.economy.cohorts.find(subject.id);const auto*home=cohort?x.economy.households.find(cohort->household_group_id):nullptr;const auto*accepted=policy(x,source->accepted_household_policy_ref);const auto*wallet=home?x.common.wallet(home->wallet_id):nullptr;const auto*inventory=home?x.common.inventory(home->inventory_id):nullptr;
 if(!home||!accepted||!wallet||!inventory||wallet->owner.kind!=Owner::Kind::household||wallet->owner.id!=home->id||inventory->owner.kind!=Owner::Kind::household||inventory->owner.id!=home->id)return fail<CohortKnownSubjectView>("DirectorCohortPrivateAssetsMismatch",subject.id);
 auto population=read_civilian_population_ref(x.economy,x.population,subject.id);if(!population.ok)return {false,{},population.error};auto original=read_population_revision_source(x.economy,x.population,population.value);if(!original.ok)return {false,{},original.error};
 const auto*authority=x.common.state().facts.find(accepted->controller_authority_ref.fact_id);if(!authority||authority->domain!="family"||authority->kind!="family_household_control")return fail<CohortKnownSubjectView>("DirectorCohortControlSourceMissing",subject.id);
 CohortKnownSubjectView out;out.subject=subject;out.household_id=home->id;out.source_revision=population.value.population_revision;out.location_source_ref=original.value.location_binding_ref;out.labor_capacity=cohort->labor_capacity;out.accepted_policy_ref=accepted->acceptance_source_ref;out.controller_authority_ref={authority->domain,authority->kind,authority->id};out.wallet_refs.push_back(home->wallet_id);out.inventory_refs.push_back(home->inventory_id);
 __int128 allocated=0;for(const auto&row:x.economy.labor_allocations.rows()){const auto&a=row.second;if(a.cohort_id==subject.id&&a.from_tick<=x.now.half_day_tick&&a.through_tick>=x.now.half_day_tick){if(a.workers<0)return fail<CohortKnownSubjectView>("InvalidCohortLaborAllocation",row.first);allocated+=a.workers;}}if(allocated>std::numeric_limits<Integer>::max()||allocated>cohort->labor_capacity)return fail<CohortKnownSubjectView>("DirectorCohortLaborOverAllocated",subject.id);out.labor_allocated=static_cast<Integer>(allocated);
 for(const auto&row:x.state.purchases.rows()){const auto*p=x.state.plans.find(row.second.plan_ref);if(!p||!belongs(*p,subject)||!same(p->accepted_policy_ref,accepted->acceptance_source_ref)||terminal(row.second.state))continue;out.purchase_responsibilities.push_back(row.second.accepted_terms_ref);if(row.second.delivery_binding)out.transport_responsibilities.push_back(*row.second.delivery_binding);}
 if(!x_.social.dynamic||!x_.social.dynamic->time2)return fail<CohortKnownSubjectView>("ExactSocialStateRequired");if(const auto*known=find_social_knowledge_table(*x_.social.dynamic,subject))for(const auto&row:known->rows()){auto receipt=read_knowledge_receipt_view_v2(x.common,x_.social,x_.results,subject,row.second.report_id,x_.stamp);if(!receipt.ok)return {false,{},receipt.error};if(!receipt.value)return fail<CohortKnownSubjectView>("DirectorCohortKnowledgeMissing",row.second.report_id);auto past=compare_world_moments(receipt.value->learned_at,x.now);if(!past.ok)return {false,{},past.error};if(past.value>0)return fail<CohortKnownSubjectView>("DirectorFutureKnowledge",row.second.report_id);out.known_report_refs.push_back({"social","report",row.second.report_id});const auto*report=x_.social.dynamic->reports.find(row.second.report_id);if(report&&report->observation.supply)out.known_market_refs.push_back({"social","report",row.second.report_id});}
 return {true,std::move(out),{}};
}
}
