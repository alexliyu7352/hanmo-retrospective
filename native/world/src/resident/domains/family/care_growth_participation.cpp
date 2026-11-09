#include "resident/domains/growth/growth_model.hpp"
#include "care_growth_participation.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
void check(Status v){if(!v.ok)throw Failure{std::move(v.error)};}
template<class T>T take(Outcome<T> v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
bool same(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b))==0;}
std::size_t index(const CommonStore& c,const SourceRef& r){const auto* h=c.state().facts.find(r.id);const auto& o=c.state().facts.order();const auto p=std::find(o.begin(),o.end(),r.id);if(!h||h->id!=r.id||h->domain!=r.domain||h->kind!=r.kind||p==o.end()||std::count(o.begin(),o.end(),r.id)!=1)fail("CareGrowthOriginalHeaderRequired",r.id);return static_cast<std::size_t>(p-o.begin());}
SourceRef ref(const FactRecord& h){return {h.domain,h.kind,h.id};}
const CharacterConditionSources& body(const CareFamilyActivitySources& x){const auto& a=x.coverage.handovers.acceptance;const auto& g=a.goals;if(g.rules.selected_care_version()!=6||&a.body.common!=&g.common||!a.body.growth||!a.body.growth_catalog||!a.body.growth_catalog->complete_exact_period_sources||x.coverage.originals.family_activity_sources()!=&x||!same(a.body.actual_now,take(g.clock.read_current_moment())))fail("CareGrowthSameOriginalSourcesRequired",{},ErrorKind::unsupported_capability);return a.body;}
}
Outcome<GrowthStartedParticipationRead> read_care_growth_start_conflict(const CareFamilyActivitySources& x,const CareFamilyActivity& a,const CareParticipationConflict& cause){try{
 const auto& b=body(x);if(x.activities.find(a.activity_id)!=&a||!actor_foundation_rule_equal(cause.compatibility_rule_ref,a.compatibility_rule_ref)||!actor_foundation_rule_equal(cause.compatibility_rule_ref,take(original_exclusive_family_care_rule(b.catalog.identity))))fail("CareGrowthActualCompatibilityRuleRequired",a.activity_id);
 const auto actual=take(read_growth_started_participation(b.common,*b.growth,cause.actual_occupation_change,a.carer_actor_id,b.actual_now));if(!actual)fail("CareGrowthActualCarerParticipationRequired",a.activity_id);
 const auto start=take(read_care_family_activity_start_provenance(x,{care_family_activity_started_source(a.activity_id),1}));
 if(index(b.common,start.completed->original_transition_ref)>=index(b.common,ref(*actual->original_header))||take(compare_world_moments(actual->started->occurred_at,a.started_at))<0)fail("CareGrowthConflictBeforeTakeover",a.activity_id);
 return {true,*actual,{}};
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Status validate_care_growth_start_conflict_coverage(const CareFamilyActivitySources& x,const CareFamilyActivity& a,const SourceRef& completed){try{
 const auto& b=body(x);const auto from=index(b.common,completed);const auto until=a.ended_by?index(b.common,care_family_activity_ended_source(a.ended_by->end_id)):b.common.state().facts.order().size();
 const FactRecord* first=nullptr;const LifeFixedServiceBoundary* started=nullptr;
 const auto& order=b.common.state().facts.order();for(std::size_t n=from+1;n<until;++n){const auto* h=b.common.state().facts.find(order[n]);if(!h)fail("CareGrowthActualFactPrefixRequired",order[n]);if(!is_growth_original_start_header(*h))continue;
  const auto actual=take(read_growth_started_participation_at_header(b.common,*b.growth,*h,a.carer_actor_id,b.actual_now));if(!actual)continue;
  if(first)fail("CareGrowthConflictDelayedPastNextAdoption",a.activity_id);first=actual->original_header;started=actual->started;
 }
 const auto* loss=a.ended_by?std::get_if<CareLossCauseV2>(&a.ended_by->cause):nullptr;const auto* cause=loss?std::get_if<CareParticipationConflict>(loss):nullptr;
 if(first){if(!cause||!started||cause->actual_occupation_change.source_schema!=1||cause->actual_occupation_change.source_ref.domain!="growth"||cause->actual_occupation_change.source_ref.kind!=first->kind||cause->actual_occupation_change.source_ref.id!=first->id||!same(a.ended_by->ended_at,started->occurred_at))fail("CareGrowthImmediateConflictEndMissing",a.activity_id);take(read_care_growth_start_conflict(x,a,*cause));}
 else if(cause&&cause->actual_occupation_change.source_ref.domain=="growth")fail("CareGrowthOriginalAdoptionMissing",a.activity_id);
 return common_success();
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
bool NativeCareGrowthStartedParticipationWrite::bound_to(const CommonStore& c,const GrowthState& g)const noexcept{
 const auto& a=binding_.sources.coverage.handovers.acceptance;
 return &binding_.common==&c&&&binding_.growth==&g&&&a.goals.common==&c&&&a.body.common==&c&&a.body.growth==&g;
}
Status NativeCareGrowthStartedParticipationWrite::consume_actual_growth_start(const MomentOwnerSource& source,const WorldMoment& at){try{
 auto& v=binding_;const auto& b=body(v.sources);const auto& g=v.sources.coverage.handovers.acceptance.goals;
 if(!bound_to(v.common,v.growth)||!same(at,b.actual_now)||!v.coverage.family_activities||&v.coverage.family_activities->sources!=&v.sources||&v.goals.requirements!=&g.goals.requirements||&v.goals.transitions!=&g.goals.transitions||&v.goals.temporal_events!=&g.goals.temporal_events)fail("CareGrowthSameWriteCandidateRequired");
 const auto identity=take(read_growth_started_business_identity(v.common,v.growth,source,at));
 if(!same(identity.actual_start->occurred_at,at)||v.common.state().history.empty()||v.common.state().history.back().result_id!=identity.actual_command->command_binding.result_id)fail("CareGrowthImmediateOriginalAdoptionRequired",source.source_ref.id);
 for(const auto& pair:v.sources.activities.rows()){
  const auto actual=take(read_growth_started_participation(v.common,v.growth,source,pair.second.carer_actor_id,at));if(!actual)continue;
  const auto start=take(read_care_family_activity_start_provenance(v.sources,{care_family_activity_started_source(pair.first),1}));
  const auto cut=ref(*actual->original_header);if(index(v.common,start.completed->original_transition_ref)>=index(v.common,cut))continue;
  const auto prior=take(read_care_requirement_before_original_fact(g,start.accepted->requirement_id,cut));
  const auto* completed=prior.last_transition?std::get_if<CareHandoverCompletedEvent>(&prior.last_transition->body):nullptr;
  if(prior.state!=StateGoalState::satisfied||!completed||completed->handover_id!=start.execution->handover_id)continue;
  auto application=take(lose_care_coverage_after_growth_start_in_candidate(v.common,v.goals,v.coverage,g,completed->binding_id,source));
  take(reconcile_applied_care_goal_in_candidate(v.common,v.goals,g,std::move(application),v.affairs,v.policies,v.goal_read,v.tasks));
 }
 check(validate_care_coverage_source_closure(v.sources.coverage));return common_success();
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
}
