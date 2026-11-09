#include "director_appointment_wake2.hpp"
#include "resident/domains/interaction/appointments/personal_appointment_time2.hpp"
#include "resident/domains/interaction/visit_appointments/visit_appointment_meeting_time2.hpp"
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char*code,const StableId&id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
void take(Status v){if(!v.ok)throw Failure{std::move(v.error)};}
template<class T,class F>Outcome<T>guard(F f){try{return {true,f(),{}};}catch(Failure&e){return {false,{},std::move(e.error)};}}
bool same(const SourceRef&a,const SourceRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
const PersonalAppointmentAttendancePolicy*policy(const PersonalAppointmentAttendanceCatalog&c,const StableId&id){for(const auto&p:c.policies)if(p.appointment_policy_id==id)return &p;return nullptr;}
const PersonalAppointmentAttendancePolicy*written_policy(const VisitAppointmentAttendanceRead&r,const VisitAppointmentAttendanceCatalog&c,const VisitAppointmentOffered&o){if(std::find(c.invitation_policy_ids.begin(),c.invitation_policy_ids.end(),o.policy_id)==c.invitation_policy_ids.end())return nullptr;const auto*p=visit_appointment_policy(r.invitations,o.policy_id);return p?policy(r.original_attendance,p->appointment_policy_id):nullptr;}
bool selected(const DirectorAppointmentWakeRead2&x,const StableId&actor){
 if(actor==x.original.common.state().metadata.player_actor_id)return false;
 const auto*r=x.director.subject2->registrations.find(director_subject_key2({SubjectKind::actor,actor}));if(!r)return false;
 const auto*p=x.policies.policies.find(r->policy_id);if(!p)fail("DirectorAppointmentPolicyMissing",r->policy_id);
 return std::find(p->templates.begin(),p->templates.end(),DirectorTemplate::personal_appointment_attendance)!=p->templates.end();
}
void emit(const DirectorAppointmentWakeRead2&x,const DirectorOriginalWakeVisitor2&visit,const StableId&actor,const SourceRef&ref,Integer schema,const WorldMoment&at){
 if(!selected(x,actor))return;if(take(compare_world_moments(at,x.actual))>0)fail("DirectorAppointmentSourceFromFuture",ref.id);
 take(visit({{SubjectKind::actor,actor},SubjectWakeKind2::commitment,ref,schema,0},at));
}
WorldMoment death_at(const PersonalAppointmentTime2Context&x,const DeathRef&d,const WorldMoment&actual){
 if(!x.life_policy.enabled||x.life_policy.selected_version!=4)fail("DirectorAppointmentDeath4Required",d.id,ErrorKind::unsupported_capability);
 const auto v=take(read_verified_death4(x.common,x.deaths,x.life_causes,d,actual));if(v.recorded_at.kind!=ActorFoundationAt::Kind::exact_moment)fail("DirectorAppointmentActualDeathMomentRequired",d.id);return v.recorded_at.exact_moment;
}
}
Status validate_director_appointment_wake_sources2(const DirectorAppointmentWakeRead2&x){return guard<Done>([&]{
 if(!x.director.subject2||!x.policies.subjects2||x.policies.personal_appointment_attendance.get()!=&x.catalog)fail("DirectorAppointmentSelectedOwnerMismatch");
 const auto&r=x.original;if(!r.exact||r.state.version!=2||r.catalog.version!=2)fail("DirectorAppointmentTime2SourcesRequired",{},ErrorKind::unsupported_capability);
 if(take(compare_world_moments(take(r.exact->clock.read_current_moment()),x.actual)))fail("DirectorAppointmentClockMismatch");
 Integer written_markers=0;for(const auto&m:r.exact->identity.module_versions)if(m.first==visit_appointment_attendance_module){++written_markers;if(m.second!="1")fail("UnsupportedVisitAppointmentAttendanceVersion");}
 if(written_markers!=(r.written_visits?1:0)||bool(r.written_visits)!=bool(r.written_catalog))fail("DirectorVisitAppointmentSelectionMismatch");
 take(validate_personal_appointment_attendance_sources(r,x.catalog));
 if(r.written_visits){const auto&w=*r.written_visits;if(!w.exact||w.state.version!=2||w.meetings.version!=2)fail("DirectorVisitAppointmentTime2SourcesRequired",{},ErrorKind::unsupported_capability);if(take(compare_world_moments(take(w.exact->clock.read_current_moment()),x.actual)))fail("DirectorVisitAppointmentClockMismatch");}
 return Done{};
});}
Status visit_director_appointment_wakes2(const DirectorAppointmentWakeRead2&x,const DirectorOriginalWakeVisitor2&visit){return guard<Done>([&]{
 take(validate_director_appointment_wake_sources2(x));if(!visit)fail("DirectorAppointmentWakeVisitorRequired");const auto&r=x.original;const auto&exact=*r.exact;
 // The actual accepted source is retained even after a later cancellation or
 // completion, so a saved pending wake can still be proved during cold restore.
 for(const auto&row:r.state.accepted.rows()){const auto&a=row.second;const auto*p=policy(x.catalog,a.policy_id);if(!p||x.actual.half_day_tick<std::max<Integer>(0,a.earliest_tick-p->depart_before_ticks))continue;const SourceRef ref{personal_appointment_module,personal_appointment_accepted_kind,a.fact_id};emit(x,visit,a.contact.provider_actor_id,ref,2,take(read_personal_appointment_source_moment(exact,ref,2)));}
 auto personal=[&](const auto&rows,const char*kind){for(const auto&row:rows.rows()){const auto*original=r.state.accepted.find(row.second.appointment_id);if(!original||!policy(x.catalog,original->policy_id))continue;const SourceRef ref{personal_appointment_module,kind,row.first};emit(x,visit,original->contact.provider_actor_id,ref,2,take(read_personal_appointment_source_moment(exact,ref,2)));}};
 personal(r.state.started,personal_appointment_started_kind);personal(r.state.cancelled,personal_appointment_cancelled_kind);personal(r.state.finished,personal_appointment_finished_kind);
 // Unstarted life interruption has no invented cancellation Fact. Its actual
 // Death4 source plus this owner's verified life result proves the invalidation.
 for(const auto&row:r.state.life_sources.rows()){const auto*v=take(read_personal_appointment_life_source(exact,row.first));const auto*a=r.state.accepted.find(v->appointment_id);if(!a||!policy(x.catalog,a->policy_id))continue;emit(x,visit,a->contact.provider_actor_id,{v->death_ref.domain,v->death_ref.kind,v->death_ref.id},4,death_at(exact,v->death_ref,x.actual));}
 if(!r.written_visits)return Done{};const auto&w=*r.written_visits;const auto&c=*r.written_catalog;const auto&time=*w.exact;
 for(const auto&row:w.state.offered.rows()){const auto&o=row.second;const auto*p=written_policy(w,c,o);if(!p||o.contact.actor_id!=o.terms.inviter_actor_id||x.actual.half_day_tick<std::max<Integer>(0,o.terms.earliest_tick-p->depart_before_ticks))continue;
  // Original offered is Calendar precision/schema1. The real gate observation
  // is x.actual, not (offered.tick,0) and not a fabricated owner event Moment.
  emit(x,visit,o.terms.inviter_actor_id,{visit_appointment_module,visit_appointment_offered_kind,o.fact_id},1,x.actual);
 }
 auto meeting=[&](const auto&rows,const char*kind){for(const auto&row:rows.rows()){const auto*o=w.state.offered.find(row.second.invitation_fact_id);if(!o||o->contact.actor_id!=o->terms.inviter_actor_id||!written_policy(w,c,*o))continue;const SourceRef ref{visit_appointment_meeting_module,kind,row.first};emit(x,visit,o->terms.inviter_actor_id,ref,2,take(read_visit_appointment_meeting_source_moment(time,ref,2)));}};
 meeting(w.meetings.started,visit_appointment_meeting_started_kind);meeting(w.meetings.cancelled,visit_appointment_meeting_cancelled_kind);meeting(w.meetings.finished,visit_appointment_meeting_finished_kind);
 for(const auto&row:w.state.life_sources.rows()){const auto*v=take(read_visit_appointment_life_source(time.invitation_life,row.first));const auto*o=w.state.offered.find(v->invitation_fact_id);if(!o||o->contact.actor_id!=o->terms.inviter_actor_id||!written_policy(w,c,*o))continue;const auto d=take(read_verified_death4(time.common,time.deaths,time.life_causes,v->death_ref,x.actual));if(time.life_policy.selected_version!=4||d.recorded_at.kind!=ActorFoundationAt::Kind::exact_moment)fail("DirectorVisitAppointmentDeath4Required",v->death_ref.id);emit(x,visit,o->terms.inviter_actor_id,{v->death_ref.domain,v->death_ref.kind,v->death_ref.id},4,d.recorded_at.exact_moment);}
 return Done{};
});}
Status validate_director_appointment_wake2(const DirectorAppointmentWakeRead2&x,const SubjectWake2&w){
 if(w.kind!=SubjectWakeKind2::commitment||w.subject.kind!=SubjectKind::actor||w.source_revision!=0)return common_failure("InvalidDirectorAppointmentWake",w.actual_source.id);
 bool found=false;auto status=visit_director_appointment_wakes2(x,[&](const SubjectWake2&actual,const WorldMoment&){if(actual.subject.id==w.subject.id&&actual.source_schema==w.source_schema&&same(actual.actual_source,w.actual_source))found=true;return common_success();});if(!status.ok)return status;return found?common_success():common_failure("DirectorAppointmentWakeSourceMissing",w.actual_source.id);
}
Status validate_director_appointment_plan2(const DirectorAppointmentWakeRead2&x,const FinitePlan&p){
 auto valid=validate_director_appointment_wake_sources2(x);if(!valid.ok)return valid;if(visit_appointment_attendance_goal(p.goal_ref)){if(!x.original.written_visits||!x.original.written_catalog)return common_failure("DirectorVisitAppointmentSourcesRequired",p.id,ErrorKind::unsupported_capability);return validate_visit_appointment_attendance_plan(x.original.common,x.original.written_visits->state,x.original.written_catalog,p);}
 return validate_personal_appointment_attendance_plan(x.original.common,x.original.state,&x.catalog,p);
}
bool director_appointment_wake_domain2(const SourceRef&r)noexcept{return r.domain==personal_appointment_module||r.domain==visit_appointment_module||r.domain==visit_appointment_meeting_module||(r.domain=="family_life"&&r.kind=="family_death");}
}
