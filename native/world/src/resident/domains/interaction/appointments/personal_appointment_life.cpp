#include "personal_appointment_time2.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "personal_appointment_detail.hpp"
#include "resident/kernel/calendar_gate.hpp"
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char*code,const StableId&id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
void take(Status v){if(!v.ok)throw Failure{std::move(v.error)};}
template<class T,class F>Outcome<T>guard(F f){try{return {true,f(),{}};}catch(Failure&e){return {false,{},std::move(e.error)};}}
bool same(const SourceRef&a,const SourceRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
bool same(const DeathRef&a,const DeathRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
SourceRef ref(const char*kind,const StableId&id){return {personal_appointment_module,kind,id};}
SourceRef ref(const DeathRef&d){return {d.domain,d.kind,d.id};}
std::size_t position(const CommonStore&c,const SourceRef&r){const auto*h=c.state().facts.find(r.id);if(!h||h->id!=r.id||h->kind!=r.kind||h->domain!=r.domain)fail("PersonalAppointmentLifeOriginalHeaderMissing",r.id);return take(read_actual_common_fact_position(c,*h));}
void selected(const PersonalAppointmentTime2Context&x){if(x.state.version!=2||x.catalog.version!=2||!x.life_policy.enabled||x.life_policy.selected_version!=4)fail("PersonalAppointmentLife4Required",{},ErrorKind::unsupported_capability);take(validate_personal_appointment_catalog(x.catalog,x.definitions));}
VerifiedDeathView death(const PersonalAppointmentTime2Context&x,const DeathRef&r){auto d=take(read_verified_death4(x.common,x.deaths,x.life_causes,r,take(x.clock.read_current_moment())));if(d.effective_at.kind!=ActorFoundationAt::Kind::exact_moment||d.recorded_at.kind!=ActorFoundationAt::Kind::exact_moment)fail("PersonalAppointmentExactDeathRequired",r.id);position(x.common,ref(r));return d;}
const PersonalAppointmentLifeSource*once(const PersonalAppointmentState&s,const DeathRef&d,const StableId&appointment,PersonalAppointmentLifeRole role){const PersonalAppointmentLifeSource*out=nullptr;for(const auto&row:s.life_sources.rows())if(same(row.second.death_ref,d)&&row.second.appointment_id==appointment&&row.second.role==role){if(out)fail("DuplicatePersonalAppointmentLifeConsumption",appointment);out=&row.second;}return out;}
// Returns true only for this original still-open participation at the actual
// Death source boundary. Previously completed/cancelled history is immutable.
bool affected(const PersonalAppointmentTime2Context&x,const PersonalAppointmentAccepted&a,const VerifiedDeathView&d,PersonalAppointmentLifeRole role){
 const auto&actor=role==PersonalAppointmentLifeRole::inviter?a.contact.actor_id:a.contact.provider_actor_id;if(actor!=d.actor_id)return false;
 const auto cut=position(x.common,ref(d.death_ref));
 for(const auto&row:x.state.cancelled.rows())if(row.second.appointment_id==a.fact_id){if(position(x.common,ref(personal_appointment_cancelled_kind,row.first))<cut)return false;fail("PersonalAppointmentCancellationAfterDeath",row.first);}
 const auto*finished=appointment_detail::finish(x.state,a.fact_id);if(finished&&position(x.common,ref(personal_appointment_finished_kind,finished->fact_id))<cut)return false;
 if(position(x.common,ref(personal_appointment_accepted_kind,a.fact_id))>=cut)fail("PersonalAppointmentAcceptanceAfterDeath",a.fact_id);
 const auto accepted=take(read_personal_appointment_source_moment(x,ref(personal_appointment_accepted_kind,a.fact_id),2));if(take(compare_world_moments(accepted,d.effective_at.exact_moment))>0)fail("PersonalAppointmentAcceptanceAfterEffectiveDeath",a.fact_id);
 const auto*start=appointment_detail::start(x.state,a.fact_id);if(!start)return take(calendar_deadline_accepts(a.latest_tick,true,d.recorded_at.exact_moment));
 if(position(x.common,ref(personal_appointment_started_kind,start->fact_id))>=cut)fail("PersonalAppointmentStartedAfterDeath",start->fact_id);
 if(finished){const auto*event=take(read_personal_appointment_event_time(x,{S3PersonalAppointmentTable::finished,finished->fact_id},S3EventRole::finished));const auto*exact=take(read_s3_exact_event(*event));const auto*terminal=std::get_if<S3ActualTerminationSource>(&exact->actual_source);if(!terminal||!same(terminal->original_cause_ref,ref(d.death_ref)))fail("PersonalAppointmentTerminalNotActualDeath",finished->fact_id);}
 return true;
}
const PersonalAppointmentLifeSource&read(const PersonalAppointmentTime2Context&x,const StableId&id){
 const auto*v=x.state.life_sources.find(id);if(!v||v->effect.result_id!=id||!appointment_detail::serial(id,"personal_appointment_life",x.common.state().next_instance_id))fail("PersonalAppointmentLifeSourceMissing",id);
 take(validate_s3_leaf_life_effect_shape(v->effect,2));const auto*a=x.state.accepted.find(v->appointment_id);if(!a)fail("PersonalAppointmentLifeAcceptanceMissing",id);auto d=death(x,v->death_ref);
 if((v->role!=PersonalAppointmentLifeRole::inviter&&v->role!=PersonalAppointmentLifeRole::invitee)||!affected(x,*a,d,v->role)||once(x.state,v->death_ref,a->fact_id,v->role)!=v)fail("PersonalAppointmentLifeNotActual",id);
 const auto*effect=std::get_if<S3AppointmentParticipationStopped>(&v->effect.effect);const auto terms=ref(personal_appointment_accepted_kind,a->fact_id);const auto*start=appointment_detail::start(x.state,a->fact_id);const auto*finish=appointment_detail::finish(x.state,a->fact_id);
 if(!effect||!v->effect.original_terms_ref||!same(*v->effect.original_terms_ref,terms)||take(compare_world_moments(v->effect.effective_game_time.exact_moment,d.effective_at.exact_moment)))fail("PersonalAppointmentLifeTermsMismatch",id);
 if(v->effect.preserved_result_refs.size()!=(start?3u:1u)||!same(v->effect.preserved_result_refs.front(),terms))fail("PersonalAppointmentLifePreservedResultMismatch",id);
 if(start){const auto begin=ref(personal_appointment_started_kind,start->fact_id);if(!finish||!effect->start_fact_ref||!same(*effect->start_fact_ref,begin)||!effect->original_action_terminal_ref||!same(*effect->original_action_terminal_ref,ref(personal_appointment_finished_kind,finish->fact_id))||!effect->actual_meeting_end_ref||!same(*effect->actual_meeting_end_ref,*effect->original_action_terminal_ref)||!same(v->effect.preserved_result_refs[1],begin)||!same(v->effect.preserved_result_refs[2],*effect->original_action_terminal_ref))fail("PersonalAppointmentLifeTerminalMissing",id);take(read_personal_appointment_source_moment(x,*effect->original_action_terminal_ref,2));}
 else if(effect->start_fact_ref||effect->original_action_terminal_ref||effect->actual_meeting_end_ref)fail("PersonalAppointmentUnstartedLifeHasAction",id);
 return *v;
}
}
Outcome<const PersonalAppointmentLifeSource*>read_personal_appointment_life_source(const PersonalAppointmentTime2Context&x,const StableId&id){return guard<const PersonalAppointmentLifeSource*>([&]{selected(x);return &read(x,id);});}
Status validate_personal_appointment_life_sources(const PersonalAppointmentTime2Context&x){return guard<Done>([&]{selected(x);if(x.state.life_sources.size()>131072)fail("PersonalAppointmentLifeLimit");Integer last=0;for(const auto&row:x.state.life_sources.rows()){const auto n=appointment_detail::serial(row.first,"personal_appointment_life",x.common.state().next_instance_id);if(!n||*n<=last)fail("PersonalAppointmentLifeOrderMismatch",row.first);last=*n;read(x,row.first);}
 for(const auto&row:x.deaths.deaths.rows()){auto d=death(x,{"family_life","family_death",row.first});for(const auto&appointment:x.state.accepted.rows())for(auto role:{PersonalAppointmentLifeRole::inviter,PersonalAppointmentLifeRole::invitee})if(affected(x,appointment.second,d,role)&&!once(x.state,d.death_ref,appointment.first,role))fail("PersonalAppointmentDeathConsumptionMissing",appointment.first);}return Done{};
});}
Outcome<std::vector<StableId>>apply_personal_appointment_death_in_candidate(CommonStore&common,PersonalAppointmentState&state,ActionElapsedStore&elapsed,const PersonalAppointmentTime2Context&x,const InteractionAccess&access,const DeathRef&source){return guard<std::vector<StableId>>([&]{
 if(&common!=&x.common||&state!=&x.state||&elapsed!=&x.elapsed)fail("PersonalAppointmentLifeCandidateMismatch");selected(x);auto d=death(x,source);if(take(compare_world_moments(d.recorded_at.exact_moment,take(x.clock.read_current_moment()))))fail("PersonalAppointmentActualDeathBoundaryRequired");for(const auto&row:state.life_sources.rows())read(x,row.first);std::vector<StableId>release;
 for(const auto&row:state.accepted.rows())for(auto role:{PersonalAppointmentLifeRole::inviter,PersonalAppointmentLifeRole::invitee}){
  if(!affected(x,row.second,d,role))continue;if(const auto*old=once(state,source,row.first,role)){read(x,old->effect.result_id);continue;}if(state.life_sources.size()>=131072)fail("PersonalAppointmentLifeLimit");
  const auto*started=appointment_detail::start(state,row.first);if(started&&!appointment_detail::finish(state,row.first)){const auto*actual=common.action(started->action_id);if(!actual)fail("PersonalAppointmentDeathActionMissing",started->action_id);const auto action=*actual;take(finish_personal_appointment_time2_in_candidate(common,state,elapsed,x,access,action,source));release.push_back(action.id);}
  const auto id=take(common.next_id("personal_appointment_life"));PersonalAppointmentLifeSource next;next.death_ref=source;next.appointment_id=row.first;next.role=role;next.effect.result_id=id;next.effect.original_terms_ref=ref(personal_appointment_accepted_kind,row.first);next.effect.effective_game_time=d.effective_at;next.effect.preserved_result_refs.push_back(*next.effect.original_terms_ref);S3AppointmentParticipationStopped effect;
  if(started){const auto*finished=appointment_detail::finish(state,row.first);if(!finished)fail("PersonalAppointmentDeathTerminalMissing",row.first);effect.start_fact_ref=ref(personal_appointment_started_kind,started->fact_id);effect.original_action_terminal_ref=ref(personal_appointment_finished_kind,finished->fact_id);effect.actual_meeting_end_ref=effect.original_action_terminal_ref;next.effect.preserved_result_refs.push_back(*effect.start_fact_ref);next.effect.preserved_result_refs.push_back(*effect.original_action_terminal_ref);}next.effect.effect=std::move(effect);if(!state.life_sources.insert(id,std::move(next)))fail("DuplicatePersonalAppointmentLifeResult",id);read(x,id);
 }
 return release;
});}
}
