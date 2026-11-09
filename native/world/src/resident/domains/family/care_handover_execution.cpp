#include "care_handover_execution.hpp"
#include <algorithm>
#include <charconv>
#include <string_view>
#include <tuple>
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
void check(Status s){if(!s.ok)throw Failure{std::move(s.error)};}
template<class T>T take(Outcome<T> v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
bool same(const SourceRef& a,const SourceRef& b){return std::tie(a.domain,a.kind,a.id)==std::tie(b.domain,b.kind,b.id);}
bool same(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b))==0;}
void text(const StableId& value){if(value.empty()||value.size()>256)fail("InvalidCareHandoverIdentifier",value);}
void source(const SourceRef& value){text(value.domain);text(value.kind);text(value.id);}
void rule(const ActorFoundationRuleRef& value){text(value.id);text(value.catalog_digest);if(value.version<1)fail("InvalidCareHandoverRule",value.id);}
void refs(const std::vector<MomentOwnerSource>& values){if(values.size()>65536)fail("CareHandoverCollectionTooLarge");for(std::size_t i=0;i<values.size();++i){source(values[i].source_ref);if(values[i].source_schema<1)fail("InvalidCareHandoverSourceSchema",values[i].source_ref.id);for(std::size_t j=0;j<i;++j)if(same(values[i].source_ref,values[j].source_ref))fail("DuplicateCareHandoverSource",values[i].source_ref.id);}}
void time(const WorldMoment& at){take(compare_world_moments(at,at));}
void frames(const std::vector<FrozenFacilityCapabilityInputs>& values,const WorldMoment& at,bool at_boundary,bool must_allow){
 if(values.size()>65536)fail("CareHandoverCollectionTooLarge");for(std::size_t i=0;i<values.size();++i){const auto& v=values[i];text(v.actor_id);rule(v.purpose_ref);rule(v.result.rule_ref);time(v.captured_at);time(v.result.observed_at);
  if(v.purpose_ref.id!="facility_assisted_clearance_worker"||v.purpose_ref.version!=1||v.condition_head_snapshot.actor_id!=v.actor_id||v.condition_head_snapshot.condition_revision<0||v.condition_head_snapshot.initial_condition_seed_ref.seed_key.empty()||!same(v.captured_at,v.result.observed_at)||take(compare_world_moments(v.captured_at,at))>0||(at_boundary&&!same(v.captured_at,at))||(must_allow&&!v.result.allowed))fail("CareHandoverWorkerFrameMismatch",v.actor_id);
  for(std::size_t j=0;j<i;++j)if(values[j].actor_id==v.actor_id)fail("DuplicateCareHandoverWorker",v.actor_id);
 }
}
void binding(const CareHandoverExecution& e){const auto* b=std::get_if<ActorActionWorkBinding>(&e.execution_binding);if(!b||b->action_id!=e.original_action_ref||b->action_kind!="care_handover"||!same(b->source_owner_ref,care_handover_execution_source(e.handover_id)))fail("CareHandoverOriginalActionBindingMismatch",e.handover_id);check(validate_work_binding(e.execution_binding));}
void header(const CommonStore& c,const SourceRef& ref){const auto* row=c.state().facts.find(ref.id);if(!row||row->id!=ref.id||row->domain!=ref.domain||row->kind!=ref.kind||std::count(c.state().facts.order().begin(),c.state().facts.order().end(),ref.id)!=1)fail("CareHandoverOriginalFactRequired",ref.id);}
const AcceptedCaregiver& acceptance_prefix(const CareHandoverSourceContext& x,const CareHandoverExecution& e,const WorldMoment& now){
 const auto& common=x.acceptance.goals.common;const auto* accepted=x.acceptance.accepted.find(e.accepted_care_terms_ref);if(!accepted||accepted->accepted_id!=e.accepted_care_terms_ref||accepted->requirement_id!=e.requirement_id)fail("CareHandoverAcceptanceMismatch",e.handover_id);
 const auto ref=care_acceptance_source(accepted->accepted_id);header(common,ref);check(validate_care_arrangement_terms_shape(accepted->terms));auto prior=take(read_care_requirement_before_original_fact(x.acceptance.goals,e.requirement_id,ref));
 if(!prior.last_transition||!same(prior.last_transition->original_transition_ref,accepted->requirement_transition_ref)||prior.state==StateGoalState::withdrawn||prior.state==StateGoalState::invalid||take(compare_world_moments(accepted->accepted_at,now))>0)fail("CareHandoverAcceptedGoalPrefixMismatch",e.handover_id);
 // No call back to the acceptance expression/Condition validator here. The
 // full acceptance owner independently checks that source before publication.
 return *accepted;
}
WorldMoment context(const CareHandoverSourceContext& x){if((x.acceptance.goals.rules.selected_care_version()!=5&&x.acceptance.goals.rules.selected_care_version()!=6))fail("CareHandoverHistoryVersion5Required",{},ErrorKind::unsupported_capability);const auto now=take(x.acceptance.goals.clock.read_current_moment());if(!same(now,x.acceptance.body.actual_now))fail("CareHandoverActualClockMismatch");check(validate_time_scale(x.time_scale));return now;}
const SourceRef* cause_source(const FixedServiceTransition& row){return std::visit([](const auto& c)->const SourceRef*{using T=std::decay_t<decltype(c)>;if constexpr(std::is_same_v<T,FixedOwnerAdopted>||std::is_same_v<T,FixedOwnerStateChanged>)return &c.owner_source_ref;else if constexpr(std::is_same_v<T,FixedEligibilityChanged>)return &c.original_cause_ref;else return &c.owner_result_ref;},row.cause);}
Integer cause_schema(const FixedServiceTransition& row){return std::visit([](const auto& c)->Integer{using T=std::decay_t<decltype(c)>;if constexpr(std::is_same_v<T,FixedOwnerAdopted>||std::is_same_v<T,FixedOwnerStateChanged>)return c.owner_source_schema;else if constexpr(std::is_same_v<T,FixedEligibilityChanged>)return c.original_cause_schema;else return c.owner_result_schema;},row.cause);}
FixedTransitionKind time_kind(CareHandoverBoundaryKind kind){switch(kind){case CareHandoverBoundaryKind::started:return FixedTransitionKind::started;case CareHandoverBoundaryKind::suspended:return FixedTransitionKind::suspended;case CareHandoverBoundaryKind::resumed:return FixedTransitionKind::resumed;case CareHandoverBoundaryKind::input_rebound:return FixedTransitionKind::input_rebound;default:return FixedTransitionKind::terminal;}}
void original_chain(const CareHandoverSourceContext& x,const CareHandoverExecution& e,const WorldMoment& now,bool stable=false){
 check(validate_care_handover_execution_shape(e,5));const auto& accepted=acceptance_prefix(x,e,now);const auto* entry=find_fixed_elapsed(x.elapsed,e.original_action_ref);if(!entry||entry->execution_source_ref!=e.handover_id)fail("CareHandoverFixedEntryMissing",e.handover_id);
 auto required=take(fixed_duration_work_units(accepted.terms.handover_half_days,x.time_scale.units_per_half_day));if(take(compare_fractions(required.value,entry->required_units.value))!=0)fail("CareHandoverAcceptedDurationMismatch",e.handover_id);
 if(e.handover_execution_boundaries.empty()||!same(e.handover_execution_boundaries.front().occurred_at,entry->started_at)||take(compare_world_moments(accepted.accepted_at,entry->started_at))>0)fail("CareHandoverOriginalStartMismatch",e.handover_id);
 std::size_t count=0;for(const auto& transition:x.elapsed.fixed_service_transitions)if(transition.action_ref==e.original_action_ref){
  if(count>=e.handover_execution_boundaries.size())fail("CareHandoverOwnerBoundaryMissing",transition.transition_id);const auto& row=e.handover_execution_boundaries[count];const auto ref=care_handover_boundary_source(row.boundary_id);
  if(transition.transition_ordinal!=row.boundary_ordinal||transition.kind!=time_kind(row.kind)||!same(transition.occurred_at,row.occurred_at)||!same(transition.execution_source_ref,care_handover_execution_source(e.handover_id))||transition.execution_source_schema!=3||!same(*cause_source(transition),ref)||cause_schema(transition)!=1)fail("CareHandoverTimeOwnerBoundaryMismatch",row.boundary_id);
  if(row.kind==CareHandoverBoundaryKind::completed&&transition.terminal_disposition!=FixedTerminalDisposition::completed)fail("CareHandoverCompletedDispositionMismatch",row.boundary_id);
  if(row.kind==CareHandoverBoundaryKind::stopped&&(!transition.terminal_disposition||*transition.terminal_disposition==FixedTerminalDisposition::completed))fail("CareHandoverStoppedDispositionMismatch",row.boundary_id);
  for(const auto& worker:row.worker_capability_inputs)if(worker.actor_id==accepted.subject_actor_id)fail("CareDependentCannotBeWorker",worker.actor_id);
  if(take(compare_world_moments(row.occurred_at,now))>0)fail("CareHandoverFutureBoundary",row.boundary_id);++count;
 }
 if(count!=e.handover_execution_boundaries.size()){
  // Original effects may be read during the one actual completion-due write.
  // The outcome already exists; only its S0 adoption is pending. This cannot
  // pass the separate stable closure used by complete owner validation.
  const auto* running=std::get_if<ElapsedRunning>(&entry->timing);
  if(stable||count+1!=e.handover_execution_boundaries.size()||!running||
   !same(entry->last_settled_at,now)||!same(running->completion_due_at,now)||
   take(compare_fractions(entry->credited_units.value,entry->required_units.value))!=0)
   fail("CareHandoverS0BoundaryMissing",e.handover_id);
  const auto& last=e.handover_execution_boundaries.back();
  const auto* event=e.completion_event_ref?x.acceptance.goals.goals.temporal_events.find(e.completion_event_ref->id):nullptr;
  const auto* completed=event?std::get_if<CareHandoverCompletedEvent>(&event->body):nullptr;
  const auto* transition=event?x.acceptance.goals.goals.transitions.find(event->event_id):nullptr;
  if(last.kind!=CareHandoverBoundaryKind::completed||!same(last.occurred_at,now)||!event||!transition||
   !same(event->occurred_at,now)||!same(event->original_transition_ref,*e.completion_event_ref)||
   transition->kind!=CareRequirementTransition::Kind::handover_completed||
   transition->requirement_id!=e.requirement_id||!completed||completed->handover_id!=e.handover_id||
   completed->requirement_id!=e.requirement_id||e.work_period_sources.empty()||
   e.work_period_sources.size()!=static_cast<std::size_t>(accepted.terms.handover_half_days)||
   !same(e.work_period_sources.back().provided_at,now)||
   !same(completed->completion_work_period_ref.source_ref,care_handover_period_source(e.work_period_sources.back().source_id))||
   completed->completion_work_period_ref.source_schema!=2)
   fail("CareHandoverActualPendingCompletionRequired",e.handover_id);
  header(x.acceptance.goals.common,event->original_transition_ref);
 }
 // This invokes the original per-action structural history reader, including
 // the one permitted completion-due transition. No other action blocks it.
 take(read_fixed_service_credit_boundary(x.elapsed,e.original_action_ref,entry->credited_units,now,x.time_scale));
}
}
SourceRef care_handover_execution_source(const StableId& id){return {"care_requirements","care_handover_execution",id};}
SourceRef care_handover_boundary_source(const StableId& id){return {"care_requirements","care_handover_execution_boundary",id};}
SourceRef care_handover_period_source(const StableId& id){return {"care_requirements","care_handover_period",id};}
Outcome<Integer> care_handover_source_schema(const SourceRef& ref){if(ref.domain=="care_requirements"&&!ref.id.empty()){if(ref.kind=="care_handover_execution")return {true,3,{}};if(ref.kind=="care_handover_execution_boundary")return {true,1,{}};if(ref.kind=="care_handover_period")return {true,2,{}};}return {false,{},common_failure("UnknownCareHandoverSource",ref.id,ErrorKind::unsupported_capability).error};}
Status validate_care_handover_boundary_shape(const CareHandoverExecutionBoundary1& v){try{
 text(v.boundary_id);text(v.execution_ref);rule(v.transition_rule_ref);time(v.occurred_at);const auto kind=static_cast<Integer>(v.kind);if(v.source_schema!=1||v.boundary_ordinal<0||kind<0||kind>5||(v.boundary_ordinal==0)!=!v.previous_boundary_ref||(kind==0)!=(v.boundary_ordinal==0))fail("InvalidCareHandoverBoundary",v.boundary_id);if(v.previous_boundary_ref){text(*v.previous_boundary_ref);if(*v.previous_boundary_ref==v.boundary_id)fail("CareHandoverBoundarySelfReference",v.boundary_id);}
 if((kind==0&&v.cause.index()>1)||(kind>0&&kind<4&&v.cause.index()!=2)||(kind>=4&&v.cause.index()!=3))fail("CareHandoverBoundaryCauseMismatch",v.boundary_id);
 if(const auto* c=std::get_if<CareHandoverActorCommand>(&v.cause)){text(c->actor_id);text(c->command_binding.command_kind);text(c->command_binding.result_id);text(c->accepted_terms_ref);if(c->command_binding.request_seq<1)fail("CareHandoverRealCommandRequired",v.boundary_id);refs(c->authority_source_refs);}
 if(const auto* c=std::get_if<CareHandoverAcceptedObligation>(&v.cause)){source(c->original_obligation_ref);rule(c->accepted_rule_ref);if(c->original_obligation_source_schema<1)fail("InvalidCareHandoverObligation",v.boundary_id);}
 if(const auto* c=std::get_if<CareHandoverDependencyChanged>(&v.cause)){rule(c->continuation_rule_ref);std::visit([&](const auto& change){using T=std::decay_t<decltype(change)>;if constexpr(std::is_same_v<T,CareHandoverConditionChanged>)text(change.condition_receipt_ref.result_id);else if constexpr(std::is_same_v<T,CareHandoverDeathChanged>){if(change.death_ref.domain!="family_life"||change.death_ref.kind!="family_death")fail("InvalidCareHandoverDeath",v.boundary_id);text(change.death_ref.id);}else{source(change.source_ref);if(change.source_schema<1)fail("InvalidCareHandoverDependencySchema",v.boundary_id);}},c->change);}
 if(const auto* c=std::get_if<CareHandoverOriginalOutcome>(&v.cause)){source(c->outcome_source_ref);if(c->outcome_source_schema<1)fail("InvalidCareHandoverOutcome",v.boundary_id);}
 if(kind<4&&v.worker_capability_inputs.empty())fail("CareHandoverWorkersRequired",v.boundary_id);frames(v.worker_capability_inputs,v.occurred_at,true,kind==0||kind==2);refs(v.actual_participation_source_refs);refs(v.facility_use_binding_refs);if(!v.worker_capability_inputs.empty()&&v.actual_participation_source_refs.empty())fail("CareHandoverParticipationRequired",v.boundary_id);return common_success();
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Status validate_care_handover_period_shape(const CareHandoverWorkPeriod2& v){try{
 text(v.source_id);text(v.execution_ref);if(v.source_schema!=2)fail("InvalidCareHandoverPeriodSchema",v.source_id);check(validate_work_period_ref2(v.work_period_ref));const auto* b=std::get_if<ActorActionWorkBinding>(&v.work_period_ref.binding);if(!b||b->action_kind!="care_handover"||!same(b->source_owner_ref,care_handover_execution_source(v.execution_ref)))fail("CareHandoverPeriodBindingMismatch",v.source_id);time(v.provided_at);if(!same(v.provided_at,v.work_period_ref.until))fail("CareHandoverPeriodProvidedAtMismatch",v.source_id);if(v.worker_capability_inputs.empty())fail("CareHandoverPeriodWorkersRequired",v.source_id);frames(v.worker_capability_inputs,v.provided_at,false,true);
 for(std::size_t i=0;i<v.worker_condition_receipt_refs.size();++i){text(v.worker_condition_receipt_refs[i].result_id);for(std::size_t j=0;j<i;++j)if(v.worker_condition_receipt_refs[i].result_id==v.worker_condition_receipt_refs[j].result_id)fail("DuplicateCareHandoverConditionReceipt",v.source_id);}return common_success();
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Status validate_care_handover_execution_shape(const CareHandoverExecution& v,Integer version){try{
 if(version<3||version>6)fail("CareExactHandoverProfileRequired",v.handover_id,ErrorKind::unsupported_capability);text(v.handover_id);text(v.requirement_id);text(v.accepted_care_terms_ref);text(v.original_action_ref);binding(v);rule(v.handover_rule_ref);refs(v.actual_placement_result_refs);
 if(v.completion_event_ref){source(*v.completion_event_ref);if(v.completion_event_ref->domain!="care_requirements"||v.completion_event_ref->kind!="care_requirement_changed"||v.terminal_source_ref||v.actual_placement_result_refs.empty())fail("CareHandoverCompletionSourceMismatch",v.handover_id);}if(v.terminal_source_ref){source(v.terminal_source_ref->source_ref);if(v.terminal_source_ref->source_schema<1||!v.actual_placement_result_refs.empty())fail("CareHandoverStopSourceMismatch",v.handover_id);}
 if(version<5){if(!v.work_period_sources.empty()||!v.handover_execution_boundaries.empty())fail("CareHandoverHistoryRequiresVersion5",v.handover_id);return common_success();}
 if(v.handover_execution_boundaries.empty()||v.work_period_sources.size()>65536||v.handover_execution_boundaries.size()>65536)fail("CareHandoverCompleteBoundaryHistoryRequired",v.handover_id);bool suspended=false,terminal=false;
 for(std::size_t i=0;i<v.handover_execution_boundaries.size();++i){const auto& row=v.handover_execution_boundaries[i];check(validate_care_handover_boundary_shape(row));if(row.execution_ref!=v.handover_id||row.boundary_ordinal!=static_cast<Integer>(i)||terminal||(i&&(!row.previous_boundary_ref||*row.previous_boundary_ref!=v.handover_execution_boundaries[i-1].boundary_id||take(compare_world_moments(v.handover_execution_boundaries[i-1].occurred_at,row.occurred_at))>0)))fail("CareHandoverBoundaryChainMismatch",row.boundary_id);for(std::size_t j=0;j<i;++j)if(v.handover_execution_boundaries[j].boundary_id==row.boundary_id)fail("DuplicateCareHandoverBoundary",row.boundary_id);
  if(row.kind==CareHandoverBoundaryKind::suspended){if(suspended)fail("CareHandoverAlreadySuspended",row.boundary_id);suspended=true;}else if(row.kind==CareHandoverBoundaryKind::resumed){if(!suspended)fail("CareHandoverNotSuspended",row.boundary_id);suspended=false;}else if(row.kind==CareHandoverBoundaryKind::input_rebound&&!suspended)for(const auto& worker:row.worker_capability_inputs)if(!worker.result.allowed)fail("CareHandoverRunningWorkerDisallowed",worker.actor_id);
  if(row.kind==CareHandoverBoundaryKind::completed||row.kind==CareHandoverBoundaryKind::stopped){terminal=true;const auto& cause=std::get<CareHandoverOriginalOutcome>(row.cause);if(row.kind==CareHandoverBoundaryKind::completed){if(!v.completion_event_ref||!same(cause.outcome_source_ref,*v.completion_event_ref)||cause.outcome_source_schema!=3)fail("CareHandoverCompletedEventMismatch",row.boundary_id);}else if(!v.terminal_source_ref||!same(cause.outcome_source_ref,v.terminal_source_ref->source_ref)||cause.outcome_source_schema!=v.terminal_source_ref->source_schema)fail("CareHandoverStoppedResultMismatch",row.boundary_id);}
 }
 if(terminal!=bool(v.completion_event_ref||v.terminal_source_ref))fail("CareHandoverTerminalBoundaryMissing",v.handover_id);
 for(std::size_t i=0;i<v.work_period_sources.size();++i){const auto& row=v.work_period_sources[i];check(validate_care_handover_period_shape(row));if(row.execution_ref!=v.handover_id||row.work_period_ref.period_ordinal!=static_cast<Integer>(i)||!same_work_binding(row.work_period_ref.binding,v.execution_binding)||(i&&take(compare_world_moments(v.work_period_sources[i-1].provided_at,row.work_period_ref.from))>0)||take(compare_world_moments(v.handover_execution_boundaries.front().occurred_at,row.work_period_ref.from))>0||(terminal&&take(compare_world_moments(row.provided_at,v.handover_execution_boundaries.back().occurred_at))>0))fail("CareHandoverPeriodOrderMismatch",row.source_id);for(std::size_t j=0;j<i;++j)if(v.work_period_sources[j].source_id==row.source_id)fail("DuplicateCareHandoverPeriod",row.source_id);}
 return common_success();
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Status validate_care_handover_time_source_closure(const CareHandoverSourceContext& x){try{
 const auto now=context(x);for(const auto& pair:x.executions.rows()){
  if(pair.first!=pair.second.handover_id)fail("CareHandoverExecutionKeyMismatch",pair.first);
  original_chain(x,pair.second,now,true);
 }
 return common_success();
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Outcome<const CareHandoverExecutionBoundary1*> read_care_handover_boundary_for_original_effect(const CareHandoverSourceContext& x,const SourceRef& ref,Integer schema){try{
 if(ref.domain!="care_requirements"||ref.kind!="care_handover_execution_boundary"||schema!=1)fail("UnknownCareHandoverBoundary",ref.id,ErrorKind::unsupported_capability);const auto now=context(x);const CareHandoverExecutionBoundary1* found=nullptr;for(const auto& pair:x.executions.rows())for(const auto& row:pair.second.handover_execution_boundaries)if(row.boundary_id==ref.id){if(found||pair.first!=pair.second.handover_id)fail("DuplicateCareHandoverBoundary",ref.id);original_chain(x,pair.second,now);found=&row;}if(!found)fail("CareHandoverBoundaryMissing",ref.id);return {true,found,{}};
 }catch(Failure& e){return {false,nullptr,std::move(e.error)};}}
Outcome<const CareHandoverWorkPeriod2*> read_care_handover_period_for_original_effect(const CareHandoverSourceContext& x,const SourceRef& ref,Integer schema){try{
 if(ref.domain!="care_requirements"||ref.kind!="care_handover_period"||schema!=2)fail("UnknownCareHandoverPeriod",ref.id,ErrorKind::unsupported_capability);const auto now=context(x);const CareHandoverWorkPeriod2* found=nullptr;for(const auto& pair:x.executions.rows())for(const auto& row:pair.second.work_period_sources)if(row.source_id==ref.id){if(found||pair.first!=pair.second.handover_id)fail("DuplicateCareHandoverPeriod",ref.id);const auto& e=pair.second;original_chain(x,e,now);const auto& accepted=acceptance_prefix(x,e,now);const auto& b=std::get<ActorActionWorkBinding>(e.execution_binding);auto actual=take(read_fixed_service_work_period2(x.elapsed,b,row.work_period_ref.period_ordinal,accepted.terms.handover_half_days,now,x.time_scale));if(!take(same_work_period_ref2(actual.ref,row.work_period_ref))||!same(actual.ref.until,row.provided_at)||!actual.actual_service.complete)fail("CareHandoverOriginalPeriodMismatch",ref.id);for(const auto& worker:row.worker_capability_inputs)if(worker.actor_id==accepted.subject_actor_id)fail("CareDependentCannotBeWorker",worker.actor_id);found=&row;}if(!found)fail("CareHandoverPeriodMissing",ref.id);return {true,found,{}};
 }catch(Failure& e){return {false,nullptr,std::move(e.error)};}}
Outcome<CareHandoverPlacementInput> read_care_handover_placement_input(const CareHandoverSourceContext& x,const SourceRef& ref,Integer schema){try{
 if(ref.domain!="care_requirements"||ref.kind!="care_handover_execution"||schema!=3)fail("CareHandoverPlacementOriginalExecutionRequired",ref.id,ErrorKind::unsupported_capability);
 const auto now=context(x);const auto& c=x.acceptance.goals.common;header(c,ref);
 const auto* e=x.executions.find(ref.id);if(!e||e->handover_id!=ref.id)fail("CareHandoverExecutionMissing",ref.id);
 original_chain(x,*e,now);const auto& accepted=acceptance_prefix(x,*e,now);
 if(e->terminal_source_ref||e->work_period_sources.empty()||e->work_period_sources.size()!=static_cast<std::size_t>(accepted.terms.handover_half_days))fail("CareHandoverPlacementRequiresAllActualPeriods",ref.id);
 const auto& ids=c.state().facts.order();const auto accepted_id=std::find(ids.begin(),ids.end(),accepted.accepted_id);
 if(accepted_id==ids.end()||accepted_id>=std::find(ids.begin(),ids.end(),ref.id))fail("CareHandoverPlacementAcceptanceOrderMismatch",ref.id);
 for(const auto& period:e->work_period_sources)take(read_care_handover_period_for_original_effect(x,care_handover_period_source(period.source_id),2));
 const auto& last=e->work_period_sources.back();const auto& at=last.provided_at;
 if(take(compare_world_moments(at,WorldMoment{accepted.terms.from_tick,{}}))<0||(accepted.terms.until_tick&&take(compare_world_moments(at,WorldMoment{*accepted.terms.until_tick,{}}))>=0))fail("CareHandoverPlacementOutsideAcceptedWindow",ref.id);
 const auto* entry=find_fixed_elapsed(x.elapsed,e->original_action_ref);
 if(!entry||take(compare_fractions(entry->credited_units.value,entry->required_units.value))!=0)fail("CareHandoverPlacementFullCreditRequired",ref.id);
 const CareTemporalEvent* completed=nullptr;
 if(e->completion_event_ref){
  completed=x.acceptance.goals.goals.temporal_events.find(e->completion_event_ref->id);
  const auto* body=completed?std::get_if<CareHandoverCompletedEvent>(&completed->body):nullptr;
  if(!completed||!body||body->handover_id!=e->handover_id||body->requirement_id!=e->requirement_id||!same(completed->occurred_at,at)||!same(completed->original_transition_ref,*e->completion_event_ref)||!same(body->completion_work_period_ref.source_ref,care_handover_period_source(last.source_id))||body->completion_work_period_ref.source_schema!=2)fail("CareHandoverPlacementCompletedSourceMismatch",ref.id);
  header(c,completed->original_transition_ref);
 }else{
  const auto* running=std::get_if<ElapsedRunning>(&entry->timing);
  const auto* action=c.action(e->original_action_ref);
  if(!running||!same(running->completion_due_at,now)||!same(entry->last_settled_at,now)||!same(at,now)||!action||action->id!=e->original_action_ref||action->kind!="care_handover")fail("CareHandoverPlacementRequiresActualCompletionDue",ref.id);
 }
 return {true,{e,&accepted,&last,completed},{}};
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Status validate_care_handover_placement_adoption_cut(const CareHandoverSourceContext& x,
 const SourceRef& execution,Integer schema,const MomentOwnerSource& result,const WorldMoment& at){try{
 const auto input=take(read_care_handover_placement_input(x,execution,schema));
 const auto& c=x.acceptance.goals.common;const auto& e=*input.execution;
 if(result.source_schema!=2||result.source_ref.domain!="travel"||result.source_ref.kind!="travel_dependent_binding_change")
  fail("CarePlacementOriginalS1ResultRequired",result.source_ref.id,ErrorKind::unsupported_capability);
 header(c,result.source_ref);
 if(!same(at,input.last_actual_period->provided_at))fail("CarePlacementActualMomentMismatch",result.source_ref.id);
 // These are the two original writers' finite next_id protocols, not an
 // ordering of arbitrary SourceRef IDs or a substitute for source validation.
 auto serial=[&](const StableId& id,std::string_view prefix){
  if(id.size()<=prefix.size()+1||id.compare(0,prefix.size(),prefix)!=0||id[prefix.size()]!=':')fail("CarePlacementOriginalAllocationRequired",id);
  const auto* first=id.data()+prefix.size()+1;const auto* end=id.data()+id.size();Integer n=0;
  const auto parsed=std::from_chars(first,end,n);
  if(*first=='0'||parsed.ec!=std::errc{}||parsed.ptr!=end||n<1||n>=c.state().next_instance_id)
   fail("CarePlacementOriginalAllocationRequired",id);
  return n;
 };
 const auto result_serial=serial(result.source_ref.id,"travel_dependent_change");
 Integer preceding=serial(e.handover_id,"care_handover");
 for(const auto& period:e.work_period_sources){
  const auto n=serial(period.source_id,"care_handover_period");
  if(n<=preceding||n>=result_serial||c.state().facts.find(period.source_id))fail("CarePlacementActualPeriodCutMismatch",period.source_id);
  std::size_t count=0;for(const auto& pair:x.executions.rows())for(const auto& retained:pair.second.work_period_sources)if(retained.source_id==period.source_id)++count;
  if(count!=1)fail("CarePlacementAmbiguousOriginalPeriod",period.source_id);
  preceding=n;
 }
 const auto& order=c.state().facts.order();
 if(std::find(order.begin(),order.end(),e.handover_id)>=std::find(order.begin(),order.end(),result.source_ref.id))
  fail("CarePlacementActualExecutionCutMismatch",e.handover_id);
 if(input.actual_completion){
  const auto& completion=*input.actual_completion;const auto& body=std::get<CareHandoverCompletedEvent>(completion.body);
  if(std::find(order.begin(),order.end(),result.source_ref.id)>=std::find(order.begin(),order.end(),completion.original_transition_ref.id)||
   body.placement_result_ref.source_schema!=result.source_schema||!same(body.placement_result_ref.source_ref,result.source_ref))
   fail("CarePlacementActualCompletionCutMismatch",e.handover_id);
  std::size_t matches=0;for(const auto& retained:e.actual_placement_result_refs)if(retained.source_schema==result.source_schema&&same(retained.source_ref,result.source_ref))++matches;
  if(matches!=1)fail("CarePlacementCompletedResultMissing",e.handover_id);
 }
 return common_success();
 }catch(Failure& error){return {false,{},std::move(error.error)};}}

}
