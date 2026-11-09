#include "care_handover_completion.hpp"
#include "care_family_activity_read.hpp"
#include "care_participant_capacity.hpp"
#include "resident/domains/actor_foundation/authored_origin.hpp"
#include "resident/kernel/common_action_occupation.hpp"
#include <limits>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
void check(Status value){if(!value.ok)throw Failure{std::move(value.error)};}
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
bool same(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b))==0;}
bool same(const MomentOwnerSource& a,const MomentOwnerSource& b){return a.source_schema==b.source_schema&&same_source_ref(a.source_ref,b.source_ref);}
bool same(const std::vector<MomentOwnerSource>& a,const std::vector<MomentOwnerSource>& b){if(a.size()!=b.size())return false;for(std::size_t n=0;n<a.size();++n)if(!same(a[n],b[n]))return false;return true;}
SourceRef transition_source(const StableId& id){return {"care_requirements","care_requirement_changed",id};}
void source(const MomentOwnerSource& value){if(value.source_ref.domain.empty()||value.source_ref.kind.empty()||value.source_ref.id.empty()||value.source_schema<1)fail("CareHandoverActualCompletionSourceRequired",value.source_ref.id);}
void refs(const std::vector<MomentOwnerSource>& values){if(values.size()>65536)fail("CareHandoverCompletionSourcesTooLarge");for(std::size_t n=0;n<values.size();++n){source(values[n]);for(std::size_t p=0;p<n;++p)if(same_source_ref(values[n].source_ref,values[p].source_ref))fail("CareHandoverDuplicateCompletionSource",values[n].source_ref.id);}}
void input(const CareHandoverCompletionInputs& v,bool native_activity){if(!native_activity)source(v.care_activity_ref);else if(!v.care_activity_ref.source_ref.id.empty())source(v.care_activity_ref);source(v.placement_result_ref);refs(v.actual_participation_refs);refs(v.fee_result_refs);if(v.actual_participation_refs.empty()||v.transition_rule_ref.id.empty()||v.transition_rule_ref.version<1||v.transition_rule_ref.catalog_digest.empty())fail("CareHandoverOriginalCompletionInputsRequired");}
// This proof is private to the real completion writer. It cannot be supplied
// by a caller or survive a candidate. Every original gate is read while the
// handover Action is still present; adoption follows its actual release.
struct PreparedCareFamilyActivity {CareFamilyActivity row;};
PreparedCareFamilyActivity prepare_activity(CommonStore& common,
 CareFamilyActivityWriteBinding& write,const CareCoverageSources& coverage,
 const CareHandoverExecution& execution,const AcceptedCaregiver& accepted,
 const CareHandoverCompletionInputs& supplied,const WorldMoment& now){
 const auto& x=write.sources;const auto& g=coverage.handovers.acceptance.goals;
 if(&write.activities!=&x.activities||&x.coverage!=&coverage||
  &x.commands.handovers!=&coverage.handovers||!x.authority.bound_to(common)||
  g.rules.selected_care_version()!=6)fail("CareActivitySameOriginalOwnersRequired",execution.handover_id,ErrorKind::unsupported_capability);
 if(!supplied.care_activity_ref.source_ref.id.empty())fail("CareActivityMustUseActualStartWriter",execution.handover_id);
 for(const auto& pair:write.activities.rows()){
  if(same(pair.second.actual_handover_execution_ref,MomentOwnerSource{care_handover_execution_source(execution.handover_id),3}))fail("CareActivityCompletionMissing",execution.handover_id);
  if(!pair.second.ended_by&&(pair.second.carer_actor_id==accepted.provider.carer_actor_id||pair.second.child_actor_id==accepted.subject_actor_id))fail("CareActivityAlreadyOccupied",execution.handover_id,ErrorKind::rule);
 }
 if(execution.handover_execution_boundaries.empty()||execution.work_period_sources.empty())fail("CareActivityOriginalServiceRequired",execution.handover_id);
 check(validate_care_handover_command_source(x.commands,execution,execution.handover_execution_boundaries.front()));
 check(validate_current_care_participants(coverage.handovers.acceptance.body,accepted.subject_actor_id,accepted.provider.carer_actor_id,now));
 check(x.authority.validate_current_activity_authority(accepted,now));
 check(x.authority.validate_actual_handover_inputs(execution,accepted,now,supplied.actual_participation_refs,supplied.fee_result_refs));
 PreparedCareFamilyActivity result;
 auto& row=result.row;row.activity_id=take(common.next_id("care_family_activity"));
 row.accepted_care_terms_ref=accepted.accepted_id;row.child_actor_id=accepted.subject_actor_id;
 row.carer_actor_id=accepted.provider.carer_actor_id;
 row.actual_handover_execution_ref={care_handover_execution_source(execution.handover_id),3};
 row.actual_start_work_period_ref={care_handover_period_source(execution.work_period_sources.back().source_id),2};
 row.actual_placement_result_ref=supplied.placement_result_ref;row.started_at=now;
 row.compatibility_rule_ref=take(original_exclusive_family_care_rule(coverage.handovers.acceptance.body.catalog.identity));
 row.adopted_participation_refs=supplied.actual_participation_refs;
 check(validate_care_family_activity_shape(row,6));return result;
}
void adopt_activity(CommonStore& common,CareFamilyActivityWriteBinding& write,
 PreparedCareFamilyActivity&& prepared,const StableId& released_action){
 const auto& row=prepared.row;const auto* actor=common.actor(row.carer_actor_id);
 if(common.action(released_action)||!actor||!actor->action_id.empty())fail("CareActivityOriginalHandoverReleaseRequired",row.activity_id);
 const auto id=row.activity_id;
 if(!write.activities.insert(id,std::move(prepared.row)))fail("DuplicateCareFamilyActivity",id);
 check(common.append_fact({id,"care_family_activity_started","care_requirements"}));
}
// Care proves the exact original participants and occupation first. S0 owns
// the shared reservation/occupation release and final Action-head removal.
void release(CommonStore& c,const ActionTimeOwnerProof& owner){
 const auto* row=c.action(owner.action_ref);if(!row||row->kind!="care_handover"||row->actor_id!=owner.actor_id||row->participants!=owner.participant_actor_ids)fail("CareHandoverActualReleaseMismatch",owner.action_ref);
 for(const auto& actor:owner.participant_actor_ids){const auto* actual=c.actor(actor);if(!actual||actual->action_id!=owner.action_ref)fail("CareHandoverWorkerOccupationMismatch",actor);}
 check(release_action_occupation_in_candidate(c,owner.action_ref));
 check(erase_released_action_in_candidate(c,owner.action_ref));
 if(c.action(owner.action_ref))fail("CareHandoverReleaseIncomplete",owner.action_ref);
}
}
Outcome<CareRequirementApplication> complete_care_handover_in_candidate(CommonStore& common,CareGoalWriteTables goals,CareCoverageWriteTables coverage,OrderedTable<CareHandoverExecution>& executions,ActionElapsedStore& elapsed,const CareCoverageSources& x,const CareHandoverBusinessRead& business,const StableId& id,const CareHandoverCompletionInputs& supplied,CareFamilyActivityWriteBinding* activities){try{
 const auto& h=x.handovers;const auto& g=h.acceptance.goals;
 if(&common!=&g.common||g.coverage!=&x||&goals.requirements!=&g.goals.requirements||&goals.transitions!=&g.goals.transitions||&goals.temporal_events!=&g.goals.temporal_events||&coverage.bindings!=&x.tables.bindings||&coverage.satisfied!=&x.tables.satisfied||&coverage.lost!=&x.tables.lost||&executions!=&h.executions||&elapsed!=&h.elapsed)fail("CareHandoverSameCandidateRequired",id);
 if((g.rules.selected_care_version()!=5&&g.rules.selected_care_version()!=6)||!business.bound_to(common)||!x.originals.bound_to(common)||!x.periods.bound_to(common))fail("CareHandoverActualCompletionOwnersRequired",id,ErrorKind::unsupported_capability);
 const bool native_activity=g.rules.selected_care_version()==6;
 if(native_activity&&(!activities||&activities->sources.coverage!=&x||&activities->activities!=&activities->sources.activities||x.originals.family_activity_sources()!=&activities->sources))fail("CareFamilyActivityOriginalWriterRequired",id,ErrorKind::unsupported_capability);
 if(!native_activity&&activities)fail("CareFamilyActivityUnexpectedWriter",id);
 input(supplied,native_activity);const auto now=take(g.clock.read_current_moment());if(!same(now,h.acceptance.body.actual_now))fail("CareHandoverActualClockMismatch",id);
 auto* e=executions.find(id);if(!e||e->handover_id!=id)fail("CareHandoverExecutionMissing",id);
 const auto reg=take(make_care_handover_action_time_registration(5));CareHandoverActionTimeRead time(h,business);
 if(e->completion_event_ref){
  const auto* event=goals.temporal_events.find(e->completion_event_ref->id);const auto* completed=event?std::get_if<CareHandoverCompletedEvent>(&event->body):nullptr;const auto* binding=completed?coverage.bindings.find(completed->binding_id):nullptr;const auto* goal=goals.requirements.find(e->requirement_id);const auto* transition=event?goals.transitions.find(event->event_id):nullptr;
  if(!event||!completed||!binding||!goal||!transition||((!native_activity||!supplied.care_activity_ref.source_ref.id.empty())&&!same(binding->care_activity_ref,supplied.care_activity_ref))||!same(binding->placement_ref,supplied.placement_result_ref)||!same(completed->actual_participation_refs,supplied.actual_participation_refs)||!same(completed->fee_result_refs,supplied.fee_result_refs)||e->handover_execution_boundaries.empty()||!actor_foundation_rule_equal(e->handover_execution_boundaries.back().transition_rule_ref,supplied.transition_rule_ref))fail("ConflictingCareHandoverCompletion",id);
  check(validate_original_care_handover_coverage(x,*goal,*transition,*event));check(time.validate_retained_fixed_sources(reg,e->original_action_ref));return {true,{goal->id,event->original_transition_ref,true},{}};
 }
 if(e->terminal_source_ref)fail("CareHandoverAlreadyStopped",id);
 auto owner=take(time.read_action_time_owner(reg,e->original_action_ref));if(owner.phase!=ActionTimeOwnerPhase::working)fail("CareHandoverCompletionRequiresActualRunning",id);check(time.validate_retained_fixed_sources(reg,e->original_action_ref));check(validate_care_handover_worker_history(h,id));
 const auto* accepted=take(read_accepted_caregiver(h.acceptance,care_acceptance_source(e->accepted_care_terms_ref)));auto* goal=goals.requirements.find(e->requirement_id);
 if(!goal||goal->subject_actor_id!=accepted->subject_actor_id||goal->state!=StateGoalState::unsatisfied||goal->current_care_binding_ref||goal->accepted_care_terms_ref)fail("CareHandoverCurrentUnsatisfiedRequirementRequired",id);
 take(read_care_requirement_goal(g,{goal->id,goal->subject_actor_id}));
 if(take(compare_world_moments(now,WorldMoment{accepted->terms.from_tick,{}}))<0||(accepted->terms.until_tick&&take(compare_world_moments(now,WorldMoment{*accepted->terms.until_tick,{}}))>=0))fail("CareHandoverCompletionOutsideAcceptedWindow",id);
 const auto* entry=find_fixed_elapsed(elapsed,e->original_action_ref);const auto* running=entry?std::get_if<ElapsedRunning>(&entry->timing):nullptr;
 if(!entry||!running||!same(entry->last_settled_at,now)||!same(running->completion_due_at,now)||take(compare_fractions(entry->credited_units.value,entry->required_units.value))!=0||e->work_period_sources.size()!=static_cast<std::size_t>(accepted->terms.handover_half_days)||e->work_period_sources.empty()||!same(e->work_period_sources.back().provided_at,now))fail("CareHandoverActualFullServiceDueRequired",id);
 // Read every original full period and its actual costs before authoring any
 // completion. No remaining-time or predicted due is a supplied period.
 for(const auto& period:e->work_period_sources){take(read_care_handover_period_for_original_effect(h,care_handover_period_source(period.source_id),2));const auto& binding=std::get<ActorActionWorkBinding>(e->execution_binding);auto actual=take(read_fixed_service_work_period2(elapsed,binding,period.work_period_ref.period_ordinal,accepted->terms.handover_half_days,now,h.time_scale));check(x.periods.validate_period_inputs(*e,*accepted,actual));check(x.periods.validate_period_effects(*e,*accepted,period));}
 if(goal->revision==std::numeric_limits<Integer>::max()||e->handover_execution_boundaries.size()>=65536)fail("CareHandoverCompletionCounterOverflow",id);
 for(const auto& pair:coverage.bindings.rows())if(pair.second.child_actor_id==goal->subject_actor_id&&pair.second.phase==CareBindingPhase::active)fail("CareDependentAlreadyHasActiveBinding",goal->subject_actor_id);
 std::optional<PreparedCareFamilyActivity> prepared;
 if(native_activity)prepared.emplace(prepare_activity(common,*activities,x,*e,*accepted,supplied,now));
 const auto activity_ref=prepared?MomentOwnerSource{care_family_activity_started_source(prepared->row.activity_id),1}:supplied.care_activity_ref;
 const auto binding_id=take(common.next_id("care_binding"));const auto satisfied_id=take(common.next_id("care_coverage_satisfied"));const auto event_id=take(common.next_id("care_transition"));const auto boundary_id=take(common.next_id("care_handover_boundary"));
 CareBinding binding{binding_id,goal->subject_actor_id,accepted->provider.carer_actor_id,accepted->accepted_id,accepted->accepted_id,now.half_day_tick,accepted->terms.until_tick,activity_ref,supplied.placement_result_ref,CareBindingPhase::active};check(validate_care_binding_shape(binding));
 CareCoverageSatisfied satisfaction{satisfied_id,goal->id,goal->source_episode,binding_id,transition_source(event_id),now.half_day_tick};check(validate_care_coverage_satisfied_shape(satisfaction));
 CareRequirementTransition transition{event_id,goal->id,goal->revision,goal->revision+1,goal->source_episode,goal->source_episode,CareRequirementTransition::Kind::handover_completed,goal->current_transition_ref,care_coverage_satisfied_source(satisfied_id)};
 CareTemporalEvent event{event_id,transition_source(event_id),now,goal->rule_version,CareHandoverCompletedEvent{id,goal->id,goal->source_episode,binding_id,satisfied_id,{care_handover_period_source(e->work_period_sources.back().source_id),2},supplied.actual_participation_refs,supplied.placement_result_ref,supplied.fee_result_refs}};
 CareHandoverExecutionBoundary1 boundary;boundary.boundary_id=boundary_id;boundary.execution_ref=id;boundary.boundary_ordinal=static_cast<Integer>(e->handover_execution_boundaries.size());boundary.previous_boundary_ref=e->handover_execution_boundaries.back().boundary_id;boundary.kind=CareHandoverBoundaryKind::completed;boundary.occurred_at=now;boundary.transition_rule_ref=supplied.transition_rule_ref;boundary.cause=CareHandoverOriginalOutcome{event.original_transition_ref,3};boundary.actual_participation_source_refs=supplied.actual_participation_refs;
 boundary.field_order={"boundary_id","source_schema","execution_ref","boundary_ordinal","previous_boundary_ref","kind","occurred_at","transition_rule_ref","cause","worker_capability_inputs","actual_participation_source_refs","facility_use_binding_refs","field_order"};
 // Real placement and ongoing activity are checked independently, before the
 // new satisfaction becomes a current goal. These reads may not use Terminal.
 check(validate_care_handover_actual_placement(x,*e,*accepted,binding,event));
 if(native_activity){
  release(common,owner);
  adopt_activity(common,*activities,std::move(*prepared),e->original_action_ref);
 }else check(x.originals.validate_original_completed_handover(*e,*accepted,binding,event));
 if(!coverage.bindings.insert(binding_id,std::move(binding))||!coverage.satisfied.insert(satisfied_id,std::move(satisfaction)))fail("DuplicateCareHandoverCompletion",id);check(common.append_fact({satisfied_id,"care_coverage_satisfied","care_requirements"}));
 if(!goals.transitions.insert(event_id,std::move(transition))||!goals.temporal_events.insert(event_id,std::move(event)))fail("DuplicateCareHandoverCompletion",id);check(common.append_fact({event_id,"care_requirement_changed","care_requirements"}));
 e->completion_event_ref=transition_source(event_id);e->actual_placement_result_refs.push_back(supplied.placement_result_ref);e->handover_execution_boundaries.push_back(std::move(boundary));check(validate_care_handover_execution_shape(*e,5));
 if(!native_activity)release(common,owner);++goal->revision;goal->state=StateGoalState::satisfied;goal->accepted_care_terms_ref=accepted->accepted_id;goal->current_care_binding_ref=binding_id;goal->current_transition_ref=transition_source(event_id);
 check(validate_original_care_handover_coverage(x,*goal,*goals.transitions.find(event_id),*goals.temporal_events.find(event_id)));
 take(adopt_care_handover_boundary_in_candidate(common,executions,elapsed,h,business,boundary_id));check(time.validate_retained_fixed_sources(reg,e->original_action_ref));check(validate_current_care_coverage(x,*goal,now));take(read_care_requirement_goal(g,{goal->id,goal->subject_actor_id}));
 return {true,{goal->id,goal->current_transition_ref,false},{}};
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Outcome<BirthCareNeedApplication> complete_care_handover_and_need_in_candidate(CommonStore& common,CareGoalWriteTables goals,CareCoverageWriteTables coverage,OrderedTable<CareHandoverExecution>& executions,ActionElapsedStore& elapsed,const CareCoverageSources& x,const CareHandoverBusinessRead& business,const StableId& handover,const CareHandoverCompletionInputs& inputs,AffairsState& affairs,const StateNeedPolicyRead& policies,const StateGoalRead& goal_read,StateTaskConsumer& tasks,CareFamilyActivityWriteBinding* activities){
 auto applied=complete_care_handover_in_candidate(common,goals,coverage,executions,elapsed,x,business,handover,inputs,activities);if(!applied.ok)return {false,{},std::move(applied.error)};
 return reconcile_applied_care_goal_in_candidate(common,goals,x.handovers.acceptance.goals,std::move(applied.value),affairs,policies,goal_read,tasks);
}
}
