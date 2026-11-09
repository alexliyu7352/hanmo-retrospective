#include "care_family_activity_read.hpp"
#include "care_participant_capacity.hpp"
#include "care_separation.hpp"
#include "care_personal_stop.hpp"
#include "care_growth_participation.hpp"
#include "resident/domains/travel/care_dependent_placement.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
void check(Status s){if(!s.ok)throw Failure{std::move(s.error)};}
template<class T>T take(Outcome<T> v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
bool same(const SourceRef& a,const SourceRef& b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
bool same(const MomentOwnerSource& a,const MomentOwnerSource& b){return a.source_schema==b.source_schema&&same(a.source_ref,b.source_ref);}
bool same(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b))==0;}
bool same(const std::vector<MomentOwnerSource>& a,const std::vector<MomentOwnerSource>& b){if(a.size()!=b.size())return false;for(std::size_t i=0;i<a.size();++i)if(!same(a[i],b[i]))return false;return true;}
const CareGoalSourceContext& context(const CareFamilyActivitySources& x){
 const auto& g=x.coverage.handovers.acceptance.goals;
 if(g.rules.selected_care_version()!=6||&x.commands.handovers!=&x.coverage.handovers||
  !x.authority.bound_to(g.common)||!x.coverage.placements||!x.coverage.placements->bound_to(g.common))
  fail("CareFamilyActivityOriginalOwnersRequired",{},ErrorKind::unsupported_capability);
 return g;
}
std::size_t index(const CommonStore& c,const SourceRef& ref){const auto* row=c.state().facts.find(ref.id);const auto& order=c.state().facts.order();const auto at=std::find(order.begin(),order.end(),ref.id);if(!row||row->id!=ref.id||row->domain!=ref.domain||row->kind!=ref.kind||at==order.end()||std::count(order.begin(),order.end(),ref.id)!=1)fail("CareActivityOriginalCommonSourceRequired",ref.id);return static_cast<std::size_t>(at-order.begin());}
void before(const CommonStore& c,const SourceRef& a,const SourceRef& b){if(index(c,a)>=index(c,b))fail("CareActivityOriginalSourceOrderMismatch",b.id);}
const CareBinding& binding_for(const CareFamilyActivitySources& x,const CareFamilyActivity& activity){
 const CareBinding* found=nullptr;for(const auto& pair:x.coverage.tables.bindings.rows())if(same(pair.second.care_activity_ref,MomentOwnerSource{care_family_activity_started_source(activity.activity_id),1})){if(found||pair.first!=pair.second.binding_id)fail("CareActivityBindingNotUnique",activity.activity_id);found=&pair.second;}
 if(!found||found->accepted_ref!=activity.accepted_care_terms_ref||found->child_actor_id!=activity.child_actor_id||found->carer_actor_id!=activity.carer_actor_id)fail("CareActivityOriginalBindingRequired",activity.activity_id);return *found;
}
void original_end_cause(const CareFamilyActivitySources& x,const CareFamilyActivityRead& view,
 const CareFamilyActivityEnd& end,const SourceRef* actual_end){
 const auto& g=context(x);const auto& a=*view.activity;
 auto cause_order=[&](const SourceRef& ref){before(g.common,view.completed->original_transition_ref,ref);if(actual_end)before(g.common,ref,*actual_end);};
 auto death=[&](const DeathRef& ref,const StableId& actor){auto actual=take(read_verified_death4(g.common,g.deaths,g.life,ref,take(g.clock.read_current_moment())));if(actual.actor_id!=actor||actual.effective_at.kind!=ActorFoundationAt::Kind::exact_moment||actual.recorded_at.kind!=ActorFoundationAt::Kind::exact_moment||!same(actual.effective_at.exact_moment,end.ended_at)||!same(actual.recorded_at.exact_moment,end.ended_at))fail("CareActivityDeathSourceMismatch",end.end_id);cause_order({ref.domain,ref.kind,ref.id});};
 if(const auto* subject=std::get_if<CareSubjectDeath>(&end.cause))death(subject->death_ref,a.child_actor_id);
 else if(const auto* loss=std::get_if<CareLossCauseV2>(&end.cause)){
  if(const auto* holder=std::get_if<CareHolderDeath>(loss))death(holder->death_ref,a.carer_actor_id);
  else if(const auto* window=std::get_if<CareAcceptedWindowEnded>(loss)){
   if(window->accepted_care_terms_ref!=a.accepted_care_terms_ref||!view.accepted->terms.until_tick||window->until_tick!=*view.accepted->terms.until_tick||!same(end.ended_at,WorldMoment{window->until_tick,{}}))fail("CareActivityOriginalWindowEndMismatch",end.end_id);
  }else if(const auto* separated=std::get_if<CareActualSeparation>(loss)){
   if(!x.coverage.separations||!x.coverage.separations->bound_to(g.common))fail("CareActivityActualSeparationReadRequired",a.activity_id,ErrorKind::unsupported_capability);
   const auto actual=take(x.coverage.separations->read_actual_separation(binding_for(x,a),*view.accepted,separated->actual_location_or_binding_transition));
   if(!actual||!actual->actual_source||actual->source_schema!=separated->actual_location_or_binding_transition.source_schema||!same(actual->occurred_at,end.ended_at))fail("CareActivityActualSeparationMismatch",a.activity_id);
   cause_order(separated->actual_location_or_binding_transition.source_ref);
  }else if(const auto* capacity=std::get_if<CareRequiredCapacityLost>(loss)){
   const auto actual=take(read_care_participant_capacity_change(x.coverage.handovers.acceptance.body,capacity->capacity_transition_ref));
   if(actual.priority_holder_death_id)fail("CareActivityHolderDeathHasPriority",end.end_id);
   fail("CareActivityNonLifeCapacityRuleRequired",end.end_id,ErrorKind::unsupported_capability);
  }else if(const auto* conflict=std::get_if<CareParticipationConflict>(loss)){
   const auto actual=take(read_care_growth_start_conflict(x,a,*conflict));
   if(!same(actual.started->occurred_at,end.ended_at))fail("CareActivityGrowthConflictMomentMismatch",a.activity_id);
   const auto& h=*actual.original_header;cause_order({h.domain,h.kind,h.id});
  }else if(actual_end)check(x.authority.validate_original_activity_end(a,end,*actual_end));
  else fail("CareActivityActualEndWriterRequired",a.activity_id,ErrorKind::unsupported_capability);
 }else if(const auto* stop=std::get_if<CarePersonalStop>(&end.cause)){
  const auto actual=take(read_care_personal_stop_source(x,stop->actual_stop_result));
  if(actual.activity!=&a||actual.end!=&end)fail("CareActivityPersonalStopOwnerMismatch",a.activity_id);
  cause_order(stop->actual_stop_result.source_ref);
 }else fail("CareActivityActualStopWriterRequired",a.activity_id,ErrorKind::unsupported_capability);
}
void actual_end(const CareFamilyActivitySources& x,const CareFamilyActivityRead& view){
 const auto& g=context(x);const auto& a=*view.activity;if(!a.ended_by)return;
 const auto& end=*a.ended_by;const auto end_source=care_family_activity_ended_source(end.end_id);
 before(g.common,care_family_activity_started_source(a.activity_id),end_source);
 if(take(compare_world_moments(end.ended_at,take(g.clock.read_current_moment())))>0)fail("CareActivityFutureEnd",end.end_id);
 original_end_cause(x,view,end,&end_source);
}
void end_coverage(const CareFamilyActivitySources& x,const CareFamilyActivityRead& view,const CareBinding& binding){
 const auto& g=context(x);const auto& a=*view.activity;
 if(!a.ended_by){if(binding.phase!=CareBindingPhase::active)fail("CareActivityActualEndMissing",a.activity_id);return;}
 const auto& end=*a.ended_by;const CareTemporalEvent* outcome=nullptr;
 for(const auto& pair:g.goals.temporal_events.rows()){
  bool match=false;
  if(const auto* loss=std::get_if<CareCoverageLostEvent>(&pair.second.body);loss&&loss->old_binding_ref==binding.binding_id){
   match=care_family_activity_end_cause_equal(end.cause,CareFamilyActivityEndCause{loss->cause});
   if(const auto* stop=std::get_if<CarePersonalStop>(&end.cause))if(const auto* ended=std::get_if<CareAuthorityOrArrangementEnded>(&loss->cause))match=same(stop->actual_stop_result,ended->actual_ended_result);
   if(match&&binding.phase!=CareBindingPhase::interrupted)fail("CareActivityLossPhaseMismatch",a.activity_id);
  }else if(const auto* closed=std::get_if<CareRequirementClosedEvent>(&pair.second.body);closed&&closed->requirement_id==view.execution->requirement_id){
   const auto* why=std::get_if<CareSubjectDeath>(&closed->cause);const auto* tr=g.goals.transitions.find(pair.first);
   match=why&&tr&&tr->previous_transition_ref&&same(*tr->previous_transition_ref,view.completed->original_transition_ref)&&care_family_activity_end_cause_equal(end.cause,CareFamilyActivityEndCause{*why});
   if(match&&binding.phase!=CareBindingPhase::ended)fail("CareActivityClosedPhaseMismatch",a.activity_id);
  }
  if(match){if(outcome||pair.first!=pair.second.event_id||!same(pair.second.occurred_at,end.ended_at))fail("CareActivityEndOutcomeMismatch",a.activity_id);outcome=&pair.second;}
 }
 if(!outcome)fail("CareActivityEndOutcomeMissing",a.activity_id);
 const auto end_ref=care_family_activity_ended_source(end.end_id);
 if(const auto* loss=std::get_if<CareCoverageLostEvent>(&outcome->body))before(g.common,end_ref,care_coverage_lost_source(loss->lost_fact_id));
 else before(g.common,end_ref,outcome->original_transition_ref);
}
void end_in_candidate(CommonStore& common,CareFamilyActivityWriteBinding& write,
 const CareBinding& binding,CareFamilyActivityEndCause cause){
 const auto& x=write.sources;const auto& g=context(x);
 if(&g.common!=&common||&write.activities!=&x.activities||x.coverage.tables.bindings.find(binding.binding_id)!=&binding||x.coverage.originals.family_activity_sources()!=&x)fail("CareActivitySameCandidateRequired",binding.binding_id);
 auto view=take(read_care_family_activity(x,binding.care_activity_ref));
 if(&binding_for(x,*view.activity)!=&binding)fail("CareActivityBindingMismatch",binding.binding_id);
 auto* a=write.activities.find(view.activity->activity_id);if(a!=view.activity)fail("CareActivitySameCandidateRequired",binding.binding_id);
 if(a->ended_by){if(!care_family_activity_end_cause_equal(a->ended_by->cause,cause))fail("ConflictingCareActivityEnd",a->activity_id);end_coverage(x,view,binding);return;}
 if(binding.phase!=CareBindingPhase::active)fail("CareActivityActiveBindingRequired",binding.binding_id);
 CareFamilyActivityEnd end;end.ended_at=take(g.clock.read_current_moment());end.cause=std::move(cause);
 // Re-read the real original cause before allocating the one end source.
 original_end_cause(x,view,end,nullptr);
 if(take(compare_world_moments(end.ended_at,a->started_at))<0)fail("CareActivityEndBeforeStart",a->activity_id);
 end.end_id=take(common.next_id("care_family_activity_end"));const auto id=end.end_id;
 a->ended_by.emplace(std::move(end));check(validate_care_family_activity_shape(*a,6));
 check(common.append_fact({id,"care_family_activity_ended","care_requirements"}));
 // Lost/Closed is appended by the calling original writer. Any later failure
 // discards this whole candidate; there is no published half-ended activity.
}

}
Outcome<CareFamilyActivityRead> read_care_family_activity_start_provenance(const CareFamilyActivitySources& x,const MomentOwnerSource& ref){try{
 const auto& g=context(x);if(ref.source_schema!=1||ref.source_ref.domain!="care_requirements"||ref.source_ref.kind!="care_family_activity_started")fail("CareFamilyActivitySourceRequired",ref.source_ref.id,ErrorKind::unsupported_capability);
 const auto* a=x.activities.find(ref.source_ref.id);if(!a||a->activity_id!=ref.source_ref.id)fail("CareFamilyActivityMissing",ref.source_ref.id);check(validate_care_family_activity_shape(*a,6));index(g.common,ref.source_ref);
 const auto rule=take(original_exclusive_family_care_rule(x.coverage.handovers.acceptance.body.catalog.identity));if(!actor_foundation_rule_equal(rule,a->compatibility_rule_ref))fail("CareFamilyActivityOriginalRuleMismatch",a->activity_id);
 auto input=take(read_care_handover_placement_input(x.coverage.handovers,a->actual_handover_execution_ref.source_ref,a->actual_handover_execution_ref.source_schema));
 if(!input.execution||!input.accepted||!input.last_actual_period||!input.actual_completion)fail("CareFamilyActivityActualCompletionRequired",a->activity_id);
 const auto& e=*input.execution;const auto& accepted=*input.accepted;const auto& completed=*input.actual_completion;const auto* body=std::get_if<CareHandoverCompletedEvent>(&completed.body);
 if(!body||accepted.accepted_id!=a->accepted_care_terms_ref||accepted.subject_actor_id!=a->child_actor_id||accepted.provider.carer_actor_id!=a->carer_actor_id||!same(a->actual_start_work_period_ref,MomentOwnerSource{care_handover_period_source(input.last_actual_period->source_id),2})||!same(a->started_at,input.last_actual_period->provided_at)||!same(a->started_at,completed.occurred_at)||!same(a->actual_placement_result_ref,body->placement_result_ref)||!same(a->adopted_participation_refs,body->actual_participation_refs)||e.handover_execution_boundaries.empty())fail("CareFamilyActivityOriginalHandoverMismatch",a->activity_id);
 check(validate_care_handover_command_source(x.commands,e,e.handover_execution_boundaries.front()));
 before(g.common,care_acceptance_source(accepted.accepted_id),a->actual_handover_execution_ref.source_ref);
 before(g.common,a->actual_handover_execution_ref.source_ref,a->actual_placement_result_ref.source_ref);
 before(g.common,a->actual_placement_result_ref.source_ref,ref.source_ref);
 before(g.common,ref.source_ref,completed.original_transition_ref);
 const auto placed=take(x.coverage.placements->read_placement_result(a->actual_placement_result_ref));if(!placed.original||placed.original->child_actor_id!=a->child_actor_id||placed.original->carer_actor_id!=a->carer_actor_id||!same(placed.original->occurred_at,a->started_at)||!same(placed.original->actual_handover_source,a->actual_handover_execution_ref))fail("CareFamilyActivityActualPlacementMismatch",a->activity_id);
 check(validate_care_participants_at_actual_handover(x.coverage.handovers.acceptance.body,a->child_actor_id,a->carer_actor_id,ref.source_ref,a->started_at,g.clock));
 check(x.authority.validate_activity_authority_before_source(accepted,ref.source_ref,a->started_at));
 check(x.authority.validate_actual_handover_inputs(e,accepted,a->started_at,
  a->adopted_participation_refs,body->fee_result_refs));
 CareFamilyActivityRead view{a,&accepted,&e,&completed};return {true,view,{}};
 }catch(Failure& error){return {false,{},std::move(error.error)};}}
