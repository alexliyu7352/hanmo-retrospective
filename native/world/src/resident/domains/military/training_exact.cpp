// S2 C06 real start/span/full-lesson/termination writers. The outer world
// candidate owns rollback of Common, military, annex and the one S0 elapsed.
#include "resident/storage/money_transfer_recipe.hpp"
#include "training_exact_detail.hpp"
#include "military_training_completion_time.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/kernel/fixed_service_runtime.hpp"
#include "resident/model/common_operations.hpp"
#include <limits>
#include <type_traits>
namespace hanmo::resident {using namespace training_exact_detail;namespace {
static_assert(std::is_nothrow_move_constructible_v<FixedAppliedInput>,"the one actual applied basis row must transfer without another copy");
static_assert(std::is_nothrow_move_constructible_v<SourceRef>,"the planned fresh completion source must transfer without another copy");
void end(CommonStore& common,MilitaryState& military,MilitaryActionTimeState& state,ActionElapsedStore& elapsed,const ExactTrainingWriteContext& context,DynamicMilitaryTrainingFact2& original,const WorldMoment& at,const StableId& code,const SourceRef& cause){
 const auto& x=context.read;if(original.phase!=DynamicMilitaryTrainingPhase::started||original.completed_at||original.terminal_code||original.terminal_source_ref||original.completion_application_source||!source_valid(cause)||!context.release_action)fail("ExactTrainingTerminalSourceRequired",original.id);
 auto* entry=find_fixed_elapsed(elapsed,original.id);if(!entry||!same_moment(entry->last_settled_at,at,x.budget))fail("ExactTrainingTerminalUnsettled",original.id);
 // Fresh original terminal row first: reserve its existing payload and exactly
 // the new cause/code before constructing or mutating either ordinary field.

 auto copied_fields=CheckpointMemoryCharge(0);DynamicMilitaryTrainingFact2 f(original);
 if(f.drill&&f.drill->phase==DynamicTrainingDrillPhase::pending){if(!f.lessons.empty())fail("ExactTrainingDrillFirstGateMissing",f.id);f.drill->challenge=take(close_original_training_drill(x.catalog,f.terms,f.drill->challenge));f.drill->phase=DynamicTrainingDrillPhase::closed;}
 if(!code.empty())take(validate_terminal_trigger(common,military,x,f,cause,at,code));
 f.phase=DynamicMilitaryTrainingPhase::completed;f.completed_at=at;f.success=code.empty()&&f.lessons.size()==4;if(!code.empty())f.terminal_code.emplace(code);f.terminal_source_ref.emplace(cause);
 if(code.empty()&&!f.success)fail("ExactTrainingPrematureSuccess",f.id);
 auto* unit=find(military.military_units,f.terms.unit_id);if(!unit||!unit->training_action_id||*unit->training_action_id!=f.id)fail("TrainingUnitOccupationMissing",f.id);
 ResourceRecipe resources;resources.operations.push_back(SetReservation{f.terms.payer_wallet_id,f.id,0,true});take(common.apply_resources(resources));*unit->training_action_id="";
 std::optional<ProducedStableId> completion_id;
 if(x.access.military_training_completion_time_schema==1){
  // The genuine wallet/unit release precedes this original occurrence head.
  completion_id.emplace(take(common.next_owned_id("military_training_completion_applied")));
  take(common.append_fact_borrowed(completion_id->id(),"military_training_completion_applied","military"));
  (copied_fields).grow(0);
  f.completion_application_source.emplace(SourceRef{"military","military_training_completion_applied",completion_id->id()});
 }
 state.dynamic_training_facts.set(f.id,f);
 // The local charged row remains alive through preparation/adoption. The
 // separately charged table target is the sole persisted original source.
 const auto& completed=fact(state,f.id);const FixedServiceTransition* prior=nullptr;
 for(const auto& old:elapsed.fixed_service_transitions)if(old.action_ref==f.id&&(!prior||old.transition_ordinal>prior->transition_ordinal))prior=&old;
 entry=find_fixed_elapsed(elapsed,f.id);if(!prior||!entry||prior->transition_ordinal==std::numeric_limits<Integer>::max())fail("ExactTrainingFixedHistoryRequired",f.id);
 auto transition_id=take(common.next_owned_id("fixed_service_transition"));const std::vector<FixedServiceInputRef> empty;
 const auto disposition=completed.success?FixedTerminalDisposition::completed:completed.terminal_code&&*completed.terminal_code=="MilitaryTrainingCancelled"?FixedTerminalDisposition::cancelled:FixedTerminalDisposition::interrupted;
 FixedServiceTransitionInput transition(FixedServiceTerminalTransitionBuildView{transition_id.id(),1,completed.id,prior->transition_ordinal+1,std::string_view(prior->transition_id),FixedTransitionKind::terminal,*completed.completed_at,entry->credited_units,{"military","military_training",completed.id},2,{{"military","military_training_completed",completed.id},2},empty,prior->input_basis_refs,disposition});
 NativeMilitaryTrainingActionTimeRead reader(common,military,state,elapsed,x);
 auto prepared=take(prepare_fixed_service_change(elapsed,completed.id,transition.row(),{},exact_military_training_registration(),x.scale,reader,x.budget));
 take(apply_fixed_service_change_in_candidate(elapsed,std::move(prepared),exact_military_training_registration(),x.scale,reader,x.budget));take(context.release_action(completed.id));
}
void append_witnesses(CommonStore& common,MilitaryActionTimeState& state,const ExactTrainingReadContext& x,const DynamicMilitaryTrainingFact2& f,const SourceRef& span_ref,const MomentSpan& span){
 auto cursor=span.from;while(take(compare_world_moments(cursor,span.to,x.budget))<0){
  auto p=take(x.periods.read_training_period(cursor.half_day_tick));period(p,cursor.half_day_tick,x.budget);
  auto end=take(normalize_world_moment(p.to_tick,integer(0,x.budget),x.budget));if(take(compare_world_moments(end,span.to,x.budget))>0)end=span.to;
  TrainingPeriodWitness2 w;w.action_ref=f.id;w.period_ref=p.period_ref;w.intersection_from=cursor;w.intersection_to=end;w.opening_receipt_ref=p.original_opening.source_ref;w.actual_participation_span_refs={span_ref};
  const auto id=take(common.next_id("military_training_period_witness"));state.training_period_witnesses.set(id,w);take(common.append_fact({id,"military_training_period_witness","military"}));cursor=end;
 }
}
}
Outcome<OriginalClosingSupplyGate2> read_original_training_closing_supply_gate(const CommonStore& common,const MilitaryState& military,const ExactTrainingReadContext& x,const DynamicMilitaryTrainingFact2& f,Integer ordinal){return guard<OriginalClosingSupplyGate2>([&]{
 bound(common,military,x);OriginalClosingSupplyGate2 out;out.lesson_ordinal=ordinal;out.gate_at=gate(f,ordinal,x.budget);
 const bool boundary=compare(out.gate_at.sub_half_day,integer(0,x.budget),x.budget)==0;
 if(boundary&&out.gate_at.half_day_tick==0)fail("ExactTrainingClosingPeriodRequired",f.id);
 const Integer from=out.gate_at.half_day_tick-(boundary?1:0);auto p=take(x.periods.read_training_period(from));period(p,from,x.budget);
 const MilitarySupplyFact* supplied=nullptr;for(const auto& row:military.supply_facts)if(row.garrison_id==f.terms.garrison_id&&row.army_id.empty()&&row.tick==p.to_tick){if(supplied)fail("MilitaryTrainingSupplyAmbiguous",f.id);supplied=&row;}
 if(!supplied)fail("MilitaryTrainingSupplyRequired",f.id);out.supply_fact_ref={"military","military_supply",supplied->id};header(common,out.supply_fact_ref);
 auto checked=validate_original_military_training_supply(*supplied,x.catalog,f.terms,p.to_tick);if(!checked.ok&&checked.error.original.code!="MilitaryTrainingUnsupplied")take(checked);
 out.selected_period_ref=p.period_ref;out.period_from_tick=p.from_tick;out.period_to_tick=p.to_tick;out.opening_receipt_ref=p.original_opening.source_ref;out.checked_terms_digest=digest(f.terms);return out;
});}
Outcome<StableId> start_exact_military_training_in_candidate(CommonStore& common,MilitaryState& military,MilitaryActionTimeState& state,ActionElapsedStore& elapsed,const ExactTrainingWriteContext& context,const ActionCoreView& action,const MilitaryTrain& request,const MilitaryQuote& offer){return guard<StableId>([&]{
 const auto& x=context.read;bound(common,military,x);const auto at=take(x.clock.read_current_moment());
 if(!context.release_action||action.id.empty()||action.kind!="military_train"||state.dynamic_training_facts.find(action.id)||find(military.training_facts,action.id)||find_fixed_elapsed(elapsed,action.id))fail("ExactTrainingStartIdentityMismatch",action.id);
 const auto* original=common.action(action.id);if(!original||original->kind!=action.kind||original->actor_id!=action.actor_id||original->participants!=action.participants||original->remaining_ticks!=4||original->start_tick!=at.half_day_tick)fail("InvalidMilitaryTrainingEnvelope",action.id);
 auto quote=take(preview_original_military_training_terms(common,military,x.catalog,x.access,action.actor_id,MilitaryCommand{request},action.id));
 if(!offer.training_terms||!quote.training_terms||!same_original_military_training_terms(*quote.training_terms,*offer.training_terms)||action.participants!=quote.training_terms->participants)fail("StaleMilitaryTrainingQuote",action.id);
 DynamicMilitaryTrainingFact2 f;f.id=action.id;f.actor_id=action.actor_id;f.started_at=at;f.terms=*quote.training_terms;f.elapsed_source_ref={action.id};f.work_binding_ref={action.id,"military_train",start_ref(action.id)};
 for(const auto& id:f.terms.participants){const auto* p=common.actor(id);if(!p||p->action_id!=action.id)fail("TrainingActorOccupationMissing",id);}
 if(take(common.spendable(f.terms.payer_wallet_id))<150)fail("InsufficientFunds",action.id);
 if(f.terms.drill){auto seeded=take(create_original_training_drill_in_candidate(common,x.catalog,x.access,f.terms,f.id));DynamicTrainingDrillInstance2 drill;drill.origin_seed=std::move(seeded.origin_seed);drill.origin_state=std::move(seeded.origin_state);drill.origin_field_order=std::move(seeded.origin_field_order);drill.challenge=std::move(seeded.challenge);f.drill=std::move(drill);}
 ResourceRecipe reserve;reserve.operations.push_back(SetReservation{f.terms.payer_wallet_id,f.id,150,false});take(common.apply_resources(reserve));
 auto* unit=find(military.military_units,f.terms.unit_id);if(!unit||!unit->training_action_id)fail("TrainingProfileRequired",f.id);*unit->training_action_id=f.id;
 state.dynamic_training_facts.set(f.id,f);take(common.append_fact({f.id,"military_training","military"}));take(x.original.validate_training_acceptance(f));
 auto entry=take(begin_fixed_elapsed(f.id,f.id,"military_train_original_4_half_days_v2",f.id,at,4,x.scale,x.budget));
 auto transition_id=take(common.next_owned_id("fixed_service_transition"));const SourceRefBuildView source{"military","military_training",f.id};
 // C06's genuine basis is one applied source, not another owner's two-row
 // applied+locked-rule recipe. Reserve this exact fresh local vector once.
 auto basis_memory=CheckpointMemoryCharge(0);
 std::vector<FixedServiceInputRef> basis;basis.reserve(1);basis.emplace_back(FixedAppliedInput{SourceRef{"military","military_training",f.id},2});
 const std::vector<FixedServiceInputRef> empty;const WorkUnits zero{};
 FixedServiceTransitionInput transition(FixedServiceAdoptionTransitionBuildView{transition_id.id(),1,f.id,0,{},FixedTransitionKind::started,f.started_at,zero,source,2,{source,2},basis,empty,{}});
 NativeMilitaryTrainingActionTimeRead reader(common,military,state,elapsed,x);auto prepared=take(prepare_fixed_service_adoption(*original,entry,transition.row(),exact_military_training_registration(),x.scale,reader,x.budget));
 take(apply_fixed_service_change_in_candidate(elapsed,std::move(prepared),exact_military_training_registration(),x.scale,reader,x.budget));return f.id;
});}
Outcome<std::optional<WorldMoment>> read_next_exact_training_gate(const MilitaryActionTimeState& state,const StableId& action,ExactArithmeticBudget budget){return guard<std::optional<WorldMoment>>([&]()->std::optional<WorldMoment>{const auto& f=fact(state,action);if(f.phase==DynamicMilitaryTrainingPhase::completed)return {};if(f.lessons.size()>=4)fail("ExactTrainingUnconsumedTerminal",action);return gate(f,Integer(f.lessons.size())+1,budget);});}
Status advance_exact_military_training_in_candidate(CommonStore& common,MilitaryState& military,MilitaryActionTimeState& state,ActionElapsedStore& elapsed,const ExactTrainingWriteContext& context,const StableId& action,const MomentSpan& span){return status([&]{
 const auto& x=context.read;bound(common,military,x);auto& f=fact(state,action);if(f.phase!=DynamicMilitaryTrainingPhase::started)fail("ExactTrainingAlreadyCompleted",action);
 auto* entry=find_fixed_elapsed(elapsed,action);if(!entry||!std::holds_alternative<ElapsedRunning>(entry->timing)||!same_moment(entry->last_settled_at,span.from,x.budget)||!same_moment(take(x.clock.read_current_moment()),span.to,x.budget)||take(compare_world_moments(span.to,span.from,x.budget))<=0||take(compare_world_moments(span.to,gate(f,Integer(f.lessons.size())+1,x.budget),x.budget))>0)fail("ExactTrainingSpanCrossesOriginalGate",action);
 auto change=take(x.original.read_training_eligibility_change(f,span.to));if(change&&(!source_valid(change->actual_source.source_ref)||change->actual_source.source_schema<1||change->original_code.empty()||take(compare_world_moments(change->occurred_at,span.to,x.budget))!=0))fail("ExactTrainingSpanCrossesEligibilityChange",action);
 auto participation=take(x.original.read_actual_training_participation(f,span));if(participation.action_ref!=action||!same_source(participation.terms_source_ref,start_ref(action))||!same_moment(participation.from,span.from,x.budget)||!same_moment(participation.to,span.to,x.budget)||!source_valid(participation.actual_participant_selection_ref)||participation.unit_and_occupation_source_refs.empty())fail("ExactTrainingActualParticipationRequired",action);
 take(x.original.validate_actual_training_participation(f,participation));const auto id=take(common.next_id("military_training_participation"));state.training_participation_spans.set(id,participation);take(common.append_fact({id,"military_training_participation","military"}));
 append_witnesses(common,state,x,f,{"military","military_training_participation",id},span);
 auto measured=take(prepare_fixed_elapsed_slice(*entry,span,x.scale,x.budget));entry->credited_units=std::move(measured.credited_units);entry->last_settled_at=span.to;
 if(change){end(common,military,state,elapsed,context,f,span.to,change->original_code,change->actual_source.source_ref);return;}
 // At the full four-half-day due the zero projection exists only within the
 // same candidate until the actual fourth lesson/terminal consumes the gate.
 if(!measured.due){ActionMutation projection;projection.action.id=action;projection.remaining_ticks=take(fixed_elapsed_remaining_ticks(*entry,x.scale,x.budget));take(common.apply_action(projection));}
});}
Outcome<MilitaryEffect> complete_exact_military_training_gate_in_candidate(CommonStore& common,MilitaryState& military,MilitaryActionTimeState& state,ActionElapsedStore& elapsed,const ExactTrainingWriteContext& context,const StableId& action){return guard<MilitaryEffect>([&]{
 const auto& x=context.read;bound(common,military,x);auto& f=fact(state,action);if(f.phase==DynamicMilitaryTrainingPhase::completed)return receipt(f,true);
 const auto at=take(x.clock.read_current_moment());const Integer ordinal=Integer(f.lessons.size())+1;auto* entry=find_fixed_elapsed(elapsed,action);
 if(!entry||!same_moment(at,gate(f,ordinal,x.budget),x.budget)||!same_moment(entry->last_settled_at,at,x.budget)||compare(entry->credited_units.value,take(fixed_duration_work_units(ordinal,x.scale.units_per_half_day,x.budget)).value,x.budget))fail("ExactTrainingLessonNotReady",action);
 MilitaryTrain request;request.office_id=f.terms.office_id;request.unit_id=f.terms.unit_id;auto quote=preview_original_military_training_terms(common,military,x.catalog,x.access,f.actor_id,MilitaryCommand{request},f.id,&f.terms,Integer(f.lessons.size()));
 if(!quote.ok&&quote.error.kind!=ErrorKind::rule)throw Failure{quote.error};
 if(!quote.ok||!quote.value.training_terms||!same_original_military_training_terms(f.terms,*quote.value.training_terms,true)){
  auto cause=take(x.original.read_training_eligibility_change(f,at));if(!cause||!same_moment(cause->occurred_at,at,x.budget))fail("ExactTrainingActualEligibilityChangeRequired",action);
  const StableId code=quote.ok?"MilitaryTrainingConditionsChanged":quote.error.original.code;if(cause->original_code!=code)fail("ExactTrainingEligibilityReasonMismatch",action);
  end(common,military,state,elapsed,context,f,at,code,cause->actual_source.source_ref);return receipt(fact(state,action));
 }
 auto supply=take(read_original_training_closing_supply_gate(common,military,x,f,ordinal));const auto* supplied=find(military.supply_facts,supply.supply_fact_ref.id);
 if(!supplied)fail("MilitaryTrainingSupplyRequired",action);if(supplied->shortage>0){end(common,military,state,elapsed,context,f,at,"MilitaryTrainingUnsupplied",supply.supply_fact_ref);return receipt(fact(state,action));}
 take(validate_exact_military_training_sources(common,military,state,elapsed,x,true));
 DynamicMilitaryTrainingLesson2 lesson;lesson.ordinal=ordinal;lesson.service_from=gate(f,ordinal-1,x.budget);lesson.closed_at=at;lesson.gate_work_units=take(fixed_duration_work_units(ordinal,x.scale.units_per_half_day,x.budget));lesson.closing_supply_gate=supply;lesson.fee_paid=take(original_military_training_fee(ordinal));
 const auto lesson_id=take(common.next_id("military_training_lesson"));lesson.fee_effect_ref={"military","military_training_lesson",lesson_id};
 for(const auto& row:state.training_participation_spans.rows())if(row.second.action_ref==action&&take(compare_world_moments(row.second.from,lesson.service_from,x.budget))>=0&&take(compare_world_moments(row.second.to,at,x.budget))<=0)lesson.participation_span_refs.push_back({"military","military_training_participation",row.first});
 for(const auto& row:state.training_period_witnesses.rows())if(row.second.action_ref==action&&take(compare_world_moments(row.second.intersection_from,lesson.service_from,x.budget))>=0&&take(compare_world_moments(row.second.intersection_to,at,x.budget))<=0)lesson.period_witness_refs.push_back({"military","military_training_period_witness",row.first});
 if(f.drill&&f.drill->phase==DynamicTrainingDrillPhase::pending){if(ordinal!=1)fail("ExactTrainingDrillFirstGateMissing",action);auto& drill=*f.drill;drill.challenge=take(close_original_training_drill(x.catalog,f.terms,drill.challenge));if(!drill.challenge.result)fail("TrainingDrillResultRequired",action);drill.phase=DynamicTrainingDrillPhase::frozen;drill.frozen_at=at;drill.performance_bp=drill.challenge.result->performance_bp;const auto id=take(common.next_id("military_training_drill_frozen"));state.drill_freeze_sources.set(id,{action,at,lesson.fee_effect_ref,*drill.performance_bp});take(common.append_fact({id,"military_training_drill_frozen","military"}));lesson.drill_frozen_source_ref=SourceRef{"military","military_training_drill_frozen",id};}
 else if(f.drill){if(f.lessons.empty()||!f.lessons.front().drill_frozen_source_ref)fail("ExactTrainingDrillFreezeSourceRequired",action);lesson.drill_frozen_source_ref=f.lessons.front().drill_frozen_source_ref;}
 auto* unit=find(military.military_units,f.terms.unit_id);if(!unit||!unit->training||!unit->fatigue||*unit->training!=current_training(f)||*unit->fatigue!=current_fatigue(f))fail("MilitaryTrainingConditionMismatch",action);
 lesson.training_before=*unit->training;lesson.fatigue_before=*unit->fatigue;lesson.training_after=take(original_military_training_after(f.terms,ordinal,f.drill?f.drill->performance_bp:std::nullopt));lesson.fatigue_after=military_training_progress::unit_fatigue(f.terms,ordinal);
 if(reservation(common,f.terms.payer_wallet_id,action)!=std::optional<Integer>{military_training_progress::unprovided_fee(ordinal-1)})fail("MilitaryTrainingReservationMismatch",action);
 ResourceRecipe fee;take(resource_recipe::append_reserved_money_payment(fee,SetReservation{f.terms.payer_wallet_id,action,military_training_progress::unprovided_fee(ordinal),false},f.terms.recipient_wallet_id,lesson.fee_paid,resource_recipe::ReservedMoneyPaymentOrder::release_then_original_balance));take(common.apply_resources(fee));
 *unit->training=lesson.training_after;*unit->fatigue=lesson.fatigue_after;f.lessons.push_back(lesson);take(common.append_fact({lesson_id,"military_training_lesson","military"}));
 f.lessons.back().learning_result_ref=take(context.learning.apply_original_training_learning_in_candidate(common,f,f.lessons.back()));
 take(x.original.validate_training_learning_result(f,f.lessons.back()));if(ordinal==4)end(common,military,state,elapsed,context,f,at,{},lesson.fee_effect_ref);return receipt(fact(state,action));
});}
Outcome<MilitaryEffect> cancel_exact_military_training_in_candidate(CommonStore& common,MilitaryState& military,MilitaryActionTimeState& state,ActionElapsedStore& elapsed,const ExactTrainingWriteContext& context,const StableId& actor,const StableId& action){return guard<MilitaryEffect>([&]{
 const auto& x=context.read;bound(common,military,x);auto& f=fact(state,action);if(f.actor_id!=actor)fail("StaleMilitaryTrainingAction",action);if(f.phase==DynamicMilitaryTrainingPhase::completed)return receipt(f,true);
 const auto id=take(common.next_id("military_training_cancelled"));const SourceRef cause{"military","military_training_cancelled",id};take(common.append_fact({id,cause.kind,cause.domain}));end(common,military,state,elapsed,context,f,take(x.clock.read_current_moment()),"MilitaryTrainingCancelled",cause);return receipt(fact(state,action));
});}
Outcome<MilitaryEffect> interrupt_exact_military_training_in_candidate(CommonStore& common,MilitaryState& military,MilitaryActionTimeState& state,ActionElapsedStore& elapsed,const ExactTrainingWriteContext& context,const StableId& action,const MomentOwnerSource& actual_cause){return guard<MilitaryEffect>([&]{
 const auto& x=context.read;bound(common,military,x);auto& f=fact(state,action);
 if(!source_valid(actual_cause.source_ref)||actual_cause.source_schema<1)fail("ExactTrainingActualEligibilityChangeRequired",action);
 if(f.phase==DynamicMilitaryTrainingPhase::completed)fail("ExactTrainingAlreadyCompleted",action);
 const auto at=take(x.clock.read_current_moment());auto cause=take(x.original.read_training_eligibility_change(f,at));
 if(!cause||!same_source(cause->actual_source.source_ref,actual_cause.source_ref)||cause->actual_source.source_schema!=actual_cause.source_schema||!same_moment(cause->occurred_at,at,x.budget)||cause->original_code.empty())fail("ExactTrainingActualEligibilityChangeRequired",action);
 end(common,military,state,elapsed,context,f,at,cause->original_code,actual_cause.source_ref);return receipt(fact(state,action));
});}
Outcome<MilitaryEffect> apply_exact_training_drill_command_in_candidate(CommonStore& common,MilitaryState& military,MilitaryActionTimeState& state,const ExactTrainingReadContext& x,const StableId& actor,const MilitaryDrillCommand& command){return guard<MilitaryEffect>([&]{
 bound(common,military,x);auto& f=fact(state,command.action_id);const auto* actual=common.action(f.id);const auto* p=common.actor(actor);
 if(f.actor_id!=actor||f.phase!=DynamicMilitaryTrainingPhase::started||!actual||actual->kind!="military_train"||actual->actor_id!=actor||!p||p->action_id!=f.id)fail("StaleMilitaryTrainingAction",f.id);
 if(!f.drill)fail("TrainingDrillNotOffered",f.id);if(f.drill->phase!=DynamicTrainingDrillPhase::pending||!f.lessons.empty())fail("TrainingDrillAlreadyFrozen",f.id);
 take(validate_original_training_drill_identity(x.catalog,f.terms,actor,f.id,f.drill->origin_seed,f.drill->origin_state,f.drill->challenge));
 f.drill->challenge=take(apply_original_training_drill_command(x.catalog,f.terms,f.drill->challenge,command));MilitaryEffect out;out.action_id=f.id;return out;
});}
Outcome<std::vector<StableId>> exact_military_training_mouths(const CommonStore& common,const MilitaryState& military,const MilitaryActionTimeState& state,const ExactTrainingReadContext& x,const MilitaryUnitRecord& unit,const StableId& garrison){return guard<std::vector<StableId>>([&]{
 bound(common,military,x);if(!unit.training_action_id||unit.training_action_id->empty())return std::vector<StableId>{};const auto& f=fact(state,*unit.training_action_id);if(f.phase!=DynamicMilitaryTrainingPhase::started||f.terms.unit_id!=unit.id||f.terms.garrison_id!=garrison)fail("InvalidTrainingSupplySource",unit.id);
 const auto* action=common.action(f.id);if(!action||action->kind!="military_train"||action->actor_id!=f.actor_id||action->participants!=f.terms.participants)fail("InvalidTrainingSupplySource",f.id);
 MilitaryTrain request;request.office_id=f.terms.office_id;request.unit_id=f.terms.unit_id;auto quote=preview_original_military_training_terms(common,military,x.catalog,x.access,f.actor_id,MilitaryCommand{request},f.id,&f.terms,Integer(f.lessons.size()));if(!quote.ok||!quote.value.training_terms||!same_original_military_training_terms(f.terms,*quote.value.training_terms,true))return std::vector<StableId>{};return std::vector<StableId>{f.actor_id};
});}
}
