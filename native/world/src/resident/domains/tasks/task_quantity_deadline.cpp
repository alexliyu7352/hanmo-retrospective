// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
#include "task_quantity_deadline.hpp"
#include "task_model.hpp"
#include "task_exact_execution.hpp"
#include "resident/domains/social/social_period_sources.hpp"
#include "resident/kernel/exact_phase_graph.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {namespace {
template<class T>Outcome<T> bad(const char*code,const StableId&id={}){return {false,{},common_failure(code,id,ErrorKind::internal).error};}
bool same(const SourceRef&a,const SourceRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
Status header(const CommonStore&c,const SourceRef&r){const auto*h=c.state().facts.find(r.id);return h&&h->id==r.id&&h->domain==r.domain&&h->kind==r.kind?common_success():common_failure("QuantityDeadlineOriginalHeaderRequired",r.id,ErrorKind::internal);}
Outcome<bool> before(const CommonStore&c,const SourceRef&a,const SourceRef&b){auto x=read_actual_common_fact_position(c,a);if(!x.ok)return {false,{},x.error};auto y=read_actual_common_fact_position(c,b);if(!y.ok)return {false,{},y.error};if(x.value==y.value)return bad<bool>("QuantityDeadlineOriginalOrderMissing",b.id);return {true,x.value<y.value,{}};}

Outcome<const TaskExactStartSource*> accepted(const CommonStore&c,const TaskState&t,const ContractRecord&contract){
 const auto*task=contract.current_task_id?t.tasks.find(*contract.current_task_id):nullptr;
 if(!t.exact_quantities||!t.exact_execution||!t.claim_establishment||!task||task->record_kind!=TaskRecordKind::exact_quantity||!task->exact_quantity||!task->contract_id||*task->contract_id!=contract.id||!contract.executor_actor_id||*contract.executor_actor_id!=task->assignee)return bad<const TaskExactStartSource*>("QuantityDeadlineExactAcceptanceRequired",contract.id);
 const auto&q=*task->exact_quantity;const TaskExactStartSource*start=nullptr;
 for(const auto&row:t.exact_execution->starts.rows())if(row.second.action_id==q.original_acceptance_action_ref){if(start||row.first!=row.second.id)return bad<const TaskExactStartSource*>("QuantityDeadlineDuplicateAcceptance",contract.id);start=&row.second;}
 if(!start||start->action_kind!="accept_offer"||start->actor_id!=task->assignee||start->accepted_action.task_id!=task->id||start->accepted_action.offer_id!=task->offer_id||start->required_half_days!=contract.frozen_terms.definition.accept_ticks||start->required_half_days<=0||q.deadline_gate.tick!=contract.frozen_terms.deadline_tick||!q.deadline_gate.inclusive||task->deadline_tick!=q.deadline_gate.tick)return bad<const TaskExactStartSource*>("QuantityDeadlineAcceptedScopeMismatch",contract.id);
 const TaskClaimEstablishedSource*claim=nullptr;for(const auto&row:t.claim_establishment->claim_establishment_sources.rows())if(row.second.task_id==task->id){if(claim||row.first!=row.second.source_id)return bad<const TaskExactStartSource*>("QuantityDeadlineClaimCoverageMismatch",task->id);claim=&row.second;}
 if(!claim||claim->contract_id!=contract.id||claim->claim_id!=contract.source_binding.claim_id||claim->acceptance_action_id!=start->action_id)return bad<const TaskExactStartSource*>("QuantityDeadlineClaimSourceRequired",task->id);
 const SourceRef claim_ref{"tasks","task_claim_established",claim->source_id},start_ref{"tasks","task_action_started",start->id};std::size_t used=0;for(const auto&r:start->actual_start_sources)if(same(r,claim_ref))++used;if(used!=1)return bad<const TaskExactStartSource*>("QuantityDeadlineAcceptanceClaimMismatch",task->id);
 auto order=before(c,claim_ref,start_ref);if(!order.ok)return {false,{},order.error};auto at=compare_world_moments(claim->established_at,start->started_at);if(!at.ok)return {false,{},at.error};if(!order.value||at.value)return bad<const TaskExactStartSource*>("QuantityDeadlineAcceptanceOrderMismatch",task->id);
 return {true,start,{}};
}
// The writer cannot close over an executing Task Action. Reconstruct that same
// fact at the original close cut even after today's Action has disappeared.
// No elapsed amount, current task status or later terminal can stand in for it.
template<class Visit> Status executions_at_close(const CommonStore&c,const TaskState&t,
 const TaskQuantityDeadlineClosure&close,const TaskExactLifeSources*life,Visit&&visit){
 if(!t.exact_execution)return common_failure("QuantityDeadlineExactExecutionRequired",close.task_id,ErrorKind::unsupported_capability);
 const SourceRef close_ref{"tasks",task_quantity_deadline_closed_kind,close.source_id};
 for(const auto&row:t.exact_execution->starts.rows()){
  const auto&start=row.second;if(start.accepted_action.task_id!=close.task_id)continue;
  const MomentOwnerSource start_ref{{"tasks","task_action_started",row.first},1};
  auto started=read_task_exact_source_moment(c,t,start_ref.source_ref,1);if(!started.ok)return {false,{},started.error};
  auto order=before(c,start_ref.source_ref,close_ref);if(!order.ok)return {false,{},order.error};
  auto at=compare_world_moments(started.value,close.closed_at);if(!at.ok)return {false,{},at.error};
  if((at.value<0&&!order.value)||(at.value>0&&order.value))return common_failure("QuantityDeadlineExecutionCausalOrderMismatch",row.first,ErrorKind::internal);
  if(!order.value)continue; // Later settlement execution is checked by its owner.
  const TaskExactTerminalSource*terminal=nullptr;
  for(const auto&end:t.exact_execution->terminals.rows())if(end.second.action_id==start.action_id){
   if(terminal||end.first!=end.second.id)return common_failure("QuantityDeadlineExecutionTerminalAmbiguous",start.action_id,ErrorKind::internal);
   terminal=&end.second;
  }
  if(!terminal)return common_failure("QuantityDeadlineActiveActionDispositionRequired",start.action_id,ErrorKind::unsupported_capability);
  const MomentOwnerSource terminal_ref{{"tasks","task_action_finished",terminal->id},task_exact_terminal_source_schema(t.exact_execution->version)};
  auto finished=read_task_exact_source_moment(c,t,terminal_ref.source_ref,terminal_ref.source_schema,life);if(!finished.ok)return {false,{},finished.error};
  order=before(c,terminal_ref.source_ref,close_ref);if(!order.ok)return {false,{},order.error};
  at=compare_world_moments(finished.value,close.closed_at);if(!at.ok)return {false,{},at.error};
  if(!order.value||at.value>0)return common_failure("QuantityDeadlineActiveActionDispositionRequired",start.action_id,ErrorKind::unsupported_capability);
  auto used=visit(start_ref);if(!used.ok)return used;used=visit(terminal_ref);if(!used.ok)return used;
 }
 return common_success();
}
Outcome<const TaskQuantityDeadlineClosure*> local(const CommonStore&c,const TaskState&t,const ContractRecord&contract,const TaskExactLifeSources*life){
 if(t.dynamic_tasks_version!=4)return {false,{},common_failure("QuantityDeadlineDynamicTasks4Required",contract.id,ErrorKind::unsupported_capability).error};
 if(!contract.deadline_closure)return bad<const TaskQuantityDeadlineClosure*>("QuantityDeadlineClosureRequired",contract.id);const auto&v=*contract.deadline_closure;auto start=accepted(c,t,contract);if(!start.ok)return {false,{},start.error};
 if(v.source_id.empty()||v.task_id!=*contract.current_task_id||v.original_acceptance_start_ref.source_schema!=1||!same(v.original_acceptance_start_ref.source_ref,{"tasks","task_action_started",start.value->id})||v.frozen_deadline_gate.tick!=contract.frozen_terms.deadline_tick||!v.frozen_deadline_gate.inclusive||v.frozen_deadline_gate.tick<0||v.frozen_deadline_gate.tick==std::numeric_limits<Integer>::max()||v.original_period_opening.source_schema!=1||v.original_period_opening.source_ref.domain!="social"||v.original_period_opening.source_ref.kind!="social_period_open"||v.original_period_opening.source_ref.id.empty())return bad<const TaskQuantityDeadlineClosure*>("QuantityDeadlineClosureScopeMismatch",contract.id);
 auto at=compare_world_moments(v.closed_at,WorldMoment{v.frozen_deadline_gate.tick+1,{}});if(!at.ok)return {false,{},at.error};if(at.value)return bad<const TaskQuantityDeadlineClosure*>("QuantityDeadlineOriginalCalendarCutMismatch",v.source_id);
 auto order=before(c,v.original_acceptance_start_ref.source_ref,{"tasks",task_quantity_deadline_closed_kind,v.source_id});if(!order.ok)return {false,{},order.error};at=compare_world_moments(start.value->started_at,v.closed_at);if(!at.ok)return {false,{},at.error};if(!order.value||at.value>=0||!contract.execution_stopped||contract.stop_reason!="expired"||(contract.status!=ContractStatus::settling&&contract.status!=ContractStatus::settled))return bad<const TaskQuantityDeadlineClosure*>("QuantityDeadlineOriginalLifecycleMismatch",contract.id);
 auto executions=executions_at_close(c,t,v,life,[](const MomentOwnerSource&){return common_success();});if(!executions.ok)return {false,{},executions.error};
 return {true,&v,{}};
}
const ContractRecord* find(const TaskState&t,const SourceRef&r){const ContractRecord*out=nullptr;for(const auto&row:t.contracts.rows())if(row.second.deadline_closure&&row.second.deadline_closure->source_id==r.id){if(out||row.first!=row.second.id)return nullptr;out=&row.second;}return out;}
Status result_order(const CommonStore&c,const TaskQuantityDeadlineClosure&close,const SourceRef&result,const WorldMoment&at){auto order=before(c,result,{"tasks",task_quantity_deadline_closed_kind,close.source_id});if(!order.ok)return {false,{},order.error};auto time=compare_world_moments(at,close.closed_at);if(!time.ok)return {false,{},time.error};if((time.value<0&&!order.value)||(time.value>0&&order.value))return common_failure("QuantityDeadlineResultCausalOrderMismatch",result.id,ErrorKind::internal);return common_success();}
}
Outcome<WorldMoment> read_task_quantity_deadline_source_moment(const CommonStore&c,const TaskState&t,const SourceRef&r,Integer schema,const TaskExactLifeSources*life){if(schema!=1||r.domain!="tasks"||r.kind!=task_quantity_deadline_closed_kind)return bad<WorldMoment>("QuantityDeadlineSourceKindMismatch",r.id);const auto*contract=find(t,r);if(!contract)return bad<WorldMoment>("QuantityDeadlineSourceMissing",r.id);auto value=local(c,t,*contract,life);return value.ok?Outcome<WorldMoment>{true,value.value->closed_at,{}}:Outcome<WorldMoment>{false,{},value.error};}
Outcome<const TaskQuantityDeadlineClosure*> read_task_quantity_deadline_closure(const TaskQuantityDeadlineContext&x,const SourceRef&r,Integer schema){
 if(schema!=1||r.domain!="tasks"||r.kind!=task_quantity_deadline_closed_kind)return bad<const TaskQuantityDeadlineClosure*>("QuantityDeadlineSourceKindMismatch",r.id);const auto*contract=find(x.tasks,r);if(!contract)return bad<const TaskQuantityDeadlineClosure*>("QuantityDeadlineSourceMissing",r.id);auto value=local(x.common,x.tasks,*contract,x.life);if(!value.ok)return value;
 auto opening=read_social_period_opening_source(x.periods,value.value->original_period_opening,x.identity);if(!opening.ok)return {false,{},opening.error};if(opening.value->through_tick!=value.value->frozen_deadline_gate.tick+1)return bad<const TaskQuantityDeadlineClosure*>("QuantityDeadlinePeriodMismatch",r.id);
 auto now=x.clock.read_current_moment();if(!now.ok)return {false,{},now.error};auto at=compare_world_moments(value.value->closed_at,now.value);if(!at.ok)return {false,{},at.error};if(at.value>0)return bad<const TaskQuantityDeadlineClosure*>("QuantityDeadlineFutureClosure",r.id);return value;
}
Outcome<SourceRef> record_task_quantity_deadline_close_in_candidate(CommonStore&c,TaskState&t,const TaskQuantityDeadlineContext&x,const StableId&id,const ExactPeriodContext&p){
 if(&c!=&x.common||&t!=&x.tasks)return bad<SourceRef>("QuantityDeadlineCandidateMismatch",id);auto*contract=t.contracts.find(id);if(!contract)return bad<SourceRef>("QuantityDeadlineContractMissing",id);
 if(contract->deadline_closure){SourceRef ref{"tasks",task_quantity_deadline_closed_kind,contract->deadline_closure->source_id};auto prior=read_task_quantity_deadline_closure(x,ref,1);if(!prior.ok)return {false,{},prior.error};if(prior.value->original_period_opening.source_ref.id!=p.period_id)return bad<SourceRef>("QuantityDeadlineRetryPeriodMismatch",id);return {true,std::move(ref),{}};}
 if(t.dynamic_tasks_version!=4)return {false,{},common_failure("QuantityDeadlineDynamicTasks4Required",id,ErrorKind::unsupported_capability).error};auto start=accepted(c,t,*contract);if(!start.ok)return {false,{},start.error};const auto deadline=contract->frozen_terms.deadline_tick;
 if(contract->execution_stopped||contract->status==ContractStatus::settling||contract->status==ContractStatus::settled||deadline<0||deadline==std::numeric_limits<Integer>::max()||p.through_tick!=deadline+1||c.tick()!=deadline)return bad<SourceRef>("QuantityDeadlineActualCalendarCloseRequired",id);
 const MomentOwnerSource period{{"social","social_period_open",p.period_id},1};auto opening=read_social_period_opening_source(x.periods,period,x.identity);if(!opening.ok)return {false,{},opening.error};auto equal=compare_world_moments(opening.value->opened_at,p.opening_at);if(!equal.ok)return {false,{},equal.error};if(equal.value||opening.value->through_tick!=p.through_tick)return bad<SourceRef>("QuantityDeadlineActualPeriodMismatch",id);
 auto now=x.clock.read_current_moment();if(!now.ok)return {false,{},now.error};equal=compare_world_moments(now.value,WorldMoment{p.through_tick,{}});if(!equal.ok)return {false,{},equal.error};if(equal.value)return bad<SourceRef>("QuantityDeadlineActualMomentMismatch",id);
 for(const auto&row:t.actions.rows())if(row.second.task_id==*contract->current_task_id&&c.action(row.first))return {false,{},common_failure("QuantityDeadlineActiveActionDispositionRequired",row.first,ErrorKind::unsupported_capability).error};
 auto allocated=c.next_id("task_quantity_deadline_closed");if(!allocated.ok)return {false,{},allocated.error};TaskQuantityDeadlineClosure v;v.source_id=allocated.value;v.task_id=*contract->current_task_id;v.original_acceptance_start_ref={{"tasks","task_action_started",start.value->id},1};v.frozen_deadline_gate={deadline,true};v.original_period_opening=period;v.closed_at=now.value;
 auto written=c.append_fact({v.source_id,task_quantity_deadline_closed_kind,"tasks"});if(!written.ok)return {false,{},written.error};SourceRef ref{"tasks",task_quantity_deadline_closed_kind,v.source_id};contract->deadline_closure=std::move(v);return {true,std::move(ref),{}};
}
Outcome<bool> read_task_quantity_result_deadline_eligibility(const CommonStore&c,const TaskState&t,const StableId&task_id,const SourceRef&r,const TaskExactLifeSources*life){
 const auto*task=t.tasks.find(task_id);const auto*contract=task&&task->contract_id?t.contracts.find(*task->contract_id):nullptr;if(!task||!contract||task->record_kind!=TaskRecordKind::exact_quantity||!task->exact_quantity||r.domain!="tasks")return bad<bool>("QuantityDeadlineActualResultScopeRequired",task_id);auto start=accepted(c,t,*contract);if(!start.ok)return {false,{},start.error};auto h=header(c,r);if(!h.ok)return {false,{},h.error};const WorldMoment*at=nullptr;
 if(r.kind=="task_delivery"){const auto*v=t.deliveries.find(r.id);if(v&&v->id==r.id&&v->task_id==task_id&&v->quantity_source_kind==TaskQuantitySourceKind::procurement_exact&&v->completed_at)at=&*v->completed_at;}
 else if(r.kind=="task_transfer"){const auto*v=t.transfers.find(r.id);if(v&&v->id==r.id&&v->task_id==task_id&&v->contract_id==contract->id&&v->quantity_source_kind==TaskQuantitySourceKind::procurement_exact&&v->completed_at)at=&*v->completed_at;}
 else if(r.kind=="task_action_finished"&&t.exact_execution){const auto*v=t.exact_execution->terminals.find(r.id);const auto*s=v?t.exact_execution->starts.find(v->start_source_id):nullptr;if(v&&v->id==r.id&&s&&s->accepted_action.task_id==task_id&&s->action_kind=="report_offer"&&v->effect.success)at=&v->finished_at;}
 if(!at)return bad<bool>("QuantityDeadlineActualCompletedSourceRequired",r.id);
 if(contract->deadline_closure){auto close=local(c,t,*contract,life);if(!close.ok)return {false,{},close.error};auto ordered=result_order(c,*close.value,r,*at);if(!ordered.ok)return {false,{},ordered.error};auto position=before(c,r,{"tasks",task_quantity_deadline_closed_kind,close.value->source_id});if(!position.ok)return {false,{},position.error};if(!position.value&&r.kind=="task_delivery")return bad<bool>("QuantityDeliveryAfterOriginalDeadlineClose",r.id);}
 else if(contract->stop_reason=="expired")return {false,{},common_failure("QuantityDeadlineOriginalCloseRequired",contract->id,ErrorKind::unsupported_capability).error};
 auto gate=compare_world_moments(*at,WorldMoment{task->exact_quantity->deadline_gate.tick,{}});if(!gate.ok)return {false,{},gate.error};return {true,gate.value<=0,{}};
}
Status validate_task_quantity_deadline_sources(const TaskQuantityDeadlineContext&x){
 auto now=x.clock.read_current_moment();if(!now.ok)return {false,{},now.error};
 std::size_t count=0;for(const auto&row:x.tasks.contracts.rows()){const auto&contract=row.second;const auto*task=contract.current_task_id?x.tasks.tasks.find(*contract.current_task_id):nullptr;if(!task)return common_failure("QuantityDeadlineTaskMissing",row.first,ErrorKind::internal);
  if(!contract.deadline_closure){if(task->record_kind==TaskRecordKind::exact_quantity){if(contract.stop_reason=="expired")return common_failure("QuantityDeadlineOriginalCloseRequired",row.first,ErrorKind::unsupported_capability);if(!contract.execution_stopped&&contract.frozen_terms.deadline_tick<std::numeric_limits<Integer>::max()){const WorldMoment due{contract.frozen_terms.deadline_tick+1,{}};auto late=compare_world_moments(now.value,due);if(!late.ok)return {false,{},late.error};bool period_closed=false;if(!late.value)for(const auto&period:x.periods.openings.rows())if(period.second.through_tick==due.half_day_tick&&x.periods.closures.find(period.first)){period_closed=true;break;}if(late.value>0||period_closed)return common_failure("QuantityDeadlineOriginalCloseRequired",row.first,ErrorKind::unsupported_capability);}}continue;}
  ++count;if(row.first!=contract.id||task->record_kind!=TaskRecordKind::exact_quantity)return common_failure("QuantityDeadlineUnexpectedClosure",row.first,ErrorKind::internal);const SourceRef ref{"tasks",task_quantity_deadline_closed_kind,contract.deadline_closure->source_id};auto actual=read_task_quantity_deadline_closure(x,ref,1);if(!actual.ok)return {false,{},actual.error};
  for(const auto&delivery:x.tasks.deliveries.rows())if(delivery.second.task_id==task->id){auto gate=read_task_quantity_result_deadline_eligibility(x.common,x.tasks,task->id,{"tasks","task_delivery",delivery.first},x.life);if(!gate.ok)return {false,{},gate.error};}
  for(const auto&transfer:x.tasks.transfers.rows())if(transfer.second.task_id==task->id){auto gate=read_task_quantity_result_deadline_eligibility(x.common,x.tasks,task->id,{"tasks","task_transfer",transfer.first},x.life);if(!gate.ok)return {false,{},gate.error};}
 }
 std::size_t heads=0;for(const auto&row:x.common.state().facts.rows())if(row.second.domain=="tasks"&&row.second.kind==task_quantity_deadline_closed_kind){++heads;if(row.first!=row.second.id||!find(x.tasks,{row.second.domain,row.second.kind,row.first}))return common_failure("QuantityDeadlineOrphanHeader",row.first,ErrorKind::internal);}return heads==count?common_success():common_failure("QuantityDeadlineHeaderCoverageMismatch",{},ErrorKind::internal);
}
Status visit_task_quantity_deadline_source_pins(const TaskQuantityDeadlineContext&x,TaskQuantityDeadlinePinSink&sink){auto valid=validate_task_quantity_deadline_sources(x);if(!valid.ok)return valid;for(const auto&row:x.tasks.contracts.rows())if(row.second.deadline_closure){const auto&v=*row.second.deadline_closure;valid=sink.pin({{"tasks",task_quantity_deadline_closed_kind,v.source_id},1});if(!valid.ok)return valid;valid=sink.pin(v.original_period_opening);if(!valid.ok)return valid;valid=executions_at_close(x.common,x.tasks,v,x.life,[&](const MomentOwnerSource&source){return sink.pin(source);});if(!valid.ok)return valid;}return common_success();}
}
