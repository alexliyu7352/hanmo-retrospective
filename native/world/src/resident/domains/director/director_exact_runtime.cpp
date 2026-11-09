#include "director_exact_runtime.hpp"
#include "director_command_acceptance.hpp"
#include "director_native_wakes2.hpp"
#include "resident/kernel/world_bundle.hpp"
#include "resident/kernel/exact_complete_world.hpp"
namespace hanmo::resident {namespace {
struct Boundary {WorldBundle&w;explicit Boundary(WorldBundle&v):w(v){w.note_write_boundary();}~Boundary(){w.note_write_boundary();}};
Status selected(const WorldBundle&w,DirectorExactWorldFactory f){if(!f.read||!f.write||!w.catalog->director||!w.catalog->director->subjects2||!w.director.subject2)return common_failure("CompleteDirectorOriginalFactoryRequired",{},ErrorKind::unsupported_capability);return common_success();}
Status valid_propagation(const WorldBundle&w,const PropagationReadContext*p,bool required){if(!required)return p?common_failure("UnselectedPropagationBinding",{},ErrorKind::internal):common_success();if(!p||&p->common!=&w.common||&p->social!=&w.social||&p->director!=&w.director||&p->director_catalog!=w.catalog->director.get())return common_failure("PropagationWorldBindingRequired",{},ErrorKind::internal);return validate_propagation_composition(p->policy,p->limits,p->channels);}
}
Outcome<SocialCalendarDispatch> read_complete_social_calendar_dispatch(const WorldBundle&w,const StableId&actor,DirectorExactWorldFactory f){
 auto valid=selected(w,f);if(!valid.ok)return {false,{},valid.error};auto now=w.actual_phase_moment();if(!now.ok)return {false,{},now.error};auto lease=f.read(w,now.value);if(!lease.ok)return {false,{},lease.error};if(!lease.value)return {false,{},common_failure("DirectorReadLeaseRequired",{},ErrorKind::unsupported_capability).error};const auto binding=lease.value->reads();valid=valid_propagation(w,binding.propagation,binding.propagation_selected);if(!valid.ok)return {false,{},valid.error};SocialCalendarDispatch result;
 if(binding.propagation_selected){const auto*registration=w.director.subject2->registrations.find(director_subject_key2({SubjectKind::actor,actor}));if(registration&&registration->lifecycle==DirectorLifecycle::active&&!registration->obligations_only){auto admission=binding.original_subjects.current_admission(registration->subject);if(!admission.ok)return {false,{},admission.error};if(admission.value.source_active&&admission.value.route==DirectorDecisionRoute2::actor)result.share_news=false;}}
 return {true,result,{}};
}
Outcome<SocialCalendarDispatch> read_native_social_calendar_dispatch(const WorldBundle&w,const StableId&actor){
 auto owners=w.complete_exact_time_bindings();if(!owners.ok)return {false,{},owners.error};
 if(!owners.value)return {false,{},common_failure("CompleteExactWorldBindingOwnerRequired",{},ErrorKind::unsupported_capability).error};
 const auto factory=owners.value->reads().director;
 // Only immutable function pointers cross the lease boundary. The actual
 // dispatch acquires its own original read lease for this same candidate.
 owners.value.reset();
 return read_complete_social_calendar_dispatch(w,actor,factory);
}
Status validate_complete_director_read_binding(const WorldBundle&w,const WorldMoment&at,const DirectorExactReadBinding&b){
 if(!w.catalog||!w.catalog->director||!w.catalog->director->subjects2||!w.director.subject2)return common_failure("CompleteDirectorOriginalFactoryRequired",{},ErrorKind::unsupported_capability);
 auto now=w.actual_phase_moment();if(!now.ok)return {false,{},now.error};auto same=compare_world_moments(now.value,at);if(!same.ok)return {false,{},same.error};if(same.value)return common_failure("DirectorReadActualMomentMismatch",{},ErrorKind::internal);
 auto valid=valid_propagation(w,b.propagation,b.propagation_selected);if(!valid.ok)return valid;valid=validate_director_subjects2(w.common,w.director,*w.catalog->director,b.original_subjects,at,&w.social);if(!valid.ok)return valid;valid=validate_director_accepted_commands(w.common,w.director,*w.catalog->director,w.catalog->identity);if(!valid.ok)return valid;if(b.propagation)return validate_social_propagation(*b.propagation);if(w.social.dynamic&&w.social.dynamic->time2&&(!w.social.dynamic->time2->propagation_decisions.empty()||!w.social.dynamic->time2->propagation_cooldowns.empty()))return common_failure("UnselectedPropagationHasHistory");return common_success();
}
Status validate_complete_director_sources(const WorldBundle&w,DirectorExactWorldFactory f){auto valid=selected(w,f);if(!valid.ok)return valid;auto now=w.actual_phase_moment();if(!now.ok)return {false,{},now.error};auto lease=f.read(w,now.value);if(!lease.ok)return {false,{},lease.error};if(!lease.value)return common_failure("DirectorReadLeaseRequired",{},ErrorKind::unsupported_capability);return validate_complete_director_read_binding(w,now.value,lease.value->reads());}
Status initialize_complete_director_in_candidate(WorldBundle&w,DirectorExactWorldFactory f){
 auto valid=selected(w,f);if(!valid.ok)return valid;auto&s=w.director;auto&subjects=*s.subject2;
 if(subjects.wake_cache_ready||!subjects.wake_sources.empty()||!subjects.registrations.empty()||!subjects.subject_plan_bindings.empty()||!subjects.subject_continuations.empty()||!subjects.adoption_results.empty()||!s.schedules.empty()||!s.plans.empty()||!s.archived_plans.empty()||!s.registrations.empty()||s.fairness_cursor||s.last_boundary_tick!=-1)return common_failure("DirectorPristineInitialStateRequired",{},ErrorKind::internal);
 // Set the empty live cache before registration so reconcile cannot mistake
 // original newly-observable seed sources for already-consumed restore history.
 {Boundary boundary(w);subjects.wake_cache_ready=true;}
 valid=collect_complete_director_wakes_in_candidate(w,f);if(!valid.ok)return valid;
 return validate_complete_director_sources(w,f);
}
Status collect_complete_director_wakes_in_candidate(WorldBundle&w,DirectorExactWorldFactory f){
 auto valid=selected(w,f);if(!valid.ok)return valid;
 if(!w.director.subject2->wake_cache_ready)return common_failure("DirectorWakeCacheRestoreRequired",{},ErrorKind::internal);
 auto actual=w.actual_phase_moment();if(!actual.ok)return {false,{},actual.error};
 {Boundary boundary(w);auto lease=f.write(w,actual.value);if(!lease.ok)return {false,{},lease.error};if(!lease.value)return common_failure("DirectorWriteLeaseRequired",{},ErrorKind::unsupported_capability);auto b=lease.value->writes();valid=reconcile_director_subjects2(w.common,w.director,*w.catalog->director,b.original_subjects,actual.value);if(!valid.ok)return valid;}
 {Boundary boundary(w);auto now=w.actual_phase_moment();if(!now.ok)return {false,{},now.error};auto same=compare_world_moments(actual.value,now.value);if(!same.ok)return {false,{},same.error};if(same.value)return common_failure("DirectorWakeCollectionClockChanged",{},ErrorKind::internal);auto lease=f.write(w,now.value);if(!lease.ok)return {false,{},lease.error};if(!lease.value)return common_failure("DirectorWriteLeaseRequired",{},ErrorKind::unsupported_capability);auto b=lease.value->writes();return collect_director_owner_wakes2(w.director,b.original_subjects,now.value);}
}
Status restore_complete_director_wake_cache_in_candidate(WorldBundle&w,DirectorExactWorldFactory f){
 auto valid=selected(w,f);if(!valid.ok)return valid;
 if(w.director.subject2->wake_cache_ready)return common_failure("DirectorWakeCacheAlreadyReady",{},ErrorKind::internal);
 Boundary boundary(w);auto now=w.actual_phase_moment();if(!now.ok)return {false,{},now.error};auto lease=f.read(w,now.value);if(!lease.ok)return {false,{},lease.error};if(!lease.value)return common_failure("DirectorReadLeaseRequired",{},ErrorKind::unsupported_capability);auto b=lease.value->reads();return rebuild_director_wake_cache2(w.director,b.original_subjects,now.value);
}
Status review_complete_director_in_candidate(WorldBundle&w,const ExactPeriodContext&period,DirectorExactWorldFactory f){
 auto valid=selected(w,f);if(!valid.ok)return valid;auto now=w.actual_phase_moment();if(!now.ok)return {false,{},now.error};auto end=compare_world_moments(now.value,{period.through_tick,{}});if(!end.ok)return {false,{},end.error};if(end.value||period.period_id.empty())return common_failure("DirectorOriginalReviewBoundaryRequired");
 // 登记/实际wake与现有义务不消耗可选预算。每个写段后重新取得同候选lease。
 {Boundary boundary(w);auto lease=f.write(w,now.value);if(!lease.ok)return {false,{},lease.error};if(!lease.value)return common_failure("DirectorWriteLeaseRequired",{},ErrorKind::unsupported_capability);auto b=lease.value->writes();valid=valid_propagation(w,b.propagation,b.propagation_selected);if(!valid.ok)return valid;valid=reconcile_director_subjects2(w.common,w.director,*w.catalog->director,b.original_subjects,now.value);if(!valid.ok)return valid;}
 {Boundary boundary(w);auto lease=f.write(w,now.value);if(!lease.ok)return {false,{},lease.error};if(!lease.value)return common_failure("DirectorWriteLeaseRequired",{},ErrorKind::unsupported_capability);auto b=lease.value->writes();valid=collect_director_owner_wakes2(w.director,b.original_subjects,now.value);if(!valid.ok)return valid;}
 {Boundary boundary(w);auto lease=f.write(w,now.value);if(!lease.ok)return {false,{},lease.error};if(!lease.value)return common_failure("DirectorWriteLeaseRequired",{},ErrorKind::unsupported_capability);auto b=lease.value->writes();valid=rebuild_director_wake_cache2(w.director,b.original_subjects,now.value);if(!valid.ok)return valid;}
 auto due=director_due_subjects2(w.director,*w.catalog->director,now.value.half_day_tick);if(!due.ok)return {false,{},due.error};
 for(const auto&subject:due.value){Boundary boundary(w);auto actual=w.actual_phase_moment();if(!actual.ok)return {false,{},actual.error};auto same=compare_world_moments(actual.value,now.value);if(!same.ok)return {false,{},same.error};if(same.value)return common_failure("DirectorReviewClockChanged",{},ErrorKind::internal);auto lease=f.write(w,actual.value);if(!lease.ok)return {false,{},lease.error};if(!lease.value)return common_failure("DirectorWriteLeaseRequired",{},ErrorKind::unsupported_capability);auto b=lease.value->writes();valid=valid_propagation(w,b.propagation,b.propagation_selected);if(!valid.ok)return valid;
  auto admission=b.original_subjects.current_admission(subject);if(!admission.ok)return {false,{},admission.error};auto proposal=propose_director_subject2(w.director,*w.catalog->director,subject,actual.value.half_day_tick,b.original_subjects);if(!proposal.ok)return {false,{},proposal.error};
  // 原成立责任优先；纯可选传播共用同review键，不创建假业务计划。
  if(subject.kind==SubjectKind::actor&&b.propagation_selected&&!admission.value.has_frozen_responsibility){if(!b.propagation_writer)return common_failure("PropagationActualWriterRequired",{},ErrorKind::unsupported_capability);const auto*registration=w.director.subject2->registrations.find(director_subject_key2(subject));const auto*schedule=w.director.schedules.find(director_subject_key2(subject));if(!registration||!schedule)return common_failure("DirectorPropagationRegistrationMissing",{},ErrorKind::internal);auto propagation=prepare_propagation_review(*b.propagation,subject,registration->epoch,schedule->decision_seq+1);if(!propagation.ok)return {false,{},propagation.error};if(propagation.value){auto adopted=adopt_propagation_review_in_candidate(w.social,w.director,*b.propagation,*b.propagation_writer,*propagation.value);if(!adopted.ok)return {false,{},adopted.error};continue;}}
  if(proposal.value){valid=adopt_director_subject2(w.common,w.director,*w.catalog->director,*proposal.value,b.original_subjects,b.original_executor,actual.value);if(!valid.ok)return valid;}
 }
 return validate_complete_director_sources(w,f);
}
}
