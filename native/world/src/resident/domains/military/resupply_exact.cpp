// C07 same original resupply reservation/transfer, one S0 H=1 entry.
#include "resupply_exact_detail.hpp"
#include "resupply_original_rules.hpp"
#include "resident/kernel/fixed_service_runtime.hpp"
#include "resident/model/common_operations.hpp"
namespace hanmo::resident {using namespace resupply_exact_detail;namespace {
FixedServiceTransition started(CommonStore& c,const DynamicMilitaryLogisticsFact2& f){FixedServiceTransition t;t.transition_id=take(c.next_id("fixed_service_transition"));t.action_ref=f.id;t.execution_source_ref=start_ref(f.id);t.execution_source_schema=2;t.occurred_at=f.started_at;t.kind=FixedTransitionKind::started;t.cause=FixedOwnerAdopted{start_ref(f.id),2};t.input_basis_refs={FixedAppliedInput{start_ref(f.id),2}};return t;}
FixedServiceTransition ended(CommonStore& c,const DynamicMilitaryLogisticsFact2& f,const ActionElapsedStore& elapsed){
 const FixedServiceTransition* prior=nullptr;for(const auto& t:elapsed.fixed_service_transitions)if(t.action_ref==f.id){if(prior)fail("ExactResupplyUnexpectedServiceTransition",f.id);prior=&t;}
 const auto* e=find_fixed_elapsed(elapsed,f.id);if(!prior||!e||prior->kind!=FixedTransitionKind::started||prior->transition_ordinal!=0||!f.completed_at)fail("ExactResupplyStartedHistoryRequired",f.id);
 FixedServiceTransition t;t.transition_id=take(c.next_id("fixed_service_transition"));t.action_ref=f.id;t.transition_ordinal=1;t.previous_transition_ref=prior->transition_id;t.kind=FixedTransitionKind::terminal;t.occurred_at=*f.completed_at;t.credited_at=e->credited_units;t.execution_source_ref=start_ref(f.id);t.execution_source_schema=2;t.cause=FixedOwnerTerminated{completed_ref(f.id),2};t.previous_input_basis_refs=prior->input_basis_refs;t.terminal_disposition=f.success?FixedTerminalDisposition::completed:FixedTerminalDisposition::interrupted;return t;
}
}
Outcome<StableId> start_exact_resupply_action_in_candidate(CommonStore& c,MilitaryState& military,MilitaryActionTimeState& state,ActionElapsedStore& elapsed,const ExactResupplyWriteContext& context,const ActionCoreView& action,const ArmyLogistics& request,const MilitaryQuote& offer){return guard<StableId>([&]{
 const auto& x=context.read;bound(c,military,state,x);if(!context.release_action||action.id.empty()||action.kind!="army_resupply")fail("ExactResupplyStartIdentityMismatch",action.id);
 if(const auto* old=state.dynamic_logistics_facts.find(action.id)){
  const auto& f=fact(state,action.id);const auto& terms=f.original_terms;const GarrisonDefinition* origin=nullptr;for(const auto& row:x.catalog.garrisons)if(row.id==terms.source_garrison_id)origin=&row;
  std::vector<std::pair<StableId,Integer>> goods;if(origin&&request.food_quantity&&*request.food_quantity>0)goods.emplace_back(origin->food_item_id,*request.food_quantity);
  if(f.actor_id!=action.actor_id||request.kind!="army_resupply"||request.army_id!=terms.army_id||!request.garrison_valid||!request.food_valid||!request.money_valid||!request.food_quantity||*request.food_quantity<0||*request.food_quantity>1000000||request.garrison_id.value_or(terms.source_garrison_id)!=terms.garrison_id||!origin||!terms.goods||goods!=*terms.goods||terms.money!=std::optional<Integer>{request.money.value_or(0)}||!offer.logistics_terms||!same_original_logistics_terms(terms,*offer.logistics_terms))fail("ExactResupplyReplayPayloadMismatch",action.id);
  take(validate_exact_resupply_sources(c,military,state,elapsed,x));return f.id;
 }
 for(const auto& old:military.logistics_facts)if(old.id==action.id)fail("DuplicateLegacyExactMilitaryAction",action.id);
 for(const auto& old:military.action_facts)if(old.id==action.id)fail("DuplicateLegacyExactMilitaryAction",action.id);
 for(const auto& old:military.logistics_actions)if(old.action_id==action.id)fail("DuplicateLegacyExactMilitaryExecution",action.id);
 if(state.dynamic_action_facts.find(action.id)||find_fixed_elapsed(elapsed,action.id))fail("DuplicateExactResupplyElapsed",action.id);
 const auto at=take(x.clock.read_current_moment());const auto* actual=c.action(action.id);
 if(!actual||actual->id!=action.id||actual->kind!=action.kind||actual->actor_id!=action.actor_id||actual->participants!=action.participants||actual->remaining_ticks!=1||actual->start_tick!=at.half_day_tick)fail("ExactResupplyActualActionRequired",action.id);
 auto quote=take(preview_military_lifecycle(c,military,x.catalog,x.access,action.actor_id,MilitaryCommand{request},action.id));
 if(request.kind!="army_resupply"||!quote.logistics_terms||quote.duration!=1||!offer.logistics_terms||offer.duration!=quote.duration||offer.participants!=quote.participants||!same_original_logistics_terms(*offer.logistics_terms,*quote.logistics_terms)||actual->participants!=quote.participants)fail("StaleMilitaryQuote",action.id);
 for(const auto& id:actual->participants){const auto* actor=c.actor(id);if(!actor||actor->action_id!=action.id)fail("MilitaryOccupationMissing",id);}
 take(reserve_original_army_resupply_in_candidate(c,action.id,*quote.logistics_terms));
 DynamicMilitaryLogisticsFact2 f;f.id=action.id;f.command=action.kind;f.actor_id=action.actor_id;f.started_at=at;f.original_terms=*quote.logistics_terms;f.elapsed_source_ref={action.id};
 state.dynamic_logistics_facts.set(f.id,f);take(c.append_fact({f.id,"military_action","military"}));take(x.original.validate_resupply_acceptance(f));if(participants(f,x)!=actual->participants)fail("ExactResupplyParticipantsMismatch",f.id);
 auto entry=take(begin_fixed_elapsed(f.id,f.id,"army_resupply_original_1_half_day_v2",f.id,at,1,x.scale,x.budget));auto transition=started(c,f);NativeResupplyActionTimeRead reader(c,military,state,elapsed,x);
 auto prepared=take(prepare_fixed_service_adoption(*actual,entry,transition,exact_resupply_action_registration(),x.scale,reader,x.budget));take(apply_fixed_service_change_in_candidate(elapsed,std::move(prepared),exact_resupply_action_registration(),x.scale,reader,x.budget));return f.id;
});}
Status advance_exact_resupply_action_in_candidate(CommonStore& c,MilitaryState& military,MilitaryActionTimeState& state,ActionElapsedStore& elapsed,const ExactResupplyWriteContext& context,const StableId& id,const MomentSpan& span){return status([&]{
 const auto& x=context.read;bound(c,military,state,x);const auto& f=fact(state,id);auto* e=find_fixed_elapsed(elapsed,id);
 if(f.phase!="started"||!e||!std::holds_alternative<ElapsedRunning>(e->timing)||!same_moment(e->last_settled_at,span.from,x.budget)||!same_moment(take(x.clock.read_current_moment()),span.to,x.budget)||take(compare_world_moments(span.to,span.from,x.budget))<=0||take(compare_world_moments(span.to,due(f,x.budget),x.budget))>0)fail("ExactResupplyInvalidActualSpan",id);
 take(x.original.validate_resupply_actual_service(f,span));auto measured=take(prepare_fixed_elapsed_slice(*e,span,x.scale,x.budget));e->credited_units=std::move(measured.credited_units);e->last_settled_at=span.to;
 if(!measured.due){ActionMutation projection;projection.action.id=id;projection.remaining_ticks=take(fixed_elapsed_remaining_ticks(*e,x.scale,x.budget));take(c.apply_action(projection));}
});}
Outcome<MilitaryEffect> complete_exact_resupply_action_in_candidate(CommonStore& c,MilitaryState& military,MilitaryActionTimeState& state,ActionElapsedStore& elapsed,const ExactResupplyWriteContext& context,const StableId& id){return guard<MilitaryEffect>([&]{
 const auto& x=context.read;bound(c,military,state,x);auto& f=fact(state,id);if(f.phase=="completed"){take(read_verified_exact_resupply_result(c,military,state,elapsed,x,completed_ref(id),2));return receipt(f,true);}
 const auto at=take(x.clock.read_current_moment());if(!context.release_action||!same_moment(at,due(f,x.budget),x.budget))fail("ExactResupplyCompletionGateRequired",id);
 const auto completion=take(read_fixed_service_completion(elapsed,f.elapsed_source_ref,at,x.scale,x.budget));if(!same_moment(completion.started_at,f.started_at,x.budget)||!same_moment(completion.completed_at,at,x.budget)||!completion.actual_service.complete)fail("ExactResupplyFullActualServiceRequired",id);
 const auto* e=find_fixed_elapsed(elapsed,id);if(!e)fail("ExactResupplyElapsedRequired",id);take(validate_fact_history(c,military,state,x,f,*e,true));const auto* actual=c.action(id);if(!actual||actual->kind!=f.command||actual->actor_id!=f.actor_id||actual->participants!=participants(f,x))fail("ExactResupplyActualActionRequired",id);
 take(validate_current_escrow(c,military,x,f));auto applied=take(complete_original_army_resupply_in_candidate(c,military,x.catalog,x.access,*actual,f.original_terms));
 ResourceRecipe release;release.operations.push_back(ReleaseActionResources{id});take(c.apply_resources(release));f.phase="completed";f.completed_at=at;f.success=applied.success;f.result=std::move(applied.result);auto effect=receipt(f);
 take(x.original.validate_resupply_completed_result(f));auto transition=ended(c,f,elapsed);NativeResupplyActionTimeRead reader(c,military,state,elapsed,x);auto prepared=take(prepare_fixed_service_change(elapsed,id,transition,{},exact_resupply_action_registration(),x.scale,reader,x.budget));take(apply_fixed_service_change_in_candidate(elapsed,std::move(prepared),exact_resupply_action_registration(),x.scale,reader,x.budget));take(context.release_action(id));if(c.action(id))fail("ExactResupplyTerminalNotReleased",id);effect.action_id=id;return effect;
});}
}
