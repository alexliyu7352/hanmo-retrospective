#include "work_patrol_sources.hpp"
#include "resident/domains/military/duty/military_duty_assignment.hpp"
#include "resident/domains/politics/governance_targets/governance_work_history.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "work_execution_detail.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void reject(const char*code,const StableId&id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
void take(Status v){if(!v.ok)throw Failure{std::move(v.error)};}
template<class T,class F>Outcome<T> guard(F fn){try{return {true,fn(),{}};}catch(Failure&e){return {false,{},std::move(e.error)};}}
int cmp(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
void append(std::vector<MomentOwnerSource>&out,const MomentOwnerSource&source){if(!work_detail::valid(source.source_ref)||source.source_schema<1)reject("PatrolOriginalSourceRequired");for(const auto&old:out)if(work_detail::same(old,source))return;out.push_back(source);}
void append(std::vector<MomentOwnerSource>&out,const std::vector<MomentOwnerSource>&sources){for(const auto&source:sources)append(out,source);}
std::size_t source_order(const CommonStore&c,const SourceRef&source){return take(read_actual_common_fact_position(c,source));}
const PatrolPlan& plan(const WorkEpisode&e){const auto*p=std::get_if<PatrolPlan>(&e.frozen_plan);if(!p)reject("PatrolWorkPlanRequired",e.work_id);return *p;}
void context(const PatrolWorkSources&x){if(!x.tasks.work_execution||!task_work_execution_version(x.tasks.work_execution->version)||!x.definitions.bound_to(x.common,x.tasks))reject("PatrolWorkSameCandidateRequired");if(cmp(take(x.clock.read_current_moment()),take(x.duty_context.stamps.read_current_moment())))reject("PatrolDutyReadMomentMismatch");}
const WorkAcceptanceSource& acceptance(const PatrolWorkSources&x,const WorkEpisode&e){
 context(x);const auto&state=*x.tasks.work_execution;const auto*a=state.acceptances.find(e.acceptance_source_ref);if(!a||a->work_id!=e.work_id||!work_detail::header(x.common,a->fact_id,"work_accepted")||!work_detail::same(a->authorization,e.authorization))reject("PatrolWorkAcceptanceRequired",e.work_id);
 const auto actual=take(x.definitions.read_accepted_work(e.authorization));if(!work_detail::same(actual.actual_task_acceptance,a->original_task_acceptance)||cmp(actual.accepted_at,a->accepted_at)||cmp(actual.earliest_service_at,a->earliest_service_at))reject("PatrolOriginalTaskAcceptanceMismatch",e.work_id);plan(e);return *a;
}
// 这里只解析保留的关联；完整 S2 original reader 仍验证实际批准、本人接受和军务开工。
MilitaryDutyWorkAssignmentAssociation work_source(const PatrolWorkSources& x,const MilitaryDutyAcceptedSource& accepted){
 return take(read_military_duty_work_assignment_association(x.common,x.duties,accepted.original_assignment_source,accepted.assignment_source_schema,x.duty_context.selected_dynamic_military));
}
bool direct(const PatrolWorkSources& x,const MilitaryDutyAcceptedSource&a,const WorkAcceptanceSource&w){
 const auto actual=work_source(x,a);if(!actual.source)return false;const auto& source=*actual.source;
 if(actual.assignment&&!work_detail::same(actual.assignment->route_definition.work_acceptance_ref,{"tasks","work_accepted",w.fact_id}))reject("PatrolAssignmentOriginalPlanMismatch",actual.assignment->id);
 if(source.kind=="work_accepted")return source.id==w.fact_id;
 if(!task_work_lifecycle_sources_version(x.tasks.work_execution->version))return false;
 const auto* accepted=x.tasks.work_execution->work_reassignment_acceptances.find(source.id);
 if(!accepted||accepted->fact_id!=source.id)reject("PatrolReplacementAcceptanceRequired",source.id);
 return accepted->work_id==w.work_id&&work_detail::same(accepted->work_acceptance_ref,{"tasks","work_accepted",w.fact_id});
}
MilitaryDutyExecutionRead execution(const PatrolWorkSources&x,const WorkEpisode&e,const StableId&duty){
 const auto&a=acceptance(x,e);const auto*term=x.duties.terms.find(duty);const auto*original=x.duties.executions.find(duty);const auto*accepted=original?x.duties.accepted_sources.find(original->accepted_source_ref):nullptr;
 if(!term||!accepted||!direct(x,*accepted,a))reject("PatrolDutyOriginalWorkAssignmentRequired",duty);
 const auto&p=plan(e);if(term->duty_kind!=MilitaryDutyKind::patrol||term->unit_ids!=p.unit_ids||!work_detail::same(term->supply_authorization_ref,p.supply_order_ref))reject("PatrolDutyFrozenTermsMismatch",duty);
 auto value=take(read_military_duty_execution(x.common,x.duties,x.duty_context,duty));if(!value)reject("PatrolDutyNotActuallyBound",duty);auto&v=*value;
 if(!work_detail::same(v.original_assignment_source,MomentOwnerSource{accepted->original_assignment_source,accepted->assignment_source_schema})||cmp(v.bound_at,a.earliest_service_at)<0)reject("PatrolDutyBeforeAcceptedService",duty);
 if(task_work_lifecycle_sources_version(x.tasks.work_execution->version)){
  if(!x.work_history||&x.work_history->programs.common!=&x.common||&x.work_history->tasks!=&x.tasks||&x.work_history->task_definitions!=&x.definitions)reject("PatrolWorkExecutorHistoryRequired",duty);
  const StableId* leader=nullptr;
  const auto assignment=work_source(x,*accepted);if(!assignment.source)reject("PatrolWorkAssignmentMissing",duty);
  if(assignment.source->kind=="work_reassignment_accepted"){
   const auto identity=take(read_governance_accepted_work_executor_at(*x.work_history,e,*assignment.source,v.bound_at,&v.actual_binding_source.source_ref));leader=identity.actor_id;
  }else{const auto identity=take(read_governance_work_executor_before_cut(*x.work_history,e,&v.actual_binding_source.source_ref));if(identity.acceptance||!identity.actor_id||*identity.actor_id!=p.leader_actor_id)reject("PatrolOriginalAssignmentAfterReplacement",duty);leader=identity.actor_id;}
  if(!leader||v.leader_actor_id!=*leader||term->leader_actor_id!=*leader)reject("PatrolDutyActualLeaderMismatch",duty);
  return std::move(v);
 }
 StableId leader=p.leader_actor_id;const auto bound_order=source_order(x.common,v.actual_binding_source.source_ref);bool matched_change=false;
 for(const auto&id:e.leader_change_fact_refs){const auto*change=x.tasks.work_execution->leader_changes.find(id);if(!change||change->work_id!=e.work_id||!work_detail::header(x.common,id,"work_leader_changed"))reject("PatrolLeaderChangeSourceMissing",id);const auto order=cmp(change->replacement_at,v.bound_at);if(order>0)continue;if(order==0&&source_order(x.common,change->original_execution_source.source_ref)>bound_order)continue;if(change->old_actor_id!=leader)reject("PatrolLeaderHistoryMismatch",id);if(work_detail::same(change->original_execution_source,v.actual_binding_source)){if(matched_change)reject("DuplicatePatrolLeaderBinding",id);matched_change=true;}leader=change->new_actor_id;}
 if(v.leader_actor_id!=leader)reject("PatrolDutyActualLeaderMismatch",duty);return std::move(v);
}
WorkExecutionSource binding(const MilitaryDutyExecutionRead&v){return WorkMilitaryActivityBinding{v.accepted_terms_source.source_ref,v.accepted_terms_source.source_schema};}
bool same(const DutyExecutionBinding&a,const DutyExecutionBinding&b){if(a.index()!=b.index())return false;if(const auto*x=std::get_if<DutyTravelAction>(&a)){const auto&y=std::get<DutyTravelAction>(b);return x->action_id==y.action_id&&x->journey_ref==y.journey_ref;}if(const auto*x=std::get_if<DutyStationAction>(&a))return x->action_id==std::get<DutyStationAction>(b).action_id;const auto&x=std::get<DutyRegisteredMilitaryActivity>(a);const auto&y=std::get<DutyRegisteredMilitaryActivity>(b);return x.activity_ref==y.activity_ref&&x.activity_schema_version==y.activity_schema_version;}
const MilitaryDutyWindowState& window(const PatrolWorkSources&x,const MilitaryDutyPeriodFact2&p){const MilitaryDutyWindowState*found=nullptr;for(const auto&row:x.duties.windows.rows())if(const auto*c=std::get_if<DutyWindowClosed>(&row.second.state);c&&c->result_ref==p.fact_id){if(found)reject("DuplicatePatrolDutyWindow",p.fact_id);found=&row.second;}if(!found)reject("PatrolDutyWindowMissing",p.fact_id);return *found;}
}
Outcome<const WorkEpisode*> read_work_assigned_to_patrol_duty(const PatrolWorkSources&x,const StableId&duty){return guard<const WorkEpisode*>([&]{
 context(x);const auto*original=x.duties.executions.find(duty);const auto*accepted=original?x.duties.accepted_sources.find(original->accepted_source_ref):nullptr;if(!accepted)reject("MilitaryDutyAcceptedSourceRequired",duty);const auto association=work_source(x,*accepted);if(!association.source)return static_cast<const WorkEpisode*>(nullptr);const auto&source=*association.source;
 const WorkAcceptanceSource* a=nullptr;
 if(source.kind=="work_accepted")a=x.tasks.work_execution->acceptances.find(source.id);
 else{if(!task_work_lifecycle_sources_version(x.tasks.work_execution->version))reject("WorkReassignmentProfileRequired",source.id);const auto* replacement=x.tasks.work_execution->work_reassignment_acceptances.find(source.id);if(!replacement||replacement->fact_id!=source.id||replacement->work_acceptance_ref.domain!="tasks"||replacement->work_acceptance_ref.kind!="work_accepted")reject("PatrolReplacementAcceptanceRequired",source.id);a=x.tasks.work_execution->acceptances.find(replacement->work_acceptance_ref.id);if(!a||a->work_id!=replacement->work_id)reject("PatrolReplacementWorkMismatch",source.id);}
 const auto*e=a?x.tasks.work_execution->episodes.find(a->work_id):nullptr;if(!e)reject("PatrolWorkAssignmentMissing",source.id);acceptance(x,*e);if(!direct(x,*accepted,*a))reject("PatrolDutyOriginalWorkAssignmentRequired",duty);return e;
});}

