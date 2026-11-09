#include "military_duty_assignment.hpp"
#include "military_duty_execution_read.hpp"
#include "military_duty_qualification_capture.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "military_duty_detail.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {using namespace movement_detail;using namespace duty_detail;namespace {
bool binding_equal(const DutyExecutionBinding& a,const DutyExecutionBinding& b){if(a.index()!=b.index())return false;if(const auto* x=std::get_if<DutyTravelAction>(&a)){const auto& y=std::get<DutyTravelAction>(b);return x->action_id==y.action_id&&x->journey_ref==y.journey_ref;}if(const auto* x=std::get_if<DutyStationAction>(&a))return x->action_id==std::get<DutyStationAction>(b).action_id;const auto& x=std::get<DutyRegisteredMilitaryActivity>(a);const auto& y=std::get<DutyRegisteredMilitaryActivity>(b);return x.activity_ref==y.activity_ref&&x.activity_schema_version==y.activity_schema_version;}
std::size_t head(const CommonStore& common,const SourceRef& source){return take(read_actual_common_fact_position(common,source));}
MomentOwnerSource exact(const ElapsedTimeSource& source){const auto* value=std::get_if<MomentOwnerSource>(&source);if(!value||value->source_schema<1||!source_valid(value->source_ref))fail("MilitaryDutyExactExecutionSourceRequired");return *value;}
void choose_terminal(const CommonStore& common,std::optional<MilitaryDutyExecutionTerminalRead>& selected,const MilitaryDutyExecutionTerminalRead& next,ExactArithmeticBudget b){if(!selected){selected=next;return;}const auto cmp=take(compare_world_moments(next.ended_at,selected->ended_at,b));if(cmp<0||(cmp==0&&head(common,next.actual_source.source_ref)<head(common,selected->actual_source.source_ref)))selected=next;}
}
Outcome<MilitaryDutyOriginalExecutionRead> MilitaryDutyMovementSources::read(const DutyExecutionBinding& binding,const WorldMoment& now)const{return guard<MilitaryDutyOriginalExecutionRead>([&]{
 MilitaryDutyOriginalExecutionRead result;result.binding=binding;
 if(const auto* travel=std::get_if<DutyTravelAction>(&binding)){
  NativeMovementActionTimeRead actual(common_,travel_,movement_,catalog_,services_,budget_);const auto proof=take(actual.read_action_time_owner(movement_action_time_registration(),travel->action_id));
  const auto* basis=movement_.exertion_bases.find(proof.original_execution_source.id);const auto* execution=basis?std::get_if<PersonalMovementExecution>(&basis->execution):nullptr;
  if(!execution||execution->action_id!=travel->action_id||execution->journey_id!=travel->journey_ref)fail("MilitaryDutyTravelOriginMismatch",travel->action_id);
  result.original_source={proof.original_execution_source,proof.source_schema};result.started_at=take(actual.read_action_time_source(proof.started_source));result.named_participants=proof.participant_actor_ids;if(!basis->original_action||basis->original_action->id!=travel->action_id)fail("MilitaryDutyTravelActionSourceRequired",travel->action_id);result.actor_or_group_ref=ActorCarrierRef{basis->original_action->actor_id};
  if(proof.ended_source)result.terminal=MilitaryDutyExecutionTerminalRead{exact(*proof.ended_source),take(actual.read_action_time_source(*proof.ended_source))};
 }else if(const auto* registered=std::get_if<DutyRegisteredMilitaryActivity>(&binding)){
  if(registered->activity_schema_version!=1)fail("MilitaryDutyRegisteredExecutionSchemaRequired",registered->activity_ref);
  const auto time=take(read_group_movement_time(common_,carriers_,movement_,groups_,catalog_,services_,registered->activity_ref,now,budget_));
  if(!std::holds_alternative<ArmyCarrierRef>(time.execution.carrier)&&!std::holds_alternative<FleetCarrierRef>(time.execution.carrier))fail("MilitaryDutyRegisteredMilitaryCarrierRequired",registered->activity_ref);
  const auto* opening=groups_.openings.find(time.opening_source.id);if(!opening||opening->journey_id!=registered->activity_ref||!opening->military_order)fail("MilitaryDutyOriginalGroupOrderRequired",registered->activity_ref);
  head(common_,time.opening_source);result.original_source={time.opening_source,1};result.started_at=time.started_at;result.named_participants=opening->named_participants;result.actor_or_group_ref=time.execution.carrier;
  if(time.ended_at){const auto terminal=take(read_actual_group_journey_terminal(common_,carriers_,movement_,groups_,registered->activity_ref,budget_));if(!time.actual_terminal_source||!same_source(*time.actual_terminal_source,terminal.terminal.actual_terminal_source)||!same_moment(*time.ended_at,terminal.terminal.occurred_at,budget_))fail("MilitaryDutyGroupTerminalMismatch",registered->activity_ref);result.terminal=MilitaryDutyExecutionTerminalRead{{terminal.terminal.actual_terminal_source,1},terminal.terminal.occurred_at};}
 }else fail("MilitaryStationOriginalReaderRequired");
 if(take(compare_world_moments(result.started_at,now,budget_))>0||(result.terminal&&take(compare_world_moments(result.terminal->ended_at,now,budget_))>0))fail("MilitaryDutyExecutionSourceInFuture");return result;
});}
Outcome<std::optional<MilitaryDutyExecutionRead>> read_military_duty_execution(const CommonStore& common,const MilitaryDutyState& state,const MilitaryDutyContext& c,const StableId& id){return guard<std::optional<MilitaryDutyExecutionRead>>([&]()->std::optional<MilitaryDutyExecutionRead>{
 const auto& t=terms(state,id);const auto* execution=state.executions.find(id);const auto* accepted=execution?state.accepted_sources.find(execution->accepted_source_ref):nullptr;const auto now=take(c.stamps.read_current_moment());
 if(!execution||!accepted||accepted->duty_action_id!=id)fail("MilitaryDutyAcceptedSourceRequired",id);const MomentOwnerSource accepted_ref{ref("military_duty_terms",accepted->id),1};const auto accepted_order=head(common,accepted_ref.source_ref);take(c.original.validate_accepted_terms(t,*accepted));
 if(!t.execution_binding){if(execution->actual_binding||execution->movement_stop||station(state,id)||t.state==MilitaryDutyPhase::active)fail("MilitaryDutyExecutionBindingMissing",id);return {};}
 MilitaryDutyExecutionRead result;result.duty_action_id=id;result.order_or_assignment_ref=t.order_or_assignment_ref;result.leader_actor_id=t.leader_actor_id;result.accepted_terms_source=accepted_ref;result.original_assignment_source={accepted->original_assignment_source,accepted->assignment_source_schema};result.binding=*t.execution_binding;result.finite_until=until(t,c.budget);result.phase=t.state;
 if(const auto* station_binding=std::get_if<DutyStationAction>(&*t.execution_binding)){
  if(execution->actual_binding||execution->movement_stop)fail("DuplicateMilitaryDutyExecutionOwner",id);const auto* source=station(state,id);if(!source||source->actual_action.id!=station_binding->action_id)fail("MilitaryStationSourceRequired",id);
  NativeMilitaryStationActionTimeRead reader(common,state,c);const auto proof=take(reader.read_action_time_owner(military_station_action_time_registration(),station_binding->action_id));result.actual_binding_source={proof.original_execution_source,proof.source_schema};result.original_execution_source=result.actual_binding_source;result.bound_at=source->started_at;result.execution_started_at=source->started_at;result.actor_or_group_ref=ActorCarrierRef{source->actual_action.actor_id};
  if(proof.ended_source)result.terminal=MilitaryDutyExecutionTerminalRead{exact(*proof.ended_source),take(reader.read_action_time_source(*proof.ended_source))};
 }else{
  if(!execution->actual_binding||!c.movement_executions)fail("MilitaryDutyMovementBindingSourceRequired",id);const auto& source=*execution->actual_binding;if(!binding_equal(source.execution_binding,*t.execution_binding))fail("MilitaryDutyMovementBindingMismatch",id);
  const auto original=take(c.movement_executions->read(source.execution_binding,now));if(!same_source(original.original_source.source_ref,source.original_execution_source.source_ref)||original.original_source.source_schema!=source.original_execution_source.source_schema||!same_moment(original.started_at,source.original_started_at,c.budget)||take(compare_world_moments(source.bound_at,original.started_at,c.budget))<0||std::find(original.named_participants.begin(),original.named_participants.end(),t.leader_actor_id)==original.named_participants.end())fail("MilitaryDutyActualMovementOriginMismatch",id);
  result.actual_binding_source={ref("military_duty_execution_bound",source.id),1};result.original_execution_source=source.original_execution_source;result.bound_at=source.bound_at;result.execution_started_at=source.original_started_at;result.terminal=original.terminal;result.actor_or_group_ref=original.actor_or_group_ref;
  take(c.original.validate_execution_binding(t,source.bound_at));
  if(execution->movement_stop){const auto& stop=*execution->movement_stop;const MomentOwnerSource stopped{ref("military_duty_execution_stopped",stop.id),1};if(head(common,stopped.source_ref)<=head(common,result.actual_binding_source.source_ref))fail("MilitaryDutyStopOrderMismatch",id);
   if(std::holds_alternative<MilitaryDutyAuthorizationEnded>(stop.reason)){if(!same_moment(stop.stopped_at,result.finite_until,c.budget))fail("MilitaryDutyExpirySourceMismatch",id);}else{const auto& cause=std::get<MilitaryDutyActualMovementEnded>(stop.reason).actual_terminal_source;if(!original.terminal||cause.source_schema!=original.terminal->actual_source.source_schema||!same_source(cause.source_ref,original.terminal->actual_source.source_ref)||!same_moment(stop.stopped_at,original.terminal->ended_at,c.budget))fail("MilitaryDutyMovementStopSourceMismatch",id);}
   result.terminal=MilitaryDutyExecutionTerminalRead{stopped,stop.stopped_at};
  }

 }
 if(head(common,result.actual_binding_source.source_ref)<=accepted_order||take(compare_world_moments(result.bound_at,accepted->accepted_at,c.budget))<0||take(compare_world_moments(result.bound_at,result.finite_until,c.budget))>=0||take(compare_world_moments(result.bound_at,now,c.budget))>0)fail("MilitaryDutyActualBindingMomentMismatch",id);
 // A safe original control may end this duty before its carrier's later route
 // terminal; both retain their own sources and neither implies the other.
 for(const auto& row:state.controls.rows())if(row.second.duty_action_id==id&&row.second.resulting_phase==MilitaryDutyPhase::released){const auto& control=row.second;take(c.original.validate_control_history(t,control));choose_terminal(common,result.terminal,{{ref("military_duty_control",control.id),1},control.occurred_at},c.budget);}
 if(result.terminal&&(take(compare_world_moments(result.terminal->ended_at,result.bound_at,c.budget))<0||take(compare_world_moments(result.terminal->ended_at,now,c.budget))>0))fail("MilitaryDutyExecutionTerminalMomentMismatch",id);
 return result;
});}
namespace {
MilitaryDutyExecutionConsumer* required_consumer(const CommonStore& common,const MilitaryDutyState& state,const MilitaryDutyContext& c,const StableId& id){
 const auto* execution=state.executions.find(id);const auto* accepted=execution?state.accepted_sources.find(execution->accepted_source_ref):nullptr;
 if(!accepted)fail("MilitaryDutyAcceptedSourceRequired",id);
 const auto assignment=take(read_military_duty_work_assignment_association(common,state,accepted->original_assignment_source,accepted->assignment_source_schema,c.selected_dynamic_military));
 if(assignment.source&&!c.execution_consumer)fail("MilitaryDutyWorkExecutionConsumerRequired",id);
 return c.execution_consumer;
}
}
Status consume_actual_military_duty_execution_begin(const CommonStore& common,const MilitaryDutyState& state,const MilitaryDutyContext& c,const StableId& id){return status([&]{
 auto* consumer=required_consumer(common,state,c,id);if(!consumer)return;
 const auto actual=take(read_military_duty_execution(common,state,c,id));
 if(!actual||actual->terminal||!same_moment(actual->bound_at,take(c.stamps.read_current_moment()),c.budget))fail("MilitaryDutyActualWorkBindingRequired",id);
 take(consumer->begin(id,actual->actual_binding_source,actual->bound_at));
});}
Status consume_actual_military_duty_execution_end(const CommonStore& common,const MilitaryDutyState& state,const MilitaryDutyContext& c,const StableId& id){return status([&]{
 auto* consumer=required_consumer(common,state,c,id);if(!consumer)return;
 const auto actual=take(read_military_duty_execution(common,state,c,id));
 if(!actual)return; // A truly unbound cancelled assignment never started Work.
 if(!actual->terminal||!same_moment(actual->terminal->ended_at,take(c.stamps.read_current_moment()),c.budget))fail("MilitaryDutyActualWorkTerminalRequired",id);
 take(consumer->end(id,actual->terminal->actual_source,actual->terminal->ended_at));
});}
Outcome<SourceRef> activate_military_movement_duty_in_candidate(CommonStore& common,MilitaryDutyState& state,const MilitaryDutyContext& c,const StableId& id,const DutyExecutionBinding& binding,const WorldMoment& at){return guard<SourceRef>([&]{
 if(!same_moment(take(c.stamps.read_current_moment()),at,c.budget)||std::holds_alternative<DutyStationAction>(binding)||!c.movement_executions)fail("MilitaryDutyActualMovementActivationRequired",id);
 auto* t=state.terms.find(id);auto* execution=state.executions.find(id);if(!t||!execution)fail("MilitaryDutyExecutionRequired",id);
 if(execution->actual_binding){if(!binding_equal(execution->actual_binding->execution_binding,binding))fail("MilitaryDutyExecutionReplayMismatch",id);take(read_military_duty_execution(common,state,c,id));return ref("military_duty_execution_bound",execution->actual_binding->id);}
 if(t->execution_binding||t->state!=MilitaryDutyPhase::assembling||execution->current_window_ref||take(compare_world_moments(at,until(*t,c.budget),c.budget))>=0)fail("MilitaryDutyMovementActivationPhaseMismatch",id);
 const auto original=take(c.movement_executions->read(binding,at));if(original.terminal||std::find(original.named_participants.begin(),original.named_participants.end(),t->leader_actor_id)==original.named_participants.end())fail("MilitaryDutyMovementNotActuallyActive",id);
 t->execution_binding=binding;take(c.original.validate_execution_binding(*t,at));
 MilitaryDutyQualificationCapture qualification_capture(common);auto q=take(c.original.read_qualification(*t,{at,at},qualification_capture.prefix()));qualification_capture.validate_unchanged();check_qualification(*t,q);if(!qualified(rule(c,*t),q))fail("MilitaryDutyActualMovementPresenceRequired",id);
 MilitaryDutyActualBindingSource source{take(common.next_id("military_duty_execution_bound")),binding,original.original_source,original.started_at,at};execution->actual_binding=source;t->state=MilitaryDutyPhase::active;take(common.append_fact({source.id,"military_duty_execution_bound","military"}));take(open_military_duty_window_in_candidate(common,state,c,id,at));take(read_military_duty_execution(common,state,c,id));take(consume_actual_military_duty_execution_begin(common,state,c,id));return ref("military_duty_execution_bound",source.id);
});}
namespace {
SourceRef stop_movement_duty(CommonStore& common,MilitaryDutyState& state,const MilitaryDutyContext& c,const StableId& id,const WorldMoment& at,const std::optional<MomentOwnerSource>& cause){
 if(!same_moment(take(c.stamps.read_current_moment()),at,c.budget))fail("MilitaryDutyActualStopMomentRequired",id);auto* t=state.terms.find(id);auto* e=state.executions.find(id);if(!t||!e||!e->actual_binding||!c.movement_executions)fail("MilitaryDutyMovementBindingSourceRequired",id);
 if(e->movement_stop){take(read_military_duty_execution(common,state,c,id));return ref("military_duty_execution_stopped",e->movement_stop->id);}
 if(t->state==MilitaryDutyPhase::released)fail("MilitaryDutyAlreadyReleased",id);const auto original=take(c.movement_executions->read(e->actual_binding->execution_binding,at));
 MilitaryDutyMovementStopSource stop;stop.id=take(common.next_id("military_duty_execution_stopped"));stop.stopped_at=at;
 if(cause){if(!original.terminal||cause->source_schema!=original.terminal->actual_source.source_schema||!same_source(cause->source_ref,original.terminal->actual_source.source_ref)||!same_moment(original.terminal->ended_at,at,c.budget))fail("MilitaryDutyActualMovementTerminalRequired",id);stop.reason=MilitaryDutyActualMovementEnded{*cause};}
 else{if(!same_moment(at,until(*t,c.budget),c.budget))fail("MilitaryDutyActualAuthorizationExpiryRequired",id);stop.reason=MilitaryDutyAuthorizationEnded{};}
 e->movement_stop=stop;take(common.append_fact({stop.id,"military_duty_execution_stopped","military"}));
 if(e->current_window_ref){const auto* w=state.windows.find(*e->current_window_ref);if(!w)fail("MilitaryDutyCurrentWindowRequired",id);MilitaryDutyQualificationCapture qualification_capture(common);auto q=take(c.original.read_qualification(*t,{at,at},qualification_capture.prefix()));qualification_capture.validate_unchanged();check_qualification(*t,q);const bool full=same_moment(w->window.planned_close_at,at,c.budget)&&compare(w->actual_service_units.value,take(fixed_duration_work_units(1,c.scale.units_per_half_day,c.budget)).value,c.budget)==0&&qualified(rule(c,*t),q);take(close_military_duty_window_in_candidate(common,state,c,id,at,full?MilitaryDutyDisposition::provided:MilitaryDutyDisposition::interrupted,full?std::optional<MomentOwnerSource>{}:std::optional<MomentOwnerSource>{{ref("military_duty_execution_stopped",stop.id),1}}));}
 t->state=MilitaryDutyPhase::suspended;take(read_military_duty_execution(common,state,c,id));take(consume_actual_military_duty_execution_end(common,state,c,id));return ref("military_duty_execution_stopped",stop.id);
}
}
Outcome<SourceRef> expire_military_movement_duty_in_candidate(CommonStore& common,MilitaryDutyState& state,const MilitaryDutyContext& c,const StableId& id,const WorldMoment& at){return guard<SourceRef>([&]{return stop_movement_duty(common,state,c,id,at,{});});}
Outcome<std::vector<SourceRef>> consume_military_duty_movement_terminal_in_candidate(CommonStore& common,MilitaryDutyState& state,const MilitaryDutyContext& c,const MomentOwnerSource& cause,const WorldMoment& at){return guard<std::vector<SourceRef>>([&]{
 if(!same_moment(take(c.stamps.read_current_moment()),at,c.budget)||!source_valid(cause.source_ref)||cause.source_schema<1)fail("MilitaryDutyActualMovementTerminalRequired");std::vector<StableId> affected;
 for(const auto& row:state.executions.rows()){const auto& e=row.second;if(!e.actual_binding||e.movement_stop)continue;const auto& t=terms(state,row.first);if(t.state==MilitaryDutyPhase::released)continue;if(!c.movement_executions)fail("MilitaryDutyMovementSourceReadRequired");const auto original=take(c.movement_executions->read(e.actual_binding->execution_binding,at));if(original.terminal&&original.terminal->actual_source.source_schema==cause.source_schema&&same_source(original.terminal->actual_source.source_ref,cause.source_ref)){if(!same_moment(original.terminal->ended_at,at,c.budget))fail("MilitaryDutyUnconsumedMovementTerminal",row.first);affected.push_back(row.first);}}
 std::vector<SourceRef> out;for(const auto& id:affected)out.push_back(stop_movement_duty(common,state,c,id,at,cause));return out;
});}
Status validate_military_duty_execution_bindings(const CommonStore& common,const MilitaryDutyState& state,const MilitaryDutyContext& c,const WorldMoment& now){return status([&]{if(!same_moment(now,take(c.stamps.read_current_moment()),c.budget))fail("MilitaryDutyActualValidationMomentRequired");std::set<StableId> sources;for(const auto& row:state.terms.rows()){const auto actual=take(read_military_duty_execution(common,state,c,row.first));if(actual&&!sources.insert(actual->actual_binding_source.source_ref.id).second)fail("DuplicateMilitaryDutyExecutionSource",row.first);if(actual&&actual->terminal&&row.second.state==MilitaryDutyPhase::active)fail("MilitaryDutyTerminalNotConsumed",row.first);const auto* e=state.executions.find(row.first);if(e&&e->actual_binding&&!e->movement_stop&&row.second.state!=MilitaryDutyPhase::released&&(actual->terminal||take(compare_world_moments(now,actual->finite_until,c.budget))>=0))fail("MilitaryDutyMovementStopNotConsumed",row.first);}});}
}
