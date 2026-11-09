#include "interaction_growth_life.hpp"
#include "interaction_model.hpp"
#include "interaction_action_confirmation.hpp"
#include "resident/domains/growth/growth_action_time_read.hpp"
#include "resident/domains/growth/growth_fixed_input_changes.hpp"
#include "resident/domains/growth/growth_terminal_publication.hpp"
#include "resident/domains/family/family_life_exact.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/world_bundle.hpp"
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char*code,const StableId&id={},ErrorKind kind=ErrorKind::internal){
 throw Failure{common_failure(code,id,kind).error};
}
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
void take(Status v){if(!v.ok)throw Failure{std::move(v.error)};}
template<class T,class F>Outcome<T>guard(F fn){try{return {true,fn(),{}};}catch(Failure&e){return {false,{},std::move(e.error)};}}
bool same(const DeathRef&a,const DeathRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
bool same(const SourceRef&a,const SourceRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
bool supported(const StableId&kind){return kind=="service"||kind=="learn_skill"||kind=="teach_skill";}
void bound(const WorldBundle&w,const CharacterConditionSources&body){
 if(!w.catalog||!w.catalog->interaction||!w.catalog->growth||!w.catalog->actor_foundation||
    !w.catalog->common||!w.actor_foundation||!w.action_elapsed||!w.world_timeline||!w.catalog->family_life||
    w.catalog->family_life->selected_version!=4||!body.life_sources||!body.deaths||
    !body.shared||!body.scale||!body.action_registrations||
    interaction_action_confirmation_schema_version(w.catalog->interaction->response_sources_version)!=1)
  fail("InteractionGrowthOriginalSourcesRequired",{},ErrorKind::unsupported_capability);
 if(&body.common!=&w.common||&body.state!=&*w.actor_foundation||
    &body.catalog!=w.catalog->actor_foundation.get()||body.growth!=&w.growth||
    body.growth_catalog!=w.catalog->growth.get()||body.elapsed!=&*w.action_elapsed||
    body.shared!=w.catalog->common.get()||body.scale!=&w.world_timeline->scale_ref||
    body.deaths!=&w.family_life||body.interaction!=&w.interaction||
    body.interaction_catalog!=w.catalog->interaction.get()||
    take(compare_world_moments(body.actual_now,take(w.actual_phase_moment()))))
  fail("InteractionGrowthCandidateMismatch");
}
const LifeDeathInputChange* stopped_by_death(const LifeFixedServiceBoundary&b){
 if(b.kind!=LifeFixedBoundaryKind::stopped)return nullptr;
 const auto*t=std::get_if<LifeOriginalTerminalApplication>(&b.cause);
 if(!t||t->disposition!=LifeOriginalTerminalApplication::Disposition::stopped)return nullptr;
 const auto*stop=std::get_if<LifeActualStopTrigger>(&t->trigger);
 const auto*input=stop?std::get_if<LifeFixedInputChange>(&stop->source):nullptr;
 return input?std::get_if<LifeDeathInputChange>(input):nullptr;
}
const ActionTimeRegistration& registration(const CharacterConditionSources&body,const StableId&kind){
 const ActionTimeRegistration*found=nullptr;
 for(const auto&row:*body.action_registrations)if(row.owner_domain=="growth"&&row.action_kind==kind){
  if(found)fail("DuplicateInteractionGrowthRegistration",kind);found=&row;
 }
 if(!found)fail("InteractionGrowthRegistrationRequired",kind,ErrorKind::unsupported_capability);
 return *found;
}
}
Outcome<std::unique_ptr<NativeInteractionGrowthLifeRead>> NativeInteractionGrowthLifeRead::create(
 const WorldBundle&w,const CharacterConditionWorldRead&body){
 return guard<std::unique_ptr<NativeInteractionGrowthLifeRead>>([&]{
  bound(w,take(body.sources()));
  return std::unique_ptr<NativeInteractionGrowthLifeRead>(new NativeInteractionGrowthLifeRead(w,body));
 });
}
bool NativeInteractionGrowthLifeRead::bound_to(const CommonStore&c,const InteractionState&s)const noexcept{
 return &c==&world_.common&&&s==&world_.interaction;
}
bool NativeInteractionGrowthLifeRead::owns_original_action(const StableId&action)const noexcept{
 if(!world_.interaction.response_sources)return false;
 for(const auto&entry:world_.interaction.response_sources->action_confirmation_sources.rows())
  if(entry.second.action_id==action&&supported(entry.second.confirmed_command.kind))return true;
 return false;
}
bool NativeInteractionGrowthLifeRead::owns_original_terminal(const SourceRef&source)const noexcept{
 if(source.domain!="growth"||source.kind!="growth_fixed_service_boundary")return false;
 const auto*b=world_.growth.fixed_service_boundaries.find(source.id);
 return b&&supported(b->action_kind);
}
Outcome<OriginalInteractionGrowthTerminal> NativeInteractionGrowthLifeRead::read(
 const StableId&session,const StableId&action,const DeathRef&death)const{
 return guard<OriginalInteractionGrowthTerminal>([&]{
  auto body=take(body_.sources());bound(world_,body);
  const InteractionActionConfirmationSource*confirmed=nullptr;
  if(!world_.interaction.response_sources)fail("InteractionOriginalConfirmationSourceMissing",action);
  for(const auto&entry:world_.interaction.response_sources->action_confirmation_sources.rows())
   if(entry.second.action_id==action){
    if(confirmed)fail("AmbiguousInteractionGrowthConfirmation",action);confirmed=&entry.second;
   }
  if(!confirmed||confirmed->session_id!=session||!supported(confirmed->confirmed_command.kind))
   fail("InteractionOriginalGrowthConfirmationRequired",action,ErrorKind::unsupported_capability);
  const auto original=take(read_original_interaction_action_confirmation(world_.common,
   world_.interaction,*world_.catalog->interaction,confirmed->committed_result_id,action));
  if(original.source!=confirmed||!original.opening||!original.opening->session_opening)
   fail("InteractionGrowthConfirmationMismatch",action);
  const GrowthTerminalPublication*publication=nullptr;
  for(const auto&entry:world_.growth.terminal_publications.rows())if(entry.second.action_id==action){
   if(publication)fail("AmbiguousInteractionGrowthTerminal",action);publication=&entry.second;
  }
  if(!publication||publication->terminal_source_schema!=1||
     !owns_original_terminal(publication->terminal_source_ref))
   fail("InteractionActualGrowthTerminalRequired",action,ErrorKind::unsupported_capability);
  const auto*b=world_.growth.fixed_service_boundaries.find(publication->terminal_source_ref.id);
  const auto*cause=b?stopped_by_death(*b):nullptr;
  if(!b||b->action_ref!=action||b->action_kind!=confirmed->confirmed_command.kind||
     !cause||!same(cause->death_ref,death))
   fail("InteractionGrowthActualDeathTerminalMismatch",action);
  GrowthActionTimeRead timing(world_.common,world_.growth,*body.growth_catalog,*body.shared,
   *body.elapsed,*body.scale,body.catalog.identity,body.actual_now,{},
   &world_.interaction,world_.catalog->interaction.get());
  const auto actual=take(read_growth_terminal_publication(world_.common,world_.growth,timing,
   registration(body,b->action_kind),publication->publication_id));
  if(actual.publication!=publication||actual.actor_id!=original.opening->session_opening->actor_id||
     actual.action_kind!=b->action_kind||world_.common.action(action)||world_.growth.actions.find(action))
   fail("InteractionGrowthOriginalReleaseRequired",action);
  // ActionTime validates its original structure and actual adopted release.
  // Its bounded input reader supplies the actual Family4/Condition cause proof.
  take(validate_growth_fixed_input_change_source(body,world_.growth,b->boundary_id));
  const auto died=take(read_verified_death4(world_.common,*body.deaths,*body.life_sources,death,body.actual_now));
  if(died.effective_at.kind!=ActorFoundationAt::Kind::exact_moment||
     take(compare_world_moments(b->occurred_at,died.effective_at.exact_moment))||
     take(compare_world_moments(publication->occurred_at,b->occurred_at))||
     take(compare_world_moments(confirmed->confirmation_at,b->occurred_at))>0)
   fail("InteractionGrowthActualTerminalMomentMismatch",action);
  const auto&opening=*original.opening->session_opening;
  if(died.actor_id!=opening.actor_id&&died.actor_id!=opening.context.provider_actor_id)
   fail("InteractionGrowthDeathParticipantMismatch",action);
  const auto*opened=world_.common.state().facts.find(original.opening->id);
  const auto*began=world_.common.state().facts.find(action);
  const auto*ended=world_.common.state().facts.find(publication->publication_id);
  const auto*death_header=world_.common.state().facts.find(death.id);
  if(!opened||!began||!ended||!death_header)fail("InteractionGrowthOriginalOrderRequired",action);
  const auto opening_at=take(read_actual_common_fact_position(world_.common,*opened));
  const auto start_at=take(read_actual_common_fact_position(world_.common,*began));
  const auto death_at=take(read_actual_common_fact_position(world_.common,*death_header));
  const auto terminal_at=take(read_actual_common_fact_position(world_.common,*ended));
  if(opening_at>=start_at||start_at>=death_at||death_at>=terminal_at)
   fail("InteractionGrowthOriginalOrderMismatch",action);
  return OriginalInteractionGrowthTerminal{publication,b,confirmed};
 });
}
Outcome<OriginalInteractionGrowthTerminal> NativeInteractionGrowthLifeRead::read_released_waiting_action(
 const StableId&s,const StableId&a,const DeathRef&d)const{return read(s,a,d);}
Outcome<OriginalInteractionGrowthTerminal> NativeInteractionGrowthLifeRead::read_original_terminal(
 const StableId&s,const SourceRef&source,const DeathRef&d)const{
 return guard<OriginalInteractionGrowthTerminal>([&]{
  if(!owns_original_terminal(source))fail("InteractionOriginalGrowthTerminalRequired",source.id);
  const auto*b=world_.growth.fixed_service_boundaries.find(source.id);
  auto actual=take(read(s,b->action_ref,d));
  if(actual.boundary!=b||!same(actual.publication->terminal_source_ref,source))
   fail("InteractionGrowthTerminalSourceMismatch",source.id);
  return actual;
 });
}
Outcome<bool> NativeInteractionGrowthLifeRead::has_original_session_death_terminal(
 const StableId&session,const DeathRef&death)const{
 return guard<bool>([&]{
  auto body=take(body_.sources());bound(world_,body);bool found=false;
  if(!world_.interaction.response_sources)fail("InteractionOriginalConfirmationSourcesRequired",session);
  for(const auto&entry:world_.interaction.response_sources->action_confirmation_sources.rows()){
   const auto&confirmation=entry.second;
   if(confirmation.session_id!=session||!supported(confirmation.confirmed_command.kind))continue;
   for(const auto&row:world_.growth.fixed_service_boundaries.rows()){
    const auto&b=row.second;if(b.action_ref!=confirmation.action_id)continue;
    const auto*cause=stopped_by_death(b);if(!cause||!same(cause->death_ref,death))continue;
    if(found)fail("AmbiguousInteractionGrowthTerminal",session);
    take(read(session,confirmation.action_id,death));found=true;
   }
  }
  return found;
 });
}
}