Outcome<WorkExecutionSourceRead> read_patrol_work_execution(const PatrolWorkSources&x,const WorkEpisode&e,const MomentOwnerSource&source){return guard<WorkExecutionSourceRead>([&]{const auto&a=acceptance(x,e);std::optional<MilitaryDutyExecutionRead> found;
 for(const auto&row:x.duties.accepted_sources.rows()){if(!direct(x,row.second,a))continue;const auto*ex=x.duties.executions.find(row.second.duty_action_id);if(!ex)reject("PatrolDutyExecutionMissing",row.first);bool matches=ex->actual_binding&&work_detail::same(source,{{"military","military_duty_execution_bound",ex->actual_binding->id},1});if(!matches)for(const auto&station:x.duties.station_sources.rows())if(station.second.duty_action_id==row.second.duty_action_id&&work_detail::same(source,{{"military","military_station_source",station.first},1}))matches=true;if(!matches)continue;if(found)reject("AmbiguousPatrolExecutionSource",source.source_ref.id);found=execution(x,e,row.second.duty_action_id);}
 if(!found||!work_detail::same(found->actual_binding_source,source))reject("PatrolActualDutyBindingRequired",source.source_ref.id);const auto&v=*found;WorkExecutionSourceRead out{binding(v),source,v.leader_actor_id,v.bound_at,v.finite_until};if(v.terminal){out.original_terminal=v.terminal->actual_source;out.ended_at=v.terminal->ended_at;}return out;
});}
Outcome<WorkPeriodRead> read_patrol_work_period(const PatrolWorkSources&x,const WorkEpisode&e,const WorkMilitaryPeriodSource&source){return guard<WorkPeriodRead>([&]{acceptance(x,e);const auto*p=take(read_military_duty_period(x.common,x.duties,x.duty_context,source.fact_ref,source.source_schema));const auto duty=execution(x,e,p->window.duty_action_id);const auto&frozen=plan(e);if(!same(p->execution_binding,duty.binding)||p->leader_actor_id!=duty.leader_actor_id||p->unit_ids!=frozen.unit_ids||p->duty_kind!=MilitaryDutyKind::patrol)reject("PatrolPeriodOriginalExecutionMismatch",p->fact_id);bool linked=false;for(const auto&original:e.original_execution_sources)if(work_detail::same(original,duty.actual_binding_source))linked=true;if(!linked)reject("PatrolPeriodExecutionNotAdopted",p->fact_id);
 WorkPeriodRead out;out.canonical_source=source;out.window={p->window.opened_at,p->window.planned_close_at};out.actual_until=p->actual_until;out.leader_actor_id=p->leader_actor_id;out.execution_source=binding(duty);append(out.actual_anchor_sources,duty.actual_binding_source);append(out.actual_anchor_sources,p->route_or_station_source_refs);append(out.actual_anchor_sources,p->participant_basis_refs);append(out.actual_anchor_sources,p->supply_fact_refs);out.encounter_result_refs=p->encounter_result_refs;
 if(cmp(out.window.from,duty.bound_at)<0||cmp(out.actual_until,duty.finite_until)>0||(duty.terminal&&cmp(out.actual_until,duty.terminal->ended_at)>0))reject("PatrolPeriodOutsideActualBinding",p->fact_id);
 const bool enough=p->eligible_force_member_count>=frozen.minimum_available_people;out.source_provided=p->provided_half_days==1&&p->leader_present&&enough&&p->unresolved_conflict_refs.empty();
 if(out.source_provided)out.reason=WorkPeriodReason::qualified;else{out.reason=!p->leader_present?WorkPeriodReason::leader_absent:!enough?WorkPeriodReason::insufficient_personnel:!p->unresolved_conflict_refs.empty()?WorkPeriodReason::active_conflict:p->disposition==MilitaryDutyDisposition::interrupted?WorkPeriodReason::source_interrupted:WorkPeriodReason::source_no_work;out.reason_source=p->reason_ref?p->reason_ref:std::optional<MomentOwnerSource>{{source.fact_ref,source.source_schema}};}
 // Only actual positive service segments with the leader's original military
 // participation establish ActualStart. A closed/waiting window alone cannot.
 for(const auto&segment:window(x,*p).actual_segments){const auto&q=segment.qualification;if(cmp(segment.span.from,segment.span.to)>=0||!q.leader_present||!q.authority_valid||!q.route_or_station_valid||!q.supplied)continue;bool leader=false;for(const auto&member:q.actual_participant_refs)if(member.actor_id==p->leader_actor_id)leader=true;if(!leader)continue;if(!out.actual_positive_start||cmp(segment.span.from,*out.actual_positive_start)<0)out.actual_positive_start=segment.span.from;}
 return out;
});}
Outcome<WorkCompletionRead> read_patrol_work_completion(const PatrolWorkSources&x,const WorkEpisode&e,const std::vector<StableId>&receipts,const WorldMoment&through){return guard<WorkCompletionRead>([&]{acceptance(x,e);if(receipts!=e.accepted_period_refs)reject("PatrolCompletionReceiptOrderMismatch",e.work_id);WorkCompletionRead out;const auto&p=plan(e);bool service=false;
 for(const auto&id:receipts){const auto*r=x.tasks.work_execution->period_receipts.find(id);if(!r||r->work_id!=e.work_id||!work_detail::header(x.common,id,"work_period_receipt"))reject("PatrolCompletionReceiptMissing",id);const auto*source=std::get_if<WorkMilitaryPeriodSource>(&r->source);if(!source)reject("PatrolCompletionWrongPeriodKind",id);const auto actual=take(read_patrol_work_period(x,e,*source));if(cmp(actual.actual_until,through)>0)reject("PatrolCompletionBeforeService",id);service=service||actual.source_provided;append(out.actual_leader_refs,{source->fact_ref,source->source_schema});}
 if(std::holds_alternative<StationWatch>(p.route)){out.route_or_stage_complete=service;return out;}
 // Each actual adopted Duty route is validated through the S1 owner. The S1
 // original route provider retains its frozen ordered checkpoints per journey.
 for(const auto&source:e.original_execution_sources){const auto execution_view=take(read_patrol_work_execution(x,e,source));if(cmp(execution_view.started_at,through)>0)continue;const auto*activity=std::get_if<WorkMilitaryActivityBinding>(&execution_view.binding);if(!activity)reject("PatrolDutyBindingRequired",e.work_id);const MilitaryDutyAcceptedSource*accepted=x.duties.accepted_sources.find(activity->military_activity_ref.id);if(!accepted)reject("PatrolDutyTermsSourceMissing",e.work_id);const auto duty=execution(x,e,accepted->duty_action_id);StableId journey;if(const auto*t=std::get_if<DutyTravelAction>(&duty.binding))journey=t->journey_ref;else if(const auto*g=std::get_if<DutyRegisteredMilitaryActivity>(&duty.binding))journey=g->activity_ref;else reject("PatrolCircuitHasStationExecution",e.work_id);
  auto route=take(x.checkpoints.read_patrol_route_execution(journey,through));if(!work_detail::same(route.original_binding_source,duty.actual_binding_source)||!work_detail::same_refs(route.ordered_checkpoint_refs,std::get<PatrolCircuit>(p.route).checkpoint_refs))reject("PatrolCheckpointFrozenRouteMismatch",journey);auto checkpoints=take(read_ordered_patrol_checkpoints(x.common,x.movement,x.checkpoints,journey,through));if(!work_detail::same(checkpoints.route_binding_source,duty.actual_binding_source))reject("PatrolCheckpointWrongWorkBinding",journey);for(const auto&source:checkpoints.actual_arrival_sources)append(out.checkpoint_completion_refs,{source,2});if(checkpoints.complete)out.route_or_stage_complete=true;
 }
 return out;
});}