Outcome<CareFamilyActivityRead> read_care_family_activity(const CareFamilyActivitySources& x,const MomentOwnerSource& ref){try{
 auto view=take(read_care_family_activity_start_provenance(x,ref));actual_end(x,view);return {true,view,{}};
 }catch(Failure& error){return {false,{},std::move(error.error)};}}
Outcome<CareFamilyActivityActualSourceRead> read_care_family_activity_source(
 const CareFamilyActivitySources& x,const SourceRef& ref,Integer schema){try{
 const auto expected=take(care_family_activity_source_schema(ref));
 if(schema!=expected)fail("CareFamilyActivitySourceSchemaMismatch",ref.id);
 const CareFamilyActivity* owner=nullptr;
 if(ref.kind=="care_family_activity_started")owner=x.activities.find(ref.id);
 else for(const auto& pair:x.activities.rows())if(pair.second.ended_by&&pair.second.ended_by->end_id==ref.id){
  if(owner)fail("CareFamilyActivityEndNotUnique",ref.id);owner=&pair.second;
 }
 if(!owner)fail("CareFamilyActivitySourceMissing",ref.id);
 const auto actual=take(read_care_family_activity(x,{care_family_activity_started_source(owner->activity_id),1}));
 if(actual.activity!=owner)fail("CareFamilyActivitySourceOwnerMismatch",ref.id);
 const auto* end=ref.kind=="care_family_activity_ended"?&*owner->ended_by:nullptr;
 return {true,{owner,end,end?&end->ended_at:&owner->started_at},{}};
 }catch(Failure& error){return {false,{},std::move(error.error)};}}
