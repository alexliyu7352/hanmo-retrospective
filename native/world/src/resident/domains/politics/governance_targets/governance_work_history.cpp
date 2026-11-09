// 历史准入按实际Common顺序与原精确时刻反查；不调用当前Grant，也不派生第二Work状态。
#include "governance_work_history.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]] void fail(const StableId& code,const StableId& id={}) {
 throw Failure{common_failure(code,id).error};
}
template<class T> T take(Outcome<T> value) {
 if(!value.ok)throw Failure{value.error};return std::move(value.value);
}
template<class T,class F> Outcome<T> guard(F&& fn) {
 try{return {true,fn(),{}};}catch(const Failure& e){return {false,{},e.error};}
}
int cmp(const WorldMoment& a,const WorldMoment& b) {return take(compare_world_moments(a,b));}
bool same(const WorkAuthorizationRef& a,const WorkAuthorizationRef& b) {
 return same_source_ref(a.program_ref,b.program_ref)&&a.target_line_id==b.target_line_id&&
  a.accepted_task_id==b.accepted_task_id&&same_terms_revision(a.terms_revision,b.terms_revision);
}
std::size_t pos(const CommonStore& c,const SourceRef& ref) {
 const auto* h=c.state().facts.find(ref.id);
 if(!valid_source_ref(ref)||!h||h->domain!=ref.domain||h->kind!=ref.kind)
  fail("GovernanceWorkOriginalSourceMissing",ref.id);
 const auto& order=c.state().facts.order();const auto it=std::find(order.begin(),order.end(),ref.id);
 if(it==order.end())fail("GovernanceWorkSourceOrderMissing",ref.id);
 return static_cast<std::size_t>(it-order.begin());
}
SourceRef binding_source(const GovernanceExecutionBinding& b) {return {"politics","governance_execution_bound",b.id};}
void before(const CommonStore& c,const SourceRef& a,const SourceRef& b) {
 if(pos(c,a)>=pos(c,b))fail("GovernanceWorkSourceOrderMismatch",b.id);
}
const TaskWorkExecutionState& state(const GovernanceWorkHistoryContext& x) {
 if(!x.tasks.work_execution||!task_work_execution_version(x.tasks.work_execution->version)||
    !x.task_definitions.bound_to(x.programs.common,x.tasks))fail("GovernanceWorkHistoryOwnerMismatch");
 return *x.tasks.work_execution;
}
void verify_binding(const GovernanceWorkHistoryContext& x,const GovernanceProgramApproval& a,
                    const GovernanceExecutionBinding& b) {
 const auto& c=x.programs.common;
 if(b.id.empty()||b.program_id!=a.id||b.task_acceptance_ref.has_value()!=b.execution_ref.has_value())
  fail("GovernanceWorkBindingMismatch",b.id);
 take(validate_work_task_execution_binding_source(c,x.tasks,governance_program_source(a),
  b.offered_task_ref,b.task_acceptance_ref,b.execution_ref));
 before(c,governance_program_source(a),b.offered_task_ref);before(c,b.offered_task_ref,binding_source(b));
 if(b.task_acceptance_ref){before(c,*b.task_acceptance_ref,binding_source(b));before(c,*b.execution_ref,binding_source(b));}
 if(cmp(b.occurred_at,a.issued_at)<0||cmp(b.occurred_at,take(x.programs.clock.read_organization_authority_now()))>0)
  fail("GovernanceWorkBindingTimeMismatch",b.id);
}
GovernanceAcceptedWorkHistory accepted(const GovernanceWorkHistoryContext& x,const WorkEpisode& e) {
 const auto& s=state(x);const auto& c=x.programs.common;
 if(s.episodes.find(e.work_id)!=&e||e.work_id.empty())fail("GovernanceWorkEpisodeOwnerMismatch",e.work_id);
 // 该原S3定义读只验证呈示/本人接受与冻结身份，不消费Work进度或监督许可。
 const auto definition=take(x.task_definitions.read_definition(e.authorization.accepted_task_id));
 const auto verified=take(read_verified_governance_program_approval(x.programs,e.authorization.program_ref));
 if(!definition.approval||!definition.specification||!definition.offered||!definition.accepted||
    definition.approval!=verified.approval||definition.specification!=verified.specification)
  fail("GovernanceWorkApprovalOwnerMismatch",e.work_id);
 const auto& a=*verified.approval;const auto& spec=*verified.specification;
 const auto& offer=*definition.offered;const auto& task=*definition.accepted;
 const auto* wa=s.acceptances.find(e.acceptance_source_ref);
 const auto* plan=std::get_if<SupervisedWorkPlan>(&e.frozen_plan);
 const auto* patrol=std::get_if<PatrolPlan>(&e.frozen_plan);
 if(!wa||wa->fact_id!=e.acceptance_source_ref||wa->work_id!=e.work_id||!same(wa->authorization,e.authorization)||
    e.authorization.target_line_id!=spec.spec_ref.id||
    !same_terms_revision(e.authorization.terms_revision,TermsRevisionRef{SourceFactTerms{e.authorization.program_ref}}))
  fail("GovernanceWorkAcceptanceMismatch",e.work_id);
 // S3 alone projects the actual approved plan into accepted Work terms. Its
 // canonical matcher covers both Patrol and Supervision; keep the expense
 // source separately equal, as in the original S3 acceptance validator.
 const auto expected=take(x.task_definitions.read_accepted_work(e.authorization));
 if(!same(expected.frozen_request.authorization,e.authorization)||
    !same_work_plan_terms(expected.frozen_request.plan,e.frozen_plan)||
    !expected.actual_expense_terms||!same_source_ref(*expected.actual_expense_terms,wa->expense_terms_ref)||
    !std::visit([&](const auto& value){return same_source_ref(value.expense_terms_ref,wa->expense_terms_ref);},e.frozen_plan))
  fail("GovernanceWorkFrozenTermsChanged",e.work_id);
 const auto* named=std::get_if<GovernanceNamedExecutor>(&a.executor);
 if(!named||named->actor_id!=task.executor_actor_id)fail("GovernanceWorkOriginalExecutorRequired",e.work_id);
 const SourceRef task_ref{"tasks","work_task_accepted",task.fact_id};
 const SourceRef work_ref{"tasks","work_accepted",wa->fact_id};
 if(wa->original_task_acceptance.source_schema!=1||!same_source_ref(wa->original_task_acceptance.source_ref,task_ref)||
    expected.actual_task_acceptance.source_schema!=wa->original_task_acceptance.source_schema||
    !same_source_ref(expected.actual_task_acceptance.source_ref,wa->original_task_acceptance.source_ref)||
    cmp(expected.accepted_at,wa->accepted_at)||cmp(expected.earliest_service_at,wa->earliest_service_at)||
    cmp(wa->accepted_at,task.accepted_at)||cmp(wa->earliest_service_at,task.earliest_service_at))
  fail("GovernanceWorkAcceptedSourceChanged",e.work_id);
 before(c,e.authorization.program_ref,{"tasks","work_task_offered",offer.fact_id});
 before(c,{"tasks","work_task_offered",offer.fact_id},task_ref);before(c,task_ref,work_ref);
 GovernanceAcceptedWorkHistory out{&a,&spec,&offer,&task,wa,nullptr,nullptr,plan,patrol};
 for(const auto& row:x.programs.state.execution_bindings.rows()) {
  const auto& b=row.second;if(b.program_id!=a.id)continue;
  if(row.first!=b.id||!same_source_ref(b.offered_task_ref,task.offered_task_ref))
   fail("GovernanceWorkForeignOfferBinding",row.first);
  verify_binding(x,a,b);
  if(b.task_acceptance_ref) {
   if(out.acceptance_binding||!same_source_ref(*b.task_acceptance_ref,task_ref)||!same_source_ref(*b.execution_ref,work_ref)||cmp(b.occurred_at,wa->accepted_at))
    fail("GovernanceWorkAcceptanceBindingMismatch",b.id);
   out.acceptance_binding=&b;
  }else {
   if(out.offer_binding||cmp(b.occurred_at,offer.offered_at))fail("GovernanceWorkOfferBindingMismatch",b.id);
   out.offer_binding=&b;
  }
 }
 if(!out.offer_binding||!out.acceptance_binding)fail("GovernanceWorkOriginalBindingsRequired",e.work_id);
 before(c,binding_source(*out.offer_binding),task_ref);
 before(c,binding_source(*out.offer_binding),binding_source(*out.acceptance_binding));
 return out;
}
// 身份层只检查 retained association，不回调 H/start/skill/lifecycle 或 Growth。
const FactRecord* identity_head(const CommonStore& common,const SourceRef& ref){(void)pos(common,ref);const auto* h=common.state().facts.find(ref.id);if(!h||h->id!=ref.id)fail("GovernanceExecutorSourceHeaderRequired",ref.id);return h;}
const WorkReassignmentAcceptanceSource& acceptance_identity(const GovernanceWorkHistoryContext& x,const WorkEpisode& e,const SourceRef& ref){
 const auto& s=state(x);const auto& p=x.programs;
 if(!task_work_lifecycle_sources_version(s.version)||p.state.schema_version!=3||ref.domain!="tasks"||ref.kind!="work_reassignment_accepted")fail("GovernanceExecutorAcceptanceRequired",ref.id);
 const auto* accepted=s.work_reassignment_acceptances.find(ref.id);
 if(!accepted||accepted->fact_id!=ref.id||accepted->work_id!=e.work_id||accepted->new_actor_id.empty()||!same_source_ref(accepted->work_acceptance_ref,{"tasks","work_accepted",e.acceptance_source_ref}))fail("GovernanceExecutorAcceptanceMismatch",ref.id);
 identity_head(p.common,ref);before(p.common,accepted->work_acceptance_ref,accepted->reassignment_approval_ref);before(p.common,accepted->reassignment_approval_ref,ref);
 const auto& authority=accepted->reassignment_approval_ref;const auto* a=p.state.work_reassignment_approvals.find(authority.id);
 if(authority.domain!="politics"||authority.kind!="governance_work_reassignment_approved"||!a||a->id!=authority.id||a->work_id!=e.work_id||!same(a->authorization,e.authorization)||a->new_actor_id!=accepted->new_actor_id||!same_source_ref(a->work_acceptance_ref,accepted->work_acceptance_ref)||cmp(a->issued_at,accepted->accepted_at)>0||cmp(accepted->earliest_service_at,accepted->accepted_at)<0)fail("GovernanceExecutorReassignmentMismatch",ref.id);
 return *accepted;
}
GovernanceWorkExecutorIdentity executor_before(const GovernanceWorkHistoryContext& x,const WorkEpisode& e,const SourceRef* cut){
 const auto& s=state(x);const auto& c=x.programs.common;
 if(s.episodes.find(e.work_id)!=&e)fail("GovernanceExecutorWorkOwnerMismatch",e.work_id);
 const auto limit=cut?pos(c,*cut):c.state().facts.order().size();
 const auto* accepted=s.acceptances.find(e.acceptance_source_ref);
 if(!accepted||accepted->fact_id!=e.acceptance_source_ref||accepted->work_id!=e.work_id||!same(accepted->authorization,e.authorization)||accepted->original_task_acceptance.source_schema!=1||accepted->original_task_acceptance.source_ref.domain!="tasks"||accepted->original_task_acceptance.source_ref.kind!="work_task_accepted")fail("GovernanceExecutorOriginalAcceptanceRequired",e.work_id);
 const SourceRef accepted_ref{"tasks","work_accepted",accepted->fact_id};const auto* predecessor=identity_head(c,accepted_ref);if(pos(c,accepted_ref)>=limit)fail("GovernanceExecutorAcceptanceAfterCut",e.work_id);
 const auto* task=s.work_task_acceptances.find(accepted->original_task_acceptance.source_ref.id);
 if(!task||task->fact_id!=accepted->original_task_acceptance.source_ref.id||task->task_id!=e.authorization.accepted_task_id)fail("GovernanceExecutorTaskAcceptanceRequired",e.work_id);
 before(c,accepted->original_task_acceptance.source_ref,accepted_ref);
 const StableId* actor=std::visit([](const auto& p)->const StableId*{using T=std::decay_t<decltype(p)>;if constexpr(std::is_same_v<T,PatrolPlan>)return &p.leader_actor_id;else return &p.supervisor_actor_id;},e.frozen_plan);
 if(actor->empty()||task->executor_actor_id!=*actor)fail("GovernanceExecutorInitialActorMismatch",e.work_id);
 const WorkReassignmentAcceptanceSource* last_acceptance=nullptr;std::size_t prior_order=pos(c,accepted_ref),covered=0;
 for(const auto& id:e.leader_change_fact_refs){
  const auto* change=s.leader_changes.find(id);if(!change||change->fact_id!=id||change->work_id!=e.work_id)fail("GovernanceExecutorLeaderChangeMissing",id);
  const SourceRef source{"tasks","work_leader_changed",id};const auto order=pos(c,source);if(order<=prior_order)fail("GovernanceExecutorLeaderChangeOrder",id);prior_order=order;if(order>=limit)continue;
  if(!task_work_lifecycle_sources_version(s.version))fail("GovernanceExecutorReplacementRequiresWork6",id);
  if(change->old_actor_id!=*actor||change->new_actor_id.empty()||change->new_actor_id==*actor||change->authority_source.source_schema!=1||change->actual_consent_source.source_schema!=1)fail("GovernanceExecutorLeaderChangeMismatch",id);
  const auto& consent=acceptance_identity(x,e,change->actual_consent_source.source_ref);const auto* authority=x.programs.state.work_reassignment_approvals.find(consent.reassignment_approval_ref.id);
  if(!authority||consent.new_actor_id!=change->new_actor_id||!same_source_ref(change->authority_source.source_ref,consent.reassignment_approval_ref)||authority->old_actor_id!=*actor||authority->executor_predecessor_ref.source_schema!=1||!same_source_ref(authority->executor_predecessor_ref.source_ref,{predecessor->domain,predecessor->kind,predecessor->id})||cmp(consent.earliest_service_at,change->replacement_at)>0)fail("GovernanceExecutorLeaderChangeAssociationMismatch",id);
  before(c,{predecessor->domain,predecessor->kind,predecessor->id},consent.reassignment_approval_ref);before(c,change->actual_consent_source.source_ref,change->original_execution_source.source_ref);before(c,change->original_execution_source.source_ref,source);
  predecessor=identity_head(c,source);actor=&authority->new_actor_id;last_acceptance=&consent;++covered;
 }
 std::size_t expected=0;for(const auto& row:s.leader_changes.rows())if(row.second.work_id==e.work_id&&pos(c,{"tasks","work_leader_changed",row.first})<limit){if(row.first!=row.second.fact_id)fail("GovernanceExecutorLeaderChangeKeyMismatch",row.first);++expected;}if(covered!=expected)fail("GovernanceExecutorLeaderChangeCoverage",e.work_id);
 return {actor,predecessor,last_acceptance};
}
GovernanceWorkExecutorIdentity accepted_executor(const GovernanceWorkHistoryContext& x,const WorkEpisode& e,const SourceRef& accepted_ref,const WorldMoment& at,const SourceRef* cut){
 const auto& accepted=acceptance_identity(x,e,accepted_ref);const auto& c=x.programs.common;
 if(cmp(accepted.earliest_service_at,at)>0||(cut&&pos(c,accepted_ref)>=pos(c,*cut)))fail("GovernanceExecutorAcceptanceAfterStart",accepted.fact_id);
 const auto* approval=x.programs.state.work_reassignment_approvals.find(accepted.reassignment_approval_ref.id);
 auto original=executor_before(x,e,&accepted.reassignment_approval_ref);
 if(!approval||approval->old_actor_id!=*original.actor_id||approval->executor_predecessor_ref.source_schema!=1||!same_source_ref(approval->executor_predecessor_ref.source_ref,{original.predecessor->domain,original.predecessor->kind,original.predecessor->id}))fail("GovernanceExecutorPredecessorMismatch",accepted.fact_id);
 auto current=executor_before(x,e,cut);
 const bool pending=same_source_ref(approval->executor_predecessor_ref.source_ref,{current.predecessor->domain,current.predecessor->kind,current.predecessor->id});
 const bool effective=current.acceptance&&current.acceptance->fact_id==accepted.fact_id;
 if(!pending&&!effective)fail("GovernanceExecutorAcceptanceSuperseded",accepted.fact_id);
 return {&approval->new_actor_id,current.predecessor,&accepted};
}
GovernanceWorkExecutorIdentity supervision_executor(const GovernanceWorkHistoryContext& x,const WorkEpisode& e,const WorldMoment& at,const SourceRef* cut){
 const auto& s=state(x);const auto& c=x.programs.common;
 if(!std::holds_alternative<SupervisedWorkPlan>(e.frozen_plan))fail("GovernanceSupervisionPlanRequired",e.work_id);
 if(!task_work_lifecycle_sources_version(s.version)){
  const auto& original=std::get<SupervisedWorkPlan>(e.frozen_plan);const auto* accepted=s.acceptances.find(e.acceptance_source_ref);
  if(s.episodes.find(e.work_id)!=&e||!accepted||accepted->work_id!=e.work_id||original.supervisor_actor_id.empty())fail("GovernanceExecutorOriginalAcceptanceRequired",e.work_id);
  const auto* predecessor=identity_head(c,{"tasks","work_accepted",accepted->fact_id});
  if(cut){const auto* start=s.supervision_starts.find(cut->id);if(cut->domain!="tasks"||cut->kind!="supervision_action_started"||!start||start->work_id!=e.work_id||start->actor_id!=original.supervisor_actor_id||cmp(start->started_at,at))fail("GovernanceExecutorActualStartMismatch",cut->id);before(c,{predecessor->domain,predecessor->kind,predecessor->id},*cut);}
  return {&original.supervisor_actor_id,predecessor,nullptr};
 }
 if(x.programs.state.schema_version!=3)fail("GovernanceWorkLifecycleSelectionRequired",e.work_id);
 const SupervisionExecutionTerms* selected=nullptr;const SupervisionActionStartSource* start=nullptr;Integer ordinal=0;
 if(cut){
  if(cut->domain!="tasks"||cut->kind!="supervision_action_started")fail("GovernanceExecutorActualStartCutRequired",cut->id);
  start=s.supervision_starts.find(cut->id);if(!start||start->fact_id!=cut->id||start->work_id!=e.work_id||cmp(start->started_at,at))fail("GovernanceExecutorActualStartMismatch",cut->id);
  selected=s.supervision_execution_terms.find(start->action_id);
  if(!selected||selected->work_id!=e.work_id||selected->action_id!=start->action_id||!same(selected->original_authorization_ref,e.authorization)||!selected->actual_start_source_ref||selected->actual_start_source_ref->source_schema!=1||!same_source_ref(selected->actual_start_source_ref->source_ref,*cut))fail("GovernanceExecutorActualTermsRequired",cut->id);
  ordinal=selected->execution_ordinal;
 }else{
  for(const auto& row:s.supervision_execution_terms.rows())if(row.second.work_id==e.work_id){if(row.first!=row.second.action_id||row.second.execution_ordinal!=ordinal||ordinal==std::numeric_limits<Integer>::max())fail("GovernanceExecutorOrdinalMismatch",e.work_id);++ordinal;}
 }
 GovernanceWorkExecutorIdentity value;
 if(ordinal==0){
  if(selected&&(!same_source_ref(selected->original_approved_execution_terms_ref.source_ref,e.authorization.program_ref)||selected->original_approved_execution_terms_ref.source_schema!=1))fail("GovernanceExecutorInitialTermsMismatch",e.work_id);
  if(selected&&selected->initial_successor_acceptance_ref)value=accepted_executor(x,e,*selected->initial_successor_acceptance_ref,at,cut);
  else if(selected){value=executor_before(x,e,cut);if(value.acceptance)fail("GovernanceExecutorInitialAcceptanceMissing",e.work_id);}
  else{
   value=executor_before(x,e,nullptr);const WorkReassignmentAcceptanceSource* pending=nullptr;
   for(const auto& row:s.work_reassignment_acceptances.rows())if(row.second.work_id==e.work_id){const auto& candidate=acceptance_identity(x,e,{"tasks","work_reassignment_accepted",row.first});const auto* approval=x.programs.state.work_reassignment_approvals.find(candidate.reassignment_approval_ref.id);if(approval&&same_source_ref(approval->executor_predecessor_ref.source_ref,{value.predecessor->domain,value.predecessor->kind,value.predecessor->id})){if(pending)fail("GovernanceExecutorPendingAcceptanceConflict",e.work_id);pending=&candidate;}}
   if(pending)value=accepted_executor(x,e,{"tasks","work_reassignment_accepted",pending->fact_id},at,nullptr);
  }
 }else{
  const GovernanceSupervisionExecutionApproval* approval=nullptr;
  for(const auto& row:x.programs.state.supervision_execution_approvals.rows())if(row.second.work_id==e.work_id&&row.second.execution_ordinal==ordinal){if(approval||row.first!=row.second.id)fail("DuplicateGovernanceSupervisionApproval",e.work_id);approval=&row.second;}
  if(!approval)fail("GovernanceNextSupervisionApprovalRequired",e.work_id);
  const SourceRef source{"politics","governance_supervision_execution_approved",approval->id};identity_head(c,source);
  if(!same(approval->authorization,e.authorization)||cmp(approval->issued_at,at)>0||(cut&&pos(c,source)>=pos(c,*cut))||approval->source_schema<1||approval->source_schema>2||(approval->source_schema==1&&approval->successor_acceptance_ref)||(selected&&(selected->initial_successor_acceptance_ref||selected->original_approved_execution_terms_ref.source_schema!=approval->source_schema||!same_source_ref(selected->original_approved_execution_terms_ref.source_ref,source))))fail("GovernanceExecutorApprovedTermsMismatch",e.work_id);
  if(approval->successor_acceptance_ref){before(c,*approval->successor_acceptance_ref,source);value=accepted_executor(x,e,*approval->successor_acceptance_ref,at,cut);}
  else{value=executor_before(x,e,cut);if(value.acceptance)fail("GovernanceExecutorApprovalAcceptanceMissing",e.work_id);}
 }
 if(!value.actor_id||value.actor_id->empty()||(start&&start->actor_id!=*value.actor_id))fail("GovernanceExecutorActualActorMismatch",e.work_id);
 return value;
}
GovernanceAcceptedWorkHistory start(const GovernanceWorkHistoryContext& x,const WorkEpisode& e,
                                   const WorldMoment& at,const SourceRef* cut) {
 auto out=accepted(x,e);const auto& c=x.programs.common;const auto now=take(x.programs.clock.read_organization_authority_now());
 if(!out.supervision)fail("GovernanceSupervisionPlanRequired",e.work_id);
 if(cmp(at,now)>0||(!cut&&cmp(at,now))||cmp(at,out.work_acceptance->earliest_service_at)<0||
    cmp(at,WorldMoment{out.supervision->completion_deadline.tick,{}})>=0)
  fail("GovernanceSupervisionStartOutsideWindow",e.work_id);
 if(cut) {
  if(cut->domain!="tasks"||cut->kind!="supervision_action_started")fail("GovernanceSupervisionStartCutRequired",cut->id);
  const auto* s=state(x).supervision_starts.find(cut->id);
  if(!s||s->fact_id!=cut->id||s->work_id!=e.work_id||s->actor_id!=*take(read_governance_supervision_executor_at(x,e,at,cut))||
     s->action_id.empty()||!same_source_ref(s->work_acceptance_ref,{"tasks","work_accepted",out.work_acceptance->fact_id})||
     cmp(s->started_at,at)||s->until_tick!=out.supervision->completion_deadline.tick)
   fail("GovernanceSupervisionActualStartMismatch",cut->id);
  before(c,binding_source(*out.acceptance_binding),*cut);
  // 同Moment的后来绑定不能被当作开工前许可；只比较tick或当前revision不足。
  if(cmp(out.acceptance_binding->occurred_at,at)>0)fail("GovernanceSupervisionStartBeforeBinding",cut->id);
 }
 return out;
}
}
Outcome<GovernanceWorkExecutorIdentity> read_governance_work_executor_before_cut(const GovernanceWorkHistoryContext& x,const WorkEpisode& e,const SourceRef* cut){return guard<GovernanceWorkExecutorIdentity>([&]{return executor_before(x,e,cut);});}
Outcome<GovernanceWorkExecutorIdentity> read_governance_supervision_executor_identity_at(const GovernanceWorkHistoryContext& x,const WorkEpisode& e,const WorldMoment& at,const SourceRef* cut){return guard<GovernanceWorkExecutorIdentity>([&]{return supervision_executor(x,e,at,cut);});}
Outcome<GovernanceWorkExecutorIdentity> read_governance_accepted_work_executor_at(const GovernanceWorkHistoryContext& x,const WorkEpisode& e,const SourceRef& acceptance,const WorldMoment& at,const SourceRef* cut){return guard<GovernanceWorkExecutorIdentity>([&]{return accepted_executor(x,e,acceptance,at,cut);});}
Outcome<const StableId*> read_governance_supervision_executor_at(const GovernanceWorkHistoryContext& x,const WorkEpisode& e,const WorldMoment& at,const SourceRef* cut){return guard<const StableId*>([&]{return supervision_executor(x,e,at,cut).actor_id;});}
Outcome<GovernanceAcceptedWorkHistory> read_governance_accepted_work_history(
 const GovernanceWorkHistoryContext& x,const WorkEpisode& e) {
 return guard<GovernanceAcceptedWorkHistory>([&]{return accepted(x,e);});
}
Outcome<GovernanceAcceptedWorkHistory> read_governance_supervision_start_history(
 const GovernanceWorkHistoryContext& x,const WorkEpisode& e,const WorldMoment& at,const SourceRef* cut) {
 return guard<GovernanceAcceptedWorkHistory>([&]{return start(x,e,at,cut);});
}
Status validate_governance_supervision_service_window(const GovernanceWorkHistoryContext& x,
 const WorkEpisode& e,const SupervisionActionStartSource& s,const MomentSpan& span) {
 return guard<Done>([&]{
  const SourceRef cut{"tasks","supervision_action_started",s.fact_id};
  take(read_governance_supervision_start_history(x,e,s.started_at,&cut));
  // /5 的 until_tick 仅保原目标期限；真实 F 服务终界由原批准 H/S0 链核。
  // 旧 /3 /4 解释保留，不将目标日期倒算为新执行时长。
  if(state(x).supervision_starts.find(s.fact_id)!=&s||cmp(span.from,span.to)>=0||cmp(span.from,s.started_at)<0||
     (!task_work_fixed_execution_terms_version(state(x).version)&&cmp(span.to,{s.until_tick,{}})>0)||
     cmp(span.to,take(x.programs.clock.read_organization_authority_now()))>0)
   fail("GovernanceSupervisionServiceWindowMismatch",s.fact_id);
  for(const auto& row:state(x).supervision_terminals.rows())if(row.second.start_source_ref==s.fact_id) {
   const SourceRef end{"tasks","supervision_action_ended",row.first};
   if(row.first!=row.second.fact_id)fail("GovernanceSupervisionTerminalKeyMismatch",row.first);
   before(x.programs.common,cut,end);
   if(cmp(span.to,row.second.ended_at)>0)fail("GovernanceSupervisionServiceAfterRelease",s.fact_id);
  }
  return Done{};
 });
}
}
