#include "resident/kernel/catalog_section_observation.hpp"
// 九目标批准、真实执行绑定和专业效果一次采用；失败只撤销原未发布候选。
#include "governance_program.hpp"
#include "governance_approval_authority.hpp"
#include "governance_supervision_approvals.hpp"
#include "resident/domains/tasks/work_execution/work_reassignment_sources.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};[[noreturn]]void fail(const StableId& code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{value.error};return std::move(value.value);}
template<class T,class F>Outcome<T> guarded(F&& fn){try{return {true,fn(),{}};}catch(const Failure& e){return {false,{},e.error};}}
int compare(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b));}
bool rule_equal(const ActorFoundationRuleRef& a,const ActorFoundationRuleRef& b){return a.id==b.id&&a.version==b.version&&a.catalog_digest==b.catalog_digest;}
bool subject_equal(const SubjectRef& a,const SubjectRef& b){return a.kind==b.kind&&a.id==b.id;}
bool refs_equal(const std::vector<SourceRef>& a,const std::vector<SourceRef>& b){if(a.size()!=b.size())return false;for(std::size_t n=0;n<a.size();++n)if(!same_source_ref(a[n],b[n]))return false;return true;}
std::vector<SourceRef> application_sources(const GovernanceApplication& value){return std::visit([](const auto& x){using T=std::decay_t<decltype(x)>;if constexpr(std::is_same_v<T,GovernancePacificationApplication>)return std::vector<SourceRef>{x.actual_work_result,x.public_relief_receipt,x.civil_consequence};else if constexpr(std::is_same_v<T,GovernancePatrolApplication>)return std::vector<SourceRef>{x.actual_work_result,x.security_consequence};else if constexpr(std::is_same_v<T,GovernanceReliefApplication>){auto out=x.actual_receipts;out.push_back(x.civil_consequence);return out;}else if constexpr(std::is_same_v<T,GovernanceAgronomyApplication>||std::is_same_v<T,GovernanceIndustryApplication>)return std::vector<SourceRef>{x.production_improvement_receipt};else if constexpr(std::is_same_v<T,GovernanceFortificationApplication>)return std::vector<SourceRef>{x.fortification_repair_result};else if constexpr(std::is_same_v<T,GovernanceFacilityApplication>)return std::vector<SourceRef>{x.facility_activation_receipt};else if constexpr(std::is_same_v<T,GovernanceRecruitApplication>)return std::vector<SourceRef>{x.population_military_result};else return std::vector<SourceRef>{x.military_training_receipt};},value);}
bool application_equal(const GovernanceApplication& a,const GovernanceApplication& b){return a.index()==b.index()&&refs_equal(application_sources(a),application_sources(b));}
// 一次采用键取真实工作/收据，不允许换后果ID或另建program重复消费同一原结果。
std::vector<SourceRef> primary_sources(const GovernanceApplication& value){return std::visit([](const auto& x){using T=std::decay_t<decltype(x)>;if constexpr(std::is_same_v<T,GovernancePacificationApplication>||std::is_same_v<T,GovernancePatrolApplication>)return std::vector<SourceRef>{x.actual_work_result};else if constexpr(std::is_same_v<T,GovernanceReliefApplication>)return x.actual_receipts;else return application_sources(GovernanceApplication{x});},value);}
bool source_overlap(const GovernanceApplication& a,const GovernanceApplication& b){if(a.index()!=b.index()&&!((a.index()==1||a.index()==6)&&(b.index()==1||b.index()==6)))return false;for(const auto& x:primary_sources(a))for(const auto& y:primary_sources(b))if(same_source_ref(x,y))return true;return false;}
bool terminal_equal(const FollowThroughTerminalView& a,const FollowThroughTerminalView& b){return same_source_ref(a.original_obligation_ref,b.original_obligation_ref)&&same_source_ref(a.accepted_goal_ref,b.accepted_goal_ref)&&same_work_result_source(a.work_result_ref,b.work_result_ref)&&a.source_schema==b.source_schema&&same_source_ref(a.actual_cause_ref,b.actual_cause_ref)&&same_source_ref(a.continuity_source_ref,b.continuity_source_ref)&&refs_equal(a.preserved_actual_result_refs,b.preserved_actual_result_refs)&&compare(a.effective_game_time,b.effective_game_time)==0;}
bool outcome_equal(const GovernanceProgramOutcome& a,const GovernanceProgramOutcome& b){if(a.program_id!=b.program_id||a.target_kind!=b.target_kind||!same_source_ref(a.execution_ref,b.execution_ref)||!same_source_ref(a.actual_result_ref,b.actual_result_ref)||a.patrol_unavailable.has_value()!=b.patrol_unavailable.has_value()||(a.patrol_unavailable&&!terminal_equal(*a.patrol_unavailable,*b.patrol_unavailable))||a.status!=b.status||!refs_equal(a.remaining_requirement,b.remaining_requirement)||!same_source_ref(a.continuity,b.continuity)||compare(a.occurred_at,b.occurred_at)||a.application_refs.size()!=b.application_refs.size())return false;for(std::size_t i=0;i<a.application_refs.size();++i)if(!application_equal(a.application_refs[i],b.application_refs[i]))return false;return true;}
bool target_equal(const GovernanceTarget& a,const GovernanceTarget& b){if(a.index()!=b.index())return false;return std::visit([&](const auto& x){using T=std::decay_t<decltype(x)>;const auto& y=std::get<T>(b);if constexpr(std::is_same_v<T,GovernancePacification>)return x.jurisdiction_id==y.jurisdiction_id&&x.relief_terms_id==y.relief_terms_id&&x.support_delta_points==y.support_delta_points;else if constexpr(std::is_same_v<T,GovernanceAgronomy>)return x.production_unit_id==y.production_unit_id&&x.improvement_policy_id==y.improvement_policy_id;else if constexpr(std::is_same_v<T,GovernancePatrol>)return x.jurisdiction_id==y.jurisdiction_id&&x.unit_ids==y.unit_ids&&x.gain.index()==y.gain.index()&&std::visit([&](const auto& g){using G=std::decay_t<decltype(g)>;const auto& h=std::get<G>(y.gain);if constexpr(std::is_same_v<G,FixedApprovedDelta>)return g.security_delta_points==h.security_delta_points;else return rule_equal(g.rule_ref,h.rule_ref);},x.gain);else if constexpr(std::is_same_v<T,GovernanceFortificationRepair>)return x.stronghold_id==y.stronghold_id&&x.topology_id==y.topology_id&&x.node_id==y.node_id&&x.target_hp==y.target_hp;else if constexpr(std::is_same_v<T,GovernanceFacilityBuild>)return x.project_spec_id==y.project_spec_id&&same_source_ref(x.site_use_ref,y.site_use_ref)&&subject_equal(x.asset_owner,y.asset_owner);else if constexpr(std::is_same_v<T,GovernanceRelief>){if(x.relief_terms_ids!=y.relief_terms_ids||x.effect_period.period_from_tick!=y.effect_period.period_from_tick||x.effect_period.period_until_tick!=y.effect_period.period_until_tick||x.effect_measure.index()!=y.effect_measure.index())return false;return std::visit([&](const auto& u){using U=std::decay_t<decltype(u)>;const auto& v=std::get<U>(y.effect_measure);if constexpr(std::is_same_v<U,GovernanceCashRelief>)return u.currency_id==v.currency_id&&u.target_amount==v.target_amount&&u.max_support_delta==v.max_support_delta;else return u.item_id==v.item_id&&u.target_quantity==v.target_quantity&&u.max_support_delta==v.max_support_delta;},x.effect_measure);}else if constexpr(std::is_same_v<T,GovernanceIndustrySupport>)return x.production_unit_id==y.production_unit_id&&x.investment_policy_id==y.investment_policy_id&&same_source_ref(x.investment_contract_ref,y.investment_contract_ref);else if constexpr(std::is_same_v<T,GovernanceRecruit>)return x.recruitment_order_id==y.recruitment_order_id;else return x.training_order_id==y.training_order_id;},a);}
void header(const CommonStore& common,const SourceRef& source){const auto* f=common.state().facts.find(source.id);if(!valid_source_ref(source)||!f||f->domain!=source.domain||f->kind!=source.kind)fail("GovernanceOriginalSourceMissing",source.id);}
std::size_t position(const CommonStore& common,const SourceRef& source){header(common,source);const auto& ids=common.state().facts.order();auto found=std::find(ids.begin(),ids.end(),source.id);if(found==ids.end())fail("GovernanceSourceOrderMissing",source.id);return static_cast<std::size_t>(found-ids.begin());}
const GovernanceProgramSpec& specification(const GovernanceProgramReadContext& x,const GovernanceProgramApproval& a){const auto* s=x.catalog.specs.find(a.spec_ref.id);if(!s||!rule_equal(s->spec_ref,a.spec_ref)||!target_equal(s->target,a.target))fail("GovernanceProgramSpecMismatch",a.id);return *s;}
const GovernanceProgramApproval& approval(const GovernanceProgramReadContext& x,const SourceRef& ref){const auto* a=x.state.approvals.find(ref.id);if(!a||!same_source_ref(governance_program_source(*a),ref))fail("GovernanceProgramApprovalMissing",ref.id);header(x.common,ref);specification(x,*a);return *a;}
std::optional<WorkKind> work_kind(const GovernanceProgramSpec& s){switch(s.target.index()){case 1:return WorkKind::agronomy_supervision;case 2:return WorkKind::patrol;case 4:return WorkKind::construction_supervision;case 6:return WorkKind::industry_supervision;default:return {};}}
void unique_ids(const std::vector<StableId>& ids,const StableId& owner){std::set<StableId> seen;for(const auto& id:ids)if(id.empty()||!seen.insert(id).second)fail("InvalidGovernanceIds",owner);}
void target_shape(const GovernanceTarget& value){std::visit([&](const auto& x){using T=std::decay_t<decltype(x)>;
 if constexpr(std::is_same_v<T,GovernancePacification>){if(x.jurisdiction_id.empty()||x.relief_terms_id.empty()||x.support_delta_points<0)fail("InvalidGovernancePacification");}
 else if constexpr(std::is_same_v<T,GovernanceAgronomy>){if(x.production_unit_id.empty()||x.improvement_policy_id.empty())fail("InvalidGovernanceAgronomy");}
 else if constexpr(std::is_same_v<T,GovernancePatrol>){if(x.jurisdiction_id.empty()||x.unit_ids.empty())fail("InvalidGovernancePatrol");unique_ids(x.unit_ids,x.jurisdiction_id);std::visit([](const auto& g){using G=std::decay_t<decltype(g)>;if constexpr(std::is_same_v<G,FixedApprovedDelta>){if(g.security_delta_points<0||g.security_delta_points>100)fail("InvalidFixedPatrolGain");}else if(g.rule_ref.id.empty()||g.rule_ref.version<1||g.rule_ref.catalog_digest.empty())fail("InvalidPatrolGainRule");},x.gain);}
 else if constexpr(std::is_same_v<T,GovernanceFortificationRepair>){if(x.stronghold_id.empty()||x.topology_id.empty()||x.node_id.empty()||x.target_hp<=0)fail("InvalidGovernanceRepair");}
 else if constexpr(std::is_same_v<T,GovernanceFacilityBuild>){if(x.project_spec_id.empty()||!valid_source_ref(x.site_use_ref)||x.asset_owner.id.empty())fail("InvalidGovernanceFacility");}
 else if constexpr(std::is_same_v<T,GovernanceRelief>){if(x.relief_terms_ids.empty()||x.effect_period.period_from_tick<0||x.effect_period.period_until_tick<=x.effect_period.period_from_tick)fail("InvalidGovernanceRelief");unique_ids(x.relief_terms_ids,{});std::visit([](const auto& m){using U=std::decay_t<decltype(m)>;if(m.max_support_delta<0)fail("InvalidGovernanceReliefMeasure");if constexpr(std::is_same_v<U,GovernanceCashRelief>){if(m.currency_id.empty()||m.target_amount<=0)fail("InvalidGovernanceReliefMeasure");}else if(m.item_id.empty()||m.target_quantity<=0)fail("InvalidGovernanceReliefMeasure");},x.effect_measure);}
 else if constexpr(std::is_same_v<T,GovernanceIndustrySupport>){if(x.production_unit_id.empty()||x.investment_policy_id.empty()||!valid_source_ref(x.investment_contract_ref))fail("InvalidGovernanceIndustry");}
 else if constexpr(std::is_same_v<T,GovernanceRecruit>){if(x.recruitment_order_id.empty())fail("InvalidGovernanceRecruit");}
 else if(x.training_order_id.empty())fail("InvalidGovernanceTrain");
},value);}
void spec_shape(const GovernanceProgramCatalog& catalog,const GovernanceProgramSpec& s){if(s.spec_ref.id.empty()||s.spec_ref.version<1||s.spec_ref.catalog_digest!=catalog.identity.content_digest||s.resource_recipe_id.empty()||s.executor_qualification_id.empty()||s.required_work_half_days<0||s.scope.id.empty()||s.offer_valid_half_days<=0||!valid_source_ref(s.continuity_policy_ref))fail("InvalidGovernanceProgramSpec",s.spec_ref.id);target_shape(s.target);if(const auto* patrol=std::get_if<GovernancePatrol>(&s.target))if(const auto* gain=std::get_if<CompletedSkillRule>(&patrol->gain)){const auto* rule=catalog.patrol_gain_rules.find(gain->rule_ref.id);if(!rule||!rule_equal(rule->rule_ref,gain->rule_ref))fail("PatrolGainRuleMissing",s.spec_ref.id);}const auto n=s.target.index();const auto expected=n==3||n==5||n==8?GovernanceCompletionPolicy::incremental_receipts:n==4||n==7?GovernanceCompletionPolicy::external_terminal:GovernanceCompletionPolicy::whole_cycle;if(s.completion_policy!=expected)fail("GovernanceTargetCompletionPolicyMismatch",s.spec_ref.id);auto kind=work_kind(s);if(kind.has_value()!=s.approved_work_terms.has_value()||kind.has_value()!=s.expense_policy_ref.has_value()||(kind&&s.required_work_half_days<=0))fail("GovernanceWorkTermsRequired",s.spec_ref.id);if(s.expense_policy_ref&&!valid_source_ref(*s.expense_policy_ref))fail("GovernanceExpensePolicyRequired",s.spec_ref.id);if(kind){const bool patrol=*kind==WorkKind::patrol;if(patrol!=std::holds_alternative<GovernancePatrolWorkTerms>(*s.approved_work_terms))fail("GovernanceWorkTermsKindMismatch",s.spec_ref.id);if(patrol){const auto& p=std::get<GovernancePatrolWorkTerms>(*s.approved_work_terms);unique_ids(p.permitted_leader_actor_ids,s.spec_ref.id);if(p.completion_deadline.tick<0)fail("InvalidGovernanceDeadline",s.spec_ref.id);if(p.permitted_leader_actor_ids.empty()||p.minimum_available_people<=0||p.rule_id.empty()||p.rule_version.empty()||!valid_source_ref(p.supply_order_ref))fail("InvalidGovernancePatrolTerms",s.spec_ref.id);}else{const auto& p=std::get<GovernanceSupervisionWorkTerms>(*s.approved_work_terms);unique_ids(p.permitted_supervisor_actor_ids,s.spec_ref.id);if(p.completion_deadline.tick<0||p.economic_work_ref.owner_domain.empty()||p.economic_work_ref.work_id.empty())fail("InvalidGovernanceSupervisionSource",s.spec_ref.id);if(p.kind!=*kind||p.permitted_supervisor_actor_ids.empty()||p.rule_id.empty()||p.rule_version.empty()||!valid_source_ref(p.target_site_or_anchor_ref))fail("InvalidGovernanceSupervisionTerms",s.spec_ref.id);if(catalog.schema_version==3||catalog.schema_version==4){if(!p.initial_execution_time_use||p.initial_execution_time_use->half_days<=0)fail("GovernanceInitialSupervisionDurationRequired",s.spec_ref.id);}else if(p.initial_execution_time_use)fail("GovernanceSupervisionDurationRequiresCatalog3",s.spec_ref.id);}}}
SourceRef append(CommonStore& c,const char* kind){auto id=take(c.next_id("governance_program"));take(c.append_fact({id,kind,"politics"}));return {"politics",kind,std::move(id)};}

