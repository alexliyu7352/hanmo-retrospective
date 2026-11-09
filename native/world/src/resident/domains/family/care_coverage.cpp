#include "care_coverage.hpp"
#include "care_family_activity_read.hpp"
#include "care_personal_stop.hpp"
#include "care_growth_participation.hpp"
#include "care_separation.hpp"
#include "care_participant_capacity.hpp"
#include <algorithm>
#include <limits>
#include <tuple>
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
void check(Status value){if(!value.ok)throw Failure{std::move(value.error)};}
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
bool same(const SourceRef& a,const SourceRef& b){return std::tie(a.domain,a.kind,a.id)==std::tie(b.domain,b.kind,b.id);}
bool same(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b))==0;}
bool same(const MomentOwnerSource& a,const MomentOwnerSource& b){return a.source_schema==b.source_schema&&same(a.source_ref,b.source_ref);}
void text(const StableId& id){if(id.empty()||id.size()>256)fail("InvalidCareCoverageIdentifier",id);}
void source(const SourceRef& ref){text(ref.domain);text(ref.kind);text(ref.id);}
void source(const MomentOwnerSource& ref){source(ref.source_ref);if(ref.source_schema<1)fail("InvalidCareCoverageSourceSchema",ref.source_ref.id);}
SourceRef transition_source(const StableId& id){return {"care_requirements","care_requirement_changed",id};}
const FactRecord& header(const CommonStore& c,const SourceRef& ref){const auto* row=c.state().facts.find(ref.id);if(!row||row->id!=ref.id||row->domain!=ref.domain||row->kind!=ref.kind||std::count(c.state().facts.order().begin(),c.state().facts.order().end(),ref.id)!=1)fail("CareCoverageOriginalFactRequired",ref.id);return *row;}
void before(const CommonStore& c,const SourceRef& a,const SourceRef& b){header(c,a);header(c,b);const auto& ids=c.state().facts.order();if(std::find(ids.begin(),ids.end(),a.id)>=std::find(ids.begin(),ids.end(),b.id))fail("CareCoverageOriginalCauseOrderMismatch",b.id);}
const CareGoalSourceContext& context(const CareCoverageSources& x){const auto& goal=x.handovers.acceptance.goals;if(goal.coverage!=&x||!x.originals.bound_to(goal.common)||!x.periods.bound_to(goal.common)||(goal.rules.selected_care_version()!=5&&goal.rules.selected_care_version()!=6))fail("CareCoverageActualOwnerSourcesRequired",{},ErrorKind::unsupported_capability);return goal;}
const CareTemporalEvent& event(const CareGoalSourceContext& x,const SourceRef& ref){take(care_goal_transition_source_schema(ref));header(x.common,ref);const auto* row=x.goals.temporal_events.find(ref.id);const auto* tr=x.goals.transitions.find(ref.id);if(!row||!tr||row->event_id!=ref.id||tr->transition_id!=ref.id||!same(row->original_transition_ref,ref))fail("CareCoverageTemporalEventRequired",ref.id);return *row;}
const CareCoverageSatisfied& satisfaction(const CareCoverageSources& x,const StableId& binding){const CareCoverageSatisfied* result=nullptr;for(const auto& pair:x.tables.satisfied.rows())if(pair.second.binding_ref==binding){if(result||pair.first!=pair.second.fact_id)fail("CareBindingSatisfactionNotUnique",binding);result=&pair.second;}if(!result)fail("CareBindingSatisfactionMissing",binding);return *result;}
const CareCoverageLost* loss(const CareCoverageSources& x,const StableId& binding){const CareCoverageLost* result=nullptr;for(const auto& pair:x.tables.lost.rows())if(pair.second.old_binding_ref==binding){if(result||pair.first!=pair.second.fact_id)fail("CareBindingLossNotUnique",binding);result=&pair.second;}return result;}
const CareTemporalEvent& loss_event(const CareCoverageSources& x,const CareCoverageLost& row){const auto& g=context(x);const CareTemporalEvent* result=nullptr;for(const auto& pair:g.goals.temporal_events.rows())if(const auto* lost=std::get_if<CareCoverageLostEvent>(&pair.second.body);lost&&lost->lost_fact_id==row.fact_id){if(result||pair.first!=pair.second.event_id)fail("CareCoverageLossEventNotUnique",row.fact_id);result=&pair.second;}if(!result)fail("CareCoverageLossEventMissing",row.fact_id);return *result;}
void transition_identity(const CareGoalSourceContext& g,const CareRequirement& goal,const CareRequirementTransition& tr,const CareTemporalEvent& ev){
 if(tr.requirement_id!=goal.id||ev.event_id!=tr.transition_id||!same(ev.original_transition_ref,transition_source(tr.transition_id))||g.goals.transitions.find(tr.transition_id)!=&tr||g.goals.temporal_events.find(ev.event_id)!=&ev||!actor_foundation_rule_equal(ev.rule_ref,goal.rule_version)||take(compare_world_moments(ev.occurred_at,take(g.clock.read_current_moment())))>0)fail("CareCoverageActualTransitionIdentityMismatch",tr.transition_id);
 header(g.common,ev.original_transition_ref);const auto* rule=take(g.rules.read_care_requirement_rule(ev.rule_ref));if(!rule||!actor_foundation_rule_equal(rule->rule_ref,ev.rule_ref))fail("CareCoverageActualRuleRequired",tr.transition_id);
}
void end_activity(CommonStore& common,CareCoverageWriteTables tables,const CareGoalSourceContext& g,const CareBinding& binding,const CareLossCauseV2& cause){
 if(g.rules.selected_care_version()!=6)return;
 if(!tables.family_activities||!g.coverage||&tables.family_activities->sources.coverage!=g.coverage)fail("CareFamilyActivityOriginalWriterRequired",binding.binding_id,ErrorKind::unsupported_capability);
 check(end_care_family_activity_for_loss_in_candidate(common,*tables.family_activities,binding,cause));
}
void end_activity(CommonStore& common,CareCoverageWriteTables tables,const CareGoalSourceContext& g,const CareBinding& binding,const DeathRef& death){
 if(g.rules.selected_care_version()!=6)return;
 if(!tables.family_activities||!g.coverage||&tables.family_activities->sources.coverage!=g.coverage)fail("CareFamilyActivityOriginalWriterRequired",binding.binding_id,ErrorKind::unsupported_capability);
 check(end_care_family_activity_for_subject_death_in_candidate(common,*tables.family_activities,binding,death));
}