Status validate_care_family_activity_sources(const CareFamilyActivitySources& x){try{
 const auto& g=context(x);for(const auto& pair:x.activities.rows()){
  if(pair.first!=pair.second.activity_id)fail("CareFamilyActivityKeyMismatch",pair.first);
  const auto view=take(read_care_family_activity(x,{care_family_activity_started_source(pair.first),1}));const auto& a=*view.activity;
  const CareBinding* binding=nullptr;for(const auto& candidate:x.coverage.tables.bindings.rows())if(same(candidate.second.care_activity_ref,MomentOwnerSource{care_family_activity_started_source(pair.first),1})){if(binding)fail("CareFamilyActivityBindingNotUnique",pair.first);binding=&candidate.second;}
  if(!binding||binding->child_actor_id!=a.child_actor_id||binding->carer_actor_id!=a.carer_actor_id||binding->accepted_ref!=a.accepted_care_terms_ref)fail("CareFamilyActivityBindingCoverageMissing",pair.first);
  end_coverage(x,view,*binding);
  const auto started=index(g.common,care_family_activity_started_source(a.activity_id));
  for(const auto& other:x.activities.rows())if(other.first!=pair.first&&(other.second.carer_actor_id==a.carer_actor_id||other.second.child_actor_id==a.child_actor_id)){
   const auto next=index(g.common,care_family_activity_started_source(other.second.activity_id));
   if(started<next&&(!a.ended_by||index(g.common,care_family_activity_ended_source(a.ended_by->end_id))>=next))fail("CareFamilyActivityHistoricalExclusiveOverlap",a.activity_id);
  }

  const auto now=take(g.clock.read_current_moment());const auto until=a.ended_by?a.ended_by->ended_at:now;std::optional<SourceRef> end_ref;if(a.ended_by)end_ref=care_family_activity_ended_source(a.ended_by->end_id);
  check(x.authority.validate_original_activity_continuation(a,*view.accepted,{a.started_at,until},end_ref?&*end_ref:nullptr));
  check(validate_care_growth_start_conflict_coverage(x,a,view.completed->original_transition_ref));
  if(!a.ended_by){const auto* actor=g.common.actor(a.carer_actor_id);if(!actor||!actor->action_id.empty())fail("CareFamilyActivityImmediateOccupationEndMissing",pair.first);
   for(const auto& other:x.activities.rows())if(other.first!=pair.first&&!other.second.ended_by&&(other.second.carer_actor_id==a.carer_actor_id||other.second.child_actor_id==a.child_actor_id))fail("CareFamilyActivityExclusiveOverlap",pair.first);
  }
 }
 for(const auto& pair:g.common.state().facts.rows())if(pair.second.domain=="care_requirements"){
  if(pair.second.kind=="care_stopped"){std::size_t count=0;for(const auto& row:x.activities.rows())if(row.second.ended_by)if(const auto* stop=std::get_if<CarePersonalStop>(&row.second.ended_by->cause);stop&&stop->actual_stop_result.source_ref.id==pair.first)++count;if(count!=1)fail("CarePersonalStopPayloadMissing",pair.first);}
  if(pair.second.kind=="care_family_activity_started"&&!x.activities.find(pair.first))fail("CareFamilyActivityPayloadMissing",pair.first);
  if(pair.second.kind=="care_family_activity_ended"){std::size_t count=0;for(const auto& a:x.activities.rows())if(a.second.ended_by&&a.second.ended_by->end_id==pair.first)++count;if(count!=1)fail("CareFamilyActivityEndPayloadMissing",pair.first);}
 }
 return common_success();}catch(Failure& error){return {false,{},std::move(error.error)};}}