Outcome<const PatrolCheckpointArrival2*> read_patrol_completion_checkpoint(const PatrolWorkSources&x,const WorkEpisode&e,const WorldMoment&through){return guard<const PatrolCheckpointArrival2*>([&]{
 acceptance(x,e);const auto&p=plan(e);if(!std::holds_alternative<PatrolCircuit>(p.route)||take(work_detail::credited(*x.tasks.work_execution,e))<p.required_half_days)return static_cast<const PatrolCheckpointArrival2*>(nullptr);
 const PatrolCheckpointArrival2* found=nullptr;WorldMoment last_service=take(x.definitions.read_accepted_work(e.authorization)).earliest_service_at;
 for(const auto&id:e.accepted_period_refs){const auto*r=x.tasks.work_execution->period_receipts.find(id);if(!r)reject("PatrolCompletionReceiptMissing",id);if(cmp(r->actual_until,last_service)>0)last_service=r->actual_until;}
 for(const auto&source:e.original_execution_sources){const auto v=take(read_patrol_work_execution(x,e,source));if(cmp(v.started_at,through)>0)continue;const auto*activity=std::get_if<WorkMilitaryActivityBinding>(&v.binding);const auto*accepted=activity?x.duties.accepted_sources.find(activity->military_activity_ref.id):nullptr;if(!accepted)reject("PatrolDutyTermsSourceMissing",e.work_id);const auto duty=execution(x,e,accepted->duty_action_id);StableId journey;
  if(const auto*t=std::get_if<DutyTravelAction>(&duty.binding))journey=t->journey_ref;else if(const auto*g=std::get_if<DutyRegisteredMilitaryActivity>(&duty.binding))journey=g->activity_ref;else reject("PatrolCircuitHasStationExecution",e.work_id);
  const auto route=take(x.checkpoints.read_patrol_route_execution(journey,through));if(!work_detail::same(route.original_binding_source,duty.actual_binding_source)||!work_detail::same_refs(route.ordered_checkpoint_refs,std::get<PatrolCircuit>(p.route).checkpoint_refs))reject("PatrolCheckpointFrozenRouteMismatch",journey);
  const auto ordered=take(read_ordered_patrol_checkpoints(x.common,x.movement,x.checkpoints,journey,through));if(!work_detail::same(ordered.route_binding_source,duty.actual_binding_source))reject("PatrolCheckpointWrongWorkBinding",journey);if(!ordered.complete)continue;if(ordered.actual_arrival_sources.empty())reject("PatrolCompletedRouteWithoutArrival",journey);
  const auto*last=take(read_patrol_checkpoint(x.common,x.movement,x.checkpoints,ordered.actual_arrival_sources.back(),2));if(cmp(last->arrived_at,last_service)<0)continue;
  const auto completion=take(read_patrol_work_completion(x,e,e.accepted_period_refs,last->arrived_at));if(!completion.route_or_stage_complete)reject("PatrolCheckpointCompletionMismatch",last->source_fact_id);
  if(!found||cmp(last->arrived_at,found->arrived_at)<0||(cmp(last->arrived_at,found->arrived_at)==0&&source_order(x.common,{"travel","patrol_checkpoint_arrival",last->source_fact_id})<source_order(x.common,{"travel","patrol_checkpoint_arrival",found->source_fact_id})))found=last;
 }return found;
});}