void original_handover(const CareCoverageSources& x,const CareRequirement& goal,const CareRequirementTransition& tr,const CareTemporalEvent& ev){
 const auto& g=context(x);transition_identity(g,goal,tr,ev);const auto* completed=std::get_if<CareHandoverCompletedEvent>(&ev.body);
 if(!completed||tr.kind!=CareRequirementTransition::Kind::handover_completed||tr.requirement_id!=goal.id||completed->requirement_id!=goal.id||tr.source_episode_after!=completed->source_episode||completed->source_episode<1)fail("CareCompletedGoalSourceMismatch",tr.transition_id);
 const auto* binding=x.tables.bindings.find(completed->binding_id);const auto* fact=x.tables.satisfied.find(completed->satisfied_fact_id);const auto* execution=x.handovers.executions.find(completed->handover_id);
 if(!binding||!fact||!execution)fail("CareCompletedOriginalRecordsMissing",tr.transition_id);
 check(validate_care_binding_shape(*binding));check(validate_care_coverage_satisfied_shape(*fact));
 if(binding->binding_id!=completed->binding_id||fact->fact_id!=completed->satisfied_fact_id||fact->requirement_id!=goal.id||fact->binding_ref!=binding->binding_id||fact->source_episode!=completed->source_episode||!same(fact->handover_result_ref,ev.original_transition_ref)||!same(tr.cause_ref,care_coverage_satisfied_source(fact->fact_id))||binding->child_actor_id!=goal.subject_actor_id||binding->started_tick!=ev.occurred_at.half_day_tick||fact->effective_tick!=ev.occurred_at.half_day_tick)fail("CareCompletedBindingProjectionMismatch",tr.transition_id);
 const auto* accepted=take(read_accepted_caregiver(x.handovers.acceptance,care_acceptance_source(binding->accepted_ref)));
 if(binding->terms_ref!=accepted->accepted_id||binding->carer_actor_id!=accepted->provider.carer_actor_id||binding->until_tick!=accepted->terms.until_tick||execution->accepted_care_terms_ref!=accepted->accepted_id||execution->requirement_id!=goal.id||!execution->completion_event_ref||!same(*execution->completion_event_ref,ev.original_transition_ref)||execution->terminal_source_ref||execution->work_period_sources.size()!=static_cast<std::size_t>(accepted->terms.handover_half_days)||execution->work_period_sources.empty())fail("CareCompletedAcceptedExecutionMismatch",tr.transition_id);
 if(take(compare_world_moments(ev.occurred_at,WorldMoment{accepted->terms.from_tick,{}}))<0||(accepted->terms.until_tick&&take(compare_world_moments(ev.occurred_at,WorldMoment{*accepted->terms.until_tick,{}}))>=0))fail("CareCompletedOutsideAcceptedWindow",tr.transition_id);
 const auto& last=execution->work_period_sources.back();
 if(!same(completed->completion_work_period_ref,MomentOwnerSource{care_handover_period_source(last.source_id),2})||!same(last.provided_at,ev.occurred_at)||!same(binding->placement_ref,completed->placement_result_ref)||completed->actual_participation_refs.empty())fail("CareCompletedActualWorkOrPlacementMismatch",tr.transition_id);
 bool placement=false;for(const auto& ref:execution->actual_placement_result_refs)if(same(ref,binding->placement_ref))placement=true;if(!placement)fail("CareCompletedPlacementSourceMissing",tr.transition_id);
 check(validate_care_handover_worker_history(x.handovers,execution->handover_id));
 for(const auto& period:execution->work_period_sources){take(read_care_handover_period_for_original_effect(x.handovers,care_handover_period_source(period.source_id),2));const auto& b=std::get<ActorActionWorkBinding>(execution->execution_binding);auto actual=take(read_fixed_service_work_period2(x.handovers.elapsed,b,period.work_period_ref.period_ordinal,accepted->terms.handover_half_days,take(g.clock.read_current_moment()),x.handovers.time_scale));check(x.periods.validate_period_inputs(*execution,*accepted,actual));check(x.periods.validate_period_effects(*execution,*accepted,period));}
 before(g.common,care_acceptance_source(accepted->accepted_id),care_coverage_satisfied_source(fact->fact_id));before(g.common,care_coverage_satisfied_source(fact->fact_id),ev.original_transition_ref);
 // Original physical result and ongoing activity must be independently proved;
 // neither this satisfaction nor its S0 terminal may stand in for that source.
 check(validate_care_handover_actual_placement(x,*execution,*accepted,*binding,ev));
 check(x.originals.validate_original_completed_handover(*execution,*accepted,*binding,ev));
}
void original_loss(const CareCoverageSources& x,const CareRequirement& goal,const CareRequirementTransition& tr,const CareTemporalEvent& ev){
 const auto& g=context(x);transition_identity(g,goal,tr,ev);const auto* body=std::get_if<CareCoverageLostEvent>(&ev.body);
 if(!body||tr.kind!=CareRequirementTransition::Kind::coverage_lost||body->requirement_id!=goal.id||tr.requirement_id!=goal.id||body->prior_source_episode<1||body->prior_source_episode==std::numeric_limits<Integer>::max()||body->next_source_episode!=body->prior_source_episode+1||tr.source_episode_before!=body->prior_source_episode||tr.source_episode_after!=body->next_source_episode)fail("CareLossGoalSourceMismatch",tr.transition_id);
 const auto* old=x.tables.bindings.find(body->old_binding_ref);const auto* lost=x.tables.lost.find(body->lost_fact_id);if(!old||!lost)fail("CareLossOriginalRecordsMissing",tr.transition_id);check(validate_care_binding_shape(*old));check(validate_care_coverage_lost_shape(*lost));
 if(old->binding_id!=body->old_binding_ref||old->child_actor_id!=goal.subject_actor_id||lost->fact_id!=body->lost_fact_id||lost->requirement_id!=goal.id||lost->old_binding_ref!=old->binding_id||lost->effective_tick!=ev.occurred_at.half_day_tick||!same(tr.cause_ref,care_coverage_lost_source(lost->fact_id)))fail("CareLossBindingProjectionMismatch",tr.transition_id);
 const auto& satisfied=satisfaction(x,old->binding_id);const auto& started=event(g,satisfied.handover_result_ref);if(!tr.previous_transition_ref||!same(*tr.previous_transition_ref,started.original_transition_ref)||satisfied.source_episode!=body->prior_source_episode||take(compare_world_moments(started.occurred_at,ev.occurred_at))>0)fail("CareLossEpisodeOrTimeMismatch",tr.transition_id);before(g.common,started.original_transition_ref,care_coverage_lost_source(lost->fact_id));before(g.common,care_coverage_lost_source(lost->fact_id),ev.original_transition_ref);
 if(old->until_tick&&take(compare_world_moments(ev.occurred_at,WorldMoment{*old->until_tick,{}}))>0)fail("CareAcceptedWindowLossMissing",old->binding_id);
 if(const auto* cause=std::get_if<CareHolderDeath>(&body->cause)){
  const SourceRef source_ref{cause->death_ref.domain,cause->death_ref.kind,cause->death_ref.id};auto actual=take(read_verified_death4(g.common,g.deaths,g.life,cause->death_ref,take(g.clock.read_current_moment())));
  if(actual.actor_id!=old->carer_actor_id||actual.effective_at.kind!=ActorFoundationAt::Kind::exact_moment||actual.recorded_at.kind!=ActorFoundationAt::Kind::exact_moment||!same(actual.effective_at.exact_moment,ev.occurred_at)||!same(actual.recorded_at.exact_moment,ev.occurred_at)||!same(lost->cause_ref,source_ref))fail("CareHolderDeathSourceMismatch",lost->fact_id);before(g.common,source_ref,care_coverage_lost_source(lost->fact_id));
 }else if(const auto* cause=std::get_if<CareAcceptedWindowEnded>(&body->cause)){
  const auto* accepted=take(read_accepted_caregiver(x.handovers.acceptance,care_acceptance_source(old->accepted_ref)));
  if(cause->accepted_care_terms_ref!=accepted->accepted_id||!accepted->terms.until_tick||!old->until_tick||cause->until_tick!=*accepted->terms.until_tick||cause->until_tick!=*old->until_tick||!same(ev.occurred_at,WorldMoment{cause->until_tick,{}})||!same(lost->cause_ref,care_acceptance_source(accepted->accepted_id)))fail("CareAcceptedWindowLossSourceMismatch",lost->fact_id);
  before(g.common,care_acceptance_source(accepted->accepted_id),care_coverage_lost_source(lost->fact_id));
 }else if(const auto* cause=std::get_if<CareActualSeparation>(&body->cause)){
  if(!x.separations||!x.separations->bound_to(g.common))fail("CareActualSeparationSourcesRequired",lost->fact_id,ErrorKind::unsupported_capability);
  const auto* accepted=take(read_accepted_caregiver(x.handovers.acceptance,care_acceptance_source(old->accepted_ref)));
  const auto actual=take(x.separations->read_actual_separation(*old,*accepted,cause->actual_location_or_binding_transition));
  if(!actual||!actual->actual_source||!same(actual->occurred_at,ev.occurred_at)||actual->source_schema!=cause->actual_location_or_binding_transition.source_schema||!same(lost->cause_ref,cause->actual_location_or_binding_transition.source_ref))fail("CareActualSeparationSourceMismatch",lost->fact_id);
  before(g.common,started.original_transition_ref,lost->cause_ref);before(g.common,lost->cause_ref,care_coverage_lost_source(lost->fact_id));
 }else if(const auto* cause=std::get_if<CareParticipationConflict>(&body->cause)){
  const auto* activities=x.originals.family_activity_sources();if(!activities||&activities->coverage!=&x)fail("CareGrowthConflictSourcesRequired",lost->fact_id,ErrorKind::unsupported_capability);
  const auto* activity=activities->activities.find(old->care_activity_ref.source_ref.id);if(!activity||!activity->ended_by)fail("CareGrowthActualActivityEndRequired",lost->fact_id);
  const auto actual=take(read_care_growth_start_conflict(*activities,*activity,*cause));
  if(activity->carer_actor_id!=old->carer_actor_id||activity->child_actor_id!=old->child_actor_id||!same(actual.started->occurred_at,ev.occurred_at)||!same(lost->cause_ref,cause->actual_occupation_change.source_ref)||!care_family_activity_end_cause_equal(activity->ended_by->cause,CareFamilyActivityEndCause{CareLossCauseV2{*cause}}))fail("CareGrowthConflictLossMismatch",lost->fact_id);
  const auto& h=*actual.original_header;const SourceRef cut{h.domain,h.kind,h.id};
  before(g.common,started.original_transition_ref,cut);before(g.common,cut,care_family_activity_ended_source(activity->ended_by->end_id));before(g.common,care_family_activity_ended_source(activity->ended_by->end_id),care_coverage_lost_source(lost->fact_id));
 }else if(const auto* cause=std::get_if<CareAuthorityOrArrangementEnded>(&body->cause)){
  const auto* activities=x.originals.family_activity_sources();if(!activities||&activities->coverage!=&x)fail("CarePersonalStopOriginalSourcesRequired",lost->fact_id,ErrorKind::unsupported_capability);
  const auto actual=take(read_care_personal_stop_source(*activities,cause->actual_ended_result));
  if(!actual.activity||!actual.end||actual.activity->activity_id!=old->care_activity_ref.source_ref.id||actual.activity->carer_actor_id!=old->carer_actor_id||actual.activity->child_actor_id!=old->child_actor_id||!same(actual.end->ended_at,ev.occurred_at)||!same(lost->cause_ref,cause->actual_ended_result.source_ref))fail("CarePersonalStopLossMismatch",lost->fact_id);
  before(g.common,started.original_transition_ref,lost->cause_ref);before(g.common,lost->cause_ref,care_family_activity_ended_source(actual.end->end_id));before(g.common,care_family_activity_ended_source(actual.end->end_id),care_coverage_lost_source(lost->fact_id));
 }else fail("CareCoverageLossOriginalCauseReaderRequired",lost->fact_id,ErrorKind::unsupported_capability);
}
}
SourceRef care_coverage_satisfied_source(const StableId& id){return {"care_requirements","care_coverage_satisfied",id};}
SourceRef care_coverage_lost_source(const StableId& id){return {"care_requirements","care_coverage_lost",id};}
Outcome<Integer> care_coverage_source_schema(const SourceRef& ref){if(ref.domain=="care_requirements"&&!ref.id.empty()&&(ref.kind=="care_coverage_satisfied"||ref.kind=="care_coverage_lost"))return {true,3,{}};return {false,{},common_failure("UnknownCareCoverageSource",ref.id,ErrorKind::unsupported_capability).error};}
Status validate_care_binding_shape(const CareBinding& v){try{for(const auto* id:{&v.binding_id,&v.child_actor_id,&v.carer_actor_id,&v.terms_ref,&v.accepted_ref})text(*id);source(v.care_activity_ref);source(v.placement_ref);if(v.child_actor_id==v.carer_actor_id||v.terms_ref!=v.accepted_ref||v.started_tick<0||(v.until_tick&&*v.until_tick<=v.started_tick)||static_cast<Integer>(v.phase)<0||static_cast<Integer>(v.phase)>2)fail("InvalidCareBinding",v.binding_id);return common_success();}catch(Failure& e){return {false,{},std::move(e.error)};}}
Status validate_care_coverage_satisfied_shape(const CareCoverageSatisfied& v){try{text(v.fact_id);text(v.requirement_id);text(v.binding_ref);source(v.handover_result_ref);take(care_goal_transition_source_schema(v.handover_result_ref));if(v.source_episode<1||v.effective_tick<0)fail("InvalidCareCoverageSatisfied",v.fact_id);return common_success();}catch(Failure& e){return {false,{},std::move(e.error)};}}
Status validate_care_coverage_lost_shape(const CareCoverageLost& v){try{text(v.fact_id);text(v.requirement_id);text(v.old_binding_ref);source(v.cause_ref);if(v.effective_tick<0)fail("InvalidCareCoverageLost",v.fact_id);return common_success();}catch(Failure& e){return {false,{},std::move(e.error)};}}
Status validate_original_care_handover_coverage(const CareCoverageSources& x,const CareRequirement& g,const CareRequirementTransition& t,const CareTemporalEvent& e){try{original_handover(x,g,t,e);return common_success();}catch(Failure& error){return {false,{},std::move(error.error)};}}
Status validate_original_care_coverage_loss(const CareCoverageSources& x,const CareRequirement& g,const CareRequirementTransition& t,const CareTemporalEvent& e){try{original_loss(x,g,t,e);return common_success();}catch(Failure& error){return {false,{},std::move(error.error)};}}
Status validate_current_care_coverage(const CareCoverageSources& x,const CareRequirement& goal,const WorldMoment& now){try{
 const auto& g=context(x);if(goal.state!=StateGoalState::satisfied){if(goal.current_care_binding_ref||goal.accepted_care_terms_ref)fail("CareInactiveGoalRetainsCurrentBinding",goal.id);return common_success();}
 if(!goal.current_care_binding_ref||!goal.accepted_care_terms_ref)fail("CareSatisfiedCurrentBindingMissing",goal.id);const auto* binding=x.tables.bindings.find(*goal.current_care_binding_ref);if(!binding||binding->phase!=CareBindingPhase::active||binding->child_actor_id!=goal.subject_actor_id||binding->accepted_ref!=*goal.accepted_care_terms_ref||loss(x,binding->binding_id))fail("CareCurrentBindingMismatch",goal.id);
 const auto& sat=satisfaction(x,binding->binding_id);if(sat.requirement_id!=goal.id||sat.source_episode!=goal.source_episode||!same(sat.handover_result_ref,goal.current_transition_ref))fail("CareCurrentSatisfactionMismatch",goal.id);
 const auto* accepted=take(read_accepted_caregiver(x.handovers.acceptance,care_acceptance_source(binding->accepted_ref)));if(binding->until_tick&&take(compare_world_moments(now,WorldMoment{*binding->until_tick,{}}))>=0)fail("CareAcceptedWindowLossMissing",binding->binding_id);
 check(validate_current_care_participants(x.handovers.acceptance.body,binding->child_actor_id,binding->carer_actor_id,now));
 if(!x.separations||!x.separations->bound_to(g.common))fail("CareActualSeparationSourcesRequired",binding->binding_id,ErrorKind::unsupported_capability);
 if(take(x.separations->read_first_actual_separation(*binding,*accepted,sat.handover_result_ref,{event(g,sat.handover_result_ref).occurred_at,now})))fail("CareImmediateSeparationLossMissing",binding->binding_id);
 check(x.originals.validate_current_care_participation(*binding,*accepted,now));return common_success();
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Outcome<const CareCoverageSatisfied*> read_care_coverage_satisfaction(const CareCoverageSources& x,const SourceRef& ref){try{
 const auto& g=context(x);take(care_coverage_source_schema(ref));if(ref.kind!="care_coverage_satisfied")fail("CareSatisfactionSourceRequired",ref.id);const auto* row=x.tables.satisfied.find(ref.id);if(!row||row->fact_id!=ref.id)fail("CareSatisfactionMissing",ref.id);const auto& ev=event(g,row->handover_result_ref);const auto* goal=g.goals.requirements.find(row->requirement_id);const auto* tr=g.goals.transitions.find(ev.event_id);if(!goal||!tr)fail("CareSatisfactionGoalSourceMissing",ref.id);original_handover(x,*goal,*tr,ev);return {true,row,{}};
 }catch(Failure& e){return {false,nullptr,std::move(e.error)};}}
Outcome<const WorldMoment*> read_care_transition_moment_v2(const CareGoalSourceContext& x,const SourceRef& ref){
 auto row=read_care_goal_transition_source(x,ref);if(!row.ok)return {false,nullptr,std::move(row.error)};
 return {true,&row.value->occurred_at,{}};
}
Outcome<const WorldMoment*> read_care_satisfaction_moment_v2(const CareCoverageSources& x,const SourceRef& ref){try{
 const auto* row=take(read_care_coverage_satisfaction(x,ref));
 const auto& original=event(context(x),row->handover_result_ref);
 return {true,&original.occurred_at,{}};
 }catch(Failure& e){return {false,nullptr,std::move(e.error)};}}
Outcome<CareBindingCoverageView2> read_care_binding_coverage_v2(const CareCoverageSources& x,const StableId& id){try{
 const auto& g=context(x);check(validate_care_coverage_source_closure(x));
 const auto* binding=x.tables.bindings.find(id);if(!binding||binding->binding_id!=id)fail("CareBindingMissing",id);
 const auto& sat=satisfaction(x,id);const auto* goal=g.goals.requirements.find(sat.requirement_id);if(!goal)fail("CareBindingGoalMissing",id);
 // This is the original source-local projection. At the accepted until gate,
 // a finished [t,t+1) can be read before that same candidate appends its loss.
 // The stable goal validator independently requires the immediate loss.
 take(read_care_coverage_satisfaction(x,care_coverage_satisfied_source(sat.fact_id)));
 const auto* start=&event(g,sat.handover_result_ref);
 const auto* accepted=take(read_accepted_caregiver(x.handovers.acceptance,care_acceptance_source(binding->accepted_ref)));
 const CareTemporalEvent* end=nullptr;
 if(const auto* lost=loss(x,id))end=&loss_event(x,*lost);
 else if(binding->phase==CareBindingPhase::ended){
  // Only the closure whose actual predecessor is this handover may end this
  // binding. Do not use today's terminal goal to close an earlier episode.
  for(const auto& pair:g.goals.transitions.rows()){
   const auto& tr=pair.second;
   if(tr.requirement_id!=goal->id||tr.kind!=CareRequirementTransition::Kind::requirement_closed||!tr.previous_transition_ref||!same(*tr.previous_transition_ref,sat.handover_result_ref))continue;
   if(end)fail("CareBindingClosureNotUnique",id);
   end=take(read_care_goal_transition_source(g,transition_source(tr.transition_id)));
  }
  if(!end)fail("CareBindingActualClosureMissing",id);
 }
 if(end){
  if(take(compare_world_moments(end->occurred_at,start->occurred_at))<0)fail("CareBindingCoverageOrderMismatch",id);
  before(g.common,start->original_transition_ref,end->original_transition_ref);
  if(accepted->terms.until_tick&&take(compare_world_moments(end->occurred_at,WorldMoment{*accepted->terms.until_tick,{}}))>0)fail("CareAcceptedWindowLossMissing",id);
 }
 else if(accepted->terms.until_tick&&take(compare_world_moments(WorldMoment{*accepted->terms.until_tick,{}},take(g.clock.read_current_moment())))<0)fail("CareAcceptedWindowLossMissing",id);
 return {true,{binding,accepted,&sat,start,end},{}};
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Status validate_care_binding_full_calendar_period(const CareCoverageSources& x,const StableId& id,Integer from,Integer to){try{
 if(from<0||from==std::numeric_limits<Integer>::max()||to!=from+1)fail("CarePeriodRequiresCompleteCalendarHalfDay",id);
 const auto view=take(read_care_binding_coverage_v2(x,id));const WorldMoment begin{from,{}},end{to,{}};
 if(take(compare_world_moments(end,take(context(x).clock.read_current_moment())))>0)fail("CarePeriodNotActuallyCompleted",id);
 if(take(compare_world_moments(view.handover_completed->occurred_at,begin))>0)fail("CarePeriodBeginsBeforeActualCoverage",id);
 if(view.ended_by&&take(compare_world_moments(end,view.ended_by->occurred_at))>0)fail("CarePeriodExtendsPastActualCoverage",id);
 if(view.accepted->terms.until_tick&&to>*view.accepted->terms.until_tick)fail("CarePeriodExtendsPastAcceptedWindow",id);
 check(validate_care_participants_full_span_after_handover(x.handovers.acceptance.body,view.binding->child_actor_id,view.binding->carer_actor_id,view.handover_completed->original_transition_ref,view.handover_completed->occurred_at,{begin,end},context(x).clock));
 return common_success();
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Status validate_care_coverage_source_closure(const CareCoverageSources& x){try{
 const auto& g=context(x);for(const auto& pair:x.tables.satisfied.rows()){if(pair.first!=pair.second.fact_id)fail("CareSatisfactionKeyMismatch",pair.first);take(read_care_coverage_satisfaction(x,care_coverage_satisfied_source(pair.first)));}
 for(const auto& pair:x.tables.lost.rows()){if(pair.first!=pair.second.fact_id)fail("CareLossKeyMismatch",pair.first);const auto& ev=loss_event(x,pair.second);const auto* goal=g.goals.requirements.find(pair.second.requirement_id);const auto* tr=g.goals.transitions.find(ev.event_id);if(!goal||!tr)fail("CareLossGoalSourceMissing",pair.first);original_loss(x,*goal,*tr,ev);}
 for(const auto& pair:x.tables.bindings.rows()){
  const auto& binding=pair.second;if(pair.first!=binding.binding_id)fail("CareBindingKeyMismatch",pair.first);check(validate_care_binding_shape(binding));const auto& sat=satisfaction(x,pair.first);const auto* lost=loss(x,pair.first);CareBindingPhase expected=lost?CareBindingPhase::interrupted:CareBindingPhase::active;
  // A subject's true terminal goal closes only the binding active immediately
  // before that event. A prior interrupted binding is retained unchanged.
  if(!lost){const auto* goal=g.goals.requirements.find(sat.requirement_id);if(!goal)fail("CareBindingGoalMissing",pair.first);const auto& last=event(g,goal->current_transition_ref);if(const auto* closed=std::get_if<CareRequirementClosedEvent>(&last.body);closed&&closed->requirement_id==goal->id){const auto* tr=g.goals.transitions.find(last.event_id);if(tr&&tr->previous_transition_ref&&same(*tr->previous_transition_ref,sat.handover_result_ref)){take(read_care_goal_transition_source(g,last.original_transition_ref));expected=CareBindingPhase::ended;}}}
  if(binding.phase!=expected)fail("CareBindingPhaseSourceMismatch",pair.first);
  if(!x.separations||!x.separations->bound_to(g.common))fail("CareActualSeparationSourcesRequired",binding.binding_id,ErrorKind::unsupported_capability);
  const auto* accepted=take(read_accepted_caregiver(x.handovers.acceptance,care_acceptance_source(binding.accepted_ref)));
  const CareTemporalEvent* ended=lost?&loss_event(x,*lost):nullptr;
  if(!ended&&expected==CareBindingPhase::ended){const auto* goal=g.goals.requirements.find(sat.requirement_id);if(!goal)fail("CareBindingGoalMissing",pair.first);ended=&event(g,goal->current_transition_ref);}
  const auto first=take(x.separations->read_first_actual_separation(binding,*accepted,sat.handover_result_ref,{event(g,sat.handover_result_ref).occurred_at,ended?ended->occurred_at:take(g.clock.read_current_moment())},ended?std::optional<SourceRef>{ended->original_transition_ref}:std::nullopt));
  if(first){
   const auto* body=ended?std::get_if<CareCoverageLostEvent>(&ended->body):nullptr;const auto* cause=body?std::get_if<CareActualSeparation>(&body->cause):nullptr;
   if(!lost||!cause||!first->actual_source||!same(first->occurred_at,ended->occurred_at)||first->source_schema!=cause->actual_location_or_binding_transition.source_schema||first->actual_source->id!=cause->actual_location_or_binding_transition.source_ref.id||first->actual_source->domain!=cause->actual_location_or_binding_transition.source_ref.domain||first->actual_source->kind!=cause->actual_location_or_binding_transition.source_ref.kind)fail("CareFirstActualSeparationLossMissing",binding.binding_id);
  }
 }
 for(const auto& pair:g.common.state().facts.rows())if(pair.second.domain=="care_requirements"){
  if(pair.second.kind=="care_coverage_satisfied"&&!x.tables.satisfied.find(pair.first))fail("CareSatisfactionPayloadMissing",pair.first);
  if(pair.second.kind=="care_coverage_lost"&&!x.tables.lost.find(pair.first))fail("CareLossPayloadMissing",pair.first);
 }
 if(g.rules.selected_care_version()==6){const auto* activities=x.originals.family_activity_sources();if(!activities||&activities->coverage!=&x)fail("CareFamilyActivitySourceClosureRequired",{},ErrorKind::unsupported_capability);check(validate_care_family_activity_sources(*activities));}
 return common_success();
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Outcome<CareRequirementApplication> lose_care_coverage_after_holder_death_in_candidate(CommonStore& common,CareGoalWriteTables goals,CareCoverageWriteTables tables,const CareGoalSourceContext& g,const StableId& id,const DeathRef& death){try{
 if(!g.coverage)fail("CareCoverageActualOwnerSourcesRequired",id,ErrorKind::unsupported_capability);const auto& x=*g.coverage;context(x);if(&common!=&g.common||&goals.requirements!=&g.goals.requirements||&goals.transitions!=&g.goals.transitions||&goals.temporal_events!=&g.goals.temporal_events||&tables.bindings!=&x.tables.bindings||&tables.satisfied!=&x.tables.satisfied||&tables.lost!=&x.tables.lost)fail("CareCoverageSameCandidateRequired",id);
 auto* binding=tables.bindings.find(id);if(!binding||binding->binding_id!=id)fail("CareBindingMissing",id);const SourceRef cause{death.domain,death.kind,death.id};auto actual=take(read_verified_death4(common,g.deaths,g.life,death,take(g.clock.read_current_moment())));if(actual.actor_id!=binding->carer_actor_id)fail("CareHolderDeathActorMismatch",id);
 if(const auto* prior=loss(x,id)){const auto& ev=loss_event(x,*prior);const auto* goal=g.goals.requirements.find(prior->requirement_id);const auto* tr=g.goals.transitions.find(ev.event_id);if(!same(prior->cause_ref,cause)||!goal||!tr)fail("ConflictingCareBindingLoss",id);original_loss(x,*goal,*tr,ev);return {true,{goal->id,ev.original_transition_ref,true},{}};}
 const auto& sat=satisfaction(x,id);auto* goal=goals.requirements.find(sat.requirement_id);if(!goal)fail("CareBindingGoalMissing",id);const auto prefix=take(read_care_requirement_before_original_fact(g,goal->id,cause));
 if(!prefix.last_transition||prefix.state!=StateGoalState::satisfied||!same(prefix.last_transition->original_transition_ref,sat.handover_result_ref)||goal->revision!=prefix.revision||goal->source_episode!=prefix.source_episode||goal->state!=StateGoalState::satisfied||!goal->current_care_binding_ref||*goal->current_care_binding_ref!=id||!same(goal->current_transition_ref,prefix.last_transition->original_transition_ref)||binding->phase!=CareBindingPhase::active)fail("CareHolderDeathNoCurrentCoverage",id);
 const auto now=take(g.clock.read_current_moment());if(actual.effective_at.kind!=ActorFoundationAt::Kind::exact_moment||actual.recorded_at.kind!=ActorFoundationAt::Kind::exact_moment||!same(actual.effective_at.exact_moment,now)||!same(actual.recorded_at.exact_moment,now))fail("CareHolderDeathMustBeImmediate",id);
 if(goal->revision==std::numeric_limits<Integer>::max()||goal->source_episode==std::numeric_limits<Integer>::max())fail("CareCoverageRevisionOverflow",id);
 end_activity(common,tables,g,*binding,CareLossCauseV2{CareHolderDeath{death}});
 const auto lost_id=take(common.next_id("care_coverage_lost"));const auto transition_id=take(common.next_id("care_transition"));
 CareCoverageLost lost{lost_id,goal->id,id,cause,now.half_day_tick};check(validate_care_coverage_lost_shape(lost));if(!tables.lost.insert(lost_id,std::move(lost)))fail("DuplicateCareCoverageLoss",lost_id);check(common.append_fact({lost_id,"care_coverage_lost","care_requirements"}));
 CareRequirementTransition tr{transition_id,goal->id,goal->revision,goal->revision+1,goal->source_episode,goal->source_episode+1,CareRequirementTransition::Kind::coverage_lost,goal->current_transition_ref,care_coverage_lost_source(lost_id)};
 CareTemporalEvent ev{transition_id,transition_source(transition_id),now,goal->rule_version,CareCoverageLostEvent{goal->id,goal->source_episode,goal->source_episode+1,id,lost_id,CareHolderDeath{death}}};
 if(!goals.transitions.insert(transition_id,std::move(tr))||!goals.temporal_events.insert(transition_id,std::move(ev)))fail("DuplicateCareTransition",transition_id);check(common.append_fact({transition_id,"care_requirement_changed","care_requirements"}));
 binding->phase=CareBindingPhase::interrupted;++goal->revision;++goal->source_episode;goal->state=StateGoalState::unsatisfied;goal->accepted_care_terms_ref.reset();goal->current_care_binding_ref.reset();goal->current_transition_ref=transition_source(transition_id);
 original_loss(x,*goal,*goals.transitions.find(transition_id),*goals.temporal_events.find(transition_id));return {true,{goal->id,goal->current_transition_ref,false},{}};
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Outcome<CareRequirementApplication> lose_care_coverage_after_actual_separation_in_candidate(CommonStore& common,CareGoalWriteTables goals,CareCoverageWriteTables tables,const CareGoalSourceContext& g,const StableId& id,const MomentOwnerSource& spatial){try{
 if(!g.coverage)fail("CareCoverageActualOwnerSourcesRequired",id,ErrorKind::unsupported_capability);const auto& x=*g.coverage;context(x);if(&common!=&g.common||&goals.requirements!=&g.goals.requirements||&goals.transitions!=&g.goals.transitions||&goals.temporal_events!=&g.goals.temporal_events||&tables.bindings!=&x.tables.bindings||&tables.satisfied!=&x.tables.satisfied||&tables.lost!=&x.tables.lost)fail("CareCoverageSameCandidateRequired",id);
 auto* binding=tables.bindings.find(id);if(!binding||binding->binding_id!=id)fail("CareBindingMissing",id);
 if(!x.separations||!x.separations->bound_to(common))fail("CareActualSeparationSourcesRequired",id,ErrorKind::unsupported_capability);
 const auto* accepted=take(read_accepted_caregiver(x.handovers.acceptance,care_acceptance_source(binding->accepted_ref)));const auto actual=take(x.separations->read_actual_separation(*binding,*accepted,spatial));if(!actual||!actual->actual_source)fail("CareActualSeparationRequired",id);
 const auto& cause=spatial.source_ref;
 if(const auto* prior=loss(x,id)){const auto& ev=loss_event(x,*prior);const auto* goal=g.goals.requirements.find(prior->requirement_id);const auto* tr=g.goals.transitions.find(ev.event_id);if(!same(prior->cause_ref,cause)||!goal||!tr)fail("ConflictingCareBindingLoss",id);const auto* lost_body=std::get_if<CareCoverageLostEvent>(&ev.body);const auto* old_cause=lost_body?std::get_if<CareActualSeparation>(&lost_body->cause):nullptr;if(!old_cause||!same(old_cause->actual_location_or_binding_transition,spatial))fail("ConflictingCareBindingLoss",id);original_loss(x,*goal,*tr,ev);return {true,{goal->id,ev.original_transition_ref,true},{}};}
 const auto& sat=satisfaction(x,id);auto* goal=goals.requirements.find(sat.requirement_id);if(!goal)fail("CareBindingGoalMissing",id);const auto prefix=take(read_care_requirement_before_original_fact(g,goal->id,cause));
 if(!prefix.last_transition||prefix.state!=StateGoalState::satisfied||!same(prefix.last_transition->original_transition_ref,sat.handover_result_ref)||goal->revision!=prefix.revision||goal->source_episode!=prefix.source_episode||goal->state!=StateGoalState::satisfied||!goal->current_care_binding_ref||*goal->current_care_binding_ref!=id||!same(goal->current_transition_ref,prefix.last_transition->original_transition_ref)||binding->phase!=CareBindingPhase::active)fail("CareActualSeparationNoCurrentCoverage",id);
 const auto now=take(g.clock.read_current_moment());if(!same(actual->occurred_at,now))fail("CareActualSeparationMustBeImmediate",id);
 const auto first=take(x.separations->read_first_actual_separation(*binding,*accepted,sat.handover_result_ref,{event(g,sat.handover_result_ref).occurred_at,now}));if(!first||!first->actual_source||first->actual_source->id!=cause.id||first->source_schema!=spatial.source_schema||!same(first->occurred_at,now))fail("CareEarlierActualSeparationLossMissing",id);
 if(goal->revision==std::numeric_limits<Integer>::max()||goal->source_episode==std::numeric_limits<Integer>::max())fail("CareCoverageRevisionOverflow",id);
 end_activity(common,tables,g,*binding,CareLossCauseV2{CareActualSeparation{spatial}});
 const auto lost_id=take(common.next_id("care_coverage_lost"));const auto transition_id=take(common.next_id("care_transition"));
 CareCoverageLost lost{lost_id,goal->id,id,cause,now.half_day_tick};check(validate_care_coverage_lost_shape(lost));if(!tables.lost.insert(lost_id,std::move(lost)))fail("DuplicateCareCoverageLoss",lost_id);check(common.append_fact({lost_id,"care_coverage_lost","care_requirements"}));
 CareRequirementTransition tr{transition_id,goal->id,goal->revision,goal->revision+1,goal->source_episode,goal->source_episode+1,CareRequirementTransition::Kind::coverage_lost,goal->current_transition_ref,care_coverage_lost_source(lost_id)};
 CareTemporalEvent ev{transition_id,transition_source(transition_id),now,goal->rule_version,CareCoverageLostEvent{goal->id,goal->source_episode,goal->source_episode+1,id,lost_id,CareActualSeparation{spatial}}};
 if(!goals.transitions.insert(transition_id,std::move(tr))||!goals.temporal_events.insert(transition_id,std::move(ev)))fail("DuplicateCareTransition",transition_id);check(common.append_fact({transition_id,"care_requirement_changed","care_requirements"}));
 binding->phase=CareBindingPhase::interrupted;++goal->revision;++goal->source_episode;goal->state=StateGoalState::unsatisfied;goal->accepted_care_terms_ref.reset();goal->current_care_binding_ref.reset();goal->current_transition_ref=transition_source(transition_id);
 original_loss(x,*goal,*goals.transitions.find(transition_id),*goals.temporal_events.find(transition_id));return {true,{goal->id,goal->current_transition_ref,false},{}};
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Outcome<CareRequirementApplication> lose_care_coverage_after_personal_stop_in_candidate(CommonStore& common,CareGoalWriteTables goals,CareCoverageWriteTables tables,const CareGoalSourceContext& g,const StableId& id,const MomentOwnerSource& source){try{
 if(!g.coverage)fail("CareCoverageActualOwnerSourcesRequired",id,ErrorKind::unsupported_capability);const auto& x=*g.coverage;context(x);
 if(&common!=&g.common||&goals.requirements!=&g.goals.requirements||&goals.transitions!=&g.goals.transitions||&goals.temporal_events!=&g.goals.temporal_events||&tables.bindings!=&x.tables.bindings||&tables.satisfied!=&x.tables.satisfied||&tables.lost!=&x.tables.lost||!tables.family_activities||&tables.family_activities->sources.coverage!=&x||&tables.family_activities->activities!=&tables.family_activities->sources.activities)fail("CarePersonalStopSameCandidateRequired",id);
 const auto actual=take(read_care_personal_stop_source(tables.family_activities->sources,source));auto* binding=tables.bindings.find(id);
 if(!binding||!actual.activity||!actual.end||binding->care_activity_ref.source_schema!=1||!same(binding->care_activity_ref.source_ref,care_family_activity_started_source(actual.activity->activity_id))||binding->carer_actor_id!=actual.activity->carer_actor_id||binding->child_actor_id!=actual.activity->child_actor_id)fail("CarePersonalStopBindingMismatch",id);
 if(const auto* prior=loss(x,id)){const auto& ev=loss_event(x,*prior);const auto* goal=g.goals.requirements.find(prior->requirement_id);const auto* tr=g.goals.transitions.find(ev.event_id);if(!same(prior->cause_ref,source.source_ref)||!goal||!tr)fail("ConflictingCareBindingLoss",id);original_loss(x,*goal,*tr,ev);return {true,{goal->id,ev.original_transition_ref,true},{}};}
 const auto& sat=satisfaction(x,id);auto* goal=goals.requirements.find(sat.requirement_id);if(!goal)fail("CareBindingGoalMissing",id);const auto prefix=take(read_care_requirement_before_original_fact(g,goal->id,source.source_ref));
 if(!prefix.last_transition||prefix.state!=StateGoalState::satisfied||!same(prefix.last_transition->original_transition_ref,sat.handover_result_ref)||goal->revision!=prefix.revision||goal->source_episode!=prefix.source_episode||goal->state!=StateGoalState::satisfied||!goal->current_care_binding_ref||*goal->current_care_binding_ref!=id||!same(goal->current_transition_ref,prefix.last_transition->original_transition_ref)||binding->phase!=CareBindingPhase::active)fail("CarePersonalStopNoCurrentCoverage",id);
 const auto now=take(g.clock.read_current_moment());if(!same(actual.end->ended_at,now))fail("CarePersonalStopMustBeImmediate",id);
 if(goal->revision==std::numeric_limits<Integer>::max()||goal->source_episode==std::numeric_limits<Integer>::max())fail("CareCoverageRevisionOverflow",id);
 const auto lost_id=take(common.next_id("care_coverage_lost"));const auto transition_id=take(common.next_id("care_transition"));
 CareCoverageLost lost{lost_id,goal->id,id,source.source_ref,now.half_day_tick};check(validate_care_coverage_lost_shape(lost));if(!tables.lost.insert(lost_id,std::move(lost)))fail("DuplicateCareCoverageLoss",lost_id);check(common.append_fact({lost_id,"care_coverage_lost","care_requirements"}));
 CareRequirementTransition tr{transition_id,goal->id,goal->revision,goal->revision+1,goal->source_episode,goal->source_episode+1,CareRequirementTransition::Kind::coverage_lost,goal->current_transition_ref,care_coverage_lost_source(lost_id)};
 CareTemporalEvent ev{transition_id,transition_source(transition_id),now,goal->rule_version,CareCoverageLostEvent{goal->id,goal->source_episode,goal->source_episode+1,id,lost_id,CareAuthorityOrArrangementEnded{source}}};
 if(!goals.transitions.insert(transition_id,std::move(tr))||!goals.temporal_events.insert(transition_id,std::move(ev)))fail("DuplicateCareTransition",transition_id);check(common.append_fact({transition_id,"care_requirement_changed","care_requirements"}));
 binding->phase=CareBindingPhase::interrupted;++goal->revision;++goal->source_episode;goal->state=StateGoalState::unsatisfied;goal->accepted_care_terms_ref.reset();goal->current_care_binding_ref.reset();goal->current_transition_ref=transition_source(transition_id);
 original_loss(x,*goal,*goals.transitions.find(transition_id),*goals.temporal_events.find(transition_id));return {true,{goal->id,goal->current_transition_ref,false},{}};
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Outcome<CareRequirementApplication> lose_care_coverage_after_growth_start_in_candidate(CommonStore& common,CareGoalWriteTables goals,CareCoverageWriteTables tables,const CareGoalSourceContext& g,const StableId& id,const MomentOwnerSource& source){try{
 if(!g.coverage||g.rules.selected_care_version()!=6||!tables.family_activities)fail("CareGrowthConflictSourcesRequired",id,ErrorKind::unsupported_capability);const auto& x=*g.coverage;context(x);
 if(&common!=&g.common||&goals.requirements!=&g.goals.requirements||&goals.transitions!=&g.goals.transitions||&goals.temporal_events!=&g.goals.temporal_events||&tables.bindings!=&x.tables.bindings||&tables.satisfied!=&x.tables.satisfied||&tables.lost!=&x.tables.lost||&tables.family_activities->sources.coverage!=&x)fail("CareCoverageSameCandidateRequired",id);
 auto* binding=tables.bindings.find(id);if(!binding)fail("CareBindingMissing",id);
 const auto* activity=tables.family_activities->sources.activities.find(binding->care_activity_ref.source_ref.id);
 if(!activity||binding->care_activity_ref.source_schema!=1||!same(binding->care_activity_ref.source_ref,care_family_activity_started_source(activity->activity_id)))fail("CareGrowthOriginalActivityRequired",id);
 const CareParticipationConflict conflict{source,activity->compatibility_rule_ref};const auto actual=take(read_care_growth_start_conflict(tables.family_activities->sources,*activity,conflict));
 const auto& h=*actual.original_header;const SourceRef cut{h.domain,h.kind,h.id};
 if(const auto* prior=loss(x,id)){const auto& ev=loss_event(x,*prior);const auto* goal=g.goals.requirements.find(prior->requirement_id);const auto* tr=g.goals.transitions.find(ev.event_id);const auto* event_body=std::get_if<CareCoverageLostEvent>(&ev.body);const auto* cause=event_body?std::get_if<CareParticipationConflict>(&event_body->cause):nullptr;if(!goal||!tr||!cause||!same(cause->actual_occupation_change,source))fail("ConflictingCareGrowthLoss",id);original_loss(x,*goal,*tr,ev);return {true,{goal->id,ev.original_transition_ref,true},{}};}
 const auto& sat=satisfaction(x,id);auto* goal=goals.requirements.find(sat.requirement_id);if(!goal)fail("CareBindingGoalMissing",id);const auto prefix=take(read_care_requirement_before_original_fact(g,goal->id,cut));
 if(!prefix.last_transition||prefix.state!=StateGoalState::satisfied||!same(prefix.last_transition->original_transition_ref,sat.handover_result_ref)||goal->revision!=prefix.revision||goal->source_episode!=prefix.source_episode||goal->state!=StateGoalState::satisfied||!goal->current_care_binding_ref||*goal->current_care_binding_ref!=id||!same(goal->current_transition_ref,prefix.last_transition->original_transition_ref)||binding->phase!=CareBindingPhase::active)fail("CareGrowthConflictNoCurrentCoverage",id);
 const auto now=take(g.clock.read_current_moment());if(!same(actual.started->occurred_at,now))fail("CareGrowthConflictMustBeImmediate",id);
 if(goal->revision==std::numeric_limits<Integer>::max()||goal->source_episode==std::numeric_limits<Integer>::max())fail("CareCoverageRevisionOverflow",id);
 end_activity(common,tables,g,*binding,CareLossCauseV2{conflict});
 const auto lost_id=take(common.next_id("care_coverage_lost"));const auto transition_id=take(common.next_id("care_transition"));
 CareCoverageLost lost{lost_id,goal->id,id,source.source_ref,now.half_day_tick};check(validate_care_coverage_lost_shape(lost));if(!tables.lost.insert(lost_id,std::move(lost)))fail("DuplicateCareCoverageLoss",lost_id);check(common.append_fact({lost_id,"care_coverage_lost","care_requirements"}));
 CareRequirementTransition tr{transition_id,goal->id,goal->revision,goal->revision+1,goal->source_episode,goal->source_episode+1,CareRequirementTransition::Kind::coverage_lost,goal->current_transition_ref,care_coverage_lost_source(lost_id)};
 CareTemporalEvent ev{transition_id,transition_source(transition_id),now,goal->rule_version,CareCoverageLostEvent{goal->id,goal->source_episode,goal->source_episode+1,id,lost_id,conflict}};
 if(!goals.transitions.insert(transition_id,std::move(tr))||!goals.temporal_events.insert(transition_id,std::move(ev)))fail("DuplicateCareTransition",transition_id);check(common.append_fact({transition_id,"care_requirement_changed","care_requirements"}));
 binding->phase=CareBindingPhase::interrupted;++goal->revision;++goal->source_episode;goal->state=StateGoalState::unsatisfied;goal->accepted_care_terms_ref.reset();goal->current_care_binding_ref.reset();goal->current_transition_ref=transition_source(transition_id);
 original_loss(x,*goal,*goals.transitions.find(transition_id),*goals.temporal_events.find(transition_id));return {true,{goal->id,goal->current_transition_ref,false},{}};
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Outcome<std::optional<WorldMoment>> next_care_coverage_window_end(const CareCoverageSources& x){try{
 const auto& g=context(x);check(validate_care_coverage_source_closure(x));const auto now=take(g.clock.read_current_moment());std::optional<WorldMoment> next;
 for(const auto& pair:x.tables.bindings.rows()){const auto& b=pair.second;if(b.phase!=CareBindingPhase::active||!b.until_tick)continue;
  const auto& sat=satisfaction(x,b.binding_id);const auto* goal=g.goals.requirements.find(sat.requirement_id);if(!goal||goal->state!=StateGoalState::satisfied||!goal->current_care_binding_ref||*goal->current_care_binding_ref!=b.binding_id||!same(goal->current_transition_ref,sat.handover_result_ref))fail("CareCurrentBindingMismatch",b.binding_id);
  const WorldMoment due{*b.until_tick,{}};if(take(compare_world_moments(due,now))<0)fail("CareAcceptedWindowLossMissing",b.binding_id);if(!next||take(compare_world_moments(due,*next))<0)next=due;
 }return {true,std::move(next),{}};
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Outcome<std::optional<CareRequirementApplication>> apply_care_coverage_window_end_in_candidate(CommonStore& common,CareGoalWriteTables goals,CareCoverageWriteTables tables,const CareGoalSourceContext& g,const StableId& id,Integer until){try{
 if(!g.coverage)fail("CareCoverageActualOwnerSourcesRequired",id,ErrorKind::unsupported_capability);const auto& x=*g.coverage;context(x);
 if(&common!=&g.common||&goals.requirements!=&g.goals.requirements||&goals.transitions!=&g.goals.transitions||&goals.temporal_events!=&g.goals.temporal_events||&tables.bindings!=&x.tables.bindings||&tables.satisfied!=&x.tables.satisfied||&tables.lost!=&x.tables.lost)fail("CareCoverageSameCandidateRequired",id);
 auto* binding=tables.bindings.find(id);if(!binding||!binding->until_tick||*binding->until_tick!=until)fail("CareActualAcceptedWindowRequired",id);check(validate_care_coverage_source_closure(x));
 if(binding->phase!=CareBindingPhase::active){if(const auto* previous=loss(x,id)){const auto& ev=loss_event(x,*previous);if(const auto* ended=std::get_if<CareAcceptedWindowEnded>(&std::get<CareCoverageLostEvent>(ev.body).cause);ended&&ended->until_tick==until)return {true,CareRequirementApplication{previous->requirement_id,ev.original_transition_ref,true},{}};}return {true,std::nullopt,{}};}
 const auto now=take(g.clock.read_current_moment());if(!same(now,WorldMoment{until,{}}))fail("CareWindowMustCloseAtActualCalendarGate",id);
 const auto& sat=satisfaction(x,id);auto* goal=goals.requirements.find(sat.requirement_id);const auto* previous=goals.transitions.find(sat.handover_result_ref.id);
 if(!goal||!previous||goal->state!=StateGoalState::satisfied||!goal->current_care_binding_ref||*goal->current_care_binding_ref!=id||!goal->accepted_care_terms_ref||*goal->accepted_care_terms_ref!=binding->accepted_ref||!same(goal->current_transition_ref,sat.handover_result_ref)||goal->revision!=previous->revision_after||goal->source_episode!=previous->source_episode_after||!common.actor(binding->child_actor_id)||!common.actor(binding->child_actor_id)->alive)fail("CareWindowCurrentGoalMismatch",id);
 if(goal->revision==std::numeric_limits<Integer>::max()||goal->source_episode==std::numeric_limits<Integer>::max())fail("CareCoverageRevisionOverflow",id);
 end_activity(common,tables,g,*binding,CareLossCauseV2{CareAcceptedWindowEnded{binding->accepted_ref,until}});
 const auto lost_id=take(common.next_id("care_coverage_lost"));const auto transition_id=take(common.next_id("care_transition"));const auto cause=care_acceptance_source(binding->accepted_ref);
 CareCoverageLost lost{lost_id,goal->id,id,cause,now.half_day_tick};check(validate_care_coverage_lost_shape(lost));if(!tables.lost.insert(lost_id,std::move(lost)))fail("DuplicateCareCoverageLoss",lost_id);check(common.append_fact({lost_id,"care_coverage_lost","care_requirements"}));
 CareRequirementTransition tr{transition_id,goal->id,goal->revision,goal->revision+1,goal->source_episode,goal->source_episode+1,CareRequirementTransition::Kind::coverage_lost,goal->current_transition_ref,care_coverage_lost_source(lost_id)};
 CareTemporalEvent ev{transition_id,transition_source(transition_id),now,goal->rule_version,CareCoverageLostEvent{goal->id,goal->source_episode,goal->source_episode+1,id,lost_id,CareAcceptedWindowEnded{binding->accepted_ref,until}}};
 if(!goals.transitions.insert(transition_id,std::move(tr))||!goals.temporal_events.insert(transition_id,std::move(ev)))fail("DuplicateCareTransition",transition_id);check(common.append_fact({transition_id,"care_requirement_changed","care_requirements"}));
 binding->phase=CareBindingPhase::interrupted;++goal->revision;++goal->source_episode;goal->state=StateGoalState::unsatisfied;goal->accepted_care_terms_ref.reset();goal->current_care_binding_ref.reset();goal->current_transition_ref=transition_source(transition_id);
 original_loss(x,*goal,*goals.transitions.find(transition_id),*goals.temporal_events.find(transition_id));return {true,CareRequirementApplication{goal->id,goal->current_transition_ref,false},{}};
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Outcome<CareRequirementApplication> close_care_coverage_after_subject_death_in_candidate(CommonStore& common,CareGoalWriteTables goals,CareCoverageWriteTables tables,const CareGoalSourceContext& g,const StableId& id,const DeathRef& death){try{
 if(!g.coverage)fail("CareCoverageActualOwnerSourcesRequired",id,ErrorKind::unsupported_capability);const auto& x=*g.coverage;context(x);
 if(&common!=&g.common||&goals.requirements!=&g.goals.requirements||&goals.transitions!=&g.goals.transitions||&goals.temporal_events!=&g.goals.temporal_events||&tables.bindings!=&x.tables.bindings||&tables.satisfied!=&x.tables.satisfied||&tables.lost!=&x.tables.lost)fail("CareCoverageSameCandidateRequired",id);
 auto* goal=goals.requirements.find(id);if(!goal)fail("CareRequirementMissing",id);const auto now=take(g.clock.read_current_moment());auto actual=take(read_verified_death4(common,g.deaths,g.life,death,now));if(actual.actor_id!=goal->subject_actor_id)fail("CareSubjectDeathActorMismatch",id);
 if(goal->state==StateGoalState::satisfied){
  const SourceRef cause{death.domain,death.kind,death.id};const auto prefix=take(read_care_requirement_before_original_fact(g,id,cause));
  if(!prefix.last_transition||prefix.state!=StateGoalState::satisfied||!same(prefix.last_transition->original_transition_ref,goal->current_transition_ref)||prefix.revision!=goal->revision||prefix.source_episode!=goal->source_episode||!goal->current_care_binding_ref)fail("CareSubjectDeathCurrentBindingMismatch",id);
  auto* binding=tables.bindings.find(*goal->current_care_binding_ref);if(!binding||binding->phase!=CareBindingPhase::active||binding->child_actor_id!=actual.actor_id||loss(x,binding->binding_id))fail("CareSubjectDeathCurrentBindingMismatch",id);
  if(actual.effective_at.kind!=ActorFoundationAt::Kind::exact_moment||actual.recorded_at.kind!=ActorFoundationAt::Kind::exact_moment||!same(actual.effective_at.exact_moment,now)||!same(actual.recorded_at.exact_moment,now))fail("CareSubjectDeathMustBeImmediate",id);
  end_activity(common,tables,g,*binding,death);
  binding->phase=CareBindingPhase::ended;
 }
 auto result=take(close_care_requirement_after_subject_death_in_candidate(common,goals,g,id,death));check(validate_care_coverage_source_closure(x));return {true,std::move(result),{}};
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Outcome<BirthCareNeedApplication> lose_care_coverage_and_need_after_holder_death_in_candidate(CommonStore& common,CareGoalWriteTables goals,CareCoverageWriteTables coverage,const CareGoalSourceContext& x,const StableId& binding,const DeathRef& death,AffairsState& affairs,const StateNeedPolicyRead& policies,const StateGoalRead& goal_read,StateTaskConsumer& tasks){
 auto applied=lose_care_coverage_after_holder_death_in_candidate(common,goals,coverage,x,binding,death);if(!applied.ok)return {false,{},applied.error};
 return reconcile_applied_care_goal_in_candidate(common,goals,x,std::move(applied.value),affairs,policies,goal_read,tasks);
}
Outcome<std::optional<BirthCareNeedApplication>> apply_care_coverage_and_need_window_end_in_candidate(CommonStore& common,CareGoalWriteTables goals,CareCoverageWriteTables coverage,const CareGoalSourceContext& x,const StableId& binding,Integer until,AffairsState& affairs,const StateNeedPolicyRead& policies,const StateGoalRead& goal_read,StateTaskConsumer& tasks){
 auto applied=apply_care_coverage_window_end_in_candidate(common,goals,coverage,x,binding,until);if(!applied.ok)return {false,{},applied.error};if(!applied.value)return {true,std::nullopt,{}};
 auto result=reconcile_applied_care_goal_in_candidate(common,goals,x,std::move(*applied.value),affairs,policies,goal_read,tasks);if(!result.ok)return {false,{},result.error};return {true,std::move(result.value),{}};
}
Status apply_due_care_coverage_and_need_window_ends_in_candidate(CommonStore& common,CareGoalWriteTables goals,CareCoverageWriteTables tables,const CareGoalSourceContext& g,AffairsState& affairs,const StateNeedPolicyRead& policies,const StateGoalRead& goal_read,StateTaskConsumer& tasks){try{
 if(!g.coverage)fail("CareCoverageActualOwnerSourcesRequired",{},ErrorKind::unsupported_capability);const auto& x=*g.coverage;const auto& bound=context(x);
 if(&bound.common!=&g.common||&bound.goals.requirements!=&g.goals.requirements||&bound.goals.transitions!=&g.goals.transitions||&bound.goals.temporal_events!=&g.goals.temporal_events||&bound.births!=&g.births||&bound.rules!=&g.rules||&bound.deaths!=&g.deaths||&bound.life!=&g.life||&bound.clock!=&g.clock)fail("CareCoverageSameCandidateRequired");
 if(&common!=&g.common||&goals.requirements!=&g.goals.requirements||&goals.transitions!=&g.goals.transitions||&goals.temporal_events!=&g.goals.temporal_events||&tables.bindings!=&x.tables.bindings||&tables.satisfied!=&x.tables.satisfied||&tables.lost!=&x.tables.lost)fail("CareCoverageSameCandidateRequired");
 const auto now=take(g.clock.read_current_moment());
 for(;;){
  if(!same(now,take(g.clock.read_current_moment())))fail("CareWindowMustCloseAtActualCalendarGate");
  // 原 predictor 每次验证实际 coverage、目标与 missed-until；它只给最早
  // M，因此 binding 仍从同一 owner 的原次序读取，不能把 M 当 ID。
  const auto next=take(next_care_coverage_window_end(x));if(!next||take(compare_world_moments(now,*next))<0)break;
  if(!same(now,*next))fail("CareAcceptedWindowLossMissing");
  StableId selected;Integer until=0;
  for(const auto& pair:x.tables.bindings.rows()){const auto& binding=pair.second;if(binding.phase==CareBindingPhase::active&&binding.until_tick&&same(WorldMoment{*binding.until_tick,{}},now)){selected=pair.first;until=*binding.until_tick;break;}}
  if(selected.empty())fail("CareActualAcceptedWindowRequired");
  // 只持真实 ID 值跨原写入；不保先前 row/goal 指针。原 writer 结束活动、
  // 产同一 loss/transition，随后原 Need reconcile，不重造 episode。
  auto applied=take(apply_care_coverage_and_need_window_end_in_candidate(common,goals,tables,g,selected,until,affairs,policies,goal_read,tasks));
  const auto* after=tables.bindings.find(selected);if(!applied||!after||after->phase==CareBindingPhase::active||!after->until_tick||*after->until_tick!=until)fail("CareAcceptedWindowLossMissing",selected);
 }
 if(!same(now,take(g.clock.read_current_moment())))fail("CareWindowMustCloseAtActualCalendarGate");
 check(validate_care_coverage_source_closure(x));return common_success();
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Outcome<BirthCareNeedApplication> close_care_coverage_and_need_after_subject_death_in_candidate(CommonStore& common,CareGoalWriteTables goals,CareCoverageWriteTables coverage,const CareGoalSourceContext& x,const StableId& id,const DeathRef& death,AffairsState& affairs,const StateNeedPolicyRead& policies,const StateGoalRead& goal_read,StateTaskConsumer& tasks){
 auto applied=close_care_coverage_after_subject_death_in_candidate(common,goals,coverage,x,id,death);if(!applied.ok)return {false,{},applied.error};return reconcile_applied_care_goal_in_candidate(common,goals,x,std::move(applied.value),affairs,policies,goal_read,tasks);
}

Outcome<BirthCareNeedApplication> lose_care_coverage_and_need_after_actual_separation_in_candidate(CommonStore& common,CareGoalWriteTables goals,CareCoverageWriteTables coverage,const CareGoalSourceContext& x,const StableId& binding,const MomentOwnerSource& source,AffairsState& affairs,const StateNeedPolicyRead& policies,const StateGoalRead& goal_read,StateTaskConsumer& tasks){
 auto applied=lose_care_coverage_after_actual_separation_in_candidate(common,goals,coverage,x,binding,source);if(!applied.ok)return {false,{},applied.error};
 return reconcile_applied_care_goal_in_candidate(common,goals,x,std::move(applied.value),affairs,policies,goal_read,tasks);
}

}
