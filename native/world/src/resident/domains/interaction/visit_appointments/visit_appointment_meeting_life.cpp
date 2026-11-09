#include "visit_appointment_meeting_time2.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "visit_appointment_detail.hpp"
#include "resident/domains/interaction/appointments/personal_appointment_detail.hpp"
#include "resident/kernel/calendar_gate.hpp"
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char*code,const StableId&id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
void take(Status v){if(!v.ok)throw Failure{std::move(v.error)};}
template<class T,class F>Outcome<T>guard(F f){try{return {true,f(),{}};}catch(Failure&e){return {false,{},std::move(e.error)};}}
bool same(const SourceRef&a,const SourceRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
bool same(const DeathRef&a,const DeathRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
SourceRef ref(const char*kind,const StableId&id){return {visit_appointment_meeting_module,kind,id};}
SourceRef ref(const DeathRef&d){return {d.domain,d.kind,d.id};}
std::size_t position(const CommonStore&c,const SourceRef&r){const auto*h=c.state().facts.find(r.id);if(!h||h->id!=r.id||h->kind!=r.kind||h->domain!=r.domain)fail("VisitAppointmentMeetingLifeOriginalHeaderMissing",r.id);return take(read_actual_common_fact_position(c,*h));}
void selected(const VisitAppointmentMeetingTime2Context&x){if(x.state.version!=2||x.catalog.version!=2||x.invitations.version!=2||&x.invitation_life.common!=&x.common||&x.invitation_life.state!=&x.invitations||&x.invitation_life.meetings!=&x.state||!x.life_policy.enabled||x.life_policy.selected_version!=4)fail("VisitAppointmentMeetingLife4Required",{},ErrorKind::unsupported_capability);take(validate_personal_appointment_catalog(x.catalog,x.definitions));}
VerifiedDeathView death(const VisitAppointmentMeetingTime2Context&x,const DeathRef&r){auto d=take(read_verified_death4(x.common,x.deaths,x.life_causes,r,take(x.clock.read_current_moment())));if(d.effective_at.kind!=ActorFoundationAt::Kind::exact_moment||d.recorded_at.kind!=ActorFoundationAt::Kind::exact_moment)fail("VisitAppointmentMeetingExactDeathRequired",r.id);position(x.common,ref(r));return d;}
const VisitAppointmentMeetingLifeSource*once(const VisitAppointmentMeetingState&s,const DeathRef&d,const StableId&appointment,VisitAppointmentLifeRole role){const VisitAppointmentMeetingLifeSource*out=nullptr;for(const auto&row:s.life_sources.rows())if(same(row.second.death_ref,d)&&row.second.invitation_fact_id==appointment&&row.second.role==role){if(out)fail("DuplicateVisitAppointmentMeetingLifeConsumption",appointment);out=&row.second;}return out;}
// Returns true only for this original still-open participation at the actual
// Death source boundary. Previously completed/cancelled history is immutable.
bool affected(const VisitAppointmentMeetingTime2Context&x,const VisitAppointmentAccepted&a,const VerifiedDeathView&d,VisitAppointmentLifeRole role){
 const auto*o=x.invitations.offered.find(a.invitation_fact_id);if(!o)fail("VisitAppointmentMeetingInvitationMissing",a.fact_id);const auto&actor=role==VisitAppointmentLifeRole::inviter?o->terms.inviter_actor_id:o->terms.invitee_actor_id;if(actor!=d.actor_id)return false;
 const auto cut=position(x.common,ref(d.death_ref));
 for(const auto&row:x.state.cancelled.rows())if(row.second.invitation_fact_id==a.invitation_fact_id){if(position(x.common,ref(visit_appointment_meeting_cancelled_kind,row.first))<cut)return false;fail("VisitAppointmentMeetingCancellationAfterDeath",row.first);}
 const auto*finished=visit_appointment_meeting_finish(x.state,a.invitation_fact_id);if(finished&&position(x.common,ref(visit_appointment_meeting_finished_kind,finished->fact_id))<cut)return false;
 if(position(x.common,SourceRef{visit_appointment_module,visit_appointment_accepted_kind,a.fact_id})>=cut)fail("VisitAppointmentMeetingAcceptanceAfterDeath",a.fact_id);
 auto present=take(x.invitation_life.people.read_presence_before_original_fact(actor,a.tick,{visit_appointment_module,visit_appointment_accepted_kind,a.fact_id}));if(!present.alive)fail("VisitAppointmentMeetingAcceptanceAfterEffectiveDeath",a.fact_id);
 const auto*start=visit_appointment_meeting_start(x.state,a.invitation_fact_id);if(!start)return take(calendar_deadline_accepts(o->terms.latest_tick,true,d.recorded_at.exact_moment));
 if(position(x.common,ref(visit_appointment_meeting_started_kind,start->fact_id))>=cut)fail("VisitAppointmentMeetingStartedAfterDeath",start->fact_id);
 if(finished){const auto*event=take(read_visit_appointment_meeting_event_time(x,{S3VisitAppointmentMeetingTable::finished,finished->fact_id},S3EventRole::finished));const auto*exact=take(read_s3_exact_event(*event));const auto*terminal=std::get_if<S3ActualTerminationSource>(&exact->actual_source);if(!terminal||!same(terminal->original_cause_ref,ref(d.death_ref)))fail("VisitAppointmentMeetingTerminalNotActualDeath",finished->fact_id);}
 return true;
}
const VisitAppointmentMeetingLifeSource&read(const VisitAppointmentMeetingTime2Context&x,const StableId&id){
 const auto*v=x.state.life_sources.find(id);if(!v||v->effect.result_id!=id||!appointment_detail::serial(id,"visit_appointment_meeting_life",x.common.state().next_instance_id))fail("VisitAppointmentMeetingLifeSourceMissing",id);
 take(validate_s3_leaf_life_effect_shape(v->effect,2));const auto*a=x.invitations.accepted.find(v->acceptance_fact_id);if(!a||a->invitation_fact_id!=v->invitation_fact_id)fail("VisitAppointmentMeetingLifeAcceptanceMissing",id);auto d=death(x,v->death_ref);
 if((v->role!=VisitAppointmentLifeRole::inviter&&v->role!=VisitAppointmentLifeRole::invitee)||!affected(x,*a,d,v->role)||once(x.state,v->death_ref,a->invitation_fact_id,v->role)!=v)fail("VisitAppointmentMeetingLifeNotActual",id);
 const auto*effect=std::get_if<S3AppointmentParticipationStopped>(&v->effect.effect);const auto terms=SourceRef{visit_appointment_module,visit_appointment_accepted_kind,a->fact_id};const auto*start=visit_appointment_meeting_start(x.state,a->invitation_fact_id);const auto*finish=visit_appointment_meeting_finish(x.state,a->invitation_fact_id);
 if(!effect||!v->effect.original_terms_ref||!same(*v->effect.original_terms_ref,terms)||take(compare_world_moments(v->effect.effective_game_time.exact_moment,d.effective_at.exact_moment)))fail("VisitAppointmentMeetingLifeTermsMismatch",id);
 if(v->effect.preserved_result_refs.size()!=(start?3u:1u)||!same(v->effect.preserved_result_refs.front(),terms))fail("VisitAppointmentMeetingLifePreservedResultMismatch",id);
 if(start){const auto begin=ref(visit_appointment_meeting_started_kind,start->fact_id);if(!finish||!effect->start_fact_ref||!same(*effect->start_fact_ref,begin)||!effect->original_action_terminal_ref||!same(*effect->original_action_terminal_ref,ref(visit_appointment_meeting_finished_kind,finish->fact_id))||!effect->actual_meeting_end_ref||!same(*effect->actual_meeting_end_ref,*effect->original_action_terminal_ref)||!same(v->effect.preserved_result_refs[1],begin)||!same(v->effect.preserved_result_refs[2],*effect->original_action_terminal_ref))fail("VisitAppointmentMeetingLifeTerminalMissing",id);take(read_visit_appointment_meeting_source_moment(x,*effect->original_action_terminal_ref,2));}
 else if(effect->start_fact_ref||effect->original_action_terminal_ref||effect->actual_meeting_end_ref)fail("VisitAppointmentMeetingUnstartedLifeHasAction",id);
 return *v;
}
}
Outcome<const VisitAppointmentMeetingLifeSource*>read_visit_appointment_meeting_life_source(const VisitAppointmentMeetingTime2Context&x,const StableId&id){return guard<const VisitAppointmentMeetingLifeSource*>([&]{selected(x);return &read(x,id);});}
Status validate_visit_appointment_meeting_life_sources(const VisitAppointmentMeetingTime2Context&x){return guard<Done>([&]{selected(x);if(x.state.life_sources.size()>131072)fail("VisitAppointmentMeetingLifeLimit");Integer last=0;for(const auto&row:x.state.life_sources.rows()){const auto n=appointment_detail::serial(row.first,"visit_appointment_meeting_life",x.common.state().next_instance_id);if(!n||*n<=last)fail("VisitAppointmentMeetingLifeOrderMismatch",row.first);last=*n;read(x,row.first);}
 for(const auto&row:x.deaths.deaths.rows()){auto d=death(x,{"family_life","family_death",row.first});for(const auto&appointment:x.invitations.accepted.rows())for(auto role:{VisitAppointmentLifeRole::inviter,VisitAppointmentLifeRole::invitee})if(affected(x,appointment.second,d,role)&&!once(x.state,d.death_ref,appointment.second.invitation_fact_id,role))fail("VisitAppointmentMeetingDeathConsumptionMissing",appointment.first);}return Done{};
});}
Outcome<std::vector<StableId>>apply_visit_appointment_meeting_death_in_candidate(CommonStore&common,VisitAppointmentMeetingState&state,ActionElapsedStore&elapsed,const VisitAppointmentMeetingTime2Context&x,const InteractionAccess&access,const DeathRef&source){return guard<std::vector<StableId>>([&]{
 if(&common!=&x.common||&state!=&x.state||&elapsed!=&x.elapsed)fail("VisitAppointmentMeetingLifeCandidateMismatch");selected(x);auto d=death(x,source);if(take(compare_world_moments(d.recorded_at.exact_moment,take(x.clock.read_current_moment()))))fail("VisitAppointmentMeetingActualDeathBoundaryRequired");for(const auto&row:state.life_sources.rows())read(x,row.first);std::vector<StableId>release;
 for(const auto&row:x.invitations.accepted.rows())for(auto role:{VisitAppointmentLifeRole::inviter,VisitAppointmentLifeRole::invitee}){
  if(!affected(x,row.second,d,role))continue;if(const auto*old=once(state,source,row.second.invitation_fact_id,role)){read(x,old->effect.result_id);continue;}if(state.life_sources.size()>=131072)fail("VisitAppointmentMeetingLifeLimit");
  const auto*started=visit_appointment_meeting_start(state,row.second.invitation_fact_id);if(started&&!visit_appointment_meeting_finish(state,row.second.invitation_fact_id)){const auto*actual=common.action(started->action_id);if(!actual)fail("VisitAppointmentMeetingDeathActionMissing",started->action_id);const auto action=*actual;take(finish_visit_appointment_meeting_time2_in_candidate(common,state,elapsed,x,access,action,source));release.push_back(action.id);}
  const auto id=take(common.next_id("visit_appointment_meeting_life"));VisitAppointmentMeetingLifeSource next;next.death_ref=source;next.invitation_fact_id=row.second.invitation_fact_id;next.acceptance_fact_id=row.first;next.role=role;next.effect.result_id=id;next.effect.original_terms_ref=SourceRef{visit_appointment_module,visit_appointment_accepted_kind,row.first};next.effect.effective_game_time=d.effective_at;next.effect.preserved_result_refs.push_back(*next.effect.original_terms_ref);S3AppointmentParticipationStopped effect;
  if(started){const auto*finished=visit_appointment_meeting_finish(state,row.second.invitation_fact_id);if(!finished)fail("VisitAppointmentMeetingDeathTerminalMissing",row.first);effect.start_fact_ref=ref(visit_appointment_meeting_started_kind,started->fact_id);effect.original_action_terminal_ref=ref(visit_appointment_meeting_finished_kind,finished->fact_id);effect.actual_meeting_end_ref=effect.original_action_terminal_ref;next.effect.preserved_result_refs.push_back(*effect.start_fact_ref);next.effect.preserved_result_refs.push_back(*effect.original_action_terminal_ref);}next.effect.effect=std::move(effect);if(!state.life_sources.insert(id,std::move(next)))fail("DuplicateVisitAppointmentMeetingLifeResult",id);read(x,id);
 }
 return release;
});}
}
