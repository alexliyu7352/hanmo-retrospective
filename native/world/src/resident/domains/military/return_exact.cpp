#include "military_return_detachment_model.hpp"
#include "military_action_completion_build.hpp"
#include "military_action_conditions.hpp"
#include "military_action_resources.hpp"
// C07 original return/unloading, one true S0 H=1 entry and one completed row.
#include "return_exact_detail.hpp"
#include "return_original_rules.hpp"
#include "resupply_original_rules.hpp"
#include "military_field_command_source.hpp"
#include "military_roster_history.hpp"
#include "resident/storage/actor_location_execution_draft.hpp"
#include <limits>
#include <type_traits>
#include "resident/kernel/fixed_service_runtime.hpp"
#include "resident/model/common_operations.hpp"
namespace hanmo::resident {using namespace return_exact_detail;namespace {
static_assert(std::is_nothrow_move_constructible_v<FixedAppliedInput>,"the original one-applied basis must transfer without another copy");

}
Outcome<StableId> start_exact_return_action_in_candidate(CommonStore& c,MilitaryState& military,MilitaryActionTimeState& state,ActionElapsedStore& elapsed,const ExactReturnWriteContext& context,const ActionCoreView& action,const ArmyLogistics& request,const MilitaryQuote& offer){return guard<StableId>([&]{
 const auto& x=context.read;bound(c,military,state,x);if(!context.release_action||!context.adopt_actual_locations||action.id.empty()||action.kind!="army_return")fail("ExactReturnStartIdentityMismatch",action.id);
 if(const auto* old=state.dynamic_logistics_facts.find(action.id)){
  const auto& f=fact(state,action.id);const auto& terms=f.original_terms;
  if(f.actor_id!=action.actor_id||request.kind!="army_return"||request.army_id!=terms.army_id||!request.garrison_valid||request.garrison_id.value_or(terms.source_garrison_id)!=terms.garrison_id||!offer.logistics_terms||!same_original_logistics_terms(terms,*offer.logistics_terms))fail("ExactReturnReplayPayloadMismatch",action.id);
  take(validate_exact_return_sources(c,military,state,elapsed,x));return f.id;
 }
 for(const auto& old:military.logistics_facts)if(old.id==action.id)fail("DuplicateLegacyExactMilitaryAction",action.id);
 for(const auto& old:military.action_facts)if(old.id==action.id)fail("DuplicateLegacyExactMilitaryAction",action.id);
 for(const auto& old:military.logistics_actions)if(old.action_id==action.id)fail("DuplicateLegacyExactMilitaryExecution",action.id);
 if(state.dynamic_action_facts.find(action.id)||find_fixed_elapsed(elapsed,action.id))fail("DuplicateExactReturnElapsed",action.id);
 const auto at=take(x.clock.read_current_moment());const auto* actual=c.action(action.id);
 if(!actual||actual->id!=action.id||actual->kind!=action.kind||actual->actor_id!=action.actor_id||actual->participants!=action.participants||actual->remaining_ticks!=1||actual->start_tick!=at.half_day_tick)fail("ExactReturnActualActionRequired",action.id);
 auto quote=take(preview_military_lifecycle(c,military,x.catalog,x.access,action.actor_id,MilitaryCommand{request},action.id));
 if(request.kind!="army_return"||!quote.logistics_terms||quote.duration!=1||!offer.logistics_terms||offer.duration!=quote.duration||offer.participants!=quote.participants||!same_original_logistics_terms(*offer.logistics_terms,*quote.logistics_terms)||actual->participants!=quote.participants)fail("StaleMilitaryQuote",action.id);
 for(const auto& id:actual->participants){const auto* actor=c.actor(id);if(!actor||actor->action_id!=action.id)fail("MilitaryOccupationMissing",id);}
 auto acceptance_conditions=take(capture_military_action_acceptance_conditions(c,state,x.access,*actual));
 const std::optional<WorldMoment> no_completion;const std::optional<MilitaryLogisticsResultBuildView> no_result;const std::optional<SourceRefBuildView> no_terminal;const std::optional<InteractionContext> no_context;
 auto elapsed_memory=CheckpointMemoryCharge(0);const ActionElapsedEntryRef elapsed_ref{action.id};
 PreparedMilitaryLogisticsCompletion2 accepted({action.id,action.kind,action.actor_id,at,no_completion,"started",false,*quote.logistics_terms,no_context,elapsed_ref,no_result,no_terminal,2,acceptance_conditions});
 auto& f=accepted.row;
 state.dynamic_logistics_facts.set(f.id,f);take(c.append_fact_borrowed(f.id,"military_action","military"));take(validate_exact_return_garrison_acceptance(c,military,x,f));take(validate_exact_return_office_authority(c,military,state,x,f));take(validate_military_action_acceptance_conditions(c,state,x.access,f.id,f.started_at,actual->participants,f.acceptance_conditions));take(x.original.validate_return_acceptance(f));if(!participants_match(actual->participants,participants(c,military,state,f,x)))fail("ExactReturnParticipantsMismatch",f.id);
 auto entry=take(begin_fixed_elapsed(f.id,f.id,"army_return_original_1_half_day_v2",f.id,at,1,x.scale,x.budget));
 auto transition_id=take(c.next_owned_id("fixed_service_transition"));const SourceRefBuildView source{"military","military_action",f.id};
 auto basis_memory=CheckpointMemoryCharge(0);
 std::vector<FixedServiceInputRef> basis;basis.reserve(1);basis.emplace_back(FixedAppliedInput{SourceRef{"military","military_action",f.id},2});
 const std::vector<FixedServiceInputRef> empty;const WorkUnits zero{};
 FixedServiceTransitionInput transition(FixedServiceAdoptionTransitionBuildView{transition_id.id(),1,f.id,0,{},FixedTransitionKind::started,f.started_at,zero,source,2,{source,2},basis,empty,{}});
 NativeReturnActionTimeRead reader(c,military,state,elapsed,x);
 auto prepared=take(prepare_fixed_service_adoption(*actual,entry,transition.row(),exact_return_action_registration(),x.scale,reader,x.budget));take(apply_fixed_service_change_in_candidate(elapsed,std::move(prepared),exact_return_action_registration(),x.scale,reader,x.budget));return f.id;
});}
Status advance_exact_return_action_in_candidate(CommonStore& c,MilitaryState& military,MilitaryActionTimeState& state,ActionElapsedStore& elapsed,const ExactReturnWriteContext& context,const StableId& id,const MomentSpan& span){return status([&]{
 const auto& x=context.read;bound(c,military,state,x);const auto& f=fact(state,id);auto* e=find_fixed_elapsed(elapsed,id);
 if(f.phase!="started"||!e||!std::holds_alternative<ElapsedRunning>(e->timing)||!same_moment(e->last_settled_at,span.from,x.budget)||!same_moment(take(x.clock.read_current_moment()),span.to,x.budget)||take(compare_world_moments(span.to,span.from,x.budget))<=0||take(compare_world_moments(span.to,due(f,x.budget),x.budget))>0)fail("ExactReturnInvalidActualSpan",id);
 take(x.original.validate_return_actual_service(f,span));auto measured=take(prepare_fixed_elapsed_slice(*e,span,x.scale,x.budget));e->credited_units=std::move(measured.credited_units);e->last_settled_at=span.to;
 if(!measured.due){ActionMutation projection;projection.action.id=id;projection.remaining_ticks=take(fixed_elapsed_remaining_ticks(*e,x.scale,x.budget));take(c.apply_action(projection));}
});}
Outcome<MilitaryEffect> complete_exact_return_action_in_candidate(CommonStore& c,MilitaryState& military,MilitaryActionTimeState& state,ActionElapsedStore& elapsed,const ExactReturnWriteContext& context,const StableId& id){return guard<MilitaryEffect>([&]{
 const auto& x=context.read;bound(c,military,state,x);auto& f=fact(state,id);if(f.phase=="completed"){take(read_verified_exact_return_result(c,military,state,elapsed,x,completed_ref(id),2));return receipt(f,true);}
 const auto at=take(x.clock.read_current_moment());if(!context.release_action||!context.adopt_actual_locations||!same_moment(at,due(f,x.budget),x.budget))fail("ExactReturnCompletionGateRequired",id);
 const auto completion=take(read_fixed_service_completion(elapsed,f.elapsed_source_ref,at,x.scale,x.budget));if(!same_moment(completion.started_at,f.started_at,x.budget)||!same_moment(completion.completed_at,at,x.budget)||!completion.actual_service.complete)fail("ExactReturnFullActualServiceRequired",id);
 const auto* e=find_fixed_elapsed(elapsed,id);if(!e)fail("ExactReturnElapsedRequired",id);take(validate_fact_history(c,military,state,x,f,*e,true));const auto* actual=c.action(id);if(!actual||actual->kind!=f.command||actual->actor_id!=f.actor_id||!participants_match(actual->participants,participants(c,military,state,f,x)))fail("ExactReturnActualActionRequired",id);
 if(!c.actor_location_capture_identity()||c.actor_location_execution_drafts().size()>=std::size_t(std::numeric_limits<Integer>::max()))fail("ExactReturnActualLocationCaptureRequired",id);
 const auto first_location_sequence=Integer(c.actor_location_execution_drafts().size())+1;
 // The sole original business writer moves assets, returns retainers/units and
 // members, and transfers the existing captive responsibility to the garrison.
 auto resource_capture=take(begin_military_action_resource_capture(c,x.access.military_action_resources_schema));
 MilitaryReturnDetachmentCapture detachment_capture;
 OriginalEstateDetachmentReleaseSink* detachment_sink=x.access.military_return_detachments_schema==1?&detachment_capture:nullptr;
 auto applied=take(complete_original_army_return_in_candidate(c,military,x.catalog,x.access,*actual,f.original_terms,resource_capture?&*resource_capture:nullptr,detachment_sink));
 if(resource_capture&&bool(resource_capture->effect())!=applied.success)fail("MilitaryReturnResourceEffectCoverage",id);
 const StableId empty_id;const FieldOrder result_order;
 std::optional<ProducedStableId> returned_id,application;
 std::optional<MilitaryLogisticsResultBuildView> result;
 std::optional<SourceRefBuildView> terminal;
 if(applied.success){
  if(!applied.actual_return)fail("ExactReturnOriginalResultRequired",id);const auto& raw=*applied.actual_return;
  const auto* place=x.access.common_catalog?x.access.common_catalog->places.find(f.original_terms.place_id):nullptr;if(!place)fail("UnknownPlace",f.original_terms.place_id);
  // Existing captive location adoption, not another availability/body effect.
  for(const auto& change:raw.actor_changes){const auto* actor=c.actor(change.actor_id);const auto* before=actor?std::get_if<InArmyLocation>(&actor->location):nullptr;if(change.status!="captured"||change.garrison_id!=raw.garrison_id||change.captor_army_id!=raw.army_id||!change.army_id.empty()||!actor||actor->available||!before||before->army_id!=raw.army_id)fail("ExactReturnCaptivePlacementMismatch",change.actor_id);ActorMutation move;move.actor_id=change.actor_id;move.location=SettlementLocation{place->settlement_id,place->id};take(c.apply_actor(move));}
  returned_id.emplace(take(c.next_owned_id("military")));
  application.emplace(take(c.next_owned_id("military_logistics_completion_applied")));
  const std::optional<MilitaryActionResourceEffect1> absent;const auto& resources=resource_capture?resource_capture->effect():absent;
  terminal.emplace(SourceRefBuildView{"military","military_logistics_completion_applied",application->id()});
  std::optional<MilitaryReturnDetachmentReleases1> detachment_releases;
  if(x.access.military_return_detachments_schema==1)detachment_releases.emplace(detachment_capture.finish());
  PreparedMilitaryReturnFact2 returned({returned_id->id(),raw.army_id,raw.order_id,raw.garrison_id,raw.source_garrison_id,raw.ownership_id,raw.actor_id,raw.authority,at,raw.goods,raw.money,raw.unit_ids,raw.returned_retainers,raw.lost_retainers,raw.prisoner_ids,raw.actor_changes,id,*terminal,resources,detachment_releases});
  if(!state.dynamic_return_facts.insert(returned.row.id,returned.row))fail("DuplicateExactReturnReceipt",returned_id->id());
  take(c.append_fact_borrowed(returned_id->id(),"military_return","military"));take(c.append_fact_borrowed(application->id(),"military_logistics_completion_applied","military"));
  const auto* adopted_return=state.dynamic_return_facts.find(returned_id->id());if(!adopted_return)fail("ExactReturnOriginalResultRequired",id);
  result.emplace(MilitaryLogisticsResultBuildView{true,adopted_return->army_id,adopted_return->order_id,adopted_return->id,&adopted_return->actor_changes,result_order});
 }
 else{
  if(applied.actual_return||c.actor_location_execution_drafts().size()!=std::size_t(first_location_sequence-1))fail("FailedReturnChangedLocation",id);
  result.emplace(MilitaryLogisticsResultBuildView{{},empty_id,empty_id,empty_id,nullptr,result_order});
 }
 take(apply_original_action_resource_release_in_candidate(c,id));
 const std::optional<WorldMoment> completed_at{at};
 PreparedMilitaryLogisticsCompletion2 completed({f.id,f.command,f.actor_id,f.started_at,completed_at,"completed",applied.success,f.original_terms,f.original_context,f.elapsed_source_ref,result,terminal,f.rule_version,f.acceptance_conditions});
 state.dynamic_logistics_facts.set(completed.row.id,completed.row);
 const auto& adopted=fact(state,id);
 if(adopted.success){const auto* returned=state.dynamic_return_facts.find(adopted.result->receipt_id);if(!returned)fail("ExactReturnOriginalResultRequired",id);take(validate_original_return_resource_result(military,x.catalog,x.access.common_catalog,adopted,*returned,x.access.military_action_resources_schema));take(validate_exact_return_garrison_completion(c,military,state,x,adopted));validate_original_retainer_disposition(c,military,state,x,adopted,*returned);}
 take(x.original.validate_return_completed_result(adopted));if(adopted.success)take(context.adopt_actual_locations(adopted,first_location_sequence));
 const auto& terminal_owner=fact(state,id);const FixedServiceTransition* prior=nullptr;for(const auto& old:elapsed.fixed_service_transitions)if(old.action_ref==id){if(prior)fail("ExactReturnUnexpectedServiceTransition",id);prior=&old;}
 const auto* terminal_entry=find_fixed_elapsed(elapsed,id);if(!prior||!terminal_entry||prior->kind!=FixedTransitionKind::started||prior->transition_ordinal!=0||!terminal_owner.completed_at)fail("ExactReturnStartedHistoryRequired",id);
 auto transition_id=take(c.next_owned_id("fixed_service_transition"));const std::vector<FixedServiceInputRef> empty;
 FixedServiceTransitionInput transition(FixedServiceTerminalTransitionBuildView{transition_id.id(),1,id,1,std::string_view(prior->transition_id),FixedTransitionKind::terminal,*terminal_owner.completed_at,terminal_entry->credited_units,{"military","military_action",id},2,{{"military","military_action_completed",id},2},empty,prior->input_basis_refs,terminal_owner.success?FixedTerminalDisposition::completed:FixedTerminalDisposition::interrupted});
 NativeReturnActionTimeRead reader(c,military,state,elapsed,x);auto prepared=take(prepare_fixed_service_change(elapsed,id,transition.row(),{},exact_return_action_registration(),x.scale,reader,x.budget));take(apply_fixed_service_change_in_candidate(elapsed,std::move(prepared),exact_return_action_registration(),x.scale,reader,x.budget));take(context.release_action(id));if(c.action(id))fail("ExactReturnTerminalNotReleased",id);auto effect=receipt(fact(state,id));effect.action_id=id;return effect;
});}
}
