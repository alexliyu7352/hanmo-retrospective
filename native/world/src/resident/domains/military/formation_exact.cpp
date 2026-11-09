#include "military_formation_reservations.hpp"
#include "military_action_conditions.hpp"
#include "military_action_resources.hpp"
#include "military_action_completion_build.hpp"
// C05 same original Army formation, one real S0 H=1 entry, one candidate.
#include "formation_exact_detail.hpp"
#include "military_action_original_rules.hpp"
#include "military_equal.hpp"
#include "resident/kernel/fixed_service_runtime.hpp"
#include "resident/model/common_operations.hpp"
#include <limits>
#include <type_traits>
#include "resident/storage/actor_location_execution_draft.hpp"
namespace hanmo::resident {using namespace formation_exact_detail;namespace {
static_assert(std::is_nothrow_move_constructible_v<FixedAppliedInput>,"the original one-applied basis must transfer without another copy");

}
Outcome<StableId> start_exact_formation_action_in_candidate(CommonStore& c,MilitaryState& military,MilitaryActionTimeState& state,ActionElapsedStore& elapsed,const ExactFormationWriteContext& context,const ActionCoreView& action,const ArmyForm& request,const MilitaryQuote& offer){return guard<StableId>([&]{
 const auto& x=context.read;bound(c,military,state,x);if(!context.release_action||!context.adopt_actual_locations||action.id.empty()||action.kind!="army_form")fail("ExactFormationStartIdentityMismatch",action.id);
 if(const auto* old=state.dynamic_action_facts.find(action.id)){
  const auto& f=fact(state,action.id);const auto& terms=std::get<ArmyFormationTerms>(f.original_terms);
  std::vector<StableId> retainers;for(const auto& row:terms.retainers)retainers.push_back(row.actor_id);
  if(f.actor_id!=action.actor_id||terms.order_id!=request.order_id||terms.unit_ids!=request.unit_ids||retainers!=request.retainer_ids||!request.unit_ids_valid||!request.retainer_ids_valid||!request.food_quantity_valid||!request.money_valid||(request.money&&*request.money!=terms.money)||!offer.terms||!same_terms(f.original_terms,*offer.terms)||f.original_context||offer.context)fail("ExactFormationReplayPayloadMismatch",action.id);
  if(request.food_quantity){const GarrisonDefinition* camp=nullptr;for(const auto& d:x.catalog.garrisons)if(d.id==terms.garrison_id)camp=&d;if(!camp)fail("UnknownGarrison",terms.garrison_id);Integer quantity=0;for(const auto& g:terms.goods)if(g.first==camp->food_item_id)quantity=g.second;if(*request.food_quantity!=quantity)fail("ExactFormationReplayPayloadMismatch",action.id);}
  take(validate_exact_formation_sources(c,military,state,elapsed,x));return f.id;
 }
 for(const auto& old:military.action_facts)if(old.id==action.id)fail("DuplicateLegacyExactMilitaryAction",action.id);
 for(const auto& old:military.actions)if(old.action_id==action.id)fail("DuplicateLegacyExactMilitaryExecution",action.id);
 if(find_fixed_elapsed(elapsed,action.id))fail("DuplicateExactFormationElapsed",action.id);
 if(state.dynamic_logistics_facts.find(action.id))fail("DuplicateExactMilitaryAction",action.id);
 for(const auto& old:state.dynamic_action_facts.rows())if(old.second.command=="army_form"&&old.second.phase=="started"){
  const auto* accepted=std::get_if<ArmyFormationTerms>(&old.second.original_terms);if(!accepted)fail("ExactFormationOriginalTermsMismatch",old.first);if(accepted->order_id==request.order_id)fail("MilitaryOrderAlreadyMobilizing",request.order_id);
 }
 const auto at=take(x.clock.read_current_moment());const auto* actual=c.action(action.id);
 if(!actual||actual->id!=action.id||actual->kind!=action.kind||actual->actor_id!=action.actor_id||actual->participants!=action.participants||actual->remaining_ticks!=1||actual->start_tick!=at.half_day_tick)fail("ExactFormationActualActionRequired",action.id);
 auto acceptance_observation=take(capture_military_action_endpoint_observation(c,state,x.access,*actual,MilitaryActionConditionEndpoint2::acceptance));
 auto quote=take(preview_military(c,military,x.catalog,x.access,action.actor_id,MilitaryCommand{request},action.id));
 if(!quote.terms||!std::holds_alternative<ArmyFormationTerms>(*quote.terms)||quote.duration!=1||!same_quote(offer,quote)||actual->participants!=quote.participants)fail("StaleMilitaryQuote",action.id);
 for(const auto& id:actual->participants){const auto* actor=c.actor(id);if(!actor||actor->action_id!=action.id)fail("MilitaryOccupationMissing",id);}
 auto acceptance_conditions=take(capture_military_action_acceptance_conditions(c,state,x.access,*actual));
 if(acceptance_observation)take(validate_military_action_endpoint_observation_unchanged(c,state,x.access,*actual,MilitaryActionConditionEndpoint2::acceptance,*acceptance_observation));
 auto reservation_capture=take(begin_military_formation_reservation_capture(c,x.access.military_formation_reservations_schema));
 take(reserve_original_army_formation_in_candidate(c,military,action.id,std::get<ArmyFormationTerms>(*quote.terms),reservation_capture?&*reservation_capture:nullptr));
 if(reservation_capture&&!reservation_capture->effect())fail("MilitaryFormationReservationCaptureRequired",action.id);
 const std::optional<WorldMoment> no_completion;const std::optional<MilitaryActionResultBuildView> no_result;const std::optional<SourceRefBuildView> no_terminal;const std::optional<MilitaryActionResourceEffect1> no_resources;const std::optional<MilitaryFormationReservations1> no_reservations;
 // The source ID is already the real accepted Action. Plan its elapsed-ref text
 // before constructing the local wrapper used by the original row BuildView.
 auto elapsed_memory=CheckpointMemoryCharge(0);const ActionElapsedEntryRef elapsed_ref{action.id};
 if(acceptance_observation)take(validate_military_action_endpoint_observation_unchanged(c,state,x.access,*actual,MilitaryActionConditionEndpoint2::acceptance,*acceptance_observation));
 PreparedMilitaryActionCompletion2 accepted({action.id,action.kind,action.actor_id,at,no_completion,"started",false,*quote.terms,quote.context,elapsed_ref,no_result,no_terminal,2,no_resources,acceptance_conditions,reservation_capture?reservation_capture->effect():no_reservations,&acceptance_observation,nullptr});
 auto& f=accepted.row;
 take(validate_original_formation_acceptance_reservations(x.catalog,f,x.access.military_formation_reservations_schema));
 state.dynamic_action_facts.set(f.id,f);take(c.append_fact_borrowed(f.id,"military_action","military"));take(validate_exact_formation_unit_activity_acceptance(c,military,state,elapsed,x,f));take(validate_exact_formation_garrison_acceptance(c,military,x,f));take(validate_exact_formation_campaign_authority(c,military,state,x,f));take(validate_military_action_acceptance_conditions(c,state,x.access,f.id,f.started_at,std::get<ArmyFormationTerms>(f.original_terms).member_actor_ids,f.acceptance_conditions));take(x.original.validate_formation_acceptance(f));
 auto entry=take(begin_fixed_elapsed(f.id,f.id,"army_form_original_1_half_day_v2",f.id,at,1,x.scale,x.budget));
 auto transition_id=take(c.next_owned_id("fixed_service_transition"));const SourceRefBuildView source{"military","military_action",f.id};
 auto basis_memory=CheckpointMemoryCharge(0);
 std::vector<FixedServiceInputRef> basis;basis.reserve(1);basis.emplace_back(FixedAppliedInput{SourceRef{"military","military_action",f.id},2});
 const std::vector<FixedServiceInputRef> empty;const WorkUnits zero{};
 FixedServiceTransitionInput transition(FixedServiceAdoptionTransitionBuildView{transition_id.id(),1,f.id,0,{},FixedTransitionKind::started,f.started_at,zero,source,2,{source,2},basis,empty,{}});
 NativeFormationActionTimeRead reader(c,military,state,elapsed,x);
 auto prepared=take(prepare_fixed_service_adoption(*actual,entry,transition.row(),exact_formation_action_registration(),x.scale,reader,x.budget));take(apply_fixed_service_change_in_candidate(elapsed,std::move(prepared),exact_formation_action_registration(),x.scale,reader,x.budget));return f.id;
});}
Status advance_exact_formation_action_in_candidate(CommonStore& c,MilitaryState& military,MilitaryActionTimeState& state,ActionElapsedStore& elapsed,const ExactFormationWriteContext& context,const StableId& id,const MomentSpan& span){return status([&]{
 const auto& x=context.read;bound(c,military,state,x);const auto& f=fact(state,id);auto* e=find_fixed_elapsed(elapsed,id);
 if(f.phase!="started"||!e||!std::holds_alternative<ElapsedRunning>(e->timing)||!same_moment(e->last_settled_at,span.from,x.budget)||!same_moment(take(x.clock.read_current_moment()),span.to,x.budget)||take(compare_world_moments(span.to,span.from,x.budget))<=0||take(compare_world_moments(span.to,due(f,x.budget),x.budget))>0)fail("ExactFormationInvalidActualSpan",id);
 take(x.original.validate_formation_actual_service(f,span));auto measured=take(prepare_fixed_elapsed_slice(*e,span,x.scale,x.budget));e->credited_units=std::move(measured.credited_units);e->last_settled_at=span.to;
 if(!measured.due){ActionMutation projection;projection.action.id=id;projection.remaining_ticks=take(fixed_elapsed_remaining_ticks(*e,x.scale,x.budget));take(c.apply_action(projection));}
});}
Outcome<MilitaryEffect> complete_exact_formation_action_in_candidate(CommonStore& c,MilitaryState& military,MilitaryActionTimeState& state,ActionElapsedStore& elapsed,const ExactFormationWriteContext& context,const StableId& id){return guard<MilitaryEffect>([&]{
 const auto& x=context.read;bound(c,military,state,x);auto& f=fact(state,id);if(f.phase=="completed"){take(read_verified_exact_formation_result(c,military,state,elapsed,x,completed_ref(id),2));return receipt(f,true);}
 const auto at=take(x.clock.read_current_moment());if(!context.release_action||!context.adopt_actual_locations||!same_moment(at,due(f,x.budget),x.budget))fail("ExactFormationCompletionGateRequired",id);
 const auto completion=take(read_fixed_service_completion(elapsed,f.elapsed_source_ref,at,x.scale,x.budget));if(!same_moment(completion.started_at,f.started_at,x.budget)||!same_moment(completion.completed_at,at,x.budget)||!completion.actual_service.complete)fail("ExactFormationFullActualServiceRequired",id);
 const auto* e=find_fixed_elapsed(elapsed,id);if(!e)fail("ExactFormationElapsedRequired",id);take(validate_fact_history(c,military,state,x,f,*e,true));const auto* actual=c.action(id);if(!actual||actual->kind!=f.command||actual->actor_id!=f.actor_id||actual->participants!=participants(f))fail("ExactFormationActualActionRequired",id);
 validate_current_escrow(c,military,f);const auto period=completion_period(x,at);
 if(c.actor_location_execution_drafts().size()>=std::size_t(std::numeric_limits<Integer>::max()))fail("ExactFormationLocationSequenceOverflow",id);
 const auto first_location_sequence=Integer(c.actor_location_execution_drafts().size())+1;
 if(!c.actor_location_capture_identity())fail("ExactFormationLocationCaptureRequired",id);
 auto completion_observation=take(capture_military_action_endpoint_observation(c,state,x.access,*actual,MilitaryActionConditionEndpoint2::completion));
 std::optional<MilitaryActionCompletionConditionGuard> condition_guard;
 if(completion_observation)condition_guard.emplace(c,state,x.access,*actual,*completion_observation);
 auto resource_capture=take(begin_military_action_resource_capture(c,x.access.military_action_resources_schema));
 auto effect=take(complete_original_army_formation_in_candidate(c,military,x.catalog,x.access,*actual,std::get<ArmyFormationTerms>(f.original_terms),period.to_tick,resource_capture?&*resource_capture:nullptr,condition_guard?&*condition_guard:nullptr));
 if(resource_capture&&bool(resource_capture->effect())!=effect.success)fail("MilitaryFormationResourceEffectCoverage",id);
 take(apply_original_action_resource_release_in_candidate(c,id));
 const std::string_view completion_kind=effect.success?"military_action_completion_applied":"military_formation_completion_checked";
 auto application=take(c.next_owned_id(completion_kind));
 const std::vector<StableId> result_order;const std::optional<WorldMoment> completed_at{at};
 const std::optional<MilitaryActionResultBuildView> result{{effect.details_ok,effect.office_id,effect.army_id,effect.order_id,result_order}};
 const std::optional<SourceRefBuildView> terminal{SourceRefBuildView{"military",completion_kind,application.id()}};
 const std::optional<MilitaryActionResourceEffect1> absent;const auto& resources=resource_capture?resource_capture->effect():absent;
 if(completion_observation)take(validate_military_action_endpoint_observation_unchanged(c,state,x.access,*actual,MilitaryActionConditionEndpoint2::completion,*completion_observation));
 PreparedMilitaryActionCompletion2 completed({f.id,f.command,f.actor_id,f.started_at,completed_at,"completed",effect.success,f.original_terms,f.original_context,f.elapsed_source_ref,result,terminal,f.rule_version,resources,f.acceptance_conditions,f.acceptance_resource_reservations,&f.acceptance_observation,&completion_observation});
 state.dynamic_action_facts.set(completed.row.id,completed.row);
 take(c.append_fact_borrowed(application.id(),completion_kind,"military"));
 const auto& adopted=fact(state,id);validate_completion_header(c,state,adopted);
 take(validate_original_formation_resource_result(military,x.catalog,adopted,x.access.military_action_resources_schema));
 if(adopted.success)take(validate_exact_formation_completion_authority(c,military,state,x,adopted));
 take(x.original.validate_formation_completed_result(adopted));
 if(adopted.success){
  const auto& people=std::get<ArmyFormationTerms>(adopted.original_terms).member_actor_ids;const auto& drafts=c.actor_location_execution_drafts();std::size_t next=std::size_t(first_location_sequence-1);
  for(const auto& person:people)if(person!=adopted.actor_id){if(next>=drafts.size()||drafts[next].actor_id!=person||drafts[next].sequence!=Integer(next+1))fail("ExactFormationActualLocationCoverage",person);const auto* location=std::get_if<InArmyLocation>(&drafts[next].after);if(!location||location->army_id!=adopted.result->army_id)fail("ExactFormationActualLocationMismatch",person);++next;}
  if(next!=drafts.size())fail("ExactFormationActualLocationCoverage",id);take(context.adopt_actual_locations(adopted,first_location_sequence));
 }
 else if(c.actor_location_execution_drafts().size()!=std::size_t(first_location_sequence-1))fail("FailedFormationChangedLocation",id);
 const auto& terminal_owner=fact(state,id);
 const FixedServiceTransition* prior=nullptr;for(const auto& old:elapsed.fixed_service_transitions)if(old.action_ref==terminal_owner.id){if(prior)fail("ExactFormationUnexpectedServiceTransition",terminal_owner.id);prior=&old;}
 const auto* terminal_entry=find_fixed_elapsed(elapsed,terminal_owner.id);if(!prior||!terminal_entry||prior->kind!=FixedTransitionKind::started||prior->transition_ordinal!=0||!terminal_owner.completed_at)fail("ExactFormationStartedHistoryRequired",terminal_owner.id);
 auto transition_id=take(c.next_owned_id("fixed_service_transition"));const std::vector<FixedServiceInputRef> empty;
 FixedServiceTransitionInput transition(FixedServiceTerminalTransitionBuildView{transition_id.id(),1,terminal_owner.id,1,std::string_view(prior->transition_id),FixedTransitionKind::terminal,*terminal_owner.completed_at,terminal_entry->credited_units,{"military","military_action",terminal_owner.id},2,{{"military","military_action_completed",terminal_owner.id},2},empty,prior->input_basis_refs,terminal_owner.success?FixedTerminalDisposition::completed:FixedTerminalDisposition::interrupted});
 NativeFormationActionTimeRead reader(c,military,state,elapsed,x);auto prepared=take(prepare_fixed_service_change(elapsed,id,transition.row(),{},exact_formation_action_registration(),x.scale,reader,x.budget));take(apply_fixed_service_change_in_candidate(elapsed,std::move(prepared),exact_formation_action_registration(),x.scale,reader,x.budget));take(context.release_action(id));if(c.action(id))fail("ExactFormationTerminalNotReleased",id);effect.action_id=id;return effect;
});}
}