void validate_binding(const GovernanceProgramReadContext& x,const GovernanceExecutionBinding& b){const auto* a=x.state.approvals.find(b.program_id);if(!a||b.id.empty()||b.task_acceptance_ref.has_value()!=b.execution_ref.has_value())fail("InvalidGovernanceExecutionBinding",b.id);const SourceRef self{"politics","governance_execution_bound",b.id};header(x.common,self);header(x.common,b.offered_task_ref);if(position(x.common,governance_program_source(*a))>=position(x.common,b.offered_task_ref)||position(x.common,b.offered_task_ref)>=position(x.common,self)||compare(b.occurred_at,a->issued_at)<0)fail("GovernanceExecutionSourceOrder",b.id);if(b.task_acceptance_ref){header(x.common,*b.task_acceptance_ref);header(x.common,*b.execution_ref);if(position(x.common,*b.task_acceptance_ref)>=position(x.common,self)||position(x.common,*b.execution_ref)>=position(x.common,self))fail("GovernanceAcceptanceSourceOrder",b.id);}take(x.originals.validate_governance_execution_binding(*a,b));}
GovernanceWorkTermsRead work_terms(const GovernanceProgramReadContext& x,const WorkProgramAuthorizationScope& scope,const SourceRef& offer,const WorldMoment& now,bool accepting,const StableId* accepted_task){auto verified=take(read_verified_governance_program_approval(x,scope.program_ref));const auto& a=*verified.approval;const auto& s=*verified.specification;if(scope.target_line_id!=s.spec_ref.id||!same_terms_revision(scope.terms_revision,TermsRevisionRef{SourceFactTerms{scope.program_ref}})||!s.approved_work_terms||!s.expense_policy_ref)fail("GovernanceWorkAuthorizationMismatch",a.id);if(compare(now,take(x.clock.read_organization_authority_now()))||compare(now,a.issued_at)<0)fail("GovernanceWorkClockMismatch",a.id);if(accepting&&compare(now,a.valid_until)>=0)fail("GovernanceOfferExpired",a.id);GovernanceWorkTermsRead out{&a,&s,nullptr,nullptr};for(const auto& row:x.state.execution_bindings.rows()){const auto& b=row.second;if(b.program_id!=a.id||!same_source_ref(b.offered_task_ref,offer))continue;validate_binding(x,b);if(b.task_acceptance_ref){if(out.acceptance_binding)fail("DuplicateGovernanceAcceptance",a.id);out.acceptance_binding=&b;}else{if(out.offer_binding)fail("DuplicateGovernanceOffer",a.id);out.offer_binding=&b;}}if(!out.offer_binding)fail("GovernanceTaskOfferRequired",a.id);if(accepted_task){if(accepted_task->empty()||(!accepting&&!out.acceptance_binding))fail("GovernanceAcceptedTaskRequired",a.id);take(x.originals.validate_governance_work_task_identity(a,offer,*accepted_task,accepting));}if(accepting)take(x.originals.validate_governance_continuity(a,s.continuity_policy_ref,now));return out;}
}
// 由原 spec_ref 的版本和目录摘要锁定首工期；不从 required/deadline 或余额计算。
Outcome<const GovernanceSupervisionActionDuration*> read_governance_initial_supervision_duration(
 const GovernanceProgramCatalog& catalog,const GovernanceProgramSpec& spec){return guarded<const GovernanceSupervisionActionDuration*>([&]{
 take(validate_governance_program_catalog(catalog));
 if((catalog.schema_version!=3&&catalog.schema_version!=4)||catalog.specs.find(spec.spec_ref.id)!=&spec)
  fail("GovernanceInitialSupervisionDurationCatalog3Required",spec.spec_ref.id);
 const auto* terms=spec.approved_work_terms?std::get_if<GovernanceSupervisionWorkTerms>(&*spec.approved_work_terms):nullptr;
 if(!terms||!terms->initial_execution_time_use||terms->initial_execution_time_use->half_days<=0)
  fail("GovernanceInitialSupervisionDurationRequired",spec.spec_ref.id);
 return &*terms->initial_execution_time_use;
});}
SourceRef governance_program_source(const GovernanceProgramApproval& a){return {"politics","governance_program_approved",a.id};}
Outcome<WorldMoment> NativeGovernanceProgramAuthorityRead::read_organization_authority_now()const{return context_.clock.read_organization_authority_now();}
// 历史Basis只投影原签发源，不递归校验自身Basis或下游Work。
Outcome<AuthorityIssueSourceView> NativeGovernanceProgramAuthorityRead::read_authority_issue_source(const SourceRef& ref)const{return guarded<AuthorityIssueSourceView>([&]{
 ActorFoundationAt at;at.kind=ActorFoundationAt::Kind::exact_moment;StableId actor;
 if(ref.domain=="politics"&&ref.kind=="governance_supervision_execution_approved"){
  const auto* a=context_.state.supervision_execution_approvals.find(ref.id);
  if((context_.state.schema_version!=2&&context_.state.schema_version!=3)||context_.catalog.schema_version!=4||!a||a->id!=ref.id)
   fail("GovernanceSupervisionApprovalSourceMissing",ref.id);
  header(context_.common,ref);const auto& original=approval(context_,a->authorization.program_ref);
  if(!rule_equal(a->spec_ref,original.spec_ref)||a->work_id.empty()||a->execution_ordinal<=0)
   fail("GovernanceSupervisionApprovalSourceMismatch",ref.id);
  at.exact_moment=a->issued_at;actor=a->issuer.actor_id;
 }else if(ref.domain=="politics"&&ref.kind=="governance_work_reassignment_approved"){
  const auto* a=context_.state.work_reassignment_approvals.find(ref.id);
  if(context_.state.schema_version!=3||context_.catalog.schema_version!=4||!a||a->id!=ref.id)fail("GovernanceWorkReassignmentSourceMissing",ref.id);
  header(context_.common,ref);const auto& original=approval(context_,a->authorization.program_ref);
  if(!rule_equal(a->spec_ref,original.spec_ref)||a->work_id.empty()||a->new_actor_id.empty())fail("GovernanceWorkReassignmentSourceMismatch",ref.id);
  at.exact_moment=a->issued_at;actor=a->issuer.actor_id;
 }else{const auto& a=approval(context_,ref);at.exact_moment=a.issued_at;actor=a.issuer.actor_id;}
 return AuthorityIssueSourceView{ref,actor,"approve_governance_program",at,{}};
});}
// 复用原 append、ID 与 Common 原子候选路径，不另写一个事实 writer。
Outcome<SourceRef> append_governance_work_lifecycle_fact_in_candidate(CommonStore& common,GovernanceWorkLifecycleFactKind kind){return guarded<SourceRef>([&]{switch(kind){case GovernanceWorkLifecycleFactKind::reassignment:return append(common,"governance_work_reassignment_approved");case GovernanceWorkLifecycleFactKind::deadline:return append(common,"governance_work_deadline_terminated");}fail("GovernanceWorkLifecycleKindRequired");});}
Status validate_governance_program_catalog(const GovernanceProgramCatalog& c){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 98, "GovernanceProgramCatalog", "validate_governance_program_catalog", &c);
return guarded<Done>([&]{
 take(validate_governance_executor_qualifications(c));
 if(c.schema_version!=4&&!c.supervision_execution_quotes.empty())fail("GovernanceExecutionQuotesRequireCatalog4");
 for(const auto& row:c.patrol_gain_rules.rows()){const auto& rule=row.second;if(row.first!=rule.rule_ref.id||rule.rule_ref.version<1||rule.rule_ref.catalog_digest!=c.identity.content_digest||rule.base_gain<0||rule.base_gain>100||rule.skill_id.empty()||rule.cap<0||rule.cap>100)fail("InvalidPatrolGainRule",row.first);}
 for(const auto& row:c.specs.rows()){if(row.first!=row.second.spec_ref.id)fail("GovernanceProgramSpecKeyMismatch",row.first);spec_shape(c,row.second);}
 for(const auto& row:c.supervision_execution_quotes.rows()){
  const auto& q=row.second;const auto* spec=c.specs.find(q.program_spec_ref.id);
  if(row.first!=q.quote_ref.id||q.quote_ref.id.empty()||q.quote_ref.version<1||q.quote_ref.catalog_digest!=c.identity.content_digest||
     q.time_use.half_days<=0||!spec||!rule_equal(spec->spec_ref,q.program_spec_ref)||!spec->approved_work_terms||
     !std::holds_alternative<GovernanceSupervisionWorkTerms>(*spec->approved_work_terms))
   fail("InvalidGovernanceSupervisionExecutionQuote",row.first);
 }
 return Done{};
});}
Outcome<SourceRef> approve_governance_program_in_candidate(CommonStore& common,OrganizationRuntime& runtime,GovernanceProgramState& state,const GovernanceProgramReadContext& x,const ApproveGovernanceProgram& request,const ReadStamp& stamp){return guarded<SourceRef>([&]{
 if(&common!=&x.common||&runtime!=&x.organization_runtime||&state!=&x.state)fail("GovernanceProgramCandidateMismatch");
 if((state.schema_version!=1&&state.schema_version!=2&&state.schema_version!=3)||((state.schema_version==2||state.schema_version==3)!=(x.catalog.schema_version==4)))fail("GovernanceProgramProfileMismatch");
 const auto* s=x.catalog.specs.find(request.spec_id);if(!s||s->spec_ref.version!=request.spec_version)fail("GovernanceProgramSpecChanged",request.spec_id);
 auto captured=take(prepare_governance_approval_authority(x,*s,request.executor,request.authority,request.budget_execution_ref,request.expected_target_revision,stamp));
 auto now=take(x.clock.read_organization_authority_now());
 auto due=take(add_world_duration(now,HalfDayDuration{take(normalize_fraction(std::to_string(s->offer_valid_half_days),"1"))}));
 GovernanceProgramApproval a;a.spec_ref=s->spec_ref;a.target=s->target;a.executor=request.executor;a.budget_execution=request.budget_execution_ref;
 a.budget_period=captured.target.budget_period;a.payer_wallet_id=captured.target.payer_wallet_id;a.contract_debtor=captured.target.contract_debtor;
 a.issuer=request.authority;a.expected_target_revision=request.expected_target_revision;a.issued_at=now;a.valid_until=due;
 auto source=append(common,"governance_program_approved");a.id=source.id;a.authority_basis_ref={source,1};
 if(!state.approvals.insert(a.id,std::move(a)))fail("DuplicateGovernanceProgramApproval",source.id);
 take(bind_governance_approval_authority_in_candidate(common,runtime,x,captured,source));return source;
});}
Outcome<SourceRef> bind_governance_execution_in_candidate(CommonStore& common,GovernanceProgramState& state,const GovernanceProgramReadContext& x,const StableId& program,const SourceRef& offered,const std::optional<SourceRef>& accepted,const std::optional<SourceRef>& execution){return guarded<SourceRef>([&]{if(&common!=&x.common||&state!=&x.state)fail("GovernanceProgramCandidateMismatch");const auto* a=state.approvals.find(program);if(!a||accepted.has_value()!=execution.has_value())fail("InvalidGovernanceExecutionBinding",program);for(const auto& row:state.execution_bindings.rows())if(row.second.program_id==program&&same_source_ref(row.second.offered_task_ref,offered)&&row.second.task_acceptance_ref.has_value()==accepted.has_value()){if(accepted&&(!same_source_ref(*row.second.task_acceptance_ref,*accepted)||!same_source_ref(*row.second.execution_ref,*execution)))fail("GovernanceExecutionAlreadyBound",program);validate_binding(x,row.second);return SourceRef{"politics","governance_execution_bound",row.first};}GovernanceExecutionBinding b;b.program_id=program;b.offered_task_ref=offered;b.task_acceptance_ref=accepted;b.execution_ref=execution;b.occurred_at=take(x.clock.read_organization_authority_now());take(x.originals.validate_governance_execution_binding(*a,b));auto source=append(common,"governance_execution_bound");b.id=source.id;if(!state.execution_bindings.insert(b.id,std::move(b)))fail("DuplicateGovernanceExecutionBinding",source.id);validate_binding(x,*state.execution_bindings.find(source.id));return source;});}
Outcome<GovernanceWorkExpenseApproval> read_governance_work_authorization(const GovernanceProgramReadContext& x,const WorkProgramAuthorizationScope& scope,const SourceRef& offer,const WorldMoment& now,bool accepting){return guarded<GovernanceWorkExpenseApproval>([&]{auto actual=work_terms(x,scope,offer,now,accepting,nullptr);const auto& a=*actual.approval;const auto& s=*actual.specification;GovernanceWorkExpenseApproval out;out.approval_ref=governance_program_source(a);out.source_schema=1;out.authorization=scope;out.offered_task_ref=offer;out.expense_policy_ref=*s.expense_policy_ref;out.budget_execution=a.budget_execution;out.budget_period=a.budget_period;out.payer_wallet_id=a.payer_wallet_id;out.contract_debtor=a.contract_debtor;out.permitted_work_kinds={*work_kind(s)};out.valid_from=a.issued_at;out.valid_until=a.valid_until;return out;});}
Outcome<GovernanceWorkTermsRead> read_governance_work_terms(const GovernanceProgramReadContext& x,const WorkAuthorizationRef& scope,const SourceRef& offer,const WorldMoment& now,bool accepting){return guarded<GovernanceWorkTermsRead>([&]{return work_terms(x,{scope.program_ref,scope.target_line_id,scope.terms_revision},offer,now,accepting,&scope.accepted_task_id);});}
Outcome<SourceRef> adopt_governance_outcome_in_candidate(CommonStore& common,GovernanceProgramState& state,const GovernanceProgramReadContext& x,const StableId& program,const SourceRef& actual){return guarded<SourceRef>([&]{if(&common!=&x.common||&state!=&x.state)fail("GovernanceProgramCandidateMismatch");const auto* a=state.approvals.find(program);if(!a)fail("GovernanceProgramApprovalMissing",program);header(common,actual);auto outcome=take(x.originals.read_governance_execution_outcome(*a,actual));if(outcome.program_id!=program||static_cast<std::size_t>(outcome.target_kind)!=a->target.index()||!outcome.id.empty()||!same_source_ref(outcome.actual_result_ref,actual)||compare(outcome.occurred_at,a->issued_at)<0||compare(outcome.occurred_at,take(x.clock.read_organization_authority_now()))>0||static_cast<unsigned>(outcome.status)>static_cast<unsigned>(GovernanceProgramStatus::failed))fail("GovernanceOutcomeSourceMismatch",program);header(common,outcome.execution_ref);take(x.originals.validate_governance_continuity(*a,outcome.continuity,outcome.occurred_at,&actual));for(const auto& app:outcome.application_refs){if(app.index()!=a->target.index())fail("GovernanceApplicationTargetMismatch",program);take(x.originals.validate_governance_application(*a,app,outcome.occurred_at));}
 for(const auto& row:state.outcome_sources.rows()){const auto& prior=row.second;if(prior.program_id==program&&same_source_ref(prior.actual_result_ref,actual)){if(!outcome_equal(prior,outcome))fail("GovernanceOutcomeReplayChanged",prior.id);const SourceRef replay{"politics","governance_program_outcome",prior.id};take(read_verified_governance_program_outcome(x,replay));return replay;}}
 for(const auto& app:outcome.application_refs)for(const auto& row:state.applied_effect_sources.rows())if(source_overlap(app,row.second.application)&&(row.second.program_id!=program||!application_equal(app,row.second.application)))fail("GovernanceSourceAlreadyConsumed",row.first);
 if(outcome.patrol_unavailable){auto& terminal=*outcome.patrol_unavailable;if(outcome.target_kind!=GovernanceTargetKind::patrol||(outcome.status!=GovernanceProgramStatus::failed&&outcome.status!=GovernanceProgramStatus::cancelled)||!terminal.source_terminal_id.empty()||terminal.source_schema!=1||!same_source_ref(terminal.actual_cause_ref,actual)||!same_source_ref(terminal.continuity_source_ref,outcome.continuity)||compare(terminal.effective_game_time,outcome.occurred_at))fail("InvalidPatrolApplicationTerminal",program);header(common,terminal.original_obligation_ref);header(common,terminal.accepted_goal_ref);for(const auto& ref:terminal.preserved_actual_result_refs)header(common,ref);}
 auto source=append(common,"governance_program_outcome");outcome.id=source.id;if(outcome.patrol_unavailable)outcome.patrol_unavailable->source_terminal_id=source.id;for(const auto& app:outcome.application_refs){bool found=false;for(const auto& row:state.applied_effect_sources.rows())if(application_equal(app,row.second.application)){found=true;break;}if(!found){auto adopted=append(common,"governance_effect_adopted");GovernanceAppliedEffectSource effect{adopted.id,program,outcome.id,app,outcome.occurred_at};if(!state.applied_effect_sources.insert(effect.id,std::move(effect)))fail("DuplicateGovernanceEffectSource",adopted.id);}}if(!state.outcome_sources.insert(outcome.id,std::move(outcome)))fail("DuplicateGovernanceOutcome",source.id);return source;});}
