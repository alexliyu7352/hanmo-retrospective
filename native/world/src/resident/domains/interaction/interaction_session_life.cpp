#include "interaction_session_life.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/domains/growth/growth_terminal_publication.hpp"
#include "resident/domains/tasks/task_exact_execution.hpp"
#include "resident/kernel/common_command_sources.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <new>
namespace hanmo::resident {
struct InteractionSessionDeathPending::Entry final:public CheckpointAllocatedObject {
 CheckpointMemoryCharge fields_memory;
 StableId life_result_id;DeathRef death_ref;InteractionResponseSource response;
 Integer original_revision=0;bool consumed=false;
 Entry(const CommonStore&c,const StableId&id,const DeathRef&death,const InteractionResponseSource&selected)
  :fields_memory([&]{return CheckpointMemoryCharge(0);}()),
   life_result_id(checkpoint_fresh_string_copy(id)),
   death_ref{checkpoint_fresh_string_copy(death.domain),checkpoint_fresh_string_copy(death.kind),checkpoint_fresh_string_copy(death.id)},
   response(selected),original_revision(c.revision()){}
};
namespace {
struct Failure{RuleError error;};
[[noreturn]]void reject(const char*code,const StableId&id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
void take(Status v){if(!v.ok)throw Failure{std::move(v.error)};}
template<class T,class F>Outcome<T>guard(F f){try{return {true,f(),{}};}catch(Failure&e){return {false,{},std::move(e.error)};}}
bool same(const SourceRef&a,const SourceRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
bool same(const DeathRef&a,const DeathRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
SourceRef death_source(const DeathRef&d){return {d.domain,d.kind,d.id};}
SourceRef occupancy_source(const ActorSocialOccupancyTransition&t){return {"interaction","actor_social_occupancy_changed",t.id};}
std::size_t position(const CommonStore&c,const SourceRef&r){return take(read_actual_common_fact_position(c,r));}
Integer serial(const CommonStore&c,const StableId&id,std::string_view prefix){std::string_view v=id;if(v.size()<=prefix.size()||v.substr(0,prefix.size())!=prefix)reject("InteractionLifeOriginalAllocationRequired",id);v.remove_prefix(prefix.size());Integer n=0;auto r=std::from_chars(v.data(),v.data()+v.size(),n);if(v.empty()||v.front()=='0'||r.ec!=std::errc{}||r.ptr!=v.data()+v.size()||n<1||n>=c.state().next_instance_id)reject("InteractionLifeOriginalAllocationRequired",id);return n;}
void selected(const InteractionSessionLifeContext&x){if(!interaction_response_version(x.catalog.response_sources_version)||!x.state.response_sources||x.state.response_sources->physical_version!=x.catalog.response_sources_version||!x.state.social_occupancy||x.state.social_occupancy->version!=3||!x.life_policy.enabled||x.life_policy.selected_version!=4)reject("InteractionSessionLife4SourcesRequired",{},ErrorKind::unsupported_capability);if(x.study_terminals&&!x.study_terminals->bound_to(x.common,x.state))reject("InteractionStudyCandidateMismatch");if(interaction_action_confirmation_schema_version(x.catalog.response_sources_version)==1&&!x.growth_terminals)reject("InteractionGrowthTerminalOwnerRequired",{},ErrorKind::unsupported_capability);if(x.growth_terminals&&!x.growth_terminals->bound_to(x.common,x.state))reject("InteractionGrowthCandidateMismatch");if(x.task_terminals&&!x.task_terminals->bound_to(x.common,x.state))reject("InteractionTaskCandidateMismatch");if(!x.occupancy.bound_to(x.common,x.state))reject("InteractionSessionLifeCandidateMismatch");}
VerifiedDeathView death(const InteractionSessionLifeContext&x,const DeathRef&r){auto d=take(read_verified_death4(x.common,x.deaths,x.causes,r,take(x.clock.read_current_moment())));if(d.effective_at.kind!=ActorFoundationAt::Kind::exact_moment||d.recorded_at.kind!=ActorFoundationAt::Kind::exact_moment)reject("InteractionSessionExactDeathRequired",r.id);position(x.common,death_source(r));return d;}
const ActorSocialOccupancyTransition*closing(const InteractionState&s,const InteractionSessionOpening&o){const ActorSocialOccupancyTransition*out=nullptr;for(const auto&entry:s.social_occupancy->transitions.rows()){const auto&t=entry.second;if(t.actor_id!=o.actor_id||t.before.interaction_id!=std::optional<StableId>{o.session_id}||t.after.interaction_id==t.before.interaction_id)continue;if(out)reject("DuplicateInteractionSessionClose",o.session_id);out=&t;}return out;}
const ActorSocialOccupancyTransition*closing(const InteractionSessionLifeContext&x,const InteractionSessionOpening&o){return closing(x.state,o);}
const StableId&actor(const InteractionSessionOpening&o,InteractionSessionLifeRole role){if(role==InteractionSessionLifeRole::speaker)return o.context.provider_actor_id;if(role==InteractionSessionLifeRole::listener)return o.actor_id;reject("InteractionSessionLifeRoleMismatch",o.session_id);}
const InteractionSessionLifeSource*once(const InteractionSessionLifeContext&x,const DeathRef&d,const StableId&id,InteractionSessionLifeRole role){const InteractionSessionLifeSource*out=nullptr;for(const auto&entry:x.state.response_sources->life_sources.rows()){const auto&r=entry.second;if(r.session_id!=id||r.role!=role||!same(r.death_ref,d))continue;if(out)reject("DuplicateInteractionSessionLifeConsumption",id);out=&r;}return out;}
bool affected(const InteractionSessionLifeContext&x,const ActorSocialOccupancyTransition&opening,const VerifiedDeathView&d,InteractionSessionLifeRole role){const auto&o=*opening.session_opening;if(actor(o,role)!=d.actor_id)return false;const auto born=position(x.common,occupancy_source(opening)),cut=position(x.common,death_source(d.death_ref));if(born>=cut)reject("InteractionSessionOpenedAfterActualDeath",o.session_id);const auto*end=closing(x,o);if(end&&position(x.common,occupancy_source(*end))<cut)return false;
 // A late notice does not rewrite an ended session or create service since the
 // effective death. Only this actual immutable original opening is compared.
 if(take(compare_world_moments(opening.occurred_at,d.effective_at.exact_moment))>0)reject("InteractionOpeningAfterEffectiveDeath",o.session_id);
 return true;
}
// Immutable original selection/life dependency cut. Family4 owns the full
// death cause and historical role read; this proves its exact original S3 join.
const InteractionSessionLifeSource*death_tail_life(const CommonStore&c,const InteractionState&s,const InteractionCatalog&cat,const InteractionResponseSource&response){
 if(!s.response_sources)reject("InteractionDeathTailOriginalRootsRequired",response.response_fact_id);
 const InteractionSessionLifeSource*found=nullptr;
 for(const auto&entry:s.response_sources->life_sources.rows())if(const auto*stop=std::get_if<S3SessionStopped>(&entry.second.effect.effect);stop&&stop->selected_response_tail_ref&&stop->selected_response_tail_ref->domain=="interaction"&&stop->selected_response_tail_ref->kind=="response_selected"&&stop->selected_response_tail_ref->id==response.response_fact_id){
  if(found||entry.first!=entry.second.effect.result_id)reject("DuplicateInteractionDeathTailStop",response.response_fact_id);found=&entry.second;
 }
 if(!found)return nullptr;
 if(!s.social_occupancy||s.social_occupancy->version!=3)reject("InteractionDeathTailOriginalRootsRequired",response.response_fact_id);
 const auto&life=*found;take(validate_s3_leaf_life_effect_shape(life.effect,0));
 const auto&stop=std::get<S3SessionStopped>(life.effect.effect);
 if(life.session_id!=response.session_id||stop.original_action_terminal_ref||life.effect.original_terms_ref||life.death_ref.domain!="family_life"||life.death_ref.kind!="family_death")reject("InteractionDeathTailLifeMismatch",response.response_fact_id);
 const auto*actual=take(read_original_interaction_response_selection(c,s,cat,*stop.selected_response_tail_ref,interaction_response_source_schema_version(cat.response_sources_version)));if(actual!=&response)reject("InteractionDeathTailOriginalResponseRequired",response.response_fact_id);
 const auto*opening=take(read_original_interaction_session_opening(c,s,cat,life.session_id));const auto&o=*opening->session_opening;actor(o,life.role);
 const auto*end=closing(s,o);const auto dead=death_source(life.death_ref);
 if(!end||end->cause!=ActorSocialOccupancyCause::session_participant_died||!end->actual_cause_source||!same(*end->actual_cause_source,dead)||end->after.interaction_id||position(c,occupancy_source(*opening))>=position(c,dead)||position(c,dead)>=position(c,occupancy_source(*end))||serial(c,life.death_ref.id,"family_death:")>=serial(c,life.effect.result_id,"interaction_session_life:")||serial(c,life.effect.result_id,"interaction_session_life:")>=serial(c,end->id,"social_occupancy:"))reject("InteractionDeathTailActualCloseRequired",life.effect.result_id);
 if(take(compare_world_moments(life.effect.effective_game_time.exact_moment,end->occurred_at))>0)reject("InteractionDeathTailLifeTimeMismatch",life.effect.result_id);
 for(const auto&current:s.interactions)if(current.id==life.session_id)reject("InteractionDeathTailSessionStillOpen",life.session_id);
 std::size_t commits=0;for(const auto&ref:life.effect.preserved_result_refs)if(same(ref,response.committed_result_ref))++commits;
 if(commits!=1)reject("InteractionDeathTailCommittedSelectionNotPreserved",life.effect.result_id);
 return found;
}
void unconsumed_death_boundary(const InteractionState&s,const InteractionResponseSource&response,const InteractionSessionLifeSource&life){
 const auto*stop=std::get_if<S3SessionStopped>(&life.effect.effect);
 const auto*b=s.response_sources->session_boundaries.find(response.session_id);const auto*tail=s.response_sources->selected_response_tails.find(response.session_id);
 if(response.tail_consumed_result_ref||!stop||!stop->selected_response_tail_ref||!b||!tail||b->generation!=response.session_generation||b->generation==std::numeric_limits<Integer>::max()||!b->current_response_fact_ref||!same(*b->current_response_fact_ref,*stop->selected_response_tail_ref)||tail->session_id!=response.session_id||tail->session_generation!=response.session_generation||!same(tail->response_fact_ref,*b->current_response_fact_ref)||!same_interaction_response_next(tail->next_node_or_terminal,response.next_node_or_terminal))reject("InteractionDeathTailOriginalBoundaryChanged",response.response_fact_id);
}
// Enumerate exactly the real already-sealed choices. No later response is
// accepted on a session whose participant has already died.
template<class F>void preserved(const InteractionSessionLifeContext&x,const ActorSocialOccupancyTransition&opening,const VerifiedDeathView&d,F consume){
 // S3 4.0 preserves actual committed results only. The immutable opening is a
 // separately verified and pinned dependency, not a selected-response result.
 const auto cut=serial(x.common,d.death_ref.id,"family_death:");
 for(const auto&entry:x.state.response_sources->response_sources.rows())if(entry.second.session_id==opening.session_opening->session_id){

  auto response_memory=CheckpointMemoryCharge(0);const auto response_ref=interaction_response_source(entry.first);
  const auto*r=take(read_original_interaction_response_selection(x.common,x.state,x.catalog,response_ref,interaction_response_source_schema_version(x.catalog.response_sources_version)));
  auto result=take(read_common_command_source(x.common,r->committed_result_ref));
  if(serial(x.common,result.result->result_id,"result:")>=cut)reject("InteractionResponseAfterParticipantDeath",entry.first);
  consume(r->committed_result_ref,1);
 }
}
const InteractionSessionLifeSource&read(const InteractionSessionLifeContext&x,const StableId&id,const StudyPendingReleaseProofs*pending=nullptr,const InteractionSessionDeathPending*death_capture=nullptr,const InteractionResponseSource*actual_closing_tail=nullptr){const auto*r=x.state.response_sources->life_sources.find(id);if(!r||r->effect.result_id!=id)reject("InteractionSessionLifeSourceMissing",id);const auto order=serial(x.common,id,"interaction_session_life:");take(validate_s3_leaf_life_effect_shape(r->effect,0));const auto*opening=take(read_original_interaction_session_opening(x.common,x.state,x.catalog,r->session_id));const auto&opened=*opening->session_opening;auto d=death(x,r->death_ref);if(!affected(x,*opening,d,r->role)||once(x,r->death_ref,r->session_id,r->role)!=r)reject("InteractionSessionLifeNotActual",id);const auto*end=closing(x,opened);
 if(!end||end->cause!=ActorSocialOccupancyCause::session_participant_died||!end->actual_cause_source||!same(*end->actual_cause_source,death_source(r->death_ref))||end->after.interaction_id||position(x.common,occupancy_source(*end))<=position(x.common,death_source(r->death_ref))||take(compare_world_moments(end->occurred_at,d.recorded_at.exact_moment))!=0||serial(x.common,r->death_ref.id,"family_death:")>=order||order>=serial(x.common,end->id,"social_occupancy:"))reject("InteractionSessionActualDeathCloseRequired",id);
 const auto*effect=std::get_if<S3SessionStopped>(&r->effect.effect);if(!effect)reject("InteractionSessionOriginalActionTerminalRequired",id,ErrorKind::unsupported_capability);
 if(effect->selected_response_tail_ref){
  const auto*response=take(read_original_interaction_response_selection(x.common,x.state,x.catalog,*effect->selected_response_tail_ref,interaction_response_source_schema_version(x.catalog.response_sources_version)));
  if(response->tail_consumed_result_ref){if(!take(validate_interaction_response_death_consumption_source(x.common,x.state,x.catalog,*response,*response->tail_consumed_result_ref)))reject("InteractionDeathTailActualConsumptionRequired",id);}
  else if(actual_closing_tail==response){
   if(interaction_response_source_schema_version(x.catalog.response_sources_version)!=2||death_tail_life(x.common,x.state,x.catalog,*response)!=r)reject("InteractionDeathTailActualCloseRequired",id);
   unconsumed_death_boundary(x.state,*response,*r);
  }else if(!death_capture||!take(death_capture->validate_original_pending_response(x.common,x.state,x.catalog,*response)))reject("InteractionDeathTailActualSealRequired",id,ErrorKind::internal);
 }
 if(effect->original_action_terminal_ref){
  const auto&terminal=*effect->original_action_terminal_ref;
  if(x.growth_terminals&&x.growth_terminals->owns_original_terminal(terminal)){
   auto actual=take(x.growth_terminals->read_original_terminal(r->session_id,terminal,r->death_ref));
   if(!actual.publication||!actual.boundary||take(compare_world_moments(actual.boundary->occurred_at,d.effective_at.exact_moment)))reject("InteractionSessionActionTerminalDeathMismatch",id);
  }else if(x.task_terminals&&x.task_terminals->owns_original_terminal(terminal)){
   auto actual=take(x.task_terminals->read_original_terminal(r->session_id,terminal,r->death_ref));
   if(!actual.terminal||take(compare_world_moments(actual.terminal->finished_at,d.effective_at.exact_moment)))reject("InteractionSessionActionTerminalDeathMismatch",id);
  }else{
   if(!x.study_terminals)reject("InteractionOriginalActionTerminalOwnerRequired",id,ErrorKind::unsupported_capability);
   auto actual=pending?take(x.study_terminals->read_original_terminal_in_capture(r->session_id,terminal,r->death_ref,*pending)):take(x.study_terminals->read_original_terminal(r->session_id,terminal,r->death_ref));
   if(!actual.terminal||!actual.time||take(compare_world_moments(actual.time->at,d.effective_at.exact_moment)))reject("InteractionSessionActionTerminalDeathMismatch",id);
  }
 }
 const bool study_terminal=x.study_terminals&&take(x.study_terminals->has_original_session_death_terminal(r->session_id,r->death_ref));
 const bool growth_terminal=x.growth_terminals&&take(x.growth_terminals->has_original_session_death_terminal(r->session_id,r->death_ref));
 const bool task_terminal=x.task_terminals&&take(x.task_terminals->has_original_session_death_terminal(r->session_id,r->death_ref));
 const unsigned terminal_count=unsigned(study_terminal)+unsigned(growth_terminal)+unsigned(task_terminal);
 if(terminal_count>1||bool(terminal_count)!=bool(effect->original_action_terminal_ref))reject("InteractionSessionOriginalTerminalCoverageMismatch",id);
 if(r->effect.original_terms_ref||take(compare_world_moments(r->effect.effective_game_time.exact_moment,d.effective_at.exact_moment))!=0)reject("InteractionSessionLifeOriginalTimeMismatch",id);
 std::size_t n=0;preserved(x,*opening,d,[&](const SourceRef&ref,Integer){if(n>=r->effect.preserved_result_refs.size()||!same(ref,r->effect.preserved_result_refs[n++]))reject("InteractionSessionPreservedResultsMismatch",id);});if(n!=r->effect.preserved_result_refs.size())reject("InteractionSessionPreservedResultsMismatch",id);
 for(const auto&s:x.state.interactions)if(s.id==r->session_id)reject("DeadInteractionSessionStillOpen",id);return *r;}
}
InteractionSessionDeathPending::InteractionSessionDeathPending(CommonStore&c,InteractionState&s,const InteractionCatalog&cat)
 :common_(&c),state_(&s),catalog_(&cat),capture_identity_(c.actor_condition_capture_identity()){}
InteractionSessionDeathPending::~InteractionSessionDeathPending()=default;
Outcome<std::unique_ptr<InteractionSessionDeathPending>> InteractionSessionDeathPending::create(CommonStore&c,InteractionState&s,const InteractionCatalog&cat){
 if(!interaction_response_version(cat.response_sources_version)||!s.response_sources||s.response_sources->physical_version!=cat.response_sources_version||!c.actor_condition_capture_identity())return {false,{},common_failure("InteractionDeathTailActualCaptureRequired",{},ErrorKind::internal).error};
 try{return {true,std::unique_ptr<InteractionSessionDeathPending>(new InteractionSessionDeathPending(c,s,cat)),{}};}
 catch(const std::bad_alloc&){return {false,{},common_failure("InteractionDeathTailCaptureResourceExhausted",{},ErrorKind::internal).error};}
}
bool InteractionSessionDeathPending::bound_to(const CommonStore&c,const InteractionState&s,const InteractionCatalog&cat)const noexcept{
 return common_==&c&&state_==&s&&catalog_==&cat&&capture_identity_&&capture_identity_==c.actor_condition_capture_identity();
}
bool InteractionSessionDeathPending::has_pending()const noexcept{for(const auto&entry:entries_)if(!entry->consumed)return true;return false;}
Outcome<bool> InteractionSessionDeathPending::validate_original_pending_response(const CommonStore&c,const InteractionState&s,const InteractionCatalog&cat,const InteractionResponseSource&response)const{return guard<bool>([&]{
 if(!bound_to(c,s,cat))reject("InteractionDeathTailCaptureMismatch",response.response_fact_id);
 const Entry*entry=nullptr;for(const auto&value:entries_)if(value->response.response_fact_id==response.response_fact_id){if(entry)reject("DuplicateInteractionDeathTailCapture",response.response_fact_id);entry=value.get();}
 if(!entry||entry->consumed)return false;
 const auto&old=entry->response;
 if(response.tail_consumed_result_ref||old.tail_consumed_result_ref||response.session_id!=old.session_id||response.session_generation!=old.session_generation||response.selected_node_id!=old.selected_node_id||response.selected_choice_id!=old.selected_choice_id||!same(response.committed_result_ref,old.committed_result_ref)||!same_interaction_response_next(response.next_node_or_terminal,old.next_node_or_terminal))reject("InteractionDeathTailOriginalChanged",response.response_fact_id);
 const auto*life=death_tail_life(c,s,cat,response);if(!life||life->effect.result_id!=entry->life_result_id||!same(life->death_ref,entry->death_ref))reject("InteractionDeathTailOriginalCloseRequired",response.response_fact_id);
 unconsumed_death_boundary(s,response,*life);
 return true;
});}
Outcome<bool> validate_interaction_response_death_consumption_source(const CommonStore&c,const InteractionState&s,const InteractionCatalog&cat,const InteractionResponseSource&response,const SourceRef&result){return guard<bool>([&]{
 const auto*life=death_tail_life(c,s,cat,response);if(!life)return false;
 if(!response.tail_consumed_result_ref||!same(*response.tail_consumed_result_ref,result))reject("InteractionDeathTailActualResultRequired",response.response_fact_id);
 const auto*opening=take(read_original_interaction_session_opening(c,s,cat,response.session_id));const auto*end=closing(s,*opening->session_opening);
 const auto selected_result=take(read_common_command_source(c,response.committed_result_ref));
 if(result.domain=="common"&&result.kind=="command_result"){
  const auto actual=take(read_common_command_source(c,result));
  if(!end||actual.result->revision<=selected_result.result->revision||actual.result->tick!=end->occurred_at.half_day_tick||serial(c,end->id,"social_occupancy:")>=serial(c,result.id,"result:"))reject("InteractionDeathTailActualSealOrderMismatch",result.id);
 }else {
  // Schema2 adds only this already-existing immutable close. The original
  // Common choice remains a real command; no revision/actor/sequence is made up.
  if(interaction_response_source_schema_version(cat.response_sources_version)!=2||!end||result.domain!="interaction"||result.kind!="actor_social_occupancy_changed"||result.id!=end->id||selected_result.result->tick>end->occurred_at.half_day_tick||serial(c,selected_result.result->result_id,"result:")>=serial(c,life->death_ref.id,"family_death:"))reject("InteractionDeathTailActualCloseSourceRequired",result.id);
  position(c,result); // Require the actual stored header through the sole append-order reader.
 }
 const auto*b=s.response_sources->session_boundaries.find(response.session_id);
 if(response.session_generation==std::numeric_limits<Integer>::max()||!b||b->generation!=response.session_generation+1||b->current_response_fact_ref||!b->last_consumed_response_fact_ref||!same(*b->last_consumed_response_fact_ref,*std::get<S3SessionStopped>(life->effect.effect).selected_response_tail_ref)||s.response_sources->selected_response_tails.find(response.session_id))reject("InteractionDeathTailConsumedBoundaryMismatch",response.response_fact_id);
 return true;
});}
Outcome<const InteractionSessionLifeSource*>read_interaction_session_life_source(const InteractionSessionLifeContext&x,const StableId&id){return guard<const InteractionSessionLifeSource*>([&]{selected(x);return &read(x,id);});}
namespace {
Status validate_life_sources(const InteractionSessionLifeContext&x,const InteractionSessionDeathPending*death_capture,const StudyPendingReleaseProofs*study){return guard<Done>([&]{selected(x);take(x.occupancy.validate_all());take(death_capture?validate_interaction_response_sources_in_death_capture(x.common,x.state,x.catalog,*death_capture):validate_interaction_response_sources(x.common,x.state,x.catalog));Integer previous=0;for(const auto&entry:x.state.response_sources->life_sources.rows()){const auto n=serial(x.common,entry.first,"interaction_session_life:");if(n<=previous)reject("InteractionSessionLifeOrderMismatch",entry.first);previous=n;read(x,entry.first,study,death_capture);}for(const auto&entry:x.state.social_occupancy->transitions.rows())if(entry.second.cause==ActorSocialOccupancyCause::session_participant_died){const InteractionSessionLifeSource*matched=nullptr;for(const auto&life:x.state.response_sources->life_sources.rows())if(entry.second.before.interaction_id==std::optional<StableId>{life.second.session_id}){const auto*opening=take(read_original_interaction_session_opening(x.common,x.state,x.catalog,life.second.session_id));if(closing(x,*opening->session_opening)!=&entry.second)continue;if(matched)reject("DuplicateInteractionSessionLifeClose",entry.first);matched=&life.second;}if(!matched)reject("OrphanInteractionSessionDeathClose",entry.first);}for(const auto&entry:x.deaths.deaths.rows()){auto d=death(x,{"family_life","family_death",entry.first});for(const auto&source:x.state.social_occupancy->transitions.rows())if(source.second.session_opening){const auto*opening=take(read_original_interaction_session_opening(x.common,x.state,x.catalog,source.second.session_opening->session_id));for(auto role:{InteractionSessionLifeRole::speaker,InteractionSessionLifeRole::listener})if(affected(x,*opening,d,role)&&!once(x,d.death_ref,opening->session_opening->session_id,role))reject("InteractionSessionDeathConsumptionMissing",opening->session_opening->session_id);}}return Done{};});}
}
Status validate_interaction_session_life_sources(const InteractionSessionLifeContext&x){return validate_life_sources(x,nullptr,nullptr);}
Status validate_interaction_session_life_sources_in_capture(const InteractionSessionLifeContext&x,const InteractionSessionDeathPending&pending,const StudyPendingReleaseProofs*study){
 if(!pending.bound_to(x.common,x.state,x.catalog))return common_failure("InteractionDeathTailCaptureMismatch",{},ErrorKind::internal);
 return validate_life_sources(x,&pending,study);
}
// 在当前候选内消费真实死亡并停止对应会话；来源、时刻或占用不符即返回原错误。
Status apply_interaction_session_death_in_candidate(
    CommonStore &c, InteractionState &s, const InteractionSessionLifeContext &x,
    const InteractionAccess &access, const DeathRef &ref,
    const StudyPendingReleaseProofs *pending,
    InteractionSessionDeathPending *death_capture) {
  return guard<Done>([&] {
    if (&c != &x.common || &s != &x.state ||
        (death_capture && !death_capture->bound_to(c, s, x.catalog)))
      reject("InteractionSessionLifeCandidateMismatch");
    selected(x);
    take(x.occupancy.validate_all());
    take(death_capture
             ? validate_interaction_response_sources_in_death_capture(
                   c, s, x.catalog, *death_capture)
             : validate_interaction_response_sources(c, s, x.catalog));
    auto d = death(x, ref);
    if (take(compare_world_moments(d.recorded_at.exact_moment,
                                   take(x.clock.read_current_moment()))))
      reject("InteractionSessionActualDeathMomentRequired", ref.id);
    for (const auto &entry : s.response_sources->life_sources.rows())
      read(x, entry.first, pending, death_capture);
    for (const auto &entry : s.response_sources->session_boundaries.rows()) {
      const auto *opening = take(read_original_interaction_session_opening(
          c, s, x.catalog, entry.first));
      const auto &original = *opening->session_opening;
      for (auto role : {InteractionSessionLifeRole::speaker,
                        InteractionSessionLifeRole::listener}) {
        if (!affected(x, *opening, d, role))
          continue;
        if (const auto *prior = once(x, ref, entry.first, role)) {
          read(x, prior->effect.result_id, pending, death_capture);
          continue;
        }
        const InteractionSession *current = nullptr;
        for (const auto &session : s.interactions)
          if (session.id == entry.first) {
            if (current)
              reject("AmbiguousInteractionSession", entry.first);
            current = &session;
          }
        if (!current || current->actor_id != original.actor_id)
          reject("InteractionActualLiveSessionRequired", entry.first);
        CheckpointMemoryCharge terminal_ref_memory;
        std::optional<SourceRef> task_terminal_ref;
        const SourceRef *original_terminal = nullptr;
        if (!current->pending_action_id.empty() ||
            current->phase == "waiting") {
          if (current->pending_action_id.empty() || current->phase != "waiting")
            reject("InteractionWaitingActionDeathConsumerRequired", entry.first,
                   ErrorKind::unsupported_capability);
          if (x.growth_terminals && x.growth_terminals->owns_original_action(
                                        current->pending_action_id)) {
            auto actual = take(x.growth_terminals->read_released_waiting_action(
                entry.first, current->pending_action_id, ref));
            if (!actual.publication || !actual.boundary ||
                !actual.confirmation ||
                actual.confirmation->session_generation !=
                    entry.second.generation)
              reject("InteractionGrowthActualTerminalRequired", entry.first);
            original_terminal = &actual.publication->terminal_source_ref;
          } else if (x.task_terminals && x.task_terminals->owns_original_action(
                                             current->pending_action_id)) {
            auto actual = take(x.task_terminals->read_released_waiting_action(
                entry.first, current->pending_action_id, ref));
            if (!actual.started || !actual.terminal || !actual.confirmation ||
                actual.confirmation->session_generation !=
                    entry.second.generation)
              reject("InteractionTaskActualTerminalRequired", entry.first);


            (terminal_ref_memory).grow(0);
            task_terminal_ref.emplace(
                SourceRef{StableId{"tasks"}, StableId{"task_action_finished"},
                          checkpoint_fresh_string_copy(actual.terminal->id)});
            original_terminal = &*task_terminal_ref;
          } else {
            if (!x.study_terminals || !pending)
              reject("InteractionWaitingActionDeathConsumerRequired",
                     entry.first, ErrorKind::unsupported_capability);
            const StudyPendingReleaseProof *proof = nullptr;
            for (const auto &candidate : *pending)
              if (candidate.action_id() &&
                  *candidate.action_id() == current->pending_action_id) {
                if (proof)
                  reject("DuplicateInteractionStudyPendingRelease",
                         entry.first);
                proof = &candidate;
              }
            if (!proof)
              reject("InteractionWaitingActionDeathConsumerRequired",
                     entry.first, ErrorKind::unsupported_capability);
            auto actual = take(x.study_terminals->read_released_waiting_action(
                entry.first, current->pending_action_id, ref, *proof));
            if (!actual.terminal || !actual.time)
              reject("InteractionStudyActualTerminalRequired", entry.first);
            original_terminal = &actual.time->original_terminal_ref;
          }
        }
        const auto *listener = c.actor(original.actor_id);
        const auto *speaker = c.actor(original.context.provider_actor_id);
        if (!listener || !speaker || !listener->action_id.empty() ||
            !speaker->action_id.empty())
          reject("InteractionParticipantActionDeathConsumerRequired",
                 entry.first, ErrorKind::unsupported_capability);
        const InteractionResponseSource *selected_response = nullptr;
        const bool close_source = interaction_response_source_schema_version(
                                      x.catalog.response_sources_version) == 2;
        const auto *selected_tail =
            s.response_sources->selected_response_tails.find(entry.first);
        if (entry.second.current_response_fact_ref || selected_tail) {
          if ((!close_source && !death_capture) ||
              !entry.second.current_response_fact_ref || !selected_tail ||
              current->phase != "stepping" || current->pending_command ||
              !current->pending_action_id.empty() || original_terminal)
            reject("InteractionDeathTailActualCaptureRequired", entry.first,
                   ErrorKind::internal);
          selected_response = take(read_interaction_response_source(
              c, s, x.catalog, *entry.second.current_response_fact_ref,
              interaction_response_source_schema_version(
                  x.catalog.response_sources_version)));
          if (selected_response->session_id != entry.first ||
              selected_response->session_generation !=
                  entry.second.generation ||
              entry.second.generation == std::numeric_limits<Integer>::max())
            reject("InteractionDeathTailOriginalBoundaryMismatch", entry.first);
          if (!close_source)
            for (const auto &capture : death_capture->entries_)
              if (capture->response.session_id == entry.first)
                reject("DuplicateInteractionDeathTailCapture", entry.first);
          if (!close_source &&
              death_capture->entries_.size() >= CodecVisitor::max_items)
            reject("InteractionDeathTailCaptureCapacity", entry.first);
        }
        if (s.response_sources->life_sources.size() >= CodecVisitor::max_items)
          reject("InteractionSessionLifeCapacity", entry.first);
        const auto id = take(c.next_id("interaction_session_life"));


        std::size_t preserved_count = 0;
        preserved(x, *opening, d, [&](const SourceRef &v, Integer) {
          if (preserved_count >= CodecVisitor::max_items)
            reject("InteractionSessionPreservedResultLimit", entry.first);
          ++preserved_count;

        });
        if (preserved_count > SIZE_MAX / sizeof(SourceRef))
          throw std::bad_alloc();

        InteractionSessionLifeSource row;
        row.copy_memory = CheckpointCopyLease::reserve();
        // 完整保留行预留完成后才开始新副本；Common
        // ID、校验临时值与表扩容分别记账。
        row.death_ref = DeathRef{ref.domain, ref.kind, ref.id};
        row.session_id = checkpoint_fresh_string_copy(entry.first);
        row.role = role;
        row.effect.result_id = checkpoint_fresh_string_copy(id);
        row.effect.effect = S3SessionStopped{};
        if (original_terminal)
          std::get<S3SessionStopped>(row.effect.effect)
              .original_action_terminal_ref = *original_terminal;
        if (selected_response) {
          const auto &source = *entry.second.current_response_fact_ref;
          std::get<S3SessionStopped>(row.effect.effect)
              .selected_response_tail_ref =
              SourceRef{checkpoint_fresh_string_copy(source.domain),
                        checkpoint_fresh_string_copy(source.kind),
                        checkpoint_fresh_string_copy(source.id)};
        }
        row.effect.effective_game_time = d.effective_at;
        row.effect.preserved_result_refs.reserve(preserved_count);
        preserved(x, *opening, d, [&](const SourceRef &v, Integer) {
          if (row.effect.preserved_result_refs.size() >= preserved_count)
            reject("InteractionSessionPreservedResultsChanged", entry.first);
          row.effect.preserved_result_refs.push_back(v);
        });
        if (row.effect.preserved_result_refs.size() != preserved_count)
          reject("InteractionSessionPreservedResultsChanged", entry.first);
        if (selected_response && !close_source)
          death_capture->entries_.push_back(
              std::unique_ptr<InteractionSessionDeathPending::Entry>(
                  new InteractionSessionDeathPending::Entry(
                      c, id, ref, *selected_response)));
        take(erase_actor_interaction_with_source_in_candidate(
            c, s, access, original.actor_id,
            ActorSocialOccupancyCause::session_participant_died,
            original.actor_id, death_source(ref)));
        if (!s.response_sources->life_sources.insert(id, std::move(row)))
          reject("DuplicateInteractionSessionLifeResult", id);
        if (selected_response && close_source)
          take(bind_interaction_session_death_close_in_candidate(c, s, x, id,
                                                                 pending));
        else
          read(x, id, pending, death_capture);
        break; // 只消费已匹配角色；插入会使借用的原开场行失效。
      }
    }
    return Done{};
  });
}
Status bind_interaction_session_death_close_in_candidate(CommonStore&c,InteractionState&s,const InteractionSessionLifeContext&x,const StableId&id,const StudyPendingReleaseProofs*study){return guard<Done>([&]{
 if(&c!=&x.common||&s!=&x.state)reject("InteractionSessionLifeCandidateMismatch",id);selected(x);
 if(interaction_response_source_schema_version(x.catalog.response_sources_version)!=2)reject("InteractionDeathCloseRequiresResponseSchema2",id,ErrorKind::unsupported_capability);
 const auto*life=s.response_sources->life_sources.find(id);const auto*stop=life?std::get_if<S3SessionStopped>(&life->effect.effect):nullptr;
 if(!life||!stop||!stop->selected_response_tail_ref)reject("InteractionDeathTailOriginalCloseRequired",id);
 auto*response=s.response_sources->response_sources.find(stop->selected_response_tail_ref->id);
 if(!response)reject("InteractionDeathTailOriginalResponseRequired",id);
 const auto*opening=take(read_original_interaction_session_opening(c,s,x.catalog,life->session_id));const auto*end=closing(x,*opening->session_opening);
 if(!end||take(compare_world_moments(end->occurred_at,take(x.clock.read_current_moment()))))reject("InteractionDeathTailActualCloseMomentRequired",id);
 // This is the full original Family4/role/Study/preserved-result read, with a
 // private exact-row cut solely for the tail this binder is about to consume.
 read(x,id,study,nullptr,response);
 auto ref_memory=CheckpointMemoryCharge(0);
 const SourceRef source=occupancy_source(*end);
 if(response->tail_consumed_result_ref){if(!same(*response->tail_consumed_result_ref,source))reject("InteractionDeathTailSealReplayConflict",id);return Done{};}
 auto*boundary=s.response_sources->session_boundaries.find(response->session_id);if(!boundary)reject("InteractionDeathTailOriginalBoundaryMismatch",id);
 InteractionSessionBoundary owned(*boundary);boundary->swap(owned);
 take(response->retain_consumed_result_copy(source));take(boundary->consume_current_response());++boundary->generation;
 s.response_sources->selected_response_tails.erase(response->session_id);
 read(x,id,study);take(validate_interaction_response_sources(c,s,x.catalog));return Done{};
});}
Status bind_interaction_session_death_result_in_candidate(CommonStore&c,InteractionState&s,const InteractionSessionLifeContext&x,InteractionSessionDeathPending&pending,const SourceRef&ref,const StudyPendingReleaseProofs*study){return guard<Done>([&]{
 if(&c!=&x.common||&s!=&x.state||!pending.bound_to(c,s,x.catalog))reject("InteractionDeathTailCaptureMismatch");selected(x);
 const auto actual=take(read_common_command_source(c,ref));
 if(actual.result->revision!=c.revision()||actual.result->tick!=c.tick())reject("InteractionDeathTailCurrentSealRequired",ref.id);
 for(auto&captured:pending.entries_){auto&entry=*captured;const auto&id=entry.response.response_fact_id;
  auto*response=s.response_sources->response_sources.find(id);if(!response)reject("InteractionDeathTailOriginalResponseRequired",id);
  if(entry.consumed){if(!response->tail_consumed_result_ref||!same(*response->tail_consumed_result_ref,ref))reject("InteractionDeathTailSealReplayConflict",id);read(x,entry.life_result_id,study);continue;}
  if(actual.result->revision<=entry.original_revision||!take(pending.validate_original_pending_response(c,s,x.catalog,*response)))reject("InteractionDeathTailActualSealOrderMismatch",id);
  const auto&life=read(x,entry.life_result_id,study,&pending);const auto*opening=take(read_original_interaction_session_opening(c,s,x.catalog,life.session_id));const auto*end=closing(x,*opening->session_opening);
  if(!end||actual.result->tick!=end->occurred_at.half_day_tick||serial(c,end->id,"social_occupancy:")>=serial(c,ref.id,"result:"))reject("InteractionDeathTailActualSealOrderMismatch",ref.id);
  auto*boundary=s.response_sources->session_boundaries.find(entry.response.session_id);if(!boundary||boundary->generation==std::numeric_limits<Integer>::max())reject("InteractionDeathTailOriginalBoundaryMismatch",id);
  // Re-own decoder/adopted strings before the exact old-last release. The
  // source and boundary acquire real late-ref bytes before any persistent copy.
  InteractionSessionBoundary owned(*boundary);boundary->swap(owned);
  take(response->retain_consumed_result_copy(ref));take(boundary->consume_current_response());++boundary->generation;
  s.response_sources->selected_response_tails.erase(entry.response.session_id);entry.consumed=true;
  read(x,entry.life_result_id,study);
 }
 take(validate_interaction_response_sources(c,s,x.catalog));
 for(const auto&entry:s.response_sources->life_sources.rows())read(x,entry.first,study);
 return Done{};
});}
Status visit_interaction_session_life_source_pins(const InteractionSessionLifeContext&x,InteractionSessionLifeSourcePinSink&sink){return guard<Done>([&]{
 take(validate_interaction_session_life_sources(x));
 for(const auto&entry:x.state.response_sources->life_sources.rows()){
  const auto&r=entry.second;
  take(sink.original_source(entry.first,InteractionSessionLifePinSlot::death,0,death_source(r.death_ref),4));
  if(const auto*stop=std::get_if<S3SessionStopped>(&r.effect.effect);stop&&stop->original_action_terminal_ref)
   take(sink.original_source(entry.first,InteractionSessionLifePinSlot::original_action_terminal,0,*stop->original_action_terminal_ref,(x.task_terminals&&x.task_terminals->owns_original_terminal(*stop->original_action_terminal_ref))?2:1));
  if(const auto*stop=std::get_if<S3SessionStopped>(&r.effect.effect);stop&&stop->selected_response_tail_ref)
   take(sink.original_source(entry.first,InteractionSessionLifePinSlot::selected_response_tail,0,*stop->selected_response_tail_ref,interaction_response_source_schema_version(x.catalog.response_sources_version)));
  for(std::size_t n=0;n<r.effect.preserved_result_refs.size();++n)
   take(sink.original_source(entry.first,InteractionSessionLifePinSlot::preserved_result,static_cast<Integer>(n),r.effect.preserved_result_refs[n],1));
  const auto*opening=take(read_original_interaction_session_opening(x.common,x.state,x.catalog,r.session_id));
  take(sink.original_source(entry.first,InteractionSessionLifePinSlot::opening,0,occupancy_source(*opening),1));
  const auto*end=closing(x,*opening->session_opening);if(!end)reject("InteractionSessionActualDeathCloseRequired",entry.first);
  take(sink.original_source(entry.first,InteractionSessionLifePinSlot::closing,0,occupancy_source(*end),1));
 }return Done{};
});}
Status visit_interaction_session_life_source_pins(const InteractionSessionLifeContext&x,InteractionResponseSourcePinSink&sink){
 class Adapter final:public InteractionSessionLifeSourcePinSink {InteractionResponseSourcePinSink&out_;public:explicit Adapter(InteractionResponseSourcePinSink&o):out_(o){}
  Status original_source(const StableId&,InteractionSessionLifePinSlot,Integer,const SourceRef&r,Integer schema)override{return out_.original_source(r,schema);}
 }adapter(sink);return visit_interaction_session_life_source_pins(x,adapter);
}
}
