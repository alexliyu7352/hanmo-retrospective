#include "interaction_task_life.hpp"
#include "interaction_model.hpp"
#include "interaction_action_confirmation.hpp"
#include "resident/domains/tasks/task_exact_execution.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/world_bundle.hpp"
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char*code,const StableId&id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
void take(Status v){if(!v.ok)throw Failure{std::move(v.error)};}
template<class T,class F>Outcome<T>guard(F fn){try{return {true,fn(),{}};}catch(Failure&e){return {false,{},std::move(e.error)};}}
bool same(const DeathRef&a,const DeathRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
bool context(const InteractionContext&a,const InteractionContext&b){return a.place_id==b.place_id&&a.provider_actor_id==b.provider_actor_id&&a.role_id==b.role_id&&a.kind==b.kind&&a.encounter_id==b.encounter_id&&a.input_error==b.input_error;}
void bound(const WorldBundle&w,const CharacterConditionSources&body){
 if(!w.catalog||!w.catalog->interaction||!w.catalog->tasks||!w.tasks.exact_execution||
    w.tasks.exact_execution->version!=2||!w.world_timeline||!w.action_elapsed||!w.catalog->family_life||
    w.catalog->family_life->selected_version!=4||!body.life_sources||!body.deaths||!body.action_registrations||
    interaction_action_confirmation_schema_version(w.catalog->interaction->response_sources_version)!=1)
  fail("InteractionTaskOriginalSourcesRequired",{},ErrorKind::unsupported_capability);
 if(&body.common!=&w.common||body.deaths!=&w.family_life||body.elapsed!=&*w.action_elapsed||
    body.scale!=&w.world_timeline->scale_ref||body.interaction!=&w.interaction||
    body.interaction_catalog!=w.catalog->interaction.get()||
    take(compare_world_moments(body.actual_now,take(w.actual_phase_moment()))))
  fail("InteractionTaskCandidateMismatch");
}
const ActionTimeRegistration&registration(const CharacterConditionSources&body,const StableId&kind){
 const ActionTimeRegistration*found=nullptr;
 for(const auto&row:*body.action_registrations)if(row.owner_domain=="tasks"&&row.action_kind==kind){if(found)fail("DuplicateInteractionTaskRegistration",kind);found=&row;}
 if(!found)fail("InteractionTaskRegistrationRequired",kind,ErrorKind::unsupported_capability);return *found;
}
}
Outcome<std::unique_ptr<NativeInteractionTaskLifeRead>> NativeInteractionTaskLifeRead::create(const WorldBundle&w,const CharacterConditionWorldRead&body){return guard<std::unique_ptr<NativeInteractionTaskLifeRead>>([&]{bound(w,take(body.sources()));return std::unique_ptr<NativeInteractionTaskLifeRead>(new NativeInteractionTaskLifeRead(w,body));});}
bool NativeInteractionTaskLifeRead::bound_to(const CommonStore&c,const InteractionState&s)const noexcept{return &c==&world_.common&&&s==&world_.interaction;}
bool NativeInteractionTaskLifeRead::owns_original_action(const StableId&a)const noexcept{
 if(!world_.interaction.response_sources)return false;
 for(const auto&row:world_.interaction.response_sources->action_confirmation_sources.rows())if(row.second.action_id==a&&exact_task_action_kind(row.second.confirmed_command.kind))return true;
 return false;
}
bool NativeInteractionTaskLifeRead::owns_original_terminal(const SourceRef&s)const noexcept{return s.domain=="tasks"&&s.kind=="task_action_finished"&&world_.tasks.exact_execution&&world_.tasks.exact_execution->version==2&&world_.tasks.exact_execution->terminals.find(s.id);}
Outcome<OriginalInteractionTaskTerminal> NativeInteractionTaskLifeRead::read(const StableId&session,const StableId&action,const DeathRef&death)const{return guard<OriginalInteractionTaskTerminal>([&]{
 auto body=take(body_.sources());bound(world_,body);
 const InteractionActionConfirmationSource*confirmed=nullptr;
 if(!world_.interaction.response_sources)fail("InteractionOriginalConfirmationSourceMissing",action);
 for(const auto&row:world_.interaction.response_sources->action_confirmation_sources.rows())if(row.second.action_id==action){if(confirmed)fail("AmbiguousInteractionTaskConfirmation",action);confirmed=&row.second;}
 if(!confirmed||confirmed->session_id!=session||!exact_task_action_kind(confirmed->confirmed_command.kind))fail("InteractionOriginalTaskConfirmationRequired",action,ErrorKind::unsupported_capability);
 const auto original=take(read_original_interaction_action_confirmation(world_.common,world_.interaction,*world_.catalog->interaction,confirmed->committed_result_id,action));
 if(original.source!=confirmed||!original.opening||!original.opening->session_opening)fail("InteractionTaskConfirmationMismatch",action);
 const TaskExactStartSource*started=nullptr;
 for(const auto&row:world_.tasks.exact_execution->starts.rows())if(row.second.action_id==action){if(started)fail("AmbiguousInteractionTaskStart",action);started=&row.second;}
 const auto&opening=*original.opening->session_opening;
 // Original pending_dialogue_request unconditionally binds the live session
 // context. Authored optional context is not an authority to replace it.
 if(!started||started->action_kind!=confirmed->confirmed_command.kind||started->actor_id!=opening.actor_id||
    started->accepted_action.offer_id!=confirmed->confirmed_command.offer_id||!context(started->accepted_action.context,opening.context)||
    take(compare_world_moments(started->started_at,confirmed->confirmation_at)))fail("InteractionTaskOriginalStartMismatch",action);
 TaskExactLifeSources life{world_.common,world_.family_life,*body.life_sources,body.actual_now};
 const auto*terminal=take(read_original_task_death_terminal(world_.common,world_.tasks,*world_.catalog->tasks,world_.catalog->identity,action,death,life));
 if(world_.common.action(action)||world_.tasks.actions.find(action))fail("InteractionTaskOriginalReleaseRequired",action);
 TaskActionTimeSourceRead timing(world_.common,world_.tasks,*world_.catalog->tasks,world_.world_timeline->scale_ref,world_.catalog->identity,{},&life);
 const auto&reg=registration(body,started->action_kind);auto owner=take(timing.read_action_time_owner(reg,action));
 if(owner.phase!=ActionTimeOwnerPhase::terminal||!owner.terminal_result_ref||*owner.terminal_result_ref!=terminal->id)fail("InteractionTaskActualTerminalMismatch",action);
 take(read_action_time_projection(reg,owner,*world_.action_elapsed,body.actual_now,world_.world_timeline->scale_ref,timing));
 take(guard_task_exact_release(world_.common,world_.tasks,*world_.action_elapsed,action,&life));
 const auto died=take(read_verified_death4(world_.common,life.deaths,life.causes,death,life.actual_now));
 if(died.actor_id!=opening.actor_id&&died.actor_id!=opening.context.provider_actor_id)fail("InteractionTaskDeathParticipantMismatch",action);
 if(died.effective_at.kind!=ActorFoundationAt::Kind::exact_moment||take(compare_world_moments(terminal->finished_at,died.effective_at.exact_moment)))fail("InteractionTaskActualTerminalMomentMismatch",action);
 const auto*opened=world_.common.state().facts.find(original.opening->id);const auto*began=world_.common.state().facts.find(started->id);
 if(!opened||!began||take(read_actual_common_fact_position(world_.common,*opened))>=take(read_actual_common_fact_position(world_.common,*began)))fail("InteractionTaskOriginalOrderMismatch",action);
 // Task's sole terminal reader proves start < actual death < terminal.
 return OriginalInteractionTaskTerminal{started,terminal,confirmed};
});}
Outcome<OriginalInteractionTaskTerminal> NativeInteractionTaskLifeRead::read_released_waiting_action(const StableId&s,const StableId&a,const DeathRef&d)const{return read(s,a,d);}
Outcome<OriginalInteractionTaskTerminal> NativeInteractionTaskLifeRead::read_original_terminal(const StableId&s,const SourceRef&ref,const DeathRef&d)const{return guard<OriginalInteractionTaskTerminal>([&]{if(!owns_original_terminal(ref))fail("InteractionOriginalTaskTerminalRequired",ref.id);const auto*t=world_.tasks.exact_execution->terminals.find(ref.id);auto result=take(read(s,t->action_id,d));if(result.terminal!=t)fail("InteractionTaskTerminalSourceMismatch",ref.id);return result;});}
Outcome<bool> NativeInteractionTaskLifeRead::has_original_session_death_terminal(const StableId&session,const DeathRef&death)const{return guard<bool>([&]{
 auto body=take(body_.sources());bound(world_,body);bool found=false;
 if(!world_.interaction.response_sources)fail("InteractionOriginalConfirmationSourcesRequired",session);
 for(const auto&row:world_.interaction.response_sources->action_confirmation_sources.rows()){
  const auto&confirmation=row.second;if(confirmation.session_id!=session||!exact_task_action_kind(confirmation.confirmed_command.kind))continue;
  for(const auto&entry:world_.tasks.exact_execution->terminals.rows()){
   const auto&t=entry.second;if(t.action_id!=confirmation.action_id||!t.participant_death||!same(t.participant_death->death_ref,death))continue;
   if(found)fail("AmbiguousInteractionTaskTerminal",session);take(read(session,confirmation.action_id,death));found=true;
  }
 }
 return found;
});}
}
