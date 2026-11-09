// 同M开工前态和全正服务窗分别读原来源；完成释放反读唯一S3结果，查询从不写权或进度。
#include "governance_supervision_scope.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]] void fail(const StableId& code,const StableId& id={}) {throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> value) {if(!value.ok)throw Failure{value.error};return std::move(value.value);}
template<class T,class F>Outcome<T> guard(F&& fn) {try{return {true,fn(),{}};}catch(const Failure& e){return {false,{},e.error};}}
int cmp(const WorldMoment& a,const WorldMoment& b) {return take(compare_world_moments(a,b));}
std::size_t position(const CommonStore& c,const SourceRef& ref) {
 const auto* fact=c.state().facts.find(ref.id);const auto& order=c.state().facts.order();
 if(!valid_source_ref(ref)||!fact||fact->domain!=ref.domain||fact->kind!=ref.kind)fail("GovernanceServiceSourceMissing",ref.id);
 const auto found=std::find(order.begin(),order.end(),ref.id);
 if(found==order.end())fail("GovernanceServiceSourceOrderMissing",ref.id);
 return static_cast<std::size_t>(found-order.begin());
}
void append(std::vector<MomentOwnerSource>& out,const MomentOwnerSource& value) {
 if(!valid_source_ref(value.source_ref)||value.source_schema<=0)fail("GovernanceServiceSourceShape");
 for(const auto& prior:out)if(same_source_ref(prior.source_ref,value.source_ref)) {
  if(prior.source_schema!=value.source_schema)fail("GovernanceServiceSourceSchemaConflict",value.source_ref.id);
  return;
 }
 out.push_back(value);
}
void originals(std::vector<MomentOwnerSource>& out,const std::vector<MomentOwnerSource>& sources) {
 if(sources.empty())fail("GovernanceServiceOriginalEvidenceRequired");
 for(const auto& source:sources)append(out,source);
}
// 作者目标/skill定义不是Common fact。具体专业reader核它的目录身份；这里只约束
// 已发生事实的同M切点，绝不为作者定义伪造一个Common头。
void before_cut(const CommonStore& common,const std::vector<MomentOwnerSource>& sources,const SourceRef* cut) {
 if(!cut)return;const auto stop=position(common,*cut);
 for(const auto& source:sources)if(common.state().facts.find(source.source_ref.id)&&position(common,source.source_ref)>=stop)
  fail("GovernanceServiceSourceAfterStartCut",source.source_ref.id);
}
SupervisionScopeView assemble(const GovernanceSupervisionScopeContext& x,const GovernanceAcceptedWorkHistory& actual,const StableId& actor,
 GovernanceSupervisionTargetView target,GovernanceServiceContinuityView continuity,const SourceRef* cut) {
 const auto& c=x.history.programs.common;
 SupervisionScopeView out;out.actor_id=actor;
 out.original_target_anchor=std::move(target.original_target_anchor);
 append(out.actual_sources,{governance_program_source(*actual.approval),1});
 append(out.actual_sources,{{"tasks","work_task_offered",actual.task_offer->fact_id},1});
 append(out.actual_sources,{{"tasks","work_task_accepted",actual.task_acceptance->fact_id},1});
 append(out.actual_sources,{{"tasks","work_accepted",actual.work_acceptance->fact_id},1});
 append(out.actual_sources,{{"politics","governance_execution_bound",actual.acceptance_binding->id},1});
 originals(out.actual_sources,target.actual_sources);originals(out.actual_sources,continuity.actual_sources);
 before_cut(c,out.actual_sources,cut);
 // 同一目标实际暂停优先于机构暂停仅作确定投影；两份原证据均保留并已被各owner验证。
 out.actual_blocking_source=target.actual_blocking_source?target.actual_blocking_source:continuity.actual_blocking_source;
 for(const auto* blocked:{&target.actual_blocking_source,&continuity.actual_blocking_source})if(*blocked) {
  position(c,(**blocked).source_ref);bool covered=false;
  for(const auto& source:out.actual_sources)if(same_source_ref(source.source_ref,(**blocked).source_ref)&&source.source_schema==(**blocked).source_schema)covered=true;
  if(!covered)fail("GovernanceServiceBlockingSourceNotCovered",(**blocked).source_ref.id);
 }
 return out;
}
}
Outcome<SupervisionScopeView> NativeGovernanceSupervisionScopeRead::read_start_scope(
 const WorkEpisode& e,const WorldMoment& at,const SourceRef* cut)const {
 return guard<SupervisionScopeView>([&]{
  auto actual=take(read_governance_supervision_start_history(context_.history,e,at,cut));
  GovernanceSupervisionTargetRequest request{*actual.approval,*actual.specification,*actual.supervision};
  auto target=take(context_.targets.read_supervision_target_at_start(request,at,cut));
  auto continuity=take(context_.continuity.read_accepted_supervision_continuity_at(actual,e,at,cut));
  take(read_governance_executor_qualification(context_.history.programs.catalog,*actual.specification,actual.supervision->required_skill_ref));
  const auto* actor=take(read_governance_supervision_executor_at(context_.history,e,at,cut));if(!actor)fail("GovernanceExecutorActualActorRequired",e.work_id);return assemble(context_,actual,*actor,std::move(target),std::move(continuity),cut);
 });
}
Outcome<SupervisionScopeView> NativeGovernanceSupervisionScopeRead::read_service_scope(
 const WorkEpisode& e,const SupervisionActionStartSource& start,const MomentSpan& span)const {
 return guard<SupervisionScopeView>([&]{
  take(validate_governance_supervision_service_window(context_.history,e,start,span));
  const SourceRef cut{"tasks","supervision_action_started",start.fact_id};
  auto actual=take(read_governance_supervision_start_history(context_.history,e,start.started_at,&cut));
  GovernanceSupervisionTargetRequest request{*actual.approval,*actual.specification,*actual.supervision};
  auto target=take(context_.targets.read_supervision_target_for_span(request,start,span));
  auto continuity=take(context_.continuity.read_accepted_supervision_continuity_span(actual,e,start,span));
  take(read_governance_executor_qualification(context_.history.programs.catalog,*actual.specification,actual.supervision->required_skill_ref));
  // 服务历史允许引用start之后、实际供给窗内的原变更；不能套开工cut丢掉它们。
  const auto* actor=take(read_governance_supervision_executor_at(context_.history,e,start.started_at,&cut));if(!actor||*actor!=start.actor_id)fail("GovernanceExecutorActualActorRequired",e.work_id);return assemble(context_,actual,*actor,std::move(target),std::move(continuity),nullptr);
 });
}
Status NativeGovernanceSupervisionScopeRead::validate_actual_release(const WorkEpisode& e,
 const SupervisionActionStartSource& start,SupervisionReleaseKind kind,
 const MomentOwnerSource& cause,const WorldMoment& at,const WorkOriginalRead& original)const {
 return guard<Done>([&]{
  const auto& c=context_.history.programs.common;const SourceRef cut{"tasks","supervision_action_started",start.fact_id};
  auto actual=take(read_governance_supervision_start_history(context_.history,e,start.started_at,&cut));
  if(!context_.history.tasks.work_execution||context_.history.tasks.work_execution->supervision_starts.find(start.fact_id)!=&start||
     cmp(at,start.started_at)<0||cmp(at,take(context_.history.programs.clock.read_organization_authority_now()))>0||
     cause.source_schema<=0||!valid_source_ref(cause.source_ref)||
     (cause.source_ref.domain=="tasks"&&supervision_source_kind(cause.source_ref.kind)))
   fail("GovernanceSupervisionActualReleaseSourceRequired",start.fact_id);
  const bool body_source=cause.source_ref.domain=="actor_foundation"&&cause.source_ref.kind=="condition_receipt"&&cause.source_schema==2;
  // ConditionReceipt2没有Common头。其真实actor/M/revision及Started前态由原K3链核；
  // 只有该具名身体分支可走typed因果，普通事实仍严格反查同M的Common先后。
  if(body_source){if(kind!=SupervisionReleaseKind::actual_interruption)fail("GovernanceConditionCannotCompleteWork",cause.source_ref.id);}
  else if(position(c,cause.source_ref)<=position(c,cut))fail("GovernanceSupervisionActualReleaseSourceRequired",start.fact_id);
  if(kind==SupervisionReleaseKind::actual_interruption) {
   take(context_.continuity.validate_actual_supervision_interruption(actual,e,start,cause,at));return Done{};
  }
  if(kind!=SupervisionReleaseKind::work_completed&&kind!=SupervisionReleaseKind::actual_work_stop)
   fail("UnknownGovernanceSupervisionReleaseKind",start.fact_id);
  // 原S3窄execution投影不反查terminal原因；read_result因此可读原Work而不自递归。
  // 不接受口头别名work_stopped；停止和完成均由同一真实supervised_work_result表承载。
  if(cause.source_schema!=1||cause.source_ref.domain!="tasks"||cause.source_ref.kind!="supervised_work_result")
   fail("GovernanceSupervisionWorkResultRequired",cause.source_ref.id);
  WorkExecutionRead work_results(c,context_.history.tasks,original);
  const auto* result=take(work_results.read_result(cause.source_ref,cause.source_schema));
  if(!result||result->work_id!=e.work_id||result->task_id!=e.authorization.accepted_task_id||
     e.terminal_result_ref!=result->fact_id||cmp(result->finished_at,at)||result->kind!=actual.supervision->kind||
     !same_source_ref(result->authorization.program_ref,e.authorization.program_ref)||
     result->authorization.target_line_id!=e.authorization.target_line_id||
     !same_terms_revision(result->authorization.terms_revision,e.authorization.terms_revision))
   fail("GovernanceSupervisionReleaseWorkMismatch",cause.source_ref.id);
  if(kind==SupervisionReleaseKind::work_completed) {
   if(result->disposition!=WorkResultDisposition::completed||result->stop_reason||result->actual_stop_source)
    fail("GovernanceSupervisionNotCompleted",result->fact_id);
  }else {
   if(result->disposition==WorkResultDisposition::completed||!result->stop_reason||!result->actual_stop_source||
      same_source_ref(result->actual_stop_source->source_ref,cause.source_ref))
    fail("GovernanceSupervisionStopCauseRequired",result->fact_id);
   // read_result已逐项反读真实S6/身体/执行停止源；窗耗尽本身不能完成此分支。
   if(position(c,result->actual_stop_source->source_ref)>=position(c,cause.source_ref))
    fail("GovernanceSupervisionStopCauseAfterResult",result->fact_id);
  }
  return Done{};
 });
}
}