bool NativeCareFamilyActivityRead::bound_to(const CommonStore& common)const noexcept{return &sources_.coverage.handovers.acceptance.goals.common==&common&&sources_.authority.bound_to(common);}
Status NativeCareFamilyActivityRead::validate_original_completed_handover(const CareHandoverExecution& execution,const AcceptedCaregiver& accepted,const CareBinding& binding,const CareTemporalEvent& event)const{try{
 const auto actual=take(read_care_family_activity(sources_,binding.care_activity_ref));if(actual.execution!=&execution||actual.accepted!=&accepted||actual.completed!=&event||actual.activity->child_actor_id!=binding.child_actor_id||actual.activity->carer_actor_id!=binding.carer_actor_id||!same(actual.activity->actual_placement_result_ref,binding.placement_ref))fail("CareActivityCompletedBindingMismatch",binding.binding_id);return common_success();
 }catch(Failure& error){return {false,{},std::move(error.error)};}}
Status NativeCareFamilyActivityRead::validate_current_care_participation(const CareBinding& binding,const AcceptedCaregiver& accepted,const WorldMoment& now)const{try{
 const auto view=take(read_care_family_activity(sources_,binding.care_activity_ref));const auto& g=context(sources_);if(view.accepted!=&accepted||view.activity->ended_by||!same(now,take(g.clock.read_current_moment())))fail("CareActivityCurrentSourceMismatch",binding.binding_id);
 const auto* actor=g.common.actor(binding.carer_actor_id);if(!actor||!actor->action_id.empty())fail("CareActivityExclusiveOccupationMismatch",binding.binding_id);
 check(sources_.authority.validate_current_activity_authority(accepted,now));
 check(sources_.authority.validate_original_activity_continuation(*view.activity,accepted,{view.activity->started_at,now},nullptr));return common_success();
 }catch(Failure& error){return {false,{},std::move(error.error)};}}
Status end_care_family_activity_for_loss_in_candidate(CommonStore& common,CareFamilyActivityWriteBinding& write,const CareBinding& binding,const CareLossCauseV2& cause){try{end_in_candidate(common,write,binding,CareFamilyActivityEndCause{cause});return common_success();}catch(Failure& error){return {false,{},std::move(error.error)};}}
Status end_care_family_activity_for_subject_death_in_candidate(CommonStore& common,CareFamilyActivityWriteBinding& write,const CareBinding& binding,const DeathRef& cause){try{end_in_candidate(common,write,binding,CareFamilyActivityEndCause{CareSubjectDeath{cause}});return common_success();}catch(Failure& error){return {false,{},std::move(error.error)};}}

}
