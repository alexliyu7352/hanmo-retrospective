#include "interaction_study_life.hpp"
#include "interaction_model.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/domains/growth/growth_equal.hpp"
#include "resident/kernel/world_bundle.hpp"
#include "resident/kernel/native_domain_read_versions.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/domains/family/family_life_exact.hpp"
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char*code,const StableId&id={},ErrorKind k=ErrorKind::internal){throw Failure{common_failure(code,id,k).error};}
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
template<class T,class F>Outcome<T>guard(F f){try{return {true,f(),{}};}catch(Failure&e){return {false,{},std::move(e.error)};}}
bool same(const SourceRef&a,const SourceRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
bool same(const DeathRef&a,const DeathRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
void bound(const WorldBundle&w,const CharacterConditionSources&b){
 if(!w.catalog||!w.catalog->interaction||!w.catalog->growth||!w.catalog->actor_foundation||!w.actor_foundation||!w.read_stamps||!w.catalog->family_life||w.catalog->family_life->selected_version!=4||!b.life_sources||!b.deaths)fail("InteractionStudyOriginalSourcesRequired",{},ErrorKind::unsupported_capability);
 if(&b.common!=&w.common||&b.state!=&*w.actor_foundation||&b.catalog!=w.catalog->actor_foundation.get()||b.growth!=&w.growth||b.growth_catalog!=w.catalog->growth.get()||b.deaths!=&w.family_life||take(compare_world_moments(b.actual_now,take(w.actual_phase_moment()))))fail("InteractionStudyCandidateMismatch");
}
}
Outcome<std::unique_ptr<NativeInteractionStudyLifeRead>> NativeInteractionStudyLifeRead::create(const WorldBundle&w,const CharacterConditionWorldRead&body){return guard<std::unique_ptr<NativeInteractionStudyLifeRead>>([&]{bound(w,take(body.sources()));return std::unique_ptr<NativeInteractionStudyLifeRead>(new NativeInteractionStudyLifeRead(w,body));});}
bool NativeInteractionStudyLifeRead::bound_to(const CommonStore&c,const InteractionState&s)const noexcept{return &c==&world_.common&&&s==&world_.interaction;}
Outcome<OriginalInteractionStudyTerminal> NativeInteractionStudyLifeRead::read(const StableId&session,const StableId&action,const DeathRef&death,const StudyPendingReleaseProof*pending)const{return guard<OriginalInteractionStudyTerminal>([&]{
 auto body=take(body_.sources());bound(world_,body);
 const auto*row=world_.growth.facts.find(action);const auto*fact=row?std::get_if<StudyFact>(row):nullptr;
 if(!fact||!fact->study_service_terminal||!fact->terms.study_teaching_agreement)fail("InteractionOriginalTeachStudyTerminalRequired",action,ErrorKind::unsupported_capability);
 const auto&candidate=*fact->study_service_terminal;const auto*accepted=std::get_if<TeachStudyRequestStart>(&candidate.accepted_execution_ref);const auto*cause=std::get_if<StudyVerifiedParticipantDeath>(&candidate.stop_cause);
 if(!accepted||accepted->action_id!=action||accepted->session_id!=session||!cause||!same(cause->death_ref,death))fail("InteractionStudySessionTerminalMismatch",action);
 const auto*opened=take(read_original_interaction_session_opening(world_.common,world_.interaction,*world_.catalog->interaction,session));const auto&opening=*opened->session_opening;const auto&agreement=*fact->terms.study_teaching_agreement;
 // Original TeachStudy is initiated by the teacher in the student's request
 // dialogue. Ordinary StudySkill has no original session binding.
 if(agreement.request_id!=accepted->request_id||agreement.session_id!=session||agreement.teacher_actor_id!=opening.actor_id||agreement.student_actor_id!=opening.context.provider_actor_id||agreement.dialogue_id!=opening.dialogue_id||!growth::context_equal(agreement.request_context,opening.context))fail("InteractionStudyOriginalRequestMismatch",action);
 NativeDomainReadVersionSource versions(world_);auto reads=world_.make_current_read_stamp_reader(versions,{1,1,1024*1024});if(!reads)fail("InteractionStudyReadStampRequired",action,ErrorKind::unsupported_capability);
 FamilyVerifiedDeathRead deaths(world_.common,*body.deaths,*body.life_sources,*world_.read_stamps,versions);
 auto memory=CheckpointMemoryCharge(0);
 std::vector<ResourceKey>actors;actors.reserve(1);actors.push_back({ResourceKey::Kind::actor,checkpoint_fresh_string_copy(cause->actor_id)});std::vector<StableId>sources;sources.reserve(1);sources.push_back(checkpoint_fresh_string_copy(death.id));
 auto stamp=take(reads->capture(actors,{},sources));auto access=world_.services.growth_access();
 const StudyTerminal ref{action};const StudyServiceTerminalV2*terminal=nullptr;
 if(pending)terminal=take(read_study_service_termination_in_capture(body,world_.growth,access,ref,*stamp,deaths,*pending));
 else terminal=take(read_study_service_termination(world_.common,world_.growth,*world_.catalog->growth,body.catalog,access,ref,body.actual_now,*stamp,deaths));
 const auto*time=take(read_study_original_terminal_time(world_.growth,ref,body.actual_now));
 const auto*opening_header=world_.common.state().facts.find(opened->id);const auto*accepted_header=world_.common.state().facts.find(action);const auto*death_header=world_.common.state().facts.find(death.id);
 if(!opening_header||!accepted_header||!death_header)fail("InteractionStudyOriginalAcceptanceOrderRequired",action);
 const auto opening_at=take(read_actual_common_fact_position(world_.common,*opening_header));const auto accepted_at=take(read_actual_common_fact_position(world_.common,*accepted_header));const auto death_at=take(read_actual_common_fact_position(world_.common,*death_header));
 if(opening_at>=accepted_at||accepted_at>=death_at||!fact->study_service_binding||take(compare_world_moments(opened->occurred_at,fact->study_service_binding->accepted_at))>0)fail("InteractionStudyOriginalAcceptanceOrderRequired",action);
 if(terminal!=&candidate||time->terminal_kind!=StudyTerminalTime::Kind::actual_service_termination||take(compare_world_moments(time->at,terminal->terminated_at)))fail("InteractionStudyActualTerminalTimeMismatch",action);
 return OriginalInteractionStudyTerminal{terminal,time};
});}
Outcome<OriginalInteractionStudyTerminal> NativeInteractionStudyLifeRead::read_released_waiting_action(const StableId&session,const StableId&action,const DeathRef&death,const StudyPendingReleaseProof&proof)const{return read(session,action,death,&proof);}
Outcome<OriginalInteractionStudyTerminal> NativeInteractionStudyLifeRead::read_terminal(const StableId&session,const SourceRef&source,const DeathRef&death,const StudyPendingReleaseProofs*pending)const{return guard<OriginalInteractionStudyTerminal>([&]{
 const StableId*action=nullptr;
 for(const auto&entry:world_.growth.facts.rows()){const auto*fact=std::get_if<StudyFact>(&entry.second);if(!fact||!fact->study_service_terminal)continue;const auto*a=std::get_if<TeachStudyRequestStart>(&fact->study_service_terminal->accepted_execution_ref);if(!a||a->session_id!=session)continue;const auto*time=world_.growth.lesson_capabilities.study_time_bindings.find(entry.first);if(!time||!time->terminal_time||!same(time->terminal_time->original_terminal_ref,source))continue;if(action)fail("AmbiguousInteractionStudyTerminal",session);action=&entry.first;}
 if(!action)fail("InteractionOriginalActionTerminalRequired",source.id,ErrorKind::unsupported_capability);
 const StudyPendingReleaseProof*proof=nullptr;
 if(pending)for(const auto&candidate:*pending)if(candidate.action_id()&&*candidate.action_id()==*action){if(proof)fail("DuplicateInteractionStudyPendingRelease",*action);proof=&candidate;}
 const auto&terminal=*std::get<StudyFact>(*world_.growth.facts.find(*action)).study_service_terminal;
 if(!terminal.reservation_release_results.empty()&&!terminal.reservation_release_results.front().original_release_commit_ref.result_id.empty())proof=nullptr;
 return take(read(session,*action,death,proof));
});}
Outcome<OriginalInteractionStudyTerminal> NativeInteractionStudyLifeRead::read_original_terminal(const StableId&s,const SourceRef&r,const DeathRef&d)const{return read_terminal(s,r,d,nullptr);}
Outcome<OriginalInteractionStudyTerminal> NativeInteractionStudyLifeRead::read_original_terminal_in_capture(const StableId&s,const SourceRef&r,const DeathRef&d,const StudyPendingReleaseProofs&p)const{return read_terminal(s,r,d,&p);}
Outcome<bool> NativeInteractionStudyLifeRead::has_original_session_death_terminal(const StableId&session,const DeathRef&death)const{return guard<bool>([&]{
 auto body=take(body_.sources());bound(world_,body);bool found=false;
 for(const auto&entry:world_.growth.facts.rows()){const auto*fact=std::get_if<StudyFact>(&entry.second);if(!fact||!fact->study_service_terminal)continue;const auto&t=*fact->study_service_terminal;const auto*a=std::get_if<TeachStudyRequestStart>(&t.accepted_execution_ref);const auto*c=std::get_if<StudyVerifiedParticipantDeath>(&t.stop_cause);if(!a||a->session_id!=session||!c||!same(c->death_ref,death))continue;if(found)fail("AmbiguousInteractionStudyTerminal",session);found=true;}
 return found;
});}
}
