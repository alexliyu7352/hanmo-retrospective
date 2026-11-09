// 原批准只给ordinal0；ordinal>0逐条消费同Work真实新签发，不复用H或目标验工量。
#include "governance_supervision_execution_approval.hpp"
#include "governance_supervision_approvals.hpp"
#include "resident/domains/tasks/work_execution/work_reassignment_sources.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]]void fail(const StableId& code,const StableId& id={},ErrorKind kind=ErrorKind::rule){throw Failure{common_failure(code,id,kind).error};}
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{value.error};return std::move(value.value);}
template<class T,class F>Outcome<T> guard(F&& fn){try{return {true,fn(),{}};}catch(const Failure& e){return {false,{},e.error};}}
int cmp(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b));}
std::size_t pos(const CommonStore& c,const SourceRef& ref){
 const auto* h=c.state().facts.find(ref.id);const auto& ids=c.state().facts.order();auto it=std::find(ids.begin(),ids.end(),ref.id);
 if(!h||h->domain!=ref.domain||h->kind!=ref.kind||it==ids.end())fail("GovernanceSupervisionOriginalSourceMissing",ref.id);return static_cast<std::size_t>(it-ids.begin());
}
const TaskWorkExecutionState& selected(const GovernanceWorkHistoryContext& x,const WorkEpisode& e,const SupervisionSourceContext& sources,const SupervisionExecutionApprovalRead& approvals){
 if((x.programs.catalog.schema_version!=3&&x.programs.catalog.schema_version!=4)||!x.tasks.work_execution||!task_work_fixed_execution_terms_version(x.tasks.work_execution->version))
  fail("GovernanceSupervisionFixedApprovalSelectionRequired",e.work_id,ErrorKind::unsupported_capability);
 if((x.programs.catalog.schema_version==4)!=(x.programs.state.schema_version==2||x.programs.state.schema_version==3)||
    (x.programs.state.schema_version!=1&&x.programs.state.schema_version!=2&&x.programs.state.schema_version!=3)||
    (task_work_lifecycle_sources_version(x.tasks.work_execution->version)!=(x.programs.state.schema_version==3)))fail("GovernanceProgramProfileMismatch",e.work_id);
 const auto& s=*x.tasks.work_execution;
 if(task_work_lifecycle_sources_version(s.version)&&(!sources.reassignments||&sources.reassignments->governance.history!=&x))fail("GovernanceSupervisionReassignmentSourcesRequired",e.work_id);
 if(s.episodes.find(e.work_id)!=&e||!x.task_definitions.bound_to(x.programs.common,x.tasks)||
    &sources.common!=&x.programs.common||&sources.tasks!=&x.tasks||&sources.definitions!=&x.task_definitions||&sources.approvals!=&approvals||
    cmp(take(x.programs.clock.read_organization_authority_now()),take(sources.clock.read_current_moment())))
  fail("GovernanceSupervisionApprovalCandidateMismatch",e.work_id);
 return s;
}
// 整数ordinal来自S3已存在执行的原顺序，不按批准条数或当前日期推算。
Integer next_ordinal(const TaskWorkExecutionState& s,const WorkEpisode& e){
 Integer ordinal=0;
 for(const auto& row:s.supervision_execution_terms.rows())if(row.second.work_id==e.work_id){
  if(row.first!=row.second.action_id||row.second.execution_ordinal!=ordinal||ordinal==std::numeric_limits<Integer>::max())fail("GovernanceSupervisionExecutionOrdinalMismatch",e.work_id);
  ++ordinal;
 }
 return ordinal;
}
ApprovedSupervisionExecution approved(const GovernanceWorkHistoryContext& x,const GovernanceSupervisionContinuityRead& continuity,
 const SupervisionSourceContext& sources,const WorkEpisode& e,Integer ordinal,const WorldMoment& at,const SourceRef* cut){
 if(ordinal<0)fail("InvalidSupervisionExecutionOrdinal",e.work_id);
 auto h=take(read_governance_supervision_start_history(x,e,at,cut));
 auto permission=take(continuity.read_accepted_supervision_continuity_at(h,e,at,cut));
 if(permission.actual_sources.empty())fail("GovernanceSupervisionContinuitySourceRequired",e.work_id);
 if(permission.actual_blocking_source)fail("GovernanceSupervisionExecutionBlocked",permission.actual_blocking_source->source_ref.id);
 const auto identity=take(read_governance_supervision_executor_identity_at(x,e,at,cut));
 if(!identity.actor_id||identity.actor_id->empty())fail("GovernanceSupervisionActualExecutorRequired",e.work_id);
 if(identity.acceptance){if(!sources.reassignments)fail("GovernanceSupervisionReassignmentSourcesRequired",e.work_id);const auto* accepted=take(read_work_reassignment_acceptance(*sources.reassignments,{"tasks","work_reassignment_accepted",identity.acceptance->fact_id}));if(accepted!=identity.acceptance||accepted->new_actor_id!=*identity.actor_id)fail("GovernanceSupervisionReassignmentOwnerMismatch",e.work_id);}
 ApprovedSupervisionExecution out;out.authorization=e.authorization;out.kind=h.supervision->kind;out.actual_executor_actor_id=identity.actor_id;out.initial_successor_acceptance=ordinal==0?identity.acceptance:nullptr;
 if(ordinal==0){
  const auto* duration=take(read_governance_initial_supervision_duration(x.programs.catalog,*h.specification));
  out.original_terms_source={governance_program_source(*h.approval),1};out.service_limit_half_days=duration->half_days;
 }else{
  if(x.programs.catalog.schema_version!=4)fail("GovernanceNextSupervisionApprovalRequired",e.work_id,ErrorKind::unsupported_capability);
  const GovernanceSupervisionExecutionApproval* a=nullptr;
  for(const auto& row:x.programs.state.supervision_execution_approvals.rows())if(row.second.work_id==e.work_id&&row.second.execution_ordinal==ordinal){
   if(a||row.first!=row.second.id)fail("DuplicateGovernanceSupervisionApproval",e.work_id);a=&row.second;
  }
  if(!a)fail("GovernanceNextSupervisionApprovalRequired",e.work_id);
  const auto ref=governance_supervision_execution_approval_source(*a);
  // 严格先验再下降前序terminal，禁止将本start以后的批准用于本start。
  if(cmp(a->issued_at,at)>0||(cut&&pos(x.programs.common,ref)>=pos(x.programs.common,*cut)))fail("GovernanceSupervisionApprovalAfterStart",a->id);
  GovernanceSupervisionApprovalValidationContext validation{x,continuity,sources};
  if(take(read_verified_governance_supervision_approval(validation,ref))!=a)fail("GovernanceSupervisionApprovalOwnerMismatch",a->id);
  out.original_terms_source={ref,a->source_schema};out.service_limit_half_days=a->service_limit_half_days;
 }
 take(validate_approved_supervision_execution(e,out));return out;
}
void match_row(const SupervisionExecutionTerms& row,const WorkEpisode& e,Integer ordinal,const SupervisionActionStartSource& start,const ApprovedSupervisionExecution& original){
 take(validate_supervision_execution_terms_shape(row));
 if(row.work_id!=e.work_id||row.execution_ordinal!=ordinal||row.action_id!=start.action_id||
    !same_source_ref(row.original_authorization_ref.program_ref,original.authorization.program_ref)||
    row.original_authorization_ref.target_line_id!=original.authorization.target_line_id||
    row.original_authorization_ref.accepted_task_id!=original.authorization.accepted_task_id||
    !same_terms_revision(row.original_authorization_ref.terms_revision,original.authorization.terms_revision)||
    row.original_approved_execution_terms_ref.source_schema!=original.original_terms_source.source_schema||
    !same_source_ref(row.original_approved_execution_terms_ref.source_ref,original.original_terms_source.source_ref)||
    row.kind!=original.kind||row.service_limit_half_days!=original.service_limit_half_days||
    !original.actual_executor_actor_id||start.actor_id!=*original.actual_executor_actor_id||
    row.initial_successor_acceptance_ref.has_value()!=(original.initial_successor_acceptance!=nullptr)||
    (row.initial_successor_acceptance_ref&&(!original.initial_successor_acceptance||!same_source_ref(*row.initial_successor_acceptance_ref,{"tasks","work_reassignment_accepted",original.initial_successor_acceptance->fact_id})))||
    !row.actual_start_source_ref||row.actual_start_source_ref->source_schema!=1||
    !same_source_ref(row.actual_start_source_ref->source_ref,{"tasks","supervision_action_started",start.fact_id}))
  fail("GovernanceSupervisionOriginalDurationMismatch",start.fact_id);
}
}
bool NativeGovernanceSupervisionExecutionApprovalRead::bound_to(const CommonStore& common,const TaskState& tasks)const noexcept{return &common==&history_.programs.common&&&tasks==&history_.tasks;}
Outcome<ApprovedSupervisionExecution> NativeGovernanceSupervisionExecutionApprovalRead::read_current_supervision_execution(
 const WorkEpisode& e,Integer ordinal,const WorldMoment& now,const SupervisionSourceContext& sources)const{return guard<ApprovedSupervisionExecution>([&]{
 const auto& s=selected(history_,e,sources,*this);
 if(e.terminal_result_ref||e.current_execution||e.phase!=WorkPhase::paused||ordinal<0||ordinal!=next_ordinal(s,e))fail("GovernanceSupervisionExecutionNotReady",e.work_id);
 for(const auto& row:s.supervision_execution_terms.rows())if(row.second.work_id==e.work_id&&!row.second.actual_terminal_result_ref)fail("GovernanceSupervisionPriorTerminalRequired",e.work_id);
 for(const auto& row:s.supervision_starts.rows())if(row.second.work_id==e.work_id&&!s.supervision_execution_terms.find(row.second.action_id))fail("GovernanceSupervisionStartWithoutTerms",row.first);
 return approved(history_,continuity_,sources,e,ordinal,now,nullptr);
});}
Outcome<ApprovedSupervisionExecution> NativeGovernanceSupervisionExecutionApprovalRead::read_original_supervision_execution(
 const WorkEpisode& e,Integer ordinal,const MomentOwnerSource& source,const WorldMoment& at,const SupervisionSourceContext& sources)const{return guard<ApprovedSupervisionExecution>([&]{
 const auto& s=selected(history_,e,sources,*this);
 if(ordinal<0||ordinal>=next_ordinal(s,e)||source.source_schema!=1||source.source_ref.domain!="tasks"||source.source_ref.kind!="supervision_action_started")fail("GovernanceSupervisionActualStartRequired",source.source_ref.id);
 const auto* start=s.supervision_starts.find(source.source_ref.id);
 if(!start||start->fact_id!=source.source_ref.id||start->work_id!=e.work_id)fail("GovernanceSupervisionActualStartMismatch",source.source_ref.id);
 auto original=approved(history_,continuity_,sources,e,ordinal,at,&source.source_ref);
 const auto* row=s.supervision_execution_terms.find(start->action_id);if(!row)fail("GovernanceSupervisionOriginalTermsRequired",start->action_id);
 match_row(*row,e,ordinal,*start,original);return original;
});}
}