Outcome<VerifiedGovernanceProgramRead> read_verified_governance_program_approval(const GovernanceProgramReadContext& x,const SourceRef& ref){return guarded<VerifiedGovernanceProgramRead>([&]{
 take(validate_governance_program_catalog(x.catalog));
 if((x.state.schema_version!=1&&x.state.schema_version!=2&&x.state.schema_version!=3)||((x.state.schema_version==2||x.state.schema_version==3)!=(x.catalog.schema_version==4)))fail("GovernanceProgramProfileMismatch");
 auto now=take(x.clock.read_organization_authority_now());
 const auto& a=approval(x,ref);const auto& s=specification(x,a);
 if(a.expected_target_revision<0||compare(a.valid_until,take(add_world_duration(a.issued_at,HalfDayDuration{take(normalize_fraction(std::to_string(s.offer_valid_half_days),"1"))})))||compare(a.issued_at,now)>0||compare(a.valid_until,a.issued_at)<=0)fail("InvalidGovernanceProgramApproval",a.id);
 take(x.originals.validate_historical_governance_approval(a,s));
 take(validate_governance_approval_authority(x,s,a.issuer,a.authority_basis_ref,a.budget_execution,governance_program_source(a)));
 return VerifiedGovernanceProgramRead{&a,&s};
});}
Outcome<const GovernanceProgramOutcome*> read_verified_governance_program_outcome(const GovernanceProgramReadContext& x,const SourceRef& ref){return guarded<const GovernanceProgramOutcome*>([&]{
 if(ref.domain!="politics"||ref.kind!="governance_program_outcome")fail("GovernanceOutcomeSourceKind",ref.id);header(x.common,ref);const auto* o=x.state.outcome_sources.find(ref.id);if(!o||o->id!=ref.id)fail("GovernanceOutcomeSourceMissing",ref.id);auto verified=take(read_verified_governance_program_approval(x,{"politics","governance_program_approved",o->program_id}));const auto& a=*verified.approval;
 if(static_cast<std::size_t>(o->target_kind)!=a.target.index()||compare(o->occurred_at,take(x.clock.read_organization_authority_now()))>0||compare(o->occurred_at,a.issued_at)<0||static_cast<unsigned>(o->status)>static_cast<unsigned>(GovernanceProgramStatus::failed))fail("InvalidGovernanceOutcome",ref.id);
 header(x.common,o->execution_ref);header(x.common,o->actual_result_ref);if(position(x.common,o->actual_result_ref)>=position(x.common,ref)||position(x.common,o->execution_ref)>=position(x.common,ref))fail("GovernanceOutcomeSourceOrder",ref.id);auto actual=take(x.originals.read_governance_execution_outcome(a,o->actual_result_ref));if(!actual.id.empty()||(actual.patrol_unavailable&&!actual.patrol_unavailable->source_terminal_id.empty())||!outcome_equal(*o,actual))fail("GovernanceOutcomeOriginalMismatch",ref.id);
 if(o->patrol_unavailable){const auto& t=*o->patrol_unavailable;if(o->target_kind!=GovernanceTargetKind::patrol||t.source_terminal_id!=o->id||t.source_schema!=1||(o->status!=GovernanceProgramStatus::failed&&o->status!=GovernanceProgramStatus::cancelled)||!same_source_ref(t.actual_cause_ref,o->actual_result_ref)||!same_source_ref(t.continuity_source_ref,o->continuity)||compare(t.effective_game_time,o->occurred_at))fail("InvalidPatrolApplicationTerminal",ref.id);header(x.common,t.original_obligation_ref);header(x.common,t.accepted_goal_ref);for(const auto& kept:t.preserved_actual_result_refs){header(x.common,kept);if(position(x.common,kept)>=position(x.common,ref))fail("GovernanceTerminalPreservedSourceOrder",kept.id);}}
 take(x.originals.validate_governance_continuity(a,o->continuity,o->occurred_at,&o->actual_result_ref));for(const auto& app:o->application_refs){if(app.index()!=a.target.index())fail("GovernanceApplicationTargetMismatch",ref.id);take(x.originals.validate_governance_application(a,app,o->occurred_at));}return o;
});}
// 整域冷验复用逐条真实outcome门，不能绕过它的开工/成果先序及Patrol保留历史。
Status validate_governance_programs(const GovernanceProgramReadContext& x,const GovernanceSupervisionApprovalValidationContext* supervision){return guarded<Done>([&]{
 take(validate_governance_program_catalog(x.catalog));
 if((x.state.schema_version!=1&&x.state.schema_version!=2&&x.state.schema_version!=3)||((x.state.schema_version==2||x.state.schema_version==3)!=(x.catalog.schema_version==4)))fail("GovernanceProgramProfileMismatch");
 if(x.state.schema_version==1){
  if(!x.state.supervision_execution_approvals.empty())fail("GovernanceExecutionApprovalsRequireState2");
  for(const auto& row:x.common.state().facts.rows())if(row.second.domain=="politics"&&row.second.kind=="governance_supervision_execution_approved")fail("GovernanceExecutionApprovalsRequireState2",row.first);
 }
 if(x.state.schema_version==2||x.state.schema_version==3){if(!supervision||&supervision->history.programs!=&x)fail("GovernanceSupervisionValidationContextRequired");take(validate_governance_supervision_approvals(*supervision));}
 if(x.state.schema_version!=3){
  if(!x.state.work_reassignment_approvals.empty()||!x.state.work_deadline_terminations.empty())fail("GovernanceWorkLifecycleRequiresState3");
  for(const auto& row:x.common.state().facts.rows())if(row.second.domain=="politics"&&(row.second.kind=="governance_work_reassignment_approved"||row.second.kind=="governance_work_deadline_terminated"))fail("GovernanceWorkLifecycleRequiresState3",row.first);
 }else{
  const auto* sources=supervision->executions.reassignments;
  if(!sources||&sources->governance.history!=&supervision->history)fail("GovernanceWorkLifecycleValidationContextRequired");
  take(validate_governance_work_lifecycle_sources(sources->governance,sources->original_sources_stamp));
  take(validate_work_reassignment_acceptances(*sources));
 }
 std::set<StableId> covered;
 const auto now=take(x.clock.read_organization_authority_now());
 for(const auto& row:x.state.approvals.rows()){
  if(row.first!=row.second.id)fail("InvalidGovernanceProgramApproval",row.first);
  take(read_verified_governance_program_approval(x,governance_program_source(row.second)));covered.insert(row.first);
 }
 for(const auto& row:x.state.execution_bindings.rows()){
  if(row.first!=row.second.id||compare(row.second.occurred_at,now)>0)fail("InvalidGovernanceExecutionBinding",row.first);
  validate_binding(x,row.second);covered.insert(row.first);
 }
 for(const auto& row:x.state.outcome_sources.rows()){
  const SourceRef source{"politics","governance_program_outcome",row.first};
  const auto* o=take(read_verified_governance_program_outcome(x,source));
  // 原已采用的同一应用可被后续项目视图引用，但其真实effect绑定恰一条。
  for(const auto& app:o->application_refs){
   std::size_t count=0;for(const auto& effect:x.state.applied_effect_sources.rows())
    if(effect.second.program_id==o->program_id&&application_equal(effect.second.application,app))++count;
   if(count!=1)fail("GovernanceEffectCoverageMismatch",o->id);
  }
  covered.insert(row.first);
 }
 std::vector<const GovernanceAppliedEffectSource*> effects;
 for(const auto& row:x.state.applied_effect_sources.rows()){
  const auto& e=row.second;const auto* outcome=x.state.outcome_sources.find(e.outcome_source_id);
  if(!outcome||row.first!=e.id||outcome->program_id!=e.program_id||compare(outcome->occurred_at,e.applied_at))
   fail("InvalidGovernanceEffectBinding",row.first);
  const SourceRef effect_source{"politics","governance_effect_adopted",e.id};
  const SourceRef outcome_source{"politics","governance_program_outcome",outcome->id};
  header(x.common,effect_source);
  // 原writer先追加outcome头，再追加其实际effect绑定；同M不能倒序自证。
  if(position(x.common,outcome_source)>=position(x.common,effect_source))fail("GovernanceEffectSourceOrder",e.id);
  bool present=false;for(const auto& app:outcome->application_refs)present=present||application_equal(app,e.application);
  if(!present)fail("OrphanGovernanceEffectBinding",e.id);
  for(const auto* previous:effects)if(source_overlap(previous->application,e.application))fail("GovernanceSourceAlreadyConsumed",e.id);
  effects.push_back(&e);covered.insert(row.first);
 }
 for(const auto& row:x.common.state().facts.rows())
  if(row.second.domain=="politics"&&(row.second.kind=="governance_program_approved"||row.second.kind=="governance_execution_bound"||
      row.second.kind=="governance_program_outcome"||row.second.kind=="governance_effect_adopted")&&!covered.count(row.first))
   fail("GovernanceProgramPayloadMissing",row.first);
 return Done{};
});}
}
