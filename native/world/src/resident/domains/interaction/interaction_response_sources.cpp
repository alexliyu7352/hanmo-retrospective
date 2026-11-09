#include "interaction_response_sources.hpp"
#include "interaction_session_life.hpp"
#include "interaction_model.hpp"
#include "interaction_actions.hpp"
#include "face_to_face_contact_source.hpp"
#include "resident/kernel/common_command_sources.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/command_types.hpp"
#include "resident/domains/family/control_continuation.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <limits>
#include <new>
#include <charconv>
#include <string_view>
namespace hanmo::resident {namespace {
bool same(const SourceRef&a,const SourceRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
bool response_ref(const SourceRef&r){return r.domain=="interaction"&&r.kind=="response_selected"&&!r.id.empty();}
bool command_ref(const SourceRef&r){return r.domain=="common"&&r.kind=="command_result"&&!r.id.empty();}
// These are the exact three IDs allocated by the existing Common.next_id
// writers in this transaction, not lexical ordering of arbitrary SourceRefs.
Integer allocated(const StableId&id,std::string_view prefix,Integer next){
 std::string_view text(id);if(text.size()<=prefix.size()||text.substr(0,prefix.size())!=prefix)return 0;text.remove_prefix(prefix.size());if(text.empty()||text.front()=='0')return 0;Integer value=0;auto decoded=std::from_chars(text.data(),text.data()+text.size(),value);return decoded.ec==std::errc{}&&decoded.ptr==text.data()+text.size()&&value>0&&value<next?value:0;
}
Status selected(const InteractionState&s,const InteractionCatalog&c){
 if(!interaction_response_version(c.response_sources_version)||!s.response_sources)return common_failure("InteractionResponseSourcesRequired",{},ErrorKind::unsupported_capability);
 if(s.response_sources->physical_version!=c.response_sources_version)return common_failure("InteractionResponseSelectionMismatch",{},ErrorKind::internal);return common_success();
}
const InteractionSession*session(const InteractionState&s,const StableId&actor){const InteractionSession*out=nullptr;for(const auto&v:s.interactions)if(v.actor_id==actor){if(out)return nullptr;out=&v;}return out;}
// Borrow only the original immutable opening owner. These scalar identities
// are not a reconstructed session or a substitute for each opening's full cold
// validation, which remains with its original Born/Known/Contact owner.
struct OriginalSession {
 const StableId* actor=nullptr;const StableId* provider=nullptr;
 const StableId* dialogue=nullptr;const FactRecord* header=nullptr;
};
Outcome<OriginalSession> origin(const CommonStore&c,const InteractionState&s,const InteractionCatalog&cat,const StableId&id){
 OriginalSession out;Integer specialized_openings=0;
 if(s.social_occupancy&&s.social_occupancy->version==3){auto opened=read_original_interaction_session_opening(c,s,cat,id);if(!opened.ok)return {false,{},opened.error};const auto&o=*opened.value->session_opening;out={&o.actor_id,&o.context.provider_actor_id,&o.dialogue_id,c.state().facts.find(opened.value->id)};}
 auto take=[&](const StableId&actor,const StableId&provider,const StableId&dialogue,const StableId&fact,const char*domain,const char*kind)->Status{
  const auto*h=c.state().facts.find(fact);
  if(!h||h->id!=fact||h->domain!=domain||h->kind!=kind||actor.empty()||provider.empty()||actor==provider||dialogue.empty())return common_failure("InteractionOriginalOpeningMismatch",id);
  if(++specialized_openings>1)return common_failure("DuplicateInteractionOriginalOpening",id);
  if(out.header){if(*out.actor!=actor||*out.provider!=provider||*out.dialogue!=dialogue)return common_failure("InteractionOriginalOpeningMismatch",id);return common_success();}
  out={&actor,&provider,&dialogue,h};return common_success();
 };
 if(s.born_personal_dialogues){if(s.born_personal_dialogues->version!=1)return {false,{},common_failure("BornPersonalDialogueVersionUnsupported",id).error};for(const auto&entry:s.born_personal_dialogues->openings.rows())if(entry.second.session_id==id){const auto&r=entry.second;if(entry.first!=r.fact_id)return {false,{},common_failure("BornPersonalDialogueOpeningKeyMismatch",id).error};auto valid=take(r.listener_actor_id,r.provider_actor_id,r.dialogue_id,r.fact_id,born_personal_dialogue_module,born_personal_dialogue_opened_kind);if(!valid.ok)return {false,{},valid.error};}}
 if(s.known_personal_dialogues){if(s.known_personal_dialogues->version!=1)return {false,{},common_failure("KnownPersonalDialogueVersionUnsupported",id).error};for(const auto&entry:s.known_personal_dialogues->openings.rows())if(entry.second.session_id==id){const auto&r=entry.second;if(entry.first!=r.fact_id)return {false,{},common_failure("KnownPersonalDialogueOpeningKeyMismatch",id).error};auto valid=take(r.listener_origin.actor_id,r.provider_origin.actor_id,r.dialogue_id,r.fact_id,known_personal_dialogue_module,known_personal_dialogue_opened_kind);if(!valid.ok)return {false,{},valid.error};}}
 if(s.face_to_face_contacts&&s.face_to_face_contacts->results.find(id)){
  auto contact=read_face_to_face_contact_source(c,s,cat,{"interaction",face_to_face_contact_source_kind,id},1);if(!contact.ok)return {false,{},contact.error};const auto&r=*contact.value;
  if(out.header){if(*out.actor!=r.session_actor_id||*out.provider!=r.provider_actor_id||*out.dialogue!=r.dialogue_id)return {false,{},common_failure("InteractionOriginalContactMismatch",id).error};}
  else {auto valid=take(r.session_actor_id,r.provider_actor_id,r.dialogue_id,r.session_id,"interaction",face_to_face_contact_source_kind);if(!valid.ok)return {false,{},valid.error};}
 }
 if(!out.header)return {false,{},common_failure("InteractionImmutableOpeningSourceRequired",id,ErrorKind::unsupported_capability).error};
 return {true,out,{}};
}
// 八槽 next 是原已选 option 的确定入口，不用今天条件重演历史分支。
Outcome<const DialogueNode*> choice_next(const InteractionCatalog&cat,const OriginalSession&o,const StableId&node_id,const StableId&choice_id,const StableId&source_id){
 const DialogueDefinition*d=nullptr;for(const auto&v:cat.dialogues)if(v.id==*o.dialogue){if(d)return {false,{},common_failure("AmbiguousInteractionResponseDialogue",source_id).error};d=&v;}
 const DialogueNode*n=nullptr;if(d)for(const auto&v:d->nodes)if(v.id==node_id){if(n)return {false,{},common_failure("AmbiguousInteractionResponseNode",source_id).error};n=&v;}
 if(!n||n->kind!="choice")return {false,{},common_failure("InteractionResponseChoiceNodeRequired",source_id).error};
 const DialogueOption*option=nullptr;for(const auto&v:n->options)if(v.id==choice_id){if(option)return {false,{},common_failure("AmbiguousInteractionResponseChoice",source_id).error};option=&v;}
 if(!option||option->next.empty())return {false,{},common_failure("InteractionResponseOriginalChoiceRequired",source_id).error};
 const DialogueNode*next=nullptr;for(const auto&v:d->nodes)if(v.id==option->next){if(next)return {false,{},common_failure("AmbiguousInteractionResponseNext",source_id).error};next=&v;}
 if(!next||(next->kind!="say"&&next->kind!="choice"&&next->kind!="action"&&next->kind!="branch"&&next->kind!="end"))return {false,{},common_failure("InteractionResponseNextNodeRequired",source_id).error};
 return {true,next,{}};
}
Outcome<const DialogueNode*> choice_next(const InteractionCatalog&cat,const OriginalSession&o,const InteractionResponseSource&r){return choice_next(cat,o,r.selected_node_id,r.selected_choice_id,r.response_fact_id);}
Status choice(const InteractionCatalog&cat,const OriginalSession&o,const InteractionResponseSource&r){
 auto target=choice_next(cat,o,r);if(!target.ok)return {false,{},target.error};
 if(target.value->kind=="end")return std::holds_alternative<InteractionResponseTerminal>(r.next_node_or_terminal)?common_success():common_failure("InteractionResponseOriginalNextMismatch",r.response_fact_id);
 const auto*next=std::get_if<InteractionResponseNode>(&r.next_node_or_terminal);
 return next&&next->node_id==target.value->id?common_success():common_failure("InteractionResponseOriginalNextMismatch",r.response_fact_id);
}
// The same original zero-time pump serves normal selected-response finalizing
// and the genuine retained-tail bridge. Its only automatic action remains the
// existing discover_offer; payments/contracts stop at their original action.
Status settle_selected_response(CommonStore&common,InteractionState&s,const InteractionCatalog&cat,const InteractionAccess&access,const StableId&actor,const StableId&session_id,const WorldMoment&at){
 if(common.tick()!=at.half_day_tick)return common_failure("InteractionResponseActualMomentChanged",session_id,ErrorKind::internal);
 auto valid=pump_dialogue(common,s,cat,access,actor);if(!valid.ok)return valid;
 const auto*stable=session(s,actor);if(stable&&(stable->id!=session_id||(stable->phase!="say"&&stable->phase!="choice"&&stable->phase!="action")))return common_failure("InteractionResponseStableBoundaryRequired",session_id);
 if(common.tick()!=at.half_day_tick)return common_failure("InteractionResponseAdvancedWorldTime",session_id,ErrorKind::internal);
 if(s.face_to_face_contacts){auto contact=publish_face_to_face_contact_in_candidate(common,s,cat,access,actor,at);if(!contact.ok)return {false,{},contact.error};}
 return common_success();
}
Status selected_boundary_request(const StabilizeContinuationBoundary&request,const InteractionResponseSource&response,const StableId&actor){
 auto valid=validate_continuation_boundary_request_shape(request);if(!valid.ok)return valid;
 const auto&boundary=request.boundary_ref;const auto*next=std::get_if<InteractionResponseNext>(&boundary.expected_next_boundary);
 if(boundary.owner!=ContinuationBoundaryOwner::interaction||request.candidate_actor_id!=actor||boundary.session_ref!=response.session_id||boundary.session_generation!=response.session_generation||boundary.frozen_segment_ordinal!=response.session_generation||!next||!same_interaction_response_next(*next,response.next_node_or_terminal))return common_failure("InteractionContinuationOriginalBoundaryMismatch",response.response_fact_id);
 return common_success();
}
Status consumed_boundary_receipt(const CommonStore&common,const InteractionResponseSource&response,const StableId&actor,const SourceRef&ref,Integer schema){
 // K3 owns the actual sealed tag20, prefix and identity reader. This owner
 // checks only its exact original consequence; it never reimplements S4
 // candidate eligibility or guesses it from today's control binding.
 auto actual=read_actual_continuation_boundary_receipt(common,ref);if(!actual.ok)return {false,{},actual.error};const auto&receipt=*actual.value;
 auto valid=selected_boundary_request(receipt.request,response,actor);if(!valid.ok)return valid;
 const auto&result=receipt.result;
 if(result.status!=ContinuationBoundaryStatus::ready||result.world_half_days!=0||result.next_boundary_ref||result.settled_source_refs.size()!=1)return common_failure("InteractionContinuationActualResultMismatch",ref.id);
 const auto&settled=result.settled_source_refs.front();
 if(settled.source_schema!=schema||!response_ref(settled.source_ref)||settled.source_ref.id!=response.response_fact_id)return common_failure("InteractionContinuationOriginalResponseRequired",ref.id);
 auto selected_result=read_common_command_source(common,response.committed_result_ref);if(!selected_result.ok)return {false,{},selected_result.error};
 auto consumed_result=read_common_command_source(common,ref);if(!consumed_result.ok)return {false,{},consumed_result.error};
 if(consumed_result.value.result->revision<=selected_result.value.result->revision||consumed_result.value.result->tick<selected_result.value.result->tick)return common_failure("InteractionContinuationConsumptionOrderMismatch",ref.id);
 return common_success();
}
Status install_consumed_result(InteractionResponseSource&record,const SourceRef&ref){
 // Both the response pump and original death stop use this single enduring
 // copy operation, only after their own exact original seal proof succeeds.
 return record.retain_consumed_result_copy(ref);
}
}
Integer interaction_response_source_schema_version(Integer v)noexcept{return v==5||v==6||v==7?1:v==8||v==9||v==10||v==11||v==12||v==13?2:0;}
bool interaction_response_version(Integer v)noexcept{return interaction_response_source_schema_version(v)!=0;}
Outcome<Integer> interaction_response_base_version(Integer v){if(!interaction_response_version(v))return {false,0,common_failure("InteractionResponseVersionUnsupported",{},ErrorKind::unsupported_capability).error};return {true,v-(v>=11?9:interaction_response_source_schema_version(v)==1?3:6),{}};}
Outcome<InteractionResponseSources> make_initial_interaction_response_sources(Integer v){auto base=interaction_response_base_version(v);if(!base.ok)return {false,{},base.error};InteractionResponseSources out;out.physical_version=v;return {true,std::move(out),{}};}
SourceRef interaction_response_source(const StableId&id){return {"interaction","response_selected",id};}
Outcome<Integer> interaction_response_source_schema(const SourceRef&ref){if(response_ref(ref))return {true,1,{}};return {false,0,common_failure("UnsupportedInteractionResponseSource",ref.id,ErrorKind::unsupported_capability).error};}
Outcome<Integer> interaction_response_source_schema(const InteractionState&s,const InteractionCatalog&cat,const SourceRef&ref){auto valid=selected(s,cat);if(!valid.ok)return {false,0,valid.error};if(!response_ref(ref))return {false,0,common_failure("UnsupportedInteractionResponseSource",ref.id,ErrorKind::unsupported_capability).error};return {true,interaction_response_source_schema_version(cat.response_sources_version),{}};}
bool same_interaction_response_next(const InteractionResponseNext&a,const InteractionResponseNext&b){if(a.index()!=b.index())return false;const auto*n=std::get_if<InteractionResponseNode>(&a);return !n||n->node_id==std::get<InteractionResponseNode>(b).node_id;}
Status validate_interaction_response_source_shape(const InteractionResponseSource&r,Integer schema){
 if(schema!=1&&schema!=2)return common_failure("UnsupportedInteractionResponseSourceSchema",r.response_fact_id,ErrorKind::unsupported_capability);
 if(r.response_fact_id.empty()||r.session_id.empty()||r.session_generation<1||r.selected_node_id.empty()||r.selected_choice_id.empty()||!command_ref(r.committed_result_ref))return common_failure("InvalidInteractionResponseSource",r.response_fact_id);
 if(const auto*n=std::get_if<InteractionResponseNode>(&r.next_node_or_terminal);n&&n->node_id.empty())return common_failure("InteractionResponseNextNodeRequired",r.response_fact_id);
 if(r.tail_consumed_result_ref&&!command_ref(*r.tail_consumed_result_ref)&&!(schema==2&&r.tail_consumed_result_ref->domain=="interaction"&&r.tail_consumed_result_ref->kind=="actor_social_occupancy_changed"&&!r.tail_consumed_result_ref->id.empty()))return common_failure("InteractionResponseConsumedResultRequired",r.response_fact_id);return common_success();
}
Status open_interaction_response_boundary_in_candidate(InteractionState&s,const InteractionCatalog&cat,const StableId&id){
 if(!cat.response_sources_version)return s.response_sources?common_failure("UnselectedInteractionResponseSources",id,ErrorKind::internal):common_success();auto valid=selected(s,cat);if(!valid.ok)return valid;
 Integer count=0;for(const auto&v:s.interactions)if(v.id==id){if(v.phase!="stepping")return common_failure("InteractionOriginalOpeningBoundaryRequired",id);++count;}if(count!=1||id.empty())return common_failure("InteractionOriginalOpeningRequired",id);
 auto&state=*s.response_sources;if(state.session_boundaries.size()>=CodecVisitor::max_items)return common_failure("InteractionSessionBoundaryLimit",id);if(state.session_boundaries.find(id))return common_failure("DuplicateInteractionSessionBoundary",id);
 InteractionSessionBoundary boundary{InteractionSessionBoundaryBuildView{id,1,nullptr,nullptr}};
 if(!state.session_boundaries.insert(id,std::move(boundary)))return common_failure("DuplicateInteractionSessionBoundary",id);return common_success();
}
Outcome<std::unique_ptr<InteractionSelectedResponsePending>> InteractionSelectedResponsePending::create(CommonStore&c,InteractionState&s,const InteractionCatalog&cat){
 auto valid=selected(s,cat);if(!valid.ok)return {false,{},valid.error};
 try{return {true,std::unique_ptr<InteractionSelectedResponsePending>(new InteractionSelectedResponsePending(c,s,cat)),{}};}
 catch(const std::bad_alloc&){return {false,{},common_failure("InteractionResponseCaptureResourceExhausted",{},ErrorKind::internal).error};}
}
bool InteractionSelectedResponsePending::bound_to(const CommonStore&c,const InteractionState&s,const InteractionCatalog&cat)const noexcept{return common_==&c&&state_==&s&&catalog_==&cat;}
Status prepare_interaction_selected_response_in_candidate(CommonStore&common,InteractionState&s,const InteractionCatalog&cat,const StableId&actor,const StableId&id,const StableId&node_id,const StableId&choice_id,const StableId&next_id,const WorldMoment&at,InteractionSelectedResponsePending&pending){
 auto valid=selected(s,cat);if(!valid.ok)return valid;
 if(!pending.bound_to(common,s,cat)||pending.has_response())return common_failure("InteractionResponseCaptureMismatch",id,ErrorKind::internal);
 valid=validate_interaction_response_sources(common,s,cat);if(!valid.ok)return valid;
 auto&state=*s.response_sources;if(state.response_sources.size()>=CodecVisitor::max_items||state.selected_response_tails.size()>=CodecVisitor::max_items)return common_failure("InteractionResponseSourceLimit",id);
 const auto*b=state.session_boundaries.find(id);if(!b||b->generation<1||b->current_response_fact_ref||state.selected_response_tails.find(id))return common_failure("InteractionResponseBoundaryMismatch",id);
 if(b->generation==std::numeric_limits<Integer>::max()||common.revision()==std::numeric_limits<Integer>::max())return common_failure("InteractionResponseGenerationOverflow",id);
 auto opened=origin(common,s,cat,id);if(!opened.ok)return {false,{},opened.error};if(*opened.value.actor!=actor)return common_failure("InteractionResponseActorMismatch",id);
 const auto*current=session(s,actor);if(!current||current->id!=id||current->phase!="choice"||current->node_id!=node_id||!current->pending_action_id.empty()||current->pending_command)return common_failure("InteractionOriginalChoiceRequired",id);
 auto canonical=compare_world_moments(at,at);if(!canonical.ok)return {false,{},canonical.error};
 auto target=choice_next(cat,opened.value,node_id,choice_id,id);if(!target.ok)return {false,{},target.error};if(target.value->id!=next_id)return common_failure("InteractionResponseOriginalNextMismatch",id);
 auto actor_memory=CheckpointMemoryCharge(0);
 auto key=common.next_id("interaction_response");if(!key.ok)return {false,{},key.error};
 InteractionResponseSource row(key.value,id,b->generation,node_id,choice_id,target.value->kind=="end"?nullptr:&next_id);
 // 只有私有 capture 拥有此行；没有未来 result，也不提前消费代次。
 pending.copy_memory_=std::move(actor_memory);pending.actor_id_=checkpoint_fresh_string_copy(actor);pending.response_=std::move(row);pending.selected_at_=at;pending.original_revision_=common.revision();return common_success();
}
Status retain_interaction_selected_response_in_candidate(CommonStore&common,InteractionState&s,const InteractionCatalog&cat,InteractionSelectedResponsePending&pending,const SourceRef&ref,const WorldMoment&actual_at){
 auto valid=selected(s,cat);if(!valid.ok)return valid;
 if(!pending.bound_to(common,s,cat)||pending.resuming_)return common_failure("InteractionResponseCaptureMismatch",{},ErrorKind::internal);
 if(!pending.has_response())return common_success();
 auto actual=read_common_command_source(common,ref);if(!actual.ok)return {false,{},actual.error};
 const auto&result=*actual.value.result;const auto&id=pending.response_.response_fact_id;
 if(!command_ref(ref)||result.kind!="dialogue_choose"||actual.value.history->actor_id!=pending.actor_id_||result.revision!=pending.original_revision_+1||result.tick!=actual_at.half_day_tick)return common_failure("InteractionResponseOriginalChooseSealRequired",ref.id);
 auto time=compare_world_moments(pending.selected_at_,actual_at);if(!time.ok)return {false,{},time.error};if(time.value)return common_failure("InteractionResponseActualMomentChanged",id,ErrorKind::internal);
 if(pending.retained_){auto prior=read_interaction_response_source(common,s,cat,interaction_response_source(id),interaction_response_source_schema_version(cat.response_sources_version));if(!prior.ok)return {false,{},prior.error};return same(prior.value->committed_result_ref,ref)&&bool(prior.value->tail_consumed_result_ref)==pending.consumed_?common_success():common_failure("InteractionResponseSealReplayConflict",id,ErrorKind::internal);}
 if(common.revision()!=result.revision)return common_failure("InteractionResponseSealNotCurrent",id,ErrorKind::internal);
 const auto*current=session(s,pending.actor_id_);const auto&saved=pending.response_;
 if(!current||current->id!=saved.session_id||current->phase!="stepping"||current->pending_command||!current->pending_action_id.empty())return common_failure("InteractionResponseSessionChangedBeforeSeal",id);
 auto opened=origin(common,s,cat,saved.session_id);if(!opened.ok)return {false,{},opened.error};
 const auto*listener=common.actor(*opened.value.actor);const auto*speaker=common.actor(*opened.value.provider);
 // 第一 settle 若已实际发生死亡，拒绝整个候选；不把私有 pending 变成死亡前提交。
 if(!listener||!speaker||!listener->alive||!speaker->alive)return common_failure("InteractionResponseParticipantUnavailableBeforeSeal",id);
 auto target=choice_next(cat,opened.value,saved);if(!target.ok)return {false,{},target.error};if(current->node_id!=target.value->id)return common_failure("InteractionResponseSessionChangedBeforeSeal",id);
 auto&state=*s.response_sources;auto*b=state.session_boundaries.find(saved.session_id);
 if(!b||b->generation!=saved.session_generation||b->current_response_fact_ref||state.response_sources.find(id)||state.selected_response_tails.find(saved.session_id))return common_failure("InteractionResponsePendingBoundaryMismatch",id);
 if(state.response_sources.size()>=CodecVisitor::max_items||state.selected_response_tails.size()>=CodecVisitor::max_items)return common_failure("InteractionResponseSourceLimit",id);
 InteractionResponseSource row(saved,ref);
 auto source_memory=CheckpointMemoryCharge(0);
 const auto source=interaction_response_source(id);
 SelectedResponseTail tail{saved.session_id,saved.session_generation,source,saved.next_node_or_terminal};
 if(!state.response_sources.insert(id,std::move(row))||!state.selected_response_tails.insert(saved.session_id,std::move(tail)))return common_failure("DuplicateInteractionResponse",id);
 InteractionSessionBoundary owned_boundary{InteractionSessionBoundaryBuildView{b->session_id,b->generation,&source,b->last_consumed_response_fact_ref?&*b->last_consumed_response_fact_ref:nullptr}};
 b->swap(owned_boundary);
 valid=validate_interaction_response_sources(common,s,cat);if(!valid.ok)return valid;
 pending.retained_=true;return common_success();
}
Status finalize_interaction_selected_response_in_candidate(CommonStore&common,InteractionState&s,const InteractionCatalog&cat,const InteractionAccess&access,InteractionSelectedResponsePending&pending,const SourceRef&ref,const WorldMoment&actual_at){
 // The actual capture already records whether this is a new dialogue choice
 // or the original Control3 retained-tail operation. The latter was pumped
 // before its real tag20 seal; only its original binder may run now.
 if(pending.resuming_)return bind_interaction_resumed_response_result_in_candidate(common,s,cat,access,pending,ref,actual_at);
 auto valid=retain_interaction_selected_response_in_candidate(common,s,cat,pending,ref,actual_at);if(!valid.ok)return valid;
 if(!pending.has_response()||pending.consumed_)return common_success();
 const auto&saved=pending.response_;const auto&id=saved.response_fact_id;auto&state=*s.response_sources;
 auto source_memory=CheckpointMemoryCharge(0);const auto source=interaction_response_source(id);
 // 先真 seal，再原唯一泵；原自动 discover_offer 顺序与真正 action 确认门不变。
 valid=settle_selected_response(common,s,cat,access,pending.actor_id_,saved.session_id,actual_at);if(!valid.ok)return valid;
 // 泵可改变原容器；重新借行，且只将这个已封结果消费一次。
 auto*b=state.session_boundaries.find(saved.session_id);auto*record=state.response_sources.find(id);const auto*retained=state.selected_response_tails.find(saved.session_id);
 if(!b||!record||!retained||b->generation!=saved.session_generation||!b->current_response_fact_ref||!same(*b->current_response_fact_ref,source)||!same(retained->response_fact_ref,source))return common_failure("InteractionResponseActualTailChanged",id,ErrorKind::internal);
 valid=install_consumed_result(*record,ref);if(!valid.ok)return valid;valid=b->consume_current_response();if(!valid.ok)return valid;++b->generation;state.selected_response_tails.erase(saved.session_id);pending.consumed_=true;
 return validate_interaction_response_sources(common,s,cat);
}
Outcome<const InteractionResponseSource*> resume_interaction_selected_response_tail_in_candidate(CommonStore&common,InteractionState&s,const InteractionCatalog&cat,const InteractionAccess&access,const StabilizeContinuationBoundary&request,const WorldMoment&actual_at,InteractionSelectedResponsePending&pending){
 if(!pending.bound_to(common,s,cat)||pending.has_response()||pending.resuming_)return {false,nullptr,common_failure("InteractionResponseCaptureMismatch",{},ErrorKind::internal).error};
 auto valid=validate_continuation_boundary_request_shape(request);if(!valid.ok)return {false,nullptr,valid.error};
 const auto&actor=request.candidate_actor_id;auto before=read_original_interaction_continuation_boundary(common,s,cat,access,actor);if(!before.ok)return {false,nullptr,before.error};
 if(!before.value.session||!before.value.boundary||!before.value.selected_response_tail)return {false,nullptr,common_failure("InteractionSelectedTailRequired",request.boundary_ref.session_ref).error};
 const auto&tail=*before.value.selected_response_tail;auto actual=read_interaction_response_source(common,s,cat,tail.response_fact_ref,interaction_response_source_schema_version(cat.response_sources_version));if(!actual.ok)return actual;
 const auto&saved=*actual.value;valid=selected_boundary_request(request,saved,actor);if(!valid.ok)return {false,nullptr,valid.error};
 if(saved.tail_consumed_result_ref||before.value.session->phase!="stepping"||before.value.boundary->generation!=saved.session_generation||tail.session_id!=saved.session_id||tail.session_generation!=saved.session_generation||!same_interaction_response_next(tail.next_node_or_terminal,saved.next_node_or_terminal))return {false,nullptr,common_failure("InteractionSelectedTailOriginalBoundaryMismatch",saved.response_fact_id).error};
 if(saved.session_generation==std::numeric_limits<Integer>::max()||common.revision()==std::numeric_limits<Integer>::max())return {false,nullptr,common_failure("InteractionResponseGenerationOverflow",saved.response_fact_id).error};
 auto canonical=compare_world_moments(actual_at,actual_at);if(!canonical.ok)return {false,nullptr,canonical.error};
 if(actual_at.half_day_tick!=common.tick()||request.boundary_ref.candidate.source_identity.revision!=common.revision()||request.boundary_ref.candidate.source_identity.tick!=common.tick())return {false,nullptr,common_failure("InteractionContinuationActualMomentMismatch",saved.response_fact_id).error};
 auto actor_memory=CheckpointMemoryCharge(0);InteractionResponseSource response(saved);
 auto*b=s.response_sources->session_boundaries.find(saved.session_id);
 InteractionSessionBoundary owned_boundary{InteractionSessionBoundaryBuildView{b->session_id,b->generation,b->current_response_fact_ref?&*b->current_response_fact_ref:nullptr,b->last_consumed_response_fact_ref?&*b->last_consumed_response_fact_ref:nullptr}};
 pending.copy_memory_=std::move(actor_memory);pending.actor_id_=checkpoint_fresh_string_copy(actor);pending.response_=std::move(response);pending.selected_at_=actual_at;pending.original_revision_=common.revision();b->swap(owned_boundary);
 valid=settle_selected_response(common,s,cat,access,pending.actor_id_,pending.response_.session_id,actual_at);if(!valid.ok)return {false,nullptr,valid.error};
 const auto*retained=s.response_sources->response_sources.find(pending.response_.response_fact_id);if(!retained)return {false,nullptr,common_failure("InteractionSelectedTailOriginalSourceLost",pending.response_.response_fact_id,ErrorKind::internal).error};
 pending.resuming_=true;return {true,retained,{}};
}
Status bind_interaction_resumed_response_result_in_candidate(CommonStore&common,InteractionState&s,const InteractionCatalog&cat,const InteractionAccess&access,InteractionSelectedResponsePending&pending,const SourceRef&ref,const WorldMoment&actual_at){
 if(!pending.bound_to(common,s,cat)||!pending.resuming_||!pending.has_response())return common_failure("InteractionResponseCaptureMismatch",{},ErrorKind::internal);
 auto valid=selected(s,cat);if(!valid.ok)return valid;
 const auto&saved=pending.response_;valid=consumed_boundary_receipt(common,saved,pending.actor_id_,ref,interaction_response_source_schema_version(cat.response_sources_version));if(!valid.ok)return valid;
 auto sealed=read_common_command_source(common,ref);if(!sealed.ok)return {false,{},sealed.error};
 auto moment=compare_world_moments(pending.selected_at_,actual_at);if(!moment.ok)return {false,{},moment.error};
 auto actual_receipt=read_actual_continuation_boundary_receipt(common,ref);if(!actual_receipt.ok)return {false,{},actual_receipt.error};auto receipt_moment=compare_world_moments(actual_receipt.value->actual_at,actual_at);if(!receipt_moment.ok)return {false,{},receipt_moment.error};
 if(moment.value||receipt_moment.value||sealed.value.result->revision!=pending.original_revision_+1||common.revision()!=sealed.value.result->revision||common.tick()!=actual_at.half_day_tick)return common_failure("InteractionContinuationActualSealRequired",ref.id);
 if(pending.consumed_){auto prior=read_interaction_response_source(common,s,cat,interaction_response_source(saved.response_fact_id),interaction_response_source_schema_version(cat.response_sources_version));if(!prior.ok)return {false,{},prior.error};return prior.value->tail_consumed_result_ref&&same(*prior.value->tail_consumed_result_ref,ref)?common_success():common_failure("InteractionResponseSealReplayConflict",ref.id,ErrorKind::internal);}
 auto&state=*s.response_sources;auto*b=state.session_boundaries.find(saved.session_id);auto*record=state.response_sources.find(saved.response_fact_id);const auto*tail=state.selected_response_tails.find(saved.session_id);
 if(!b||!record||!tail||b->generation!=saved.session_generation||!b->current_response_fact_ref||!response_ref(*b->current_response_fact_ref)||b->current_response_fact_ref->id!=saved.response_fact_id||record->tail_consumed_result_ref||record->response_fact_id!=saved.response_fact_id||record->session_id!=saved.session_id||record->session_generation!=saved.session_generation||record->selected_node_id!=saved.selected_node_id||record->selected_choice_id!=saved.selected_choice_id||!same_interaction_response_next(record->next_node_or_terminal,saved.next_node_or_terminal)||!same(record->committed_result_ref,saved.committed_result_ref)||!same(tail->response_fact_ref,*b->current_response_fact_ref)||tail->session_generation!=saved.session_generation||!same_interaction_response_next(tail->next_node_or_terminal,saved.next_node_or_terminal))return common_failure("InteractionResponseActualTailChanged",saved.response_fact_id,ErrorKind::internal);
 const auto*stable=session(s,pending.actor_id_);if(stable&&(stable->id!=saved.session_id||(stable->phase!="say"&&stable->phase!="choice"&&stable->phase!="action")))return common_failure("InteractionResponseStableBoundaryRequired",saved.response_fact_id);
 valid=install_consumed_result(*record,ref);if(!valid.ok)return valid;
 valid=b->consume_current_response();if(!valid.ok)return valid;++b->generation;state.selected_response_tails.erase(saved.session_id);pending.consumed_=true;
 valid=validate_interaction_response_sources(common,s,cat);if(!valid.ok)return valid;return validate_interaction_state(common,s,cat,access);
}
Status bind_interaction_selected_response_result_in_candidate(CommonStore&common,InteractionState&s,const InteractionCatalog&cat,const SourceRef&ref){
 if(!cat.response_sources_version)return s.response_sources?common_failure("UnselectedInteractionResponseSources",{},ErrorKind::internal):common_success();auto valid=selected(s,cat);if(!valid.ok)return valid;
 auto actual=read_common_command_source(common,ref);if(!actual.ok)return {false,{},actual.error};
 bool found=false;for(const auto&entry:s.response_sources->response_sources.rows())if(same(entry.second.committed_result_ref,ref)){if(found)return common_failure("DuplicateInteractionResponseCommit",ref.id);found=true;}
 if(actual.value.result->kind=="dialogue_choose"&&!found)return common_failure("InteractionOriginalResponseCaptureRequired",ref.id,ErrorKind::unsupported_capability);
 return validate_interaction_response_sources(common,s,cat);
}
Outcome<const InteractionResponseSource*> read_original_interaction_response_selection(const CommonStore&common,const InteractionState&s,const InteractionCatalog&cat,const SourceRef&ref,Integer schema){
 auto valid=selected(s,cat);if(!valid.ok)return {false,nullptr,valid.error};if(schema!=interaction_response_source_schema_version(cat.response_sources_version)||!response_ref(ref))return {false,nullptr,common_failure("UnsupportedInteractionResponseSource",ref.id,ErrorKind::unsupported_capability).error};
 const auto*r=s.response_sources->response_sources.find(ref.id);if(!r||r->response_fact_id!=ref.id)return {false,nullptr,common_failure("InteractionResponseSourceMissing",ref.id).error};valid=validate_interaction_response_source_shape(*r,schema);if(!valid.ok)return {false,nullptr,valid.error};
 auto opened=origin(common,s,cat,r->session_id);if(!opened.ok)return {false,nullptr,opened.error};valid=choice(cat,opened.value,*r);if(!valid.ok)return {false,nullptr,valid.error};auto actual=read_common_command_source(common,r->committed_result_ref);if(!actual.ok)return {false,nullptr,actual.error};
 if(actual.value.result->kind!="dialogue_choose"||actual.value.history->actor_id!=*opened.value.actor)return {false,nullptr,common_failure("InteractionResponseSealedActorMismatch",ref.id).error};
 // There is no invented exact moment in this fixed eight-slot source. The
 // original CommitResult retains its original Calendar precision.
 const auto limit=common.state().next_instance_id;const auto opened_order=allocated(r->session_id,"interaction:",limit),selected_order=allocated(r->response_fact_id,"interaction_response:",limit),sealed_order=allocated(actual.value.result->result_id,"result:",limit);
 if(!opened_order||!selected_order||!sealed_order||opened_order>=selected_order||selected_order>=sealed_order)return {false,nullptr,common_failure("InteractionResponseOriginalAllocationOrderMismatch",ref.id).error};
 return {true,r,{}};
}
namespace {
Outcome<const InteractionResponseSource*> read_response(const CommonStore&common,const InteractionState&s,const InteractionCatalog&cat,const SourceRef&ref,Integer schema,const InteractionSessionDeathPending*pending){
 auto selection=read_original_interaction_response_selection(common,s,cat,ref,schema);if(!selection.ok)return selection;const auto*r=selection.value;
 auto opened=origin(common,s,cat,r->session_id);if(!opened.ok)return {false,nullptr,opened.error};Status valid=common_success();
 if(r->tail_consumed_result_ref&&!same(*r->tail_consumed_result_ref,r->committed_result_ref)){
  auto death=validate_interaction_response_death_consumption_source(common,s,cat,*r,*r->tail_consumed_result_ref);if(!death.ok)return {false,nullptr,death.error};
  if(!death.value){valid=consumed_boundary_receipt(common,*r,*opened.value.actor,*r->tail_consumed_result_ref,schema);if(!valid.ok)return {false,nullptr,valid.error};}
 }
 if(!r->tail_consumed_result_ref){
  if(pending){auto captured=pending->validate_original_pending_response(common,s,cat,*r);if(!captured.ok)return {false,nullptr,captured.error};if(captured.value)return {true,r,{}};}
  const auto*b=s.response_sources->session_boundaries.find(r->session_id);const auto*t=s.response_sources->selected_response_tails.find(r->session_id);
  const auto*current=session(s,*opened.value.actor);auto target=choice_next(cat,opened.value,*r);if(!target.ok)return {false,nullptr,target.error};
  if(!b||!t||b->generation!=r->session_generation||!b->current_response_fact_ref||!same(*b->current_response_fact_ref,ref)||t->session_id!=r->session_id||t->session_generation!=r->session_generation||!same(t->response_fact_ref,ref)||!same_interaction_response_next(t->next_node_or_terminal,r->next_node_or_terminal)||!current||current->id!=r->session_id||current->phase!="stepping"||current->node_id!=target.value->id||current->pending_command||!current->pending_action_id.empty())return {false,nullptr,common_failure("InteractionSelectedTailOriginalBoundaryMismatch",ref.id).error};
 }
 return {true,r,{}};
}
 }
Outcome<const InteractionResponseSource*> read_interaction_response_source(const CommonStore&common,const InteractionState&s,const InteractionCatalog&cat,const SourceRef&ref,Integer schema){return read_response(common,s,cat,ref,schema,nullptr);}
Outcome<const InteractionResponseSource*> InteractionSelectedResponsePending::read_committed_response(const CommonStore&c,const InteractionState&s,const InteractionCatalog&cat)const{
 if(!bound_to(c,s,cat))return {false,nullptr,common_failure("InteractionResponseCaptureMismatch",{},ErrorKind::internal).error};
 if(!has_response())return {true,nullptr,{}};
 if(!consumed_)return {false,nullptr,common_failure("InteractionResponseActualFinalizationRequired",response_.response_fact_id,ErrorKind::unsupported_capability).error};
 auto actual=read_interaction_response_source(c,s,cat,interaction_response_source(response_.response_fact_id),interaction_response_source_schema_version(cat.response_sources_version));if(!actual.ok)return actual;
 if(!actual.value->tail_consumed_result_ref)return {false,nullptr,common_failure("InteractionResponseActualFinalizationRequired",response_.response_fact_id,ErrorKind::internal).error};return actual;
}
Outcome<const SelectedResponseTail*> read_interaction_selected_response_tail(const CommonStore&c,const InteractionState&s,const InteractionCatalog&cat,const StableId&id){
 auto valid=selected(s,cat);if(!valid.ok)return {false,nullptr,valid.error};
 const auto*tail=s.response_sources->selected_response_tails.find(id);if(!tail)return {true,nullptr,{}};
 auto actual=read_interaction_response_source(c,s,cat,tail->response_fact_ref,interaction_response_source_schema_version(cat.response_sources_version));if(!actual.ok)return {false,nullptr,actual.error};
 if(actual.value->session_id!=id||actual.value->tail_consumed_result_ref)return {false,nullptr,common_failure("InteractionSelectedTailOriginalBoundaryMismatch",id).error};return {true,tail,{}};
}
namespace {
Status validate_response_sources(const CommonStore&common,const InteractionState&s,const InteractionCatalog&cat,const InteractionSessionDeathPending*pending){
 auto valid=selected(s,cat);if(!valid.ok)return valid;const auto&state=*s.response_sources;
 for(const auto&entry:state.life_sources.rows()){if(entry.first!=entry.second.effect.result_id||!state.session_boundaries.find(entry.second.session_id))return common_failure("InteractionSessionLifeBoundaryMismatch",entry.first);auto shape=validate_s3_leaf_life_effect_shape(entry.second.effect,0);if(!shape.ok)return shape;} // Complete Family4/cause/coverage remains with its original life context.
 for(const auto&entry:state.session_boundaries.rows()){
  const auto&b=entry.second;if(entry.first!=b.session_id||b.generation<1)return common_failure("InvalidInteractionSessionBoundary",entry.first);
  auto opened=origin(common,s,cat,b.session_id);if(!opened.ok)return {false,{},opened.error};Integer generation=1,last_revision=0;const InteractionResponseSource*last=nullptr;const InteractionResponseSource*active=nullptr;bool death_closed=false;
  for(const auto&row:state.response_sources.rows())if(row.second.session_id==b.session_id){
   if(active||death_closed||row.first!=row.second.response_fact_id||row.second.session_generation!=generation)return common_failure("InteractionResponseGenerationMismatch",row.first);
   auto actual=read_response(common,s,cat,interaction_response_source(row.first),interaction_response_source_schema_version(cat.response_sources_version),pending);if(!actual.ok)return {false,{},actual.error};auto result=read_common_command_source(common,row.second.committed_result_ref);if(!result.ok)return {false,{},result.error};
   if(result.value.result->revision<=last_revision)return common_failure("InteractionResponseOriginalResultOrderMismatch",row.first);last_revision=result.value.result->revision;
   if(row.second.tail_consumed_result_ref){
    if(command_ref(*row.second.tail_consumed_result_ref)){auto consumed=read_common_command_source(common,*row.second.tail_consumed_result_ref);if(!consumed.ok)return {false,{},consumed.error};if(consumed.value.result->revision<last_revision)return common_failure("InteractionResponseConsumptionOrderMismatch",row.first);last_revision=consumed.value.result->revision;}
    else { // read_response already proved the exact original death-close join.
     if(interaction_response_source_schema_version(cat.response_sources_version)!=2)return common_failure("InteractionResponseConsumedResultRequired",row.first);death_closed=true;
    }
    if(generation==std::numeric_limits<Integer>::max())return common_failure("InteractionResponseGenerationOverflow",row.first);++generation;last=&row.second;
   }else active=&row.second;
  }
  if(b.generation!=generation||bool(last)!=bool(b.last_consumed_response_fact_ref)||(last&&(!response_ref(*b.last_consumed_response_fact_ref)||b.last_consumed_response_fact_ref->id!=last->response_fact_id))||bool(active)!=bool(b.current_response_fact_ref)||(active&&(!response_ref(*b.current_response_fact_ref)||b.current_response_fact_ref->id!=active->response_fact_id))||bool(active)!=bool(state.selected_response_tails.find(b.session_id)))return common_failure("InteractionResponseBoundaryCoverageMismatch",entry.first);
 }
 for(const auto&entry:state.selected_response_tails.rows()){if(entry.first!=entry.second.session_id||!state.session_boundaries.find(entry.first))return common_failure("InteractionSelectedTailBoundaryMissing",entry.first);auto actual=read_response(common,s,cat,entry.second.response_fact_ref,interaction_response_source_schema_version(cat.response_sources_version),pending);if(!actual.ok)return {false,{},actual.error};if(actual.value->session_id!=entry.first||actual.value->tail_consumed_result_ref)return common_failure("InteractionSelectedTailOriginalBoundaryMismatch",entry.first);}
 for(const auto&row:state.response_sources.rows()){if(!state.session_boundaries.find(row.second.session_id))return common_failure("InteractionResponseBoundaryMissing",row.first);for(const auto&prior:state.response_sources.rows()){if(prior.first==row.first)break;if(same(prior.second.committed_result_ref,row.second.committed_result_ref))return common_failure("DuplicateInteractionResponseCommit",row.first);}}
 if(s.social_occupancy&&s.social_occupancy->version==3)for(const auto&entry:s.social_occupancy->transitions.rows())if(entry.second.session_opening&&!state.session_boundaries.find(entry.second.session_opening->session_id))return common_failure("InteractionOriginalOpeningBoundaryMissing",entry.first);
 for(const auto&v:s.interactions)if(!state.session_boundaries.find(v.id))return common_failure("InteractionLiveSessionBoundaryMissing",v.id);
 return validate_interaction_action_confirmation_sources(common,s,cat);
}
}
Status validate_interaction_response_sources(const CommonStore&c,const InteractionState&s,const InteractionCatalog&cat){return validate_response_sources(c,s,cat,nullptr);}
Status validate_interaction_response_sources_in_death_capture(const CommonStore&c,const InteractionState&s,const InteractionCatalog&cat,const InteractionSessionDeathPending&pending){
 if(!pending.bound_to(c,s,cat))return common_failure("InteractionDeathTailCaptureMismatch",{},ErrorKind::internal);
 return validate_response_sources(c,s,cat,&pending);
}
Outcome<OriginalInteractionContinuationBoundary> read_original_interaction_continuation_boundary(const CommonStore&c,const InteractionState&s,const InteractionCatalog&cat,const InteractionAccess&access,const StableId&actor){
 auto valid=validate_interaction_response_sources(c,s,cat);if(!valid.ok)return {false,{},valid.error};
 valid=validate_interaction_state(c,s,cat,access);if(!valid.ok)return {false,{},valid.error};
 const auto*person=c.actor(actor);if(!person||!person->alive)return {false,{},common_failure("InteractionContinuationCandidateUnavailable",actor).error};
 const auto*current=session(s,actor);if(!current)return {true,{}, {}};
 auto opened=origin(c,s,cat,current->id);if(!opened.ok)return {false,{},opened.error};
 const auto*provider=c.actor(*opened.value.provider);if(*opened.value.actor!=actor||!provider||!provider->alive)return {false,{},common_failure("InteractionContinuationParticipantUnavailable",current->id).error};
 const auto*boundary=s.response_sources->session_boundaries.find(current->id);if(!boundary)return {false,{},common_failure("InteractionContinuationBoundaryMissing",current->id,ErrorKind::internal).error};
 auto tail=read_interaction_selected_response_tail(c,s,cat,current->id);if(!tail.ok)return {false,{},tail.error};
 // Waiting is an existing original long Action, already checked by the original
 // state validator. This projection neither waits for nor completes that work.
 return {true,{current,boundary,tail.value},{}};
}
Status visit_interaction_response_source_pins(const CommonStore&common,const InteractionState&s,const InteractionCatalog&cat,InteractionResponseSourcePinSink&sink){
 auto valid=validate_interaction_response_sources(common,s,cat);if(!valid.ok)return valid;
 for(const auto&entry:s.response_sources->session_boundaries.rows()){auto opened=origin(common,s,cat,entry.first);if(!opened.ok)return {false,{},opened.error};const auto&h=*opened.value.header;valid=sink.original_source({h.domain,h.kind,h.id},1);if(!valid.ok)return valid;}
 for(const auto&entry:s.response_sources->response_sources.rows()){const auto&r=entry.second;valid=sink.original_source(r.committed_result_ref,1);if(!valid.ok)return valid;if(r.tail_consumed_result_ref){valid=sink.original_source(*r.tail_consumed_result_ref,1);if(!valid.ok)return valid;}}
 return common_success();
}
Integer interaction_action_confirmation_schema_version(Integer v)noexcept{return v==11||v==12||v==13?1:0;}
namespace {
Status confirmation_selected(const InteractionState&s,const InteractionCatalog&cat){
 auto valid=selected(s,cat);if(!valid.ok)return valid;
 if(interaction_action_confirmation_schema_version(cat.response_sources_version)!=1||!s.social_occupancy||s.social_occupancy->version!=3)return common_failure("InteractionActionConfirmationSourcesRequired",{},ErrorKind::unsupported_capability);
 return common_success();
}
bool exact_confirmation_context(const InteractionContext&a,const InteractionContext&b){return a.place_id==b.place_id&&a.provider_actor_id==b.provider_actor_id&&a.role_id==b.role_id&&a.kind==b.kind&&a.encounter_id==b.encounter_id&&a.input_error==b.input_error&&a.field_order==b.field_order;}
bool exact_confirmation_command(const DialogueCommand&a,const DialogueCommand&b){return same_dialogue_command(a,b)&&a.field_order==b.field_order&&(!a.context||exact_confirmation_context(*a.context,*b.context));}
Outcome<const DialogueNode*> confirmation_node(const InteractionCatalog&cat,const StableId&dialogue,const StableId&node){
 const DialogueDefinition*definition=nullptr;for(const auto&d:cat.dialogues)if(d.id==dialogue){if(definition)return {false,nullptr,common_failure("DuplicateInteractionConfirmationDialogue",dialogue).error};definition=&d;}
 const DialogueNode*out=nullptr;if(definition)for(const auto&n:definition->nodes)if(n.id==node){if(out)return {false,nullptr,common_failure("DuplicateInteractionConfirmationNode",node).error};out=&n;}
 if(!out||out->kind!="action"||!out->command||out->command->kind.empty()||out->command->kind=="dialogue_confirm")return {false,nullptr,common_failure("InteractionOriginalConfirmationNodeRequired",node).error};return {true,out,{}};
}
InteractionActionConfirmationBuildView confirmation_view(const InteractionActionConfirmationSource&r,const StableId&result,const StableId&action){return {result,r.session_id,r.session_generation,r.dialogue_id,r.action_node_id,r.confirmed_command,action,r.confirmation_at};}
}
Status validate_interaction_action_confirmation_shape(const InteractionActionConfirmationSource&r){
 if(r.committed_result_id.empty()||r.session_id.empty()||r.session_generation<1||r.dialogue_id.empty()||r.action_node_id.empty()||r.confirmed_command.kind.empty()||r.confirmed_command.kind=="dialogue_confirm"||r.action_id.empty()||(r.confirmed_command.context&&r.confirmed_command.context->input_error))return common_failure("InvalidInteractionActionConfirmation",r.committed_result_id);
 auto at=compare_world_moments(r.confirmation_at,r.confirmation_at);return at.ok?common_success():Status{false,{},at.error};
}
Outcome<std::unique_ptr<InteractionActionConfirmationPending>> InteractionActionConfirmationPending::create(CommonStore&common,InteractionState&s,const InteractionCatalog&cat){
 auto valid=confirmation_selected(s,cat);if(!valid.ok)return {false,{},valid.error};
 try{return {true,std::unique_ptr<InteractionActionConfirmationPending>(new InteractionActionConfirmationPending(common,s,cat)),{}};}catch(const std::bad_alloc&){return {false,{},common_failure("InteractionConfirmationCaptureResourceExhausted",{},ErrorKind::internal).error};}
}
bool InteractionActionConfirmationPending::bound_to(const CommonStore&c,const InteractionState&s,const InteractionCatalog&cat)const noexcept{return common_==&c&&state_==&s&&catalog_==&cat;}
Status prepare_interaction_action_confirmation_in_candidate(CommonStore&common,InteractionState&s,const InteractionCatalog&cat,const StableId&actor,const WorldMoment&at,InteractionActionConfirmationPending&pending){
 auto valid=confirmation_selected(s,cat);if(!valid.ok)return valid;
 if(!pending.bound_to(common,s,cat)||pending.prepared_||pending.execution_recorded_||pending.adopted_)return common_failure("InteractionConfirmationCaptureMismatch",actor,ErrorKind::internal);
 const auto*current=session(s,actor);if(!current||current->phase!="action"||!current->pending_command||!current->pending_action_id.empty())return common_failure("NoPendingDialogueAction",actor);
 // A regional runtime TaskCommand is not this authored DialogueCommand. Its
 // durable confirmation join is separate and remains unsupported here.
 if(current->regional_binding&&current->regional_binding->task_command)return common_success();
 valid=validate_interaction_response_sources(common,s,cat);if(!valid.ok)return valid;
 auto opening=read_original_interaction_session_opening(common,s,cat,current->id);if(!opening.ok)return {false,{},opening.error};
 const auto&o=*opening.value->session_opening;
 if(o.actor_id!=actor||o.dialogue_id!=current->dialogue_id||!exact_confirmation_context(o.context,current->context))return common_failure("InteractionConfirmationOpeningMismatch",current->id);
 auto node=confirmation_node(cat,current->dialogue_id,current->node_id);if(!node.ok)return {false,{},node.error};
 if(!exact_confirmation_command(*node.value->command,*current->pending_command))return common_failure("DialogueCommandMismatch",current->id);
 const auto*b=s.response_sources->session_boundaries.find(current->id);
 if(!b||b->generation<1||b->current_response_fact_ref||s.response_sources->selected_response_tails.find(current->id))return common_failure("InteractionConfirmationSafeBoundaryRequired",current->id);
 auto canonical=compare_world_moments(at,at);if(!canonical.ok)return {false,{},canonical.error};auto opened=compare_world_moments(opening.value->occurred_at,at);if(!opened.ok)return {false,{},opened.error};
 if(at.half_day_tick!=common.tick()||opened.value>0||common.revision()==std::numeric_limits<Integer>::max())return common_failure("InteractionConfirmationActualMomentMismatch",current->id);
 if(s.response_sources->action_confirmation_sources.size()>=CodecVisitor::max_items)return common_failure("InteractionConfirmationSourceLimit",current->id);
 const StableId empty;pending.prepared_.emplace(InteractionActionConfirmationBuildView{empty,current->id,b->generation,current->dialogue_id,current->node_id,*current->pending_command,empty,at});pending.original_revision_=common.revision();return common_success();
}
Status record_interaction_action_confirmation_produced_in_candidate(CommonStore&common,InteractionState&s,const InteractionCatalog&cat,InteractionActionConfirmationPending&pending,const ReceiptDetails&receipt){
 if(!pending.bound_to(common,s,cat)||pending.execution_recorded_||pending.adopted_)return common_failure("InteractionConfirmationCaptureMismatch",{},ErrorKind::internal);
 auto valid=confirmation_selected(s,cat);if(!valid.ok)return valid;
 if(!pending.prepared_){pending.execution_recorded_=true;return common_success();}
 if(common.revision()!=pending.original_revision_)return common_failure("InteractionConfirmationNestedSealForbidden",{},ErrorKind::internal);
 const auto*started=std::get_if<ActionStarted>(&receipt.data);if(!started){pending.prepared_.reset();pending.execution_recorded_=true;return common_success();}
 auto opening=read_original_interaction_session_opening(common,s,cat,pending.prepared_->session_id);if(!opening.ok)return {false,{},opening.error};
 const auto*action=common.action(started->action_id);if(!action||action->id!=started->action_id||action->kind!=pending.prepared_->confirmed_command.kind||action->actor_id!=opening.value->actor_id||action->start_tick!=pending.prepared_->confirmation_at.half_day_tick)return common_failure("InteractionActualNestedActionRequired",started->action_id);
 InteractionActionConfirmationSource produced{confirmation_view(*pending.prepared_,pending.prepared_->committed_result_id,started->action_id)};
 pending.prepared_->swap(produced);pending.execution_recorded_=true;return common_success();
}
Outcome<OriginalInteractionActionConfirmation> read_original_interaction_action_confirmation(const CommonStore&common,const InteractionState&s,const InteractionCatalog&cat,const SourceRef&ref,const StableId&action){
 if(!command_ref(ref))return {false,{},common_failure("InteractionActualConfirmationResultRequired",ref.id).error};return read_original_interaction_action_confirmation(common,s,cat,ref.id,action);
}
namespace {
Outcome<OriginalInteractionActionConfirmation> read_confirmation(const CommonStore&common,const InteractionState&s,const InteractionCatalog&cat,const StableId&result_id,const StableId&action,InteractionActionConfirmationPinSink*sink){
 auto pin=[&](InteractionActionConfirmationPinRole role,const SourceRefBuildView&source,Integer schema){return sink?sink->original_source(role,source,schema):common_success();};
 auto valid=confirmation_selected(s,cat);if(!valid.ok)return {false,{},valid.error};
 const auto*r=s.response_sources->action_confirmation_sources.find(result_id);if(!r||r->committed_result_id!=result_id||r->action_id!=action)return {false,{},common_failure("InteractionOriginalConfirmationSourceMissing",result_id).error};
 valid=validate_interaction_action_confirmation_shape(*r);if(!valid.ok)return {false,{},valid.error};
 auto actual=read_common_command_source(common,std::string_view(result_id));if(!actual.ok)return {false,{},actual.error};const auto&sealed=*actual.value.result;
 const auto*started=sealed.details?std::get_if<ActionStarted>(&sealed.details->data):nullptr;
 if(sealed.kind!="dialogue_confirm"||!started||started->action_id!=r->action_id||sealed.tick!=r->confirmation_at.half_day_tick)return {false,{},common_failure("InteractionOriginalActionStartedSealRequired",result_id).error};
 valid=pin(InteractionActionConfirmationPinRole::outer_result,{"common","command_result",result_id},1);if(!valid.ok)return {false,{},valid.error};
 auto opening=read_original_interaction_session_opening(common,s,cat,r->session_id);if(!opening.ok)return {false,{},opening.error};const auto&o=*opening.value->session_opening;
 if(o.actor_id!=actual.value.history->actor_id||o.dialogue_id!=r->dialogue_id)return {false,{},common_failure("InteractionConfirmationOpeningMismatch",result_id).error};
 valid=pin(InteractionActionConfirmationPinRole::session_opening,{"interaction","actor_social_occupancy_changed",opening.value->id},1);if(!valid.ok)return {false,{},valid.error};
 auto node=confirmation_node(cat,r->dialogue_id,r->action_node_id);if(!node.ok)return {false,{},node.error};if(!exact_confirmation_command(*node.value->command,r->confirmed_command))return {false,{},common_failure("InteractionConfirmationOriginalCommandMismatch",result_id).error};
 auto opened_at=compare_world_moments(opening.value->occurred_at,r->confirmation_at);if(!opened_at.ok)return {false,{},opened_at.error};if(opened_at.value>0)return {false,{},common_failure("InteractionConfirmationBeforeOpening",result_id).error};
 const auto limit=common.state().next_instance_id;const auto session_order=allocated(r->session_id,"interaction:",limit),opening_order=allocated(opening.value->id,"social_occupancy:",limit),action_order=allocated(r->action_id,"action:",limit),seal_order=allocated(result_id,"result:",limit);
 if(!session_order||!opening_order||!action_order||!seal_order||session_order>=opening_order||opening_order>=action_order||action_order>=seal_order)return {false,{},common_failure("InteractionConfirmationOriginalAllocationOrderMismatch",result_id).error};
 // An original close after this action began is allowed. An earlier close
 // cannot be repaired by a matching command in another/current session.
 for(const auto&entry:s.social_occupancy->transitions.rows()){
  const auto&t=entry.second;if(t.actor_id!=o.actor_id||!t.before.interaction_id||*t.before.interaction_id!=r->session_id||(t.after.interaction_id&&*t.after.interaction_id==r->session_id))continue;
  const auto closed=allocated(t.id,"social_occupancy:",limit);const auto*header=common.state().facts.find(t.id);
  if(entry.first!=t.id||!header||header->id!=t.id||header->domain!="interaction"||header->kind!="actor_social_occupancy_changed"||!closed||closed<action_order)return {false,{},common_failure("InteractionConfirmationAfterSessionClosed",result_id).error};
  auto actual_close=read_actual_common_fact_position(common,*header);if(!actual_close.ok)return {false,{},actual_close.error};auto close_at=compare_world_moments(t.occurred_at,r->confirmation_at);if(!close_at.ok)return {false,{},close_at.error};if(close_at.value<0)return {false,{},common_failure("InteractionConfirmationAfterSessionClosed",result_id).error};
  valid=pin(InteractionActionConfirmationPinRole::checked_session_close,{header->domain,header->kind,header->id},1);if(!valid.ok)return {false,{},valid.error};
 }
 const auto*b=s.response_sources->session_boundaries.find(r->session_id);if(!b)return {false,{},common_failure("InteractionConfirmationBoundaryMissing",result_id).error};
 Integer generation=1;
 for(const auto&entry:s.response_sources->response_sources.rows()){
  const auto&response=entry.second;if(response.session_id!=r->session_id)continue;
  // The actual immutable response leaf is checked even when its later commit
  // is outside this confirmation's historical generation cut. Emit every
  // checked direct edge, never an invented earlier-consumption relationship.
  const auto schema=interaction_response_source_schema_version(cat.response_sources_version);
  auto ref_memory=CheckpointMemoryCharge(0);
  auto selection=read_original_interaction_response_selection(common,s,cat,interaction_response_source(entry.first),schema);if(!selection.ok)return {false,{},selection.error};
  valid=pin(InteractionActionConfirmationPinRole::checked_response,{"interaction","response_selected",entry.first},schema);if(!valid.ok)return {false,{},valid.error};
  auto chosen=read_common_command_source(common,response.committed_result_ref);if(!chosen.ok)return {false,{},chosen.error};
  valid=pin(InteractionActionConfirmationPinRole::checked_response_commit,{response.committed_result_ref.domain,response.committed_result_ref.kind,response.committed_result_ref.id},1);if(!valid.ok)return {false,{},valid.error};
  if(chosen.value.result->revision>=sealed.revision)continue;
  if(response.session_generation!=generation||!response.tail_consumed_result_ref||!command_ref(*response.tail_consumed_result_ref))return {false,{},common_failure("InteractionConfirmationSelectedTailNotConsumed",result_id).error};auto consumed=read_common_command_source(common,*response.tail_consumed_result_ref);if(!consumed.ok)return {false,{},consumed.error};if(!same(*response.tail_consumed_result_ref,response.committed_result_ref)){valid=consumed_boundary_receipt(common,response,o.actor_id,*response.tail_consumed_result_ref,schema);if(!valid.ok)return {false,{},valid.error};}if(consumed.value.result->revision<chosen.value.result->revision||consumed.value.result->revision>=sealed.revision||generation==std::numeric_limits<Integer>::max())return {false,{},common_failure("InteractionConfirmationGenerationMismatch",result_id).error};valid=pin(InteractionActionConfirmationPinRole::earlier_response_consumption,{response.tail_consumed_result_ref->domain,response.tail_consumed_result_ref->kind,response.tail_consumed_result_ref->id},1);if(!valid.ok)return {false,{},valid.error};++generation;
 }
 if(generation!=r->session_generation||b->generation<generation)return {false,{},common_failure("InteractionConfirmationGenerationMismatch",result_id).error};
 return {true,{r,opening.value},{}};
}
}
Outcome<OriginalInteractionActionConfirmation> read_original_interaction_action_confirmation(const CommonStore&common,const InteractionState&s,const InteractionCatalog&cat,const StableId&result_id,const StableId&action){return read_confirmation(common,s,cat,result_id,action,nullptr);}
Status visit_interaction_action_confirmation_pins(const CommonStore&common,const InteractionState&s,const InteractionCatalog&cat,const InteractionActionConfirmationSource&row,InteractionActionConfirmationPinSink&sink){
 if(!s.response_sources||s.response_sources->action_confirmation_sources.find(row.committed_result_id)!=&row)return common_failure("InteractionActualBorrowedConfirmationRequired",row.committed_result_id);
 auto original=read_confirmation(common,s,cat,row.committed_result_id,row.action_id,&sink);return original.ok?common_success():Status{false,{},original.error};
}
Status bind_interaction_action_confirmation_result_in_candidate(CommonStore&common,InteractionState&s,const InteractionCatalog&cat,InteractionActionConfirmationPending&pending,const SourceRef&ref){
 if(!pending.bound_to(common,s,cat))return common_failure("InteractionConfirmationCaptureMismatch",ref.id,ErrorKind::internal);
 if(!pending.prepared_)return common_success();
 if(!pending.execution_recorded_||pending.prepared_->action_id.empty())return common_failure("InteractionConfirmationActualExecutionRequired",ref.id,ErrorKind::internal);
 auto valid=confirmation_selected(s,cat);if(!valid.ok)return valid;auto actual=read_common_command_source(common,ref);if(!actual.ok)return {false,{},actual.error};
 auto opening=read_original_interaction_session_opening(common,s,cat,pending.prepared_->session_id);if(!opening.ok)return {false,{},opening.error};
 const auto&sealed=*actual.value.result;const auto*started=sealed.details?std::get_if<ActionStarted>(&sealed.details->data):nullptr;
 if(sealed.kind!="dialogue_confirm"||!started||started->action_id!=pending.prepared_->action_id||actual.value.history->actor_id!=opening.value->actor_id||sealed.tick!=pending.prepared_->confirmation_at.half_day_tick||sealed.revision!=pending.original_revision_+1||common.revision()!=sealed.revision)return common_failure("InteractionOriginalActionStartedSealRequired",ref.id);
 if(pending.adopted_){auto prior=read_original_interaction_action_confirmation(common,s,cat,ref,pending.prepared_->action_id);return prior.ok?common_success():Status{false,{},prior.error};}
 auto&rows=s.response_sources->action_confirmation_sources;if(rows.size()>=CodecVisitor::max_items||rows.find(ref.id))return common_failure("DuplicateInteractionConfirmationSource",ref.id);
 for(const auto&row:rows.rows())if(row.second.action_id==pending.prepared_->action_id)return common_failure("DuplicateInteractionConfirmationAction",pending.prepared_->action_id);
 InteractionActionConfirmationSource row{confirmation_view(*pending.prepared_,ref.id,pending.prepared_->action_id)};
 if(!rows.insert(ref.id,std::move(row)))return common_failure("DuplicateInteractionConfirmationSource",ref.id);
 auto retained=read_original_interaction_action_confirmation(common,s,cat,ref,pending.prepared_->action_id);if(!retained.ok)return {false,{},retained.error};pending.adopted_=true;return common_success();
}
Status validate_interaction_action_confirmation_sources(const CommonStore&common,const InteractionState&s,const InteractionCatalog&cat){
 if(!s.response_sources)return common_failure("InteractionResponseSourcesRequired");
 const auto&rows=s.response_sources->action_confirmation_sources;
 if(!interaction_action_confirmation_schema_version(cat.response_sources_version))return rows.size()==0?common_success():common_failure("InteractionActionConfirmationExplicitVersionRequired",{},ErrorKind::unsupported_capability);
 auto selected_source=confirmation_selected(s,cat);if(!selected_source.ok)return selected_source;
 for(const auto&row:rows.rows()){if(row.first!=row.second.committed_result_id)return common_failure("InteractionConfirmationKeyMismatch",row.first);auto original=read_original_interaction_action_confirmation(common,s,cat,row.first,row.second.action_id);if(!original.ok)return {false,{},original.error};for(const auto&prior:rows.rows()){if(prior.first==row.first)break;if(prior.second.action_id==row.second.action_id)return common_failure("DuplicateInteractionConfirmationAction",row.first);}}
 return common_success();
}

}
