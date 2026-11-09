#include "work_exact_runtime.hpp"
#include "work_reassignment_sources.hpp"
#include "resident/domains/politics/governance_targets/governance_supervision_approvals.hpp"
#include <limits>
#include "resident/kernel/common_fact_prefix.hpp"
#include "work_execution_detail.hpp"
#include "resident/kernel/world_bundle.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]]void reject(const char* code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
void take(Status v){if(!v.ok)throw Failure{std::move(v.error)};}
template<class T,class Fn>Outcome<T> guard(Fn fn){try{return {true,fn(),{}};}catch(Failure& e){return {false,{},std::move(e.error)};}}
int cmp(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
SourceRef start_ref(const StableId&id){return {"tasks","supervision_action_started",id};}
bool has_terminal(const TaskWorkExecutionState&s,const StableId&start){for(const auto&r:s.supervision_terminals.rows())if(r.second.start_source_ref==start)return true;return false;}
bool has_closure(const TaskWorkExecutionState&s,const StableId&start){for(const auto&r:s.supervision_window_closures.rows())if(r.second.start_source_ref==start)return true;return false;}
void binding(const WorldBundle&w,const SupervisionSourceContext&x){if(&w.common!=&x.common||&w.tasks!=&x.tasks||!w.tasks.work_execution||!task_work_fixed_execution_terms_version(w.tasks.work_execution->version)||!w.action_elapsed||!w.world_timeline||&x.elapsed!=&*w.action_elapsed||&x.scale!=&w.world_timeline->scale_ref)reject("TaskWorkWorldBindingMismatch");}
std::shared_ptr<const TaskWorkReadLease> read(const WorldBundle&w,const WorldMoment&at,TaskWorkWorldFactory f){
 if(!f.read||!f.write)reject("TaskWorkWorldFactoryRequired");auto out=take(f.read(w,at));if(!out)reject("TaskWorkWorldReadLeaseRequired");binding(w,out->bindings().execution);return out;
}
std::shared_ptr<TaskWorkWriteLease> write(WorldBundle&w,const WorldMoment&at,TaskWorkWorldFactory f){
 if(!f.read||!f.write)reject("TaskWorkWorldFactoryRequired");auto out=take(f.write(w,at));if(!out)reject("TaskWorkWorldWriteLeaseRequired");binding(w,out->bindings().execution);return out;
}
// /6 的数据根由同一个原 Work lease 保活，不能从 World 的相似字段猜治理根。
const WorkReassignmentSourceContext& lifecycle(const WorldBundle& w,const SupervisionSourceContext& x){
 if(!w.tasks.work_execution||!task_work_lifecycle_sources_version(w.tasks.work_execution->version)||!x.reassignments||&x.reassignments->tasks.common!=&w.common||&x.reassignments->tasks.tasks!=&w.tasks||&x.reassignments->governance.history.task_definitions!=&x.definitions||&x.reassignments->governance.elapsed!=&x.elapsed)reject("WorkLifecycleOriginalRootsRequired");
 take(validate_governance_work_lifecycle_bindings(x.reassignments->governance,x.reassignments->original_sources_stamp));return *x.reassignments;
}
const PatrolWorkSources& patrol(const WorldBundle&w,const PatrolWorkSources* x){if(!x||&x->common!=&w.common||&x->tasks!=&w.tasks)reject("PatrolWorkOriginalSourcesRequired");return *x;}
bool consumed_duty(const TaskWorkExecutionState&s,const WorkEpisode&e,const StableId&id){bool found=false;for(const auto&row:s.period_receipts.rows())if(const auto*p=std::get_if<WorkMilitaryPeriodSource>(&row.second.source);p&&p->fact_ref.domain=="military"&&p->fact_ref.kind=="military_duty_period"&&p->fact_ref.id==id){if(found||row.second.work_id!=e.work_id||p->source_schema!=2)reject("PatrolDutyPeriodConsumedTwice",id);found=true;}return found;}
std::size_t fact_order(const CommonStore&c,const StableId&id){const auto*head=c.state().facts.find(id);if(!head)reject("PatrolSourceHeaderMissing",id);return take(read_actual_common_fact_position(c,*head));}
bool after_work_result(const CommonStore&c,const TaskWorkExecutionState&s,const WorkEpisode&e,const MilitaryDutyPeriodFact2&p){if(!e.terminal_result_ref)return false;const auto*r=s.results.find(*e.terminal_result_ref);if(!r)reject("PatrolWorkResultMissing",e.work_id);return cmp(p.actual_until,r->finished_at)>0||(cmp(p.actual_until,r->finished_at)==0&&fact_order(c,p.fact_id)>fact_order(c,r->fact_id));}
struct WorkDue {MomentSpan scheduled;WorldMoment at;Integer consumed=0;};
WorkDue next(const TaskWorkExecutionState&s,const WorkEpisode&e,const SupervisionActionStartSource&start,const SupervisionSourceContext&x){
 WorldMoment from=start.started_at;Integer consumed=0;const WorkExecutionSource action=WorkCommonActionBinding{start.action_id};
 for(const auto&id:e.accepted_period_refs){const auto*p=s.period_receipts.find(id);if(!p)reject("SupervisionDuePeriodMissing",id);if(!work_detail::same(p->execution_source,action))continue;if(cmp(p->window.from,from)||cmp(p->actual_until,from)<=0)reject("SupervisionDueCoverageMismatch",id);from=p->actual_until;++consumed;}
 const auto*entry=find_fixed_elapsed(x.elapsed,start.action_id);const auto*running=entry?std::get_if<ElapsedRunning>(&entry->timing):nullptr;if(!running)reject("SupervisionFixedRunningEntryRequired",start.action_id);const auto until=running->completion_due_at;if(cmp(until,take(read_supervision_execution_until(x,start))))reject("SupervisionFixedDueTermsMismatch",start.action_id);if(cmp(from,until)>=0)reject("SupervisionWindowClosureDue",start.action_id);
 auto end=take(add_world_duration(from,HalfDayDuration{take(normalize_fraction("1","1"))}));return {{from,end},cmp(end,until)>0?until:end,consumed};
}
const WorkEpisode* active(const WorldBundle&w,const SupervisionActionStartSource&start){
 const auto&s=*w.tasks.work_execution;if(has_terminal(s,start.fact_id)||has_closure(s,start.fact_id))return nullptr;
 const auto*e=s.episodes.find(start.work_id);if(!e||e->phase!=WorkPhase::active||e->terminal_result_ref||!e->current_execution||!work_detail::same(*e->current_execution,WorkExecutionSource{WorkCommonActionBinding{start.action_id}}))reject("SupervisionActiveExecutionMismatch",start.fact_id);return e;
}
}
Status validate_supervision_world_sources(const WorldBundle&w,const WorldMoment&at,TaskWorkWorldFactory f){return guard<Done>([&]{
 auto lease=read(w,at,f);auto x=lease->bindings();if(cmp(take(x.execution.clock.read_current_moment()),at))reject("TaskWorkValidationMomentMismatch");
 if(!x.governance||!w.governance_programs||!w.catalog||!w.catalog->governance_program_catalog||
  !w.organization_runtime||!w.catalog->organization||
  &x.governance->executions!=&x.execution||&x.governance->history.programs.common!=&w.common||
  &x.governance->history.tasks!=&w.tasks||&x.governance->history.task_definitions!=&x.execution.definitions||
  &x.governance->history.programs.organization!=&w.organization||
  &x.governance->history.programs.organization_runtime!=&*w.organization_runtime||
  &x.governance->history.programs.organization_catalog!=w.catalog->organization.get()||
  &x.governance->history.programs.state!=&*w.governance_programs||
  &x.governance->history.programs.catalog!=w.catalog->governance_program_catalog.get())
  reject("TaskWorkGovernanceValidationRootsRequired");
 take(validate_governance_programs(x.governance->history.programs,x.governance));
 take(validate_task_work_execution(w.common,w.tasks,x.execution.original,at));take(validate_supervision_sources(x.execution));const auto& original=patrol(w,x.patrol);
 for(const auto&row:original.duties.period_facts.rows()){const auto*e=take(read_work_assigned_to_patrol_duty(original,row.second.window.duty_action_id));if(!e||after_work_result(w.common,*w.tasks.work_execution,*e,row.second))continue;take(read_patrol_work_period(original,*e,{{"military","military_duty_period",row.first},2}));if(!consumed_duty(*w.tasks.work_execution,*e,row.first))reject("PatrolDutyPeriodNotConsumed",row.first);}
 for(const auto&row:w.tasks.work_execution->episodes.rows()){const auto&e=row.second;if(e.terminal_result_ref||!std::holds_alternative<PatrolPlan>(e.frozen_plan))continue;const auto*arrival=take(read_patrol_completion_checkpoint(original,e,at));if(arrival){const auto&deadline=work_detail::deadline(e.frozen_plan);if(take(calendar_deadline_accepts(deadline.tick,deadline.inclusive,arrival->arrived_at)))reject("PatrolCheckpointCompletionUnconsumed",arrival->source_fact_id);}}return Done{};
});}
Outcome<std::vector<ExactEventPrediction>> predict_supervision_work(const WorldBundle&w,const WorldMoment&from,const WorldMoment&through,TaskWorkWorldFactory f){return guard<std::vector<ExactEventPrediction>>([&]{
 auto lease=read(w,take(w.actual_phase_moment()),f);auto x=lease->bindings();std::vector<ExactEventPrediction> out;
 for(const auto&row:w.tasks.work_execution->supervision_starts.rows()){
  const auto&s=*take(read_supervision_action_start(x.execution,row.first));const auto*e=active(w,s);if(!e)continue;auto due=next(*w.tasks.work_execution,*e,s,x.execution);
  if(cmp(due.at,from)<0)reject("SupervisionWorkDueUnconsumed",s.fact_id);
  if(cmp(due.at,through)<=0)out.push_back({due.at,ExactStage::work_inspection,start_ref(s.fact_id),1,due.consumed});
 }
 const auto& original=patrol(w,x.patrol);for(const auto&row:original.duties.period_facts.rows()){const auto&p=row.second;const auto*e=take(read_work_assigned_to_patrol_duty(original,p.window.duty_action_id));if(!e||after_work_result(w.common,*w.tasks.work_execution,*e,p)||consumed_duty(*w.tasks.work_execution,*e,row.first))continue;
  take(read_patrol_work_period(original,*e,{{"military","military_duty_period",row.first},2}));if(cmp(p.actual_until,from)<0)reject("PatrolWorkDueUnconsumed",row.first);if(cmp(p.actual_until,through)<=0)out.push_back({p.actual_until,ExactStage::work_inspection,{"military","military_duty_period",row.first},2,static_cast<Integer>(e->accepted_period_refs.size())});
 }
 for(const auto&row:w.tasks.work_execution->episodes.rows()){const auto&e=row.second;if(e.terminal_result_ref||!std::holds_alternative<PatrolPlan>(e.frozen_plan))continue;const auto*arrival=take(read_patrol_completion_checkpoint(original,e,take(x.execution.clock.read_current_moment())));if(!arrival)continue;const auto&deadline=work_detail::deadline(e.frozen_plan);if(!take(calendar_deadline_accepts(deadline.tick,deadline.inclusive,arrival->arrived_at)))continue;if(cmp(arrival->arrived_at,from)<0)reject("PatrolCheckpointCompletionUnconsumed",arrival->source_fact_id);out.push_back({arrival->arrived_at,ExactStage::work_inspection,{"travel","patrol_checkpoint_arrival",arrival->source_fact_id},2,static_cast<Integer>(e.accepted_period_refs.size())});}
 // 每个前驱之后 phase graph 会重新预测；已完成的最后合法窗口不再产生截止事件。
 if(task_work_lifecycle_sources_version(w.tasks.work_execution->version)){
  (void)lifecycle(w,x.execution);
  for(const auto& row:w.tasks.work_execution->episodes.rows()){
   const auto& e=row.second;const auto* plan=std::get_if<SupervisedWorkPlan>(&e.frozen_plan);if(!plan||e.terminal_result_ref)continue;
   const auto accepted=take(x.execution.definitions.read_accepted_work(e.authorization));
   if(!work_detail::same(accepted.frozen_request.authorization,e.authorization))reject("WorkDeadlineAcceptedWorkMismatch",e.work_id);
   const WorldMoment at{plan->completion_deadline.tick,{}};
   if(cmp(at,from)<0)reject("WorkDeadlineDueUnconsumed",e.work_id);
   if(cmp(at,through)<=0){if(e.accepted_period_refs.size()>static_cast<std::uint64_t>(std::numeric_limits<Integer>::max()))reject("WorkDeadlinePeriodCountTooLarge",e.work_id);out.push_back({at,ExactStage::political_due_close,{"tasks","work_accepted",e.acceptance_source_ref},1,static_cast<Integer>(e.accepted_period_refs.size())});}
  }
 }
 return out;
});}
Status provide_supervision_action_interval(WorldBundle&w,const MomentSpan&span,TaskWorkWorldFactory f){return guard<Done>([&]{
 auto lease=write(w,span.to,f);auto x=lease->bindings();if(cmp(span.from,span.to)>=0||cmp(take(x.execution.clock.read_current_moment()),span.to))reject("SupervisionIntervalMomentMismatch");
 // 唯一 F 服务只消费本人原资格/位置/权源的真实正片；经济产量不进入 elapsed。
 for(const auto&row:w.tasks.work_execution->supervision_starts.rows()){
  const auto&s=*take(read_supervision_action_start(x.execution,row.first));const auto*episode=active(w,s);if(!episode)continue;
  auto*entry=find_fixed_elapsed(*w.action_elapsed,s.action_id);const auto*a=w.common.action(s.action_id);if(!entry||!a)reject("SupervisionFixedIntervalOwnerMissing",s.fact_id);
  take(x.execution.eligibility.validate_actual_service_span(*episode,s,span));auto part=take(prepare_fixed_elapsed_slice(*entry,span,x.execution.scale));
  entry->credited_units=std::move(part.credited_units);entry->last_settled_at=std::move(part.last_settled_at);
  ActionMutation m;m.action=*a;m.remaining_ticks=take(fixed_elapsed_remaining_ticks(*entry,x.execution.scale));take(w.common.apply_action(m));
 }return Done{};
});}
Outcome<ExactEventApplied> apply_supervision_work_due(WorldBundle&w,const ExactEventPrediction&p,TaskWorkWorldFactory f,ExactPhaseWriteContext& phase){return guard<ExactEventApplied>([&]{
 if(p.stage!=ExactStage::work_inspection&&p.stage!=ExactStage::political_due_close)reject("TaskWorkInspectionStageRequired");
 auto lease=write(w,p.at,f);auto x=lease->bindings();if(cmp(take(x.execution.clock.read_current_moment()),p.at))reject("SupervisionDueMomentMismatch");
 if(p.stage==ExactStage::political_due_close){
  const auto& roots=lifecycle(w,x.execution);
  if(!x.lifecycle_programs||x.lifecycle_programs!=&roots.governance.history.programs.state||p.actual_source_basis.domain!="tasks"||p.actual_source_basis.kind!="work_accepted"||p.source_schema!=1)reject("WorkDeadlinePredictionSourceMismatch",p.actual_source_basis.id);
  const auto& state=*w.tasks.work_execution;const auto* acceptance=state.acceptances.find(p.actual_source_basis.id);const auto* e=acceptance?state.episodes.find(acceptance->work_id):nullptr;
  const auto* plan=e?std::get_if<SupervisedWorkPlan>(&e->frozen_plan):nullptr;
  if(!e||!plan||e->acceptance_source_ref!=p.actual_source_basis.id||e->terminal_result_ref||cmp(p.at,{plan->completion_deadline.tick,{}})||p.dependency_revision!=static_cast<Integer>(e->accepted_period_refs.size()))reject("WorkDeadlinePredictionStale",p.actual_source_basis.id);
  // 原完整 Work 读口先验真实接受和所有已消费劳动，不按 phase 的字符串推未完成。
  WorkExecutionRead original(w.common,w.tasks,x.execution.original);if(take(original.read_work(e->work_id))!=e)reject("WorkDeadlineOriginalWorkRequired",e->work_id);
  const StableId work=e->work_id;const SupervisionActionStartSource* current=nullptr;
  for(const auto& row:state.supervision_starts.rows())if(row.second.work_id==work&&!has_terminal(state,row.first)&&!has_closure(state,row.first)){if(current)reject("WorkDeadlineMultipleActiveExecutions",work);current=take(read_supervision_action_start(x.execution,row.first));if(active(w,*current)!=e)reject("WorkDeadlineActiveExecutionMismatch",work);}
  if(current){
   const auto due=next(state,*e,*current,x.execution);const auto* elapsed=find_fixed_elapsed(x.execution.elapsed,current->action_id);
   if(!elapsed||cmp(elapsed->last_settled_at,p.at)||cmp(due.scheduled.from,p.at)>0||cmp(p.at,due.at)>0)reject("WorkDeadlineOriginalPeriodNotReady",work);
   // 截止可落在原窗口内：只关闭原 S5 已真实提供的尾片，先产生/消费 period，后作决定。
   if(cmp(due.scheduled.from,p.at)<0)take(consume_supervised_economic_window_in_candidate(w.common,w.tasks,x.native,x.execution,x.economic,x.production,x.expenses,work,due.scheduled));
  }
  const auto* updated=state.episodes.find(work);if(!updated)reject("WorkDeadlineWorkMissing",work);
  if(take(work_detail::credited(state,*updated))>=work_detail::required(updated->frozen_plan)){
   const auto completion=take(x.execution.original.read_completion(*updated,updated->accepted_period_refs,p.at));const auto& deadline=work_detail::deadline(updated->frozen_plan);
   if(completion.route_or_stage_complete&&take(calendar_deadline_accepts(deadline.tick,deadline.inclusive,p.at))){
    const auto result=take(complete_work_if_ready_in_candidate(w.common,w.tasks,x.execution.original,x.expenses,work,p.at,x.completed_results));const SourceRef source{"tasks","supervised_work_result",result};
    if(current){take(release_supervision_action_in_candidate(w.common,w.tasks,*w.action_elapsed,x.execution,current->action_id,SupervisionReleaseKind::work_completed,{source,1}));ActionCompletion completed;completed.completed=true;completed.success=true;if(!phase.advance.completed_actions.insert(current->action_id,std::move(completed)))reject("DuplicateSupervisionActionCompletion",current->action_id);}
    return ExactEventApplied{source,1};
   }
   // 目标已得足额劳动而仍未结项，须原可冷读目标证据，不能把未见结果当失败。
   reject("GovernanceWorkDeadlineCompletionNotSettled",work);
  }
  const auto decision=take(record_governance_work_deadline_termination_in_candidate(w.common,*x.lifecycle_programs,roots.governance,work,roots.original_sources_stamp));
  if(current){
   take(release_supervision_action_in_candidate(w.common,w.tasks,*w.action_elapsed,x.execution,current->action_id,SupervisionReleaseKind::actual_interruption,{decision,1}));
   if(std::find(phase.advance.suspended_actors.begin(),phase.advance.suspended_actors.end(),current->actor_id)==phase.advance.suspended_actors.end())phase.advance.suspended_actors.push_back(current->actor_id);
  }
  // 新决定并不自称已安全释放；原 lifecycle 必须反查真实 F terminal/binding 后才能停止 Work。
  const auto result=take(stop_work_in_candidate(w.common,w.tasks,x.execution.original,x.expenses,work,{decision,1},p.at));return ExactEventApplied{{"tasks","supervised_work_result",result},1};
 }
 if(p.actual_source_basis.domain=="travel"&&p.actual_source_basis.kind=="patrol_checkpoint_arrival"&&p.source_schema==2){const auto&original=patrol(w,x.patrol);const WorkEpisode*matched=nullptr;
  for(const auto&row:w.tasks.work_execution->episodes.rows()){const auto&e=row.second;if(e.terminal_result_ref||!std::holds_alternative<PatrolPlan>(e.frozen_plan))continue;const auto*arrival=take(read_patrol_completion_checkpoint(original,e,p.at));if(!arrival||arrival->source_fact_id!=p.actual_source_basis.id)continue;if(matched)reject("PatrolCheckpointMultipleWorkOwners",arrival->source_fact_id);if(cmp(arrival->arrived_at,p.at)||p.dependency_revision!=static_cast<Integer>(e.accepted_period_refs.size()))reject("PatrolCheckpointPredictionStale",arrival->source_fact_id);matched=&e;}
  if(!matched)reject("PatrolCheckpointWorkNotReady",p.actual_source_basis.id);const auto result=take(complete_work_if_ready_in_candidate(w.common,w.tasks,x.execution.original,x.expenses,matched->work_id,p.at,x.completed_results));return ExactEventApplied{{"tasks","patrol_work_result",result},1};
 }
 if(p.actual_source_basis.domain=="military"&&p.actual_source_basis.kind=="military_duty_period"&&p.source_schema==2){const auto&original=patrol(w,x.patrol);const auto*period=take(read_military_duty_period(w.common,original.duties,original.duty_context,p.actual_source_basis,2));const auto*e=take(read_work_assigned_to_patrol_duty(original,period->window.duty_action_id));if(!e||e->terminal_result_ref||consumed_duty(*w.tasks.work_execution,*e,period->fact_id)||cmp(period->actual_until,p.at)||p.dependency_revision!=static_cast<Integer>(e->accepted_period_refs.size()))reject("PatrolWorkPredictionStale",period->fact_id);const auto work=e->work_id;
  auto receipt=take(consume_work_period_in_candidate(w.common,w.tasks,x.execution.original,x.expenses,work,WorkMilitaryPeriodSource{p.actual_source_basis,2},p.at));const auto*current=w.tasks.work_execution->episodes.find(work);if(!current)reject("PatrolWorkLostAfterPeriod",work);if(take(work_detail::credited(*w.tasks.work_execution,*current))>=work_detail::required(current->frozen_plan)){const auto completion=take(x.execution.original.read_completion(*current,current->accepted_period_refs,p.at));const auto&deadline=work_detail::deadline(current->frozen_plan);if(completion.route_or_stage_complete&&take(calendar_deadline_accepts(deadline.tick,deadline.inclusive,p.at))){auto result=take(complete_work_if_ready_in_candidate(w.common,w.tasks,x.execution.original,x.expenses,work,p.at,x.completed_results));return ExactEventApplied{{"tasks","patrol_work_result",result},1};}}
  // Work completion does not cancel a military order, release the Army, or
  // counterfeit its original movement/Station terminal.
  return ExactEventApplied{{"tasks","work_period_receipt",receipt},2};
 }
 if(p.actual_source_basis.domain!="tasks"||p.actual_source_basis.kind!="supervision_action_started"||p.source_schema!=1)reject("SupervisionWorkPredictionSourceMismatch",p.actual_source_basis.id);
 const auto*s=take(read_supervision_action_start(x.execution,p.actual_source_basis.id));const auto*e=active(w,*s);if(!e)reject("SupervisionWorkAlreadyConsumed",s->fact_id);const auto due=next(*w.tasks.work_execution,*e,*s,x.execution);
 if(cmp(due.at,p.at)||due.consumed!=p.dependency_revision)reject("SupervisionWorkPredictionStale",s->fact_id);
 const StableId work=e->work_id,action=s->action_id;const auto until=take(read_supervision_execution_until(x.execution,*s));
 const auto receipt=take(consume_supervised_economic_window_in_candidate(w.common,w.tasks,x.native,x.execution,x.economic,x.production,x.expenses,work,due.scheduled));
 auto*current=w.tasks.work_execution->episodes.find(work);if(!current)reject("SupervisionWorkLostAfterPeriod",work);
 const auto count=take(work_detail::credited(*w.tasks.work_execution,*current));
 if(count>=work_detail::required(current->frozen_plan)){
  const auto completion=take(x.execution.original.read_completion(*current,current->accepted_period_refs,p.at));
  const auto&deadline=work_detail::deadline(current->frozen_plan);const auto timely=take(calendar_deadline_accepts(deadline.tick,deadline.inclusive,p.at));
  if(completion.route_or_stage_complete&&timely){
   const auto result=take(complete_work_if_ready_in_candidate(w.common,w.tasks,x.execution.original,x.expenses,work,p.at,x.completed_results));
   const SourceRef source{"tasks","supervised_work_result",result};
   take(release_supervision_action_in_candidate(w.common,w.tasks,*w.action_elapsed,x.execution,action,SupervisionReleaseKind::work_completed,{source,1}));
   ActionCompletion completed;completed.completed=true;completed.success=true;if(!phase.advance.completed_actions.insert(action,std::move(completed)))reject("DuplicateSupervisionActionCompletion",action);
   return ExactEventApplied{source,1};
  }
 }
 if(cmp(p.at,until)==0){auto closed=take(close_supervision_window_in_candidate(w.common,w.tasks,*w.action_elapsed,x.execution,action));ActionCompletion completed;completed.completed=true;completed.success=true;if(!phase.advance.completed_actions.insert(action,std::move(completed)))reject("DuplicateSupervisionActionCompletion",action);if(std::find(phase.advance.suspended_actors.begin(),phase.advance.suspended_actors.end(),s->actor_id)==phase.advance.suspended_actors.end())phase.advance.suspended_actors.push_back(s->actor_id);return ExactEventApplied{closed,1};}
 return ExactEventApplied{{"tasks","work_period_receipt",receipt},2};
});}
Outcome<std::optional<SourceRef>> consume_supervision_interruption_in_candidate(WorldBundle&w,const StableId&actor,const MomentOwnerSource&cause,TaskWorkWorldFactory f,ExactPhaseWriteContext&phase){return guard<std::optional<SourceRef>>([&]{
 const auto at=take(w.actual_phase_moment());auto lease=write(w,at,f);auto x=lease->bindings();if(cmp(take(x.execution.clock.read_current_moment()),at)||actor.empty()||!work_detail::valid(cause.source_ref)||cause.source_schema<1)reject("SupervisionInterruptionActualSourceRequired",actor);
 const SupervisionActionStartSource*found=nullptr;const auto&state=*w.tasks.work_execution;
 const SupervisionActionTerminalSource*replay=nullptr;for(const auto&row:state.supervision_terminals.rows()){const auto&terminal=row.second;const auto*start=state.supervision_starts.find(terminal.start_source_ref);if(start&&start->actor_id==actor&&terminal.disposition==SupervisionReleaseKind::actual_interruption&&work_detail::same(terminal.actual_release_cause,cause)){if(replay||cmp(terminal.ended_at,at))reject("SupervisionInterruptionReplayMismatch",actor);replay=&terminal;}}
 if(replay){const SourceRef source{"tasks","supervision_action_ended",replay->fact_id};take(read_supervision_source_moment(x.execution,source,1));return std::optional<SourceRef>{source};}
 for(const auto&row:state.supervision_starts.rows())if(row.second.actor_id==actor&&!has_terminal(state,row.first)&&!has_closure(state,row.first)){if(found)reject("AmbiguousSupervisionActorExecution",actor);found=take(read_supervision_action_start(x.execution,row.first));}
 if(!found)return std::optional<SourceRef>{};const auto*s=found;const auto*e=state.episodes.find(s->work_id);if(!e)reject("SupervisionWorkMissing",s->work_id);
 // The original S6/body cause reader must establish actual interruption at M;
 // waiting, a future deadline, or an unrelated Condition source cannot stop it.
 take(x.execution.eligibility.validate_actual_release(*e,*s,SupervisionReleaseKind::actual_interruption,cause,at,x.execution.original));
 const StableId action=s->action_id,work=e->work_id;
 if(!has_closure(state,s->fact_id)){
  const auto due=next(state,*e,*s,x.execution);if(cmp(due.scheduled.from,at)>0||cmp(at,due.at)>0)reject("SupervisionInterruptionHasUnconsumedPriorWindow",action);
  if(cmp(due.scheduled.from,at)<0)take(consume_supervised_economic_window_in_candidate(w.common,w.tasks,x.native,x.execution,x.economic,x.production,x.expenses,work,due.scheduled));
 }
 const auto terminal=take(release_supervision_action_in_candidate(w.common,w.tasks,*w.action_elapsed,x.execution,action,SupervisionReleaseKind::actual_interruption,cause));
 if(std::find(phase.advance.suspended_actors.begin(),phase.advance.suspended_actors.end(),actor)==phase.advance.suspended_actors.end())phase.advance.suspended_actors.push_back(actor);
 return std::optional<SourceRef>{terminal};
});}
Status validate_supervision_applied_source(const WorldBundle&w,const ExactEventApplied&source,TaskWorkWorldFactory f){return guard<Done>([&]{
 auto lease=read(w,take(w.actual_phase_moment()),f);auto x=lease->bindings();
 if(supervision_source_kind(source.applied_result.kind))take(read_supervision_source_moment(x.execution,source.applied_result,source.source_schema));
 else take(read_work_source_moment(w.common,w.tasks,x.execution.original,source.applied_result,source.source_schema));
 take(validate_supervision_sources(x.execution));return Done{};
});}
}
