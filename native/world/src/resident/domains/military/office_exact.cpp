// C04 same original Office writer, one real S0 H=1 entry, one candidate.
#include "office_exact_detail.hpp"
#include "military_action_original_rules.hpp"
#include "military_equal.hpp"
#include "resident/kernel/fixed_service_runtime.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <type_traits>
#include "resident/model/common_operations.hpp"
#include "military_action_completion_build.hpp"
#include "military_action_conditions.hpp"
namespace hanmo::resident {using namespace office_exact_detail;namespace {
static_assert(std::is_nothrow_move_constructible_v<FixedAppliedInput>,"original Office applied input transfer must not copy payload");
}
Outcome<StableId> start_exact_office_action_in_candidate(CommonStore& c,MilitaryState& military,MilitaryActionTimeState& state,ActionElapsedStore& elapsed,const ExactOfficeWriteContext& context,const ActionCoreView& action,const OfficeAccept& request,const MilitaryQuote& offer){return guard<StableId>([&]{
 const auto& x=context.read;bound(c,military,state,x);if(!context.release_action||action.id.empty()||action.kind!="office_accept")fail("ExactOfficeStartIdentityMismatch",action.id);
 if(const auto* old=state.dynamic_action_facts.find(action.id)){
  const auto& f=fact(state,action.id);const auto& terms=std::get<OfficeTerms>(f.original_terms);if(f.actor_id!=action.actor_id||terms.office_id!=request.office_id||!f.original_context||!same_context(*f.original_context,request.context)||!offer.terms||!same_terms(f.original_terms,*offer.terms)||!same_context(f.original_context,offer.context))fail("ExactOfficeReplayPayloadMismatch",action.id);
  take(validate_exact_office_sources(c,military,state,elapsed,x));return f.id;
 }
 for(const auto& old:military.action_facts)if(old.id==action.id)fail("DuplicateLegacyExactMilitaryAction",action.id);
 for(const auto& old:military.actions)if(old.action_id==action.id)fail("DuplicateLegacyExactMilitaryExecution",action.id);
 if(find_fixed_elapsed(elapsed,action.id))fail("DuplicateExactOfficeElapsed",action.id);
 for(const auto& old:state.dynamic_action_facts.rows())if(old.second.command=="office_accept"&&old.second.phase=="started"){
  const auto* accepted=std::get_if<OfficeTerms>(&old.second.original_terms);if(!accepted)fail("ExactOfficeOriginalTermsMismatch",old.first);if(accepted->office_id==request.office_id)fail("OfficeAppointmentReserved",request.office_id);
 }
 const auto at=take(x.clock.read_current_moment());const auto* actual=c.action(action.id);
 if(!actual||actual->id!=action.id||actual->kind!=action.kind||actual->actor_id!=action.actor_id||actual->participants!=action.participants||actual->remaining_ticks!=1||actual->start_tick!=at.half_day_tick)fail("ExactOfficeActualActionRequired",action.id);
 auto office_acceptance_observation=take(capture_military_action_endpoint_observation(c,state,x.access,*actual,MilitaryActionConditionEndpoint2::acceptance));
 auto quote=take(preview_military(c,military,x.catalog,x.access,action.actor_id,MilitaryCommand{request},action.id));
 if(!quote.terms||!std::holds_alternative<OfficeTerms>(*quote.terms)||quote.duration!=1||!same_quote(offer,quote)||actual->participants!=quote.participants)fail("StaleMilitaryQuote",action.id);
 for(const auto& id:actual->participants){const auto* actor=c.actor(id);if(!actor||actor->action_id!=action.id)fail("MilitaryOccupationMissing",id);}
 if(office_acceptance_observation)take(validate_military_action_endpoint_observation_unchanged(c,state,x.access,*actual,MilitaryActionConditionEndpoint2::acceptance,*office_acceptance_observation));

 auto office_elapsed_ref_memory=CheckpointMemoryCharge(0);
 const ActionElapsedEntryRef office_elapsed_ref{action.id};
 const std::optional<WorldMoment> office_no_completed_at;
 const std::optional<MilitaryActionResultBuildView> office_no_result;
 const std::optional<SourceRefBuildView> office_no_terminal;
 const std::optional<MilitaryActionResourceEffect1> office_no_resource_effect;
 const std::optional<MilitaryActionAcceptanceConditions1> office_no_acceptance_conditions;
 const std::optional<MilitaryFormationReservations1> office_no_reservations;
 PreparedMilitaryActionCompletion2 office_started_fact(DynamicMilitaryActionFactBuildView2{action.id,action.kind,action.actor_id,at,office_no_completed_at,"started",false,*quote.terms,quote.context,office_elapsed_ref,office_no_result,office_no_terminal,2,office_no_resource_effect,office_no_acceptance_conditions,office_no_reservations,&office_acceptance_observation,nullptr});
 const auto& f=office_started_fact.row;
 state.dynamic_action_facts.set(f.id,f);take(c.append_fact_borrowed(f.id,"military_action","military"));validate_original_role_source(c,x,f,false);take(x.original.validate_office_acceptance(f));
 validate_original_interaction_availability(c,military,x,f,false);
 auto entry=take(begin_fixed_elapsed(f.id,f.id,"office_accept_original_1_half_day_v2",f.id,at,1,x.scale,x.budget));auto office_transition_id=take(c.next_owned_id("fixed_service_transition"));
 const SourceRefBuildView office_start_source{"military","military_action",f.id};

 auto office_basis_memory=CheckpointMemoryCharge(0);
 std::vector<FixedServiceInputRef> office_basis;office_basis.reserve(1);office_basis.emplace_back(FixedAppliedInput{SourceRef{"military","military_action",f.id},2});
 const std::vector<FixedServiceInputRef> office_empty_inputs;
 const WorkUnits office_zero{};
 FixedServiceTransitionInput transition(FixedServiceAdoptionTransitionBuildView{office_transition_id.id(),1,f.id,0,{},FixedTransitionKind::started,f.started_at,office_zero,office_start_source,2,{office_start_source,2},office_basis,office_empty_inputs,{}});NativeOfficeActionTimeRead reader(c,military,state,elapsed,x);
 auto prepared=take(prepare_fixed_service_adoption(*actual,entry,transition.row(),exact_office_action_registration(),x.scale,reader,x.budget));take(apply_fixed_service_change_in_candidate(elapsed,std::move(prepared),exact_office_action_registration(),x.scale,reader,x.budget));return f.id;
});}
Status advance_exact_office_action_in_candidate(CommonStore& c,MilitaryState& military,MilitaryActionTimeState& state,ActionElapsedStore& elapsed,const ExactOfficeWriteContext& context,const StableId& id,const MomentSpan& span){return status([&]{
 const auto& x=context.read;bound(c,military,state,x);const auto& f=fact(state,id);auto* e=find_fixed_elapsed(elapsed,id);
 if(f.phase!="started"||!e||!std::holds_alternative<ElapsedRunning>(e->timing)||!same_moment(e->last_settled_at,span.from,x.budget)||!same_moment(take(x.clock.read_current_moment()),span.to,x.budget)||take(compare_world_moments(span.to,span.from,x.budget))<=0||take(compare_world_moments(span.to,due(f,x.budget),x.budget))>0)fail("ExactOfficeInvalidActualSpan",id);
 take(x.original.validate_office_actual_service(f,span));auto measured=take(prepare_fixed_elapsed_slice(*e,span,x.scale,x.budget));e->credited_units=std::move(measured.credited_units);e->last_settled_at=span.to;
 if(!measured.due){ActionMutation projection;projection.action.id=id;projection.remaining_ticks=take(fixed_elapsed_remaining_ticks(*e,x.scale,x.budget));take(c.apply_action(projection));}
});}
Outcome<MilitaryEffect> complete_exact_office_action_in_candidate(CommonStore& c,MilitaryState& military,MilitaryActionTimeState& state,ActionElapsedStore& elapsed,const ExactOfficeWriteContext& context,const StableId& id){return guard<MilitaryEffect>([&]{
 const auto& x=context.read;bound(c,military,state,x);auto& f=fact(state,id);if(f.phase=="completed"){auto office_replay_source_memory=CheckpointMemoryCharge(0);const auto office_replay_source=completed_ref(id);take(read_verified_exact_office_result(c,military,state,elapsed,x,office_replay_source,2));return receipt(f,true);}
 const auto at=take(x.clock.read_current_moment());if(!context.release_action||!same_moment(at,due(f,x.budget),x.budget))fail("ExactOfficeCompletionGateRequired",id);
 const auto completion=take(read_fixed_service_completion(elapsed,f.elapsed_source_ref,at,x.scale,x.budget));if(!same_moment(completion.started_at,f.started_at,x.budget)||!same_moment(completion.completed_at,at,x.budget)||!completion.actual_service.complete)fail("ExactOfficeFullActualServiceRequired",id);
 const auto* e=find_fixed_elapsed(elapsed,id);if(!e)fail("ExactOfficeElapsedRequired",id);take(validate_fact_history(c,military,state,x,f,*e,true));const auto* actual=c.action(id);
 const auto office_actual_participants_match=[&]{const auto refs=participant_refs(f);if(actual->participants.size()!=refs.size())return false;for(std::size_t n=0;n<refs.size();++n)if(actual->participants[n]!=*refs[n])return false;return true;};
 if(!actual||actual->kind!=f.command||actual->actor_id!=f.actor_id||!office_actual_participants_match())fail("ExactOfficeActualActionRequired",id);
 auto office_completion_observation=take(capture_military_action_endpoint_observation(c,state,x.access,*actual,MilitaryActionConditionEndpoint2::completion));
 std::optional<MilitaryActionCompletionConditionGuard> office_condition_guard;if(office_completion_observation)office_condition_guard.emplace(c,state,x.access,*actual,*office_completion_observation);
 auto office_completion=take(complete_original_office_action_in_candidate(c,military,x.catalog,x.access,*actual,std::get<OfficeTerms>(f.original_terms),f.original_context,office_condition_guard?&*office_condition_guard:nullptr));
 auto& effect=office_completion.effect;
 take(apply_original_action_resource_release_in_candidate(c,id));
 if(office_completion_observation)take(validate_military_action_endpoint_observation_unchanged(c,state,x.access,*actual,MilitaryActionConditionEndpoint2::completion,*office_completion_observation));
 // The /2 slot is the actual application of this original completed result,
 // including a genuine failed recheck. It is never the earlier start's head.
 const auto application=take(c.next_owned_id("military_office_completion_applied"));
 const auto& application_id=application.id();
 const std::vector<StableId> result_order;const std::optional<WorldMoment> completed_at{at};
 const std::optional<MilitaryActionResultBuildView> result{{effect.details_ok,effect.office_id,effect.army_id,effect.order_id,result_order}};
 const std::optional<SourceRefBuildView> terminal{{"military","military_office_completion_applied",application_id}};
 PreparedMilitaryActionCompletion2 completed({f.id,f.command,f.actor_id,f.started_at,completed_at,"completed",effect.success,f.original_terms,f.original_context,f.elapsed_source_ref,result,terminal,f.rule_version,f.actual_resource_effect,f.acceptance_conditions,f.acceptance_resource_reservations,&f.acceptance_observation,&office_completion_observation});
 // The original table owns its target copy. No borrowed old row survives set.
 state.dynamic_action_facts.set(completed.row.id,completed.row);
 take(c.append_fact_borrowed(application_id,"military_office_completion_applied","military"));
 const auto& adopted=fact(state,completed.row.id);validate_completion_header(c,state,adopted);validate_original_role_source(c,x,adopted,true);
 take(x.original.validate_office_completed_result(adopted));
 if(adopted.success)validate_original_interaction_availability(c,military,x,adopted,true);
 const FixedServiceTransition* office_prior_transition=nullptr;for(const auto& old:elapsed.fixed_service_transitions)if(old.action_ref==adopted.id){if(office_prior_transition)fail("ExactOfficeUnexpectedServiceTransition",adopted.id);office_prior_transition=&old;}
 const auto* office_terminal_entry=find_fixed_elapsed(elapsed,adopted.id);if(!office_prior_transition||!office_terminal_entry||office_prior_transition->kind!=FixedTransitionKind::started||office_prior_transition->transition_ordinal!=0||!adopted.completed_at)fail("ExactOfficeStartedHistoryRequired",adopted.id);
 auto office_transition_id=take(c.next_owned_id("fixed_service_transition"));
 const std::vector<FixedServiceInputRef> office_empty_inputs;
 FixedServiceTransitionInput transition(FixedServiceTerminalTransitionBuildView{office_transition_id.id(),1,adopted.id,1,std::string_view(office_prior_transition->transition_id),FixedTransitionKind::terminal,*adopted.completed_at,office_terminal_entry->credited_units,{"military","military_action",adopted.id},2,{{"military","military_action_completed",adopted.id},2},office_empty_inputs,office_prior_transition->input_basis_refs,adopted.success?FixedTerminalDisposition::completed:FixedTerminalDisposition::interrupted});NativeOfficeActionTimeRead reader(c,military,state,elapsed,x);auto prepared=take(prepare_fixed_service_change(elapsed,completed.row.id,transition.row(),{},exact_office_action_registration(),x.scale,reader,x.budget));take(apply_fixed_service_change_in_candidate(elapsed,std::move(prepared),exact_office_action_registration(),x.scale,reader,x.budget));take(context.release_action(completed.row.id));if(c.action(completed.row.id))fail("ExactOfficeTerminalNotReleased",completed.row.id);effect.action_id=completed.row.id;return std::move(effect);
});}
}
