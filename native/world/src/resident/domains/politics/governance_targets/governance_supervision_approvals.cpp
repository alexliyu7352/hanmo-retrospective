// 后继F批准沿原治理权源签发，原S3终局必须先于本次签发；重放不再批、收款或开工。
#include "governance_supervision_approvals.hpp"
#include "governance_approval_authority.hpp"
#include "resident/domains/tasks/work_execution/work_reassignment_sources.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]] void fail(const StableId& code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{value.error};return std::move(value.value);}
template<class T,class F>Outcome<T> guard(F&& fn){try{return {true,fn(),{}};}catch(const Failure& e){return {false,{},e.error};}}
int cmp(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b));}
bool same(const ActorFoundationRuleRef& a,const ActorFoundationRuleRef& b){return a.id==b.id&&a.version==b.version&&a.catalog_digest==b.catalog_digest;}
bool same(const MomentOwnerSource& a,const MomentOwnerSource& b){return a.source_schema==b.source_schema&&same_source_ref(a.source_ref,b.source_ref);}
bool same(const WorkAuthorizationRef& a,const WorkAuthorizationRef& b){return same_source_ref(a.program_ref,b.program_ref)&&a.target_line_id==b.target_line_id&&a.accepted_task_id==b.accepted_task_id&&same_terms_revision(a.terms_revision,b.terms_revision);}
bool same(const ActingAuthority& a,const ActingAuthority& b){return a.actor_id==b.actor_id&&a.organization_id==b.organization_id&&a.office_id==b.office_id&&a.authority_epoch==b.authority_epoch;}
bool same(const BudgetExecutionRef& a,const BudgetExecutionRef& b){return a.budget_id==b.budget_id&&a.budget_revision==b.budget_revision&&a.policy_revision==b.policy_revision&&a.approval_fact_id==b.approval_fact_id;}
bool same(const BudgetPeriod& a,const BudgetPeriod& b){return a.period_from_tick==b.period_from_tick&&a.period_until_tick==b.period_until_tick;}
std::size_t pos(const CommonStore& c,const SourceRef& ref){
 const auto* fact=c.state().facts.find(ref.id);const auto& ids=c.state().facts.order();auto it=std::find(ids.begin(),ids.end(),ref.id);
 if(!valid_source_ref(ref)||!fact||fact->domain!=ref.domain||fact->kind!=ref.kind||it==ids.end())fail("GovernanceSupervisionOriginalSourceMissing",ref.id);
 return static_cast<std::size_t>(it-ids.begin());
}
// 验明同一候选全部借用，禁止跨候选真实源或与另一个S3定义图拼接。
const TaskWorkExecutionState& selected(const GovernanceSupervisionApprovalValidationContext& x){
 const auto& h=x.history;const auto& p=h.programs;const auto& e=x.executions;
 if(p.catalog.schema_version!=4||(p.state.schema_version!=2&&p.state.schema_version!=3)||!h.tasks.work_execution||!task_work_fixed_execution_terms_version(h.tasks.work_execution->version)||(task_work_lifecycle_sources_version(h.tasks.work_execution->version)!=(p.state.schema_version==3)))
  fail("GovernanceSupervisionApprovalProfileRequired");
 if(&p.common!=&e.common||&h.tasks!=&e.tasks||&h.task_definitions!=&e.definitions||
    !h.task_definitions.bound_to(p.common,h.tasks)||!e.approvals.bound_to(p.common,h.tasks)||
    cmp(take(p.clock.read_organization_authority_now()),take(e.clock.read_current_moment())))
  fail("GovernanceSupervisionApprovalCandidateMismatch");
 if(task_work_lifecycle_sources_version(h.tasks.work_execution->version)&&(!e.reassignments||&e.reassignments->governance.history!=&h))fail("GovernanceSupervisionReassignmentSourcesRequired");
 return *h.tasks.work_execution;
}
const WorkEpisode& episode(const GovernanceSupervisionApprovalValidationContext& x,const StableId& id){
 const auto* row=selected(x).episodes.find(id);if(!row||row->work_id!=id||!std::holds_alternative<SupervisedWorkPlan>(row->frozen_plan))fail("GovernanceSupervisionWorkRequired",id);return *row;
}
const GovernanceSupervisionExecutionQuote& quote(const GovernanceProgramCatalog& catalog,const ActorFoundationRuleRef& ref,const GovernanceProgramSpec& spec){
 take(validate_governance_program_catalog(catalog));const auto* row=catalog.supervision_execution_quotes.find(ref.id);
 if(!row||!same(row->quote_ref,ref)||!same(row->program_spec_ref,spec.spec_ref))fail("GovernanceSupervisionQuoteChanged",ref.id);return *row;
}
void permit(const GovernanceSupervisionApprovalValidationContext& x,const GovernanceAcceptedWorkHistory& h,const WorkEpisode& e,const WorldMoment& at,const SourceRef* cut){
 auto value=take(x.continuity.read_accepted_supervision_continuity_at(h,e,at,cut));
 if(value.actual_sources.empty())fail("GovernanceSupervisionContinuitySourceRequired",e.work_id);
 if(value.actual_blocking_source)fail("GovernanceSupervisionExecutionBlocked",value.actual_blocking_source->source_ref.id);
}
struct Predecessor {const SupervisionExecutionTerms* terms=nullptr;WorldMoment ended_at;};
// 先核真实row/start/terminal同一身份和严格先序，再向S3原读口下降一ordinal。
Predecessor preceding(const GovernanceSupervisionApprovalValidationContext& x,const WorkEpisode& e,Integer ordinal,const WorldMoment& at,const SourceRef* cut){
 if(ordinal<=0)fail("GovernanceSupervisionLaterOrdinalRequired",e.work_id);
 const auto& s=selected(x);const SupervisionExecutionTerms* prior=nullptr;
 for(const auto& row:s.supervision_execution_terms.rows())if(row.second.work_id==e.work_id&&row.second.execution_ordinal==ordinal-1){
  if(prior||row.first!=row.second.action_id)fail("GovernanceSupervisionPredecessorAmbiguous",e.work_id);prior=&row.second;
 }
 if(!prior||!prior->actual_start_source_ref||!prior->actual_terminal_result_ref||!same(prior->original_authorization_ref,e.authorization))fail("GovernanceSupervisionPriorTerminalRequired",e.work_id);
 take(validate_supervision_execution_terms_shape(*prior));
 const auto* start=s.supervision_starts.find(prior->actual_start_source_ref->source_ref.id);
 if(!start||start->fact_id!=prior->actual_start_source_ref->source_ref.id||start->work_id!=e.work_id||start->action_id!=prior->action_id)
  fail("GovernanceSupervisionPredecessorStartMismatch",e.work_id);
 const auto& ref=*prior->actual_terminal_result_ref;WorldMoment ended;
 if(ref.source_ref.kind=="supervision_window_closed"){
  const auto* row=s.supervision_window_closures.find(ref.source_ref.id);
  if(!row||row->fact_id!=ref.source_ref.id||row->start_source_ref!=start->fact_id)fail("GovernanceSupervisionPredecessorTerminalMismatch",ref.source_ref.id);ended=row->closed_at;
 }else{
  const auto* row=s.supervision_terminals.find(ref.source_ref.id);
  if(!row||row->fact_id!=ref.source_ref.id||row->start_source_ref!=start->fact_id||row->disposition!=SupervisionReleaseKind::actual_interruption)
   fail("GovernanceSupervisionPermanentWorkTerminal",ref.source_ref.id);ended=row->ended_at;
 }
 const auto& c=x.history.programs.common;
 if(pos(c,prior->actual_start_source_ref->source_ref)>=pos(c,ref.source_ref)||cmp(ended,at)>0||
    (cut&&pos(c,ref.source_ref)>=pos(c,*cut)))fail("GovernanceSupervisionApprovalBeforeTerminal",e.work_id);
 const auto actual=take(read_supervision_source_moment(x.executions,ref.source_ref,ref.source_schema));
 if(cmp(actual,ended))fail("GovernanceSupervisionTerminalMomentMismatch",ref.source_ref.id);
 return {prior,ended};
}
// 新 H 的 executor 先由真实接受和共同身份核派生，不把 caller actor 或旧 executor 当替任。
const StableId& executor(const GovernanceSupervisionApprovalValidationContext& x,const WorkEpisode& e,const std::optional<SourceRef>& accepted,const WorldMoment& at,const SourceRef* cut){
 if(!task_work_lifecycle_sources_version(selected(x).version)){if(accepted)fail("GovernanceSuccessorRequiresWork6",e.work_id);return std::get<SupervisedWorkPlan>(e.frozen_plan).supervisor_actor_id;}
 if(accepted){const auto* row=take(read_work_reassignment_acceptance(*x.executions.reassignments,*accepted));const auto identity=take(read_governance_accepted_work_executor_at(x.history,e,*accepted,at,cut));if(identity.acceptance!=row||!identity.actor_id)fail("GovernanceSupervisionReassignmentOwnerMismatch",e.work_id);return *identity.actor_id;}
 const auto identity=take(read_governance_work_executor_before_cut(x.history,e,cut));if(!identity.actor_id||identity.acceptance)fail("GovernanceSupervisionAcceptanceRequired",e.work_id);return *identity.actor_id;
}
const GovernanceSupervisionExecutionApproval& verified(const GovernanceSupervisionApprovalValidationContext& x,const SourceRef& ref){
 const auto& p=x.history.programs;selected(x);
 const auto* a=p.state.supervision_execution_approvals.find(ref.id);
 if(ref.domain!="politics"||ref.kind!="governance_supervision_execution_approved"||!a||a->id!=ref.id)fail("GovernanceSupervisionApprovalSourceMissing",ref.id);
 pos(p.common,ref);const auto& e=episode(x,a->work_id);auto h=take(read_governance_accepted_work_history(x.history,e));
 const auto& q=quote(p.catalog,a->quote_ref,*h.specification);
 if((p.state.schema_version==2&&(a->source_schema!=1||a->successor_acceptance_ref))||(p.state.schema_version==3&&(a->source_schema<1||a->source_schema>2||(a->source_schema==1&&a->successor_acceptance_ref))))fail("GovernanceSupervisionApprovalSchemaMismatch",a->id);
 const auto& actual_actor=executor(x,e,a->successor_acceptance_ref,a->issued_at,&ref);(void)actual_actor;
 if(a->successor_acceptance_ref&&pos(p.common,*a->successor_acceptance_ref)>=pos(p.common,ref))fail("GovernanceSupervisionAcceptanceAfterApproval",a->id);
 if(a->execution_ordinal<=0||!same(a->authorization,e.authorization)||!same(a->spec_ref,h.specification->spec_ref)||
    a->service_limit_half_days!=q.time_use.half_days||a->expected_target_revision<0||
    !same(a->budget_execution,h.approval->budget_execution)||!same(a->budget_period,h.approval->budget_period)||
    a->issuer.organization_id!=h.approval->issuer.organization_id||cmp(a->issued_at,h.work_acceptance->earliest_service_at)<0||
    cmp(a->issued_at,take(p.clock.read_organization_authority_now()))>0||
    cmp(a->issued_at,WorldMoment{h.supervision->completion_deadline.tick,{}})>=0||
    pos(p.common,{"politics","governance_execution_bound",h.acceptance_binding->id})>=pos(p.common,ref))
  fail("GovernanceSupervisionApprovalMismatch",a->id);
 // 后继批准同Work/ordinal恰一条；不能换quote重批来替换已签发来源。
 for(const auto& row:p.state.supervision_execution_approvals.rows())if(&row.second!=a&&row.second.work_id==a->work_id&&row.second.execution_ordinal==a->execution_ordinal)
  fail("DuplicateGovernanceSupervisionApproval",a->work_id);
 auto prior=preceding(x,e,a->execution_ordinal,a->issued_at,&ref);
 if(!same(a->preceding_terminal_ref,*prior.terms->actual_terminal_result_ref))fail("GovernanceSupervisionApprovalPredecessorChanged",a->id);
 // 同一ordinal只能在批准后开工；不能在未来补写批准给过去Action背书。
 for(const auto& row:selected(x).supervision_execution_terms.rows())if(row.second.work_id==e.work_id&&row.second.execution_ordinal>=a->execution_ordinal&&row.second.actual_start_source_ref){
  if(pos(p.common,ref)>=pos(p.common,row.second.actual_start_source_ref->source_ref))fail("GovernanceSupervisionApprovalAfterStart",a->id);
 }
 take(validate_governance_approval_authority(p,*h.specification,a->issuer,a->authority_basis_ref,a->budget_execution,ref));
 take(p.originals.validate_historical_supervision_execution_approval(*h.approval,*a,*h.specification));
 permit(x,h,e,a->issued_at,&ref);return *a;
}
// 后序 H 只读请求核；当前权限与原回放都不在预检写事实或绑定 Basis。
template<class Existing,class Fresh>auto supervision_approval_request(const GovernanceSupervisionApprovalValidationContext& x,const TaskWorkExecutionState& s,const ApproveGovernanceSupervisionExecution& request,const ReadStamp& stamp,Existing&& existing,Fresh&& fresh){
 const auto& p=x.history.programs;const auto& e=episode(x,request.work_id);
 // 业务once在当前条件之前；同一请求读回不可变原签发，不消费新的权限/预算。
 for(const auto& row:p.state.supervision_execution_approvals.rows())if(row.second.work_id==e.work_id&&row.second.execution_ordinal==request.execution_ordinal){
  const auto& a=row.second;
  if(!same(a.issuer,request.authority)||!same(a.quote_ref,request.quote_ref)||a.expected_target_revision!=request.expected_target_revision||a.successor_acceptance_ref.has_value()!=request.successor_acceptance_ref.has_value()||(a.successor_acceptance_ref&&!same_source_ref(*a.successor_acceptance_ref,*request.successor_acceptance_ref)))
   fail("GovernanceSupervisionApprovalReplayChanged",a.id);
  auto ref=governance_supervision_execution_approval_source(a);(void)verified(x,ref);return existing(std::move(ref));
 }
 WorkExecutionRead current(p.common,x.history.tasks,x.executions.original);
 if(take(current.read_work(e.work_id))!=&e||e.terminal_result_ref||e.current_execution||e.phase!=WorkPhase::paused)
  fail("GovernanceSupervisionExecutionNotReady",e.work_id);
 Integer next=0;
 for(const auto& row:s.supervision_execution_terms.rows())if(row.second.work_id==e.work_id){
  if(row.first!=row.second.action_id||row.second.execution_ordinal!=next||!row.second.actual_terminal_result_ref||next==std::numeric_limits<Integer>::max())
   fail("GovernanceSupervisionExecutionOrdinalMismatch",e.work_id);++next;
 }
 if(next<=0||next!=request.execution_ordinal)fail("GovernanceSupervisionNextOrdinalRequired",e.work_id);
 const auto now=take(p.clock.read_organization_authority_now());auto h=take(read_governance_supervision_start_history(x.history,e,now,nullptr));
 const auto& q=quote(p.catalog,request.quote_ref,*h.specification);auto prior=preceding(x,e,next,now,nullptr);permit(x,h,e,now,nullptr);
 if(request.authority.organization_id!=h.approval->issuer.organization_id)fail("GovernanceSupervisionIssuerOrganizationChanged",e.work_id);
 const auto& actual_actor=executor(x,e,request.successor_acceptance_ref,now,nullptr);
 auto authority=take(prepare_governance_approval_authority(p,*h.specification,GovernanceNamedExecutor{actual_actor},request.authority,h.approval->budget_execution,request.expected_target_revision,stamp));
 if(!same(authority.target.budget_period,h.approval->budget_period)||authority.target.payer_wallet_id!=h.approval->payer_wallet_id||
    authority.target.contract_debtor.kind!=h.approval->contract_debtor.kind||authority.target.contract_debtor.id!=h.approval->contract_debtor.id)
  fail("GovernanceSupervisionOriginalExpenseBasisChanged",e.work_id);
 return fresh(e,next,now,h,q,prior,authority);
}
}
SourceRef governance_supervision_execution_approval_source(const GovernanceSupervisionExecutionApproval& a){return {"politics","governance_supervision_execution_approved",a.id};}
Outcome<const GovernanceSupervisionExecutionApproval*> read_verified_governance_supervision_approval(const GovernanceSupervisionApprovalValidationContext& x,const SourceRef& ref){return guard<const GovernanceSupervisionExecutionApproval*>([&]{return &verified(x,ref);});}
Status validate_governance_supervision_execution_request(const GovernanceSupervisionApprovalValidationContext& x,const ApproveGovernanceSupervisionExecution& request,const StableId& actor,const ReadStamp& stamp){return guard<Done>([&]{
 if(actor.empty()||actor!=request.authority.actor_id)fail("GovernanceSupervisionAuthenticatedIssuerMismatch",actor);
 const auto& s=selected(x);return supervision_approval_request(x,s,request,stamp,
 [](const SourceRef&){return Done{};},
 [](const WorkEpisode&,Integer,const WorldMoment&,const GovernanceAcceptedWorkHistory&,const auto&,const auto&,const GovernanceApprovalAuthorityCapture&){return Done{};});
});}
Outcome<SourceRef> approve_governance_supervision_execution_in_candidate(CommonStore& common,OrganizationRuntime& runtime,GovernanceProgramState& state,const GovernanceSupervisionApprovalValidationContext& x,const ApproveGovernanceSupervisionExecution& request,const StableId& actor,const ReadStamp& stamp){return guard<SourceRef>([&]{
 const auto& p=x.history.programs;
 if(&common!=&p.common||&runtime!=&p.organization_runtime||&state!=&p.state)fail("GovernanceProgramCandidateMismatch");
 if(actor.empty()||actor!=request.authority.actor_id)fail("GovernanceSupervisionAuthenticatedIssuerMismatch",actor);
 const auto& s=selected(x);return supervision_approval_request(x,s,request,stamp,
 [](SourceRef&& ref)->SourceRef{return std::move(ref);},
 [&](const WorkEpisode& e,Integer next,const WorldMoment& now,const GovernanceAcceptedWorkHistory& h,const auto& q,const auto& prior,const GovernanceApprovalAuthorityCapture& authority)->SourceRef{
 GovernanceSupervisionExecutionApproval a;a.id=take(common.next_id("governance_program"));a.work_id=e.work_id;a.authorization=e.authorization;
 a.execution_ordinal=next;a.preceding_terminal_ref=*prior.terms->actual_terminal_result_ref;a.quote_ref=q.quote_ref;a.spec_ref=h.specification->spec_ref;
 a.service_limit_half_days=q.time_use.half_days;a.issuer=request.authority;a.budget_execution=h.approval->budget_execution;a.budget_period=h.approval->budget_period;
 a.expected_target_revision=request.expected_target_revision;a.issued_at=now;a.source_schema=state.schema_version==3?2:1;a.successor_acceptance_ref=request.successor_acceptance_ref;auto ref=governance_supervision_execution_approval_source(a);a.authority_basis_ref={ref,1};
 take(common.append_fact({a.id,ref.kind,ref.domain}));if(!state.supervision_execution_approvals.insert(ref.id,std::move(a)))fail("DuplicateGovernanceSupervisionApproval",ref.id);
 take(bind_governance_approval_authority_in_candidate(common,runtime,p,authority,ref));(void)verified(x,ref);return ref;
 });
});}
Status validate_governance_supervision_approvals(const GovernanceSupervisionApprovalValidationContext& x){return guard<Done>([&]{
 selected(x);std::set<StableId> covered;
 for(const auto& row:x.history.programs.state.supervision_execution_approvals.rows()){
  if(row.first!=row.second.id)fail("GovernanceSupervisionApprovalKeyMismatch",row.first);
  (void)verified(x,governance_supervision_execution_approval_source(row.second));covered.insert(row.first);
 }
 for(const auto& row:x.history.programs.common.state().facts.rows())if(row.second.domain=="politics"&&row.second.kind=="governance_supervision_execution_approved"&&!covered.count(row.first))fail("GovernanceSupervisionApprovalPayloadMissing",row.first);
 return Done{};
});}
}
