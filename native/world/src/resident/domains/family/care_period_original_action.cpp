#include "care_period_original_action.hpp"
#include <algorithm>
#include <limits>
#include <tuple>
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
template<class T>T take(Outcome<T> v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
const FactRecord& header(const CommonStore& c,const SourceRef& ref){const auto* h=c.state().facts.find(ref.id);if(!h||h->id!=ref.id||h->domain!=ref.domain||h->kind!=ref.kind||std::count(c.state().facts.order().begin(),c.state().facts.order().end(),ref.id)!=1)fail("CareOriginalActionFactRequired",ref.id);return *h;}
void before(const CommonStore& c,const SourceRef& a,const SourceRef& b){header(c,a);header(c,b);const auto& o=c.state().facts.order();if(std::find(o.begin(),o.end(),a.id)>=std::find(o.begin(),o.end(),b.id))fail("CareOriginalActionSourceOrderMismatch",b.id);}
void period(const CommonStore& c,const WorldMoment& now,Integer from,Integer to,Integer duration,Integer start_revision,Integer end_revision,const StableId& id){
 if(duration!=1)fail("CareOriginalWholeHalfDayPeriodRequired",id,ErrorKind::unsupported_capability);
 if(from<0||from==std::numeric_limits<Integer>::max()||to!=from+1||start_revision<0||end_revision<start_revision||end_revision>c.revision()||take(compare_world_moments(WorldMoment{to,{}},now))>0)fail("CareOriginalCalendarPeriodMismatch",id);
}
}
Outcome<CareOriginalActionPeriodView> read_care_original_action_period(const CareOriginalActionPeriodSources& x,const OriginalCareActionParticipation& refs){try{
 const auto& c=x.common;const auto now=take(x.clock.read_current_moment());const auto& a=refs.original_action_ref;const auto& p=refs.provided_period_ref;
 if(a.source_schema!=1||p.source_schema!=1)fail("CareOriginalActionSourceSchemaRequired",p.source_ref.id,ErrorKind::unsupported_capability);
 const auto& sh=header(c,a.source_ref);const auto& ph=header(c,p.source_ref);before(c,a.source_ref,p.source_ref);
 if(a.source_ref.domain=="family_birth"&&a.source_ref.kind=="family_child_care_started"&&p.source_ref.domain=="family_birth"&&p.source_ref.kind=="family_child_care"){
  const auto* s=x.births.care_starts.find(a.source_ref.id);const auto* r=x.births.care.find(p.source_ref.id);
  if(!s||!r||s->kind!=sh.kind||r->kind!=ph.kind||s->action_id!=a.source_ref.id||r->action_id!=s->action_id||
   std::tie(r->accepted_id,r->child_actor_id,r->caregiver_actor_id,r->place_id,r->started_tick,r->period_index)!=std::tie(s->accepted_id,s->child_actor_id,s->caregiver_actor_id,s->place_id,s->tick,s->period_index))fail("CareOriginalChildActionPayloadMismatch",p.source_ref.id);
  std::size_t count=0;for(const auto& pair:x.births.care.rows())if(pair.second.action_id==s->action_id)++count;if(count!=1)fail("CareOriginalCompletionNotUnique",p.source_ref.id);
  period(c,now,s->tick,r->tick,s->duration,s->revision,r->revision,p.source_ref.id);
  const auto* accepted=x.births.replies.find(s->accepted_id);
  if(!accepted||!accepted->confirmation||accepted->decision!="accepted"||accepted->kind!="family_birth_accepted")fail("CareOriginalChildAcceptanceRequired",s->accepted_id);
  before(c,{"family_birth","family_birth_accepted",s->accepted_id},a.source_ref);
  const auto& terms=accepted->confirmation->terms;
  if(s->caregiver_actor_id!=terms.caregiver_actor_id||s->place_id!=terms.place_id||s->duration!=terms.care_period_ticks||s->period_index<1||s->period_index>terms.prepaid_care_periods||r->outcome!="completed"||!r->code.empty()||!r->interruption_actor_id.empty()||!r->interruption_fact_id.empty()||r->food_consumed!=terms.food_per_period||r->food_consumed<0)fail("CareOriginalChildCompletionMismatch",p.source_ref.id);
  const FamilyBirthRecord* child=nullptr;const StableId* born_id=nullptr;for(const auto& pair:x.births.births.rows())if(pair.second.accepted_id==s->accepted_id){if(child)fail("CareOriginalChildBirthNotUnique",s->child_actor_id);child=&pair.second;born_id=&pair.first;}
  if(!child||child->actor_id!=s->child_actor_id)fail("CareOriginalChildBirthRequired",s->child_actor_id);
  before(c,{"family_birth","family_birth_recorded",*born_id},a.source_ref);
  return {true,{&s->caregiver_actor_id,&s->child_actor_id,&s->action_id,&s->accepted_id,&s->place_id,&sh,&ph,s->tick,r->tick,r->food_consumed,s,r,nullptr,nullptr},{}};
 }
 if(a.source_ref.domain=="family_care_relief"&&a.source_ref.kind=="family_care_relief_started"&&p.source_ref.domain=="family_care_relief"&&p.source_ref.kind=="family_care_relief_provided"){
  if(!x.relief)fail("CareOriginalReliefSourcesRequired",p.source_ref.id,ErrorKind::unsupported_capability);
  const auto* s=x.relief->starts.find(a.source_ref.id);const auto* r=x.relief->care.find(p.source_ref.id);
  if(!s||!r||s->kind!=sh.kind||r->kind!=ph.kind||s->action_id!=a.source_ref.id||r->action_id!=s->action_id||
   std::tie(r->accepted_id,r->child_actor_id,r->provider_actor_id,r->place_id,r->started_tick,r->period_index)!=std::tie(s->accepted_id,s->child_actor_id,s->provider_actor_id,s->place_id,s->tick,s->period_index))fail("CareOriginalReliefActionPayloadMismatch",p.source_ref.id);
  std::size_t count=0;for(const auto& pair:x.relief->care.rows())if(pair.second.action_id==s->action_id)++count;if(count!=1)fail("CareOriginalCompletionNotUnique",p.source_ref.id);
  period(c,now,s->tick,r->tick,s->duration,s->revision,r->revision,p.source_ref.id);
  const auto* accepted=x.relief->agreements.find(s->accepted_id);if(!accepted||accepted->kind!="family_care_relief_accepted")fail("CareOriginalReliefAcceptanceRequired",s->accepted_id);
  before(c,{"family_care_relief","family_care_relief_accepted",s->accepted_id},a.source_ref);const auto& original=accepted->source;const auto& terms=original.terms;
  if(original.provider_actor_id!=s->provider_actor_id||original.child_actor_id!=s->child_actor_id||s->place_id!=terms.place_id||s->duration!=terms.period_ticks||s->period_index<1||s->period_index>terms.prepaid_periods||r->outcome!="completed"||!r->code.empty()||!r->interruption_fact_id.empty()||r->food_consumed!=terms.food_per_period||r->food_consumed<0)fail("CareOriginalReliefCompletionMismatch",p.source_ref.id);
  const auto* child=x.births.births.find(original.birth_fact_id);if(!child||child->actor_id!=s->child_actor_id||child->accepted_id!=original.original_accepted_id)fail("CareOriginalReliefBirthRequired",s->child_actor_id);
  before(c,{"family_birth","family_birth_recorded",original.birth_fact_id},{"family_care_relief","family_care_relief_accepted",s->accepted_id});
  return {true,{&s->provider_actor_id,&s->child_actor_id,&s->action_id,&s->accepted_id,&s->place_id,&sh,&ph,s->tick,r->tick,r->food_consumed,nullptr,nullptr,s,r},{}};
 }
 fail("CareRegisteredOriginalActionRequired",p.source_ref.id,ErrorKind::unsupported_capability);
 }catch(Failure& error){return {false,{},std::move(error.error)};}}
}