Status NativePatrolDutyExecutionConsumer::begin(const StableId&duty,const MomentOwnerSource&source,const WorldMoment&at){return guard<Done>([&]{
 if(&common_!=&sources_.common||&tasks_!=&sources_.tasks||cmp(at,take(sources_.clock.read_current_moment())))reject("PatrolDutyConsumerCandidateMismatch",duty);
 const auto*e=take(read_work_assigned_to_patrol_duty(sources_,duty));if(!e)return Done{};const auto actual=execution(sources_,*e,duty);
 if(!work_detail::same(actual.actual_binding_source,source)||cmp(actual.bound_at,at)||actual.terminal)reject("PatrolDutyActualBeginRequired",duty);
 take(bind_original_work_execution_in_candidate(tasks_,original_,e->work_id,source,at));
 const auto assignment=take(read_military_duty_work_assignment_association(common_,sources_.duties,actual.original_assignment_source.source_ref,actual.original_assignment_source.source_schema,sources_.duty_context.selected_dynamic_military));
 if(task_work_lifecycle_sources_version(tasks_.work_execution->version)&&assignment.source&&assignment.source->kind=="work_reassignment_accepted"){
  if(!sources_.work_history)reject("PatrolWorkExecutorHistoryRequired",duty);
  const auto identity=take(read_governance_accepted_work_executor_at(*sources_.work_history,*e,*assignment.source,at,&source.source_ref));
  const auto previous=take(read_governance_work_executor_before_cut(*sources_.work_history,*e,nullptr));
  if(!identity.actor_id||!identity.acceptance||!previous.actor_id)reject("PatrolReplacementAcceptedActorRequired",duty);
  if(*identity.actor_id!=*previous.actor_id){WorkLeaderChange change;change.work_id=e->work_id;change.replacement_at=at;change.old_actor_id=*previous.actor_id;change.new_actor_id=*identity.actor_id;change.authority_source={identity.acceptance->reassignment_approval_ref,1};change.actual_consent_source={*assignment.source,assignment.source_schema};change.actual_arrival_source=source;change.new_execution_source=binding(actual);change.original_execution_source=source;take(replace_work_leader_in_candidate(common_,tasks_,original_,e->work_id,std::move(change),at));}
 }
 return Done{};
});}
Status NativePatrolDutyExecutionConsumer::end(const StableId&duty,const MomentOwnerSource&source,const WorldMoment&at){return guard<Done>([&]{
 if(&common_!=&sources_.common||&tasks_!=&sources_.tasks||cmp(at,take(sources_.clock.read_current_moment())))reject("PatrolDutyConsumerCandidateMismatch",duty);
 const auto*e=take(read_work_assigned_to_patrol_duty(sources_,duty));if(!e)return Done{};const auto actual=execution(sources_,*e,duty);
 if(!actual.terminal||!work_detail::same(actual.terminal->actual_source,source)||cmp(actual.terminal->ended_at,at))reject("PatrolDutyActualEndRequired",duty);
 take(close_original_work_execution_binding_in_candidate(tasks_,original_,e->work_id,source,at));return Done{};
});}
namespace {
std::optional<StableId> patrol_journey(const DutyExecutionBinding&binding){
 if(const auto*t=std::get_if<DutyTravelAction>(&binding))return t->journey_ref;
 if(const auto*g=std::get_if<DutyRegisteredMilitaryActivity>(&binding))return g->activity_ref;
 return {};
}
bool adopted(const WorkEpisode&e,const MomentOwnerSource&source){for(const auto&r:e.original_execution_sources)if(work_detail::same(r,source))return true;return false;}
}
Outcome<std::vector<StableId>> NativePatrolCheckpointOriginalRead::active_patrol_journeys(const WorldMoment&at)const{return guard<std::vector<StableId>>([&]{
 const auto x=sources();context(x);if(cmp(at,take(clock_.read_current_moment())))reject("PatrolRoutePredictionMomentMismatch");std::vector<StableId>out;
 for(const auto&row:duties_.accepted_sources.rows()){
  if(!work_source(x,row.second).source)continue;
  const auto*e=take(read_work_assigned_to_patrol_duty(x,row.second.duty_action_id));if(!e||!std::holds_alternative<PatrolCircuit>(plan(*e).route))continue;
  const auto raw=take(read_military_duty_execution(common_,duties_,duty_context_,row.second.duty_action_id));if(!raw)continue;
  const auto actual=execution(x,*e,row.second.duty_action_id);if(!adopted(*e,actual.actual_binding_source))reject("PatrolRouteExecutionNotAdopted",e->work_id);
  if(cmp(actual.bound_at,at)>0||cmp(actual.finite_until,at)<0||(actual.terminal&&cmp(actual.terminal->ended_at,at)<0))continue;
  if(e->terminal_result_ref){const auto*r=tasks_.work_execution->results.find(*e->terminal_result_ref);if(!r||r->work_id!=e->work_id||!work_detail::header(common_,r->fact_id,work_detail::result_kind(*r)))reject("PatrolRouteWorkTerminalMissing",e->work_id);if(cmp(r->finished_at,at)<0)continue;}
  auto id=patrol_journey(actual.binding);if(!id)reject("PatrolCircuitHasStationExecution",e->work_id);if(std::find(out.begin(),out.end(),*id)!=out.end())reject("AmbiguousPatrolJourneyAssignment",*id);out.push_back(std::move(*id));
 }return out;
});}
Outcome<PatrolRouteExecutionRead> NativePatrolCheckpointOriginalRead::read_patrol_route_execution(const StableId&id,const WorldMoment&through)const{return guard<PatrolRouteExecutionRead>([&]{
 const auto x=sources();context(x);if(cmp(through,take(clock_.read_current_moment()))>0)reject("PatrolRouteHistoryFromFuture",id);std::optional<PatrolRouteExecutionRead>out;
 for(const auto&row:duties_.accepted_sources.rows()){
  if(!work_source(x,row.second).source)continue;
  const auto*raw=duties_.executions.find(row.second.duty_action_id);if(!raw||!raw->actual_binding)continue;auto journey=patrol_journey(raw->actual_binding->execution_binding);if(!journey||*journey!=id)continue;
  const auto*e=take(read_work_assigned_to_patrol_duty(x,row.second.duty_action_id));if(!e)reject("PatrolOriginalWorkRequired",id);const auto*circuit=std::get_if<PatrolCircuit>(&plan(*e).route);if(!circuit||circuit->checkpoint_refs.empty())reject("PatrolFrozenCircuitRequired",id);
  const auto actual=execution(x,*e,row.second.duty_action_id);if(cmp(actual.bound_at,through)>0)continue;if(!adopted(*e,actual.actual_binding_source))reject("PatrolRouteExecutionNotAdopted",e->work_id);if(out)reject("AmbiguousPatrolJourneyAssignment",id);
  for(const auto&checkpoint:circuit->checkpoint_refs)take(read_original_spatial_site_anchor(common_catalog_,travel_catalog_,checkpoint));
  out=PatrolRouteExecutionRead{actual.actual_binding_source,id,actual.actor_or_group_ref,circuit->checkpoint_refs};
 }
 if(!out)reject("PatrolActualRouteBindingRequired",id);return std::move(*out);
});}
Outcome<MovementAnchor> NativePatrolCheckpointOriginalRead::read_patrol_checkpoint_anchor(const PatrolRouteExecutionRead&route,const SourceRef&checkpoint)const{return guard<MovementAnchor>([&]{
 const auto original=take(read_patrol_route_execution(route.journey_id,take(clock_.read_current_moment())));
 if(!work_detail::same(route.original_binding_source,original.original_binding_source)||spatial_carrier_key(route.actor_or_group_ref)!=spatial_carrier_key(original.actor_or_group_ref)||!work_detail::same_refs(route.ordered_checkpoint_refs,original.ordered_checkpoint_refs))reject("PatrolRouteProjectionMismatch",route.journey_id);
 bool present=false;for(const auto&ref:original.ordered_checkpoint_refs)if(work_detail::same(ref,checkpoint))present=true;if(!present)reject("PatrolCheckpointOutsideFrozenRoute",checkpoint.id);
 return take(read_original_spatial_site_anchor(common_catalog_,travel_catalog_,checkpoint));
});}

}
