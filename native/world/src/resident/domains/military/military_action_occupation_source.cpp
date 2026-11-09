#include "military_action_occupation_source.hpp"
#include "military_training_completion_time.hpp"
#include "formation_exact_detail.hpp"
#include "return_exact_detail.hpp"
#include "office_exact_detail.hpp"
#include "resident/kernel/fixed_service_history.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {using namespace movement_detail;namespace {
bool selected(const MilitaryActionTimeReadSelection& s,const StableId& kind){
 if(kind=="office_accept")return s.office_accept;
 if(kind=="army_form")return s.army_form;
 if(kind=="military_train")return s.military_train;
 if(kind=="army_return")return s.army_return;
 if(kind=="army_resupply")return s.army_resupply;
 fail("MilitaryOccupationOriginalKindRequired",kind);
}
void header(const CommonStore& c,const SourceRef& source){
 const auto* h=c.state().facts.find(source.id);
 if(!h||h->id!=source.id||h->domain!=source.domain||h->kind!=source.kind)
  fail("MilitaryOccupationOriginalHeaderRequired",source.id);
}
void legacy_collision(const MilitaryActionOccupationSourceContext& x,const StableId& id){
 for(const auto& f:x.military.action_facts)if(f.id==id)fail("DuplicateLegacyExactMilitaryAction",id);
 for(const auto& f:x.military.logistics_facts)if(f.id==id)fail("DuplicateLegacyExactMilitaryAction",id);
 for(const auto& f:x.military.training_facts)if(f.id==id)fail("DuplicateLegacyExactMilitaryAction",id);
 for(const auto& a:x.military.actions)if(a.action_id==id)fail("DuplicateLegacyExactMilitaryExecution",id);
 for(const auto& a:x.military.logistics_actions)if(a.action_id==id)fail("DuplicateLegacyExactMilitaryExecution",id);
}
void participants(const MilitaryActionOccupationSourceContext& x,const MilitaryActionOccupationSource& out){
 std::set<StableId> seen;bool actor=false;
 for(const auto& id:out.participant_actor_ids){
  if(id.empty()||!x.common.actor(id)||!seen.insert(id).second)fail("MilitaryOccupationParticipantsMismatch",out.action_ref);
  actor=actor||id==out.actor_id;
 }
 if(!actor)fail("MilitaryOccupationParticipantsMismatch",out.action_ref);
}
void execution_history(const MilitaryActionOccupationSourceContext& x,MilitaryActionOccupationSource& out,
 const StableId& duration,Integer required_half_days,FixedTerminalDisposition disposition){
 const auto& id=out.action_ref;const auto* e=find_fixed_elapsed(x.elapsed,id);
 if(!e||e->action_ref!=id||e->execution_source_ref!=id||e->duration_definition_ref!=duration||
  e->owner_cost_state_ref!=id||e->owner_work_remainder||!same_moment(e->started_at,out.started_at,x.budget)||
  compare(e->required_units.value,take(fixed_duration_work_units(required_half_days,x.scale.units_per_half_day,x.budget)).value,x.budget))
  fail("MilitaryOccupationElapsedBindingMismatch",id);
 const auto now=take(x.clock.read_current_moment());
 // These original five writers have no suspension/resumption command. Do not
 // infer that a different owner's paused action released occupation.
 for(const auto& t:x.elapsed.fixed_service_transitions)if(t.action_ref==id){
  if(!out.started_transition)out.started_transition=&t;
  else if(!out.terminal_transition)out.terminal_transition=&t;
  else fail("MilitaryOccupationUnexpectedServiceTransition",id);
 }
 const auto* first=out.started_transition;
 if(!first||first->source_schema!=1||first->kind!=FixedTransitionKind::started||first->transition_ordinal||
  first->previous_transition_ref||first->execution_source_schema!=2||!same_source(first->execution_source_ref,out.execution_source.source_ref)||
  !same_moment(first->occurred_at,out.started_at,x.budget)||first->input_basis_refs.size()!=1||first->terminal_disposition||
  compare(first->credited_at.value,{},x.budget))fail("MilitaryOccupationStartedHistoryMismatch",id);
 const auto* adopted=std::get_if<FixedOwnerAdopted>(&first->cause);
 const auto* input=std::get_if<FixedAppliedInput>(&first->input_basis_refs.front());
 if(!adopted||adopted->owner_source_schema!=2||!same_source(adopted->owner_source_ref,out.execution_source.source_ref)||
  !input||input->source_schema!=2||!same_source(input->source_ref,out.execution_source.source_ref))
  fail("MilitaryOccupationOriginalAdoptionRequired",id);
 if(out.ended_at){
  const auto* last=out.terminal_transition;
  if(!out.result_source||!last||last->source_schema!=1||last->kind!=FixedTransitionKind::terminal||last->transition_ordinal!=1||
   last->previous_transition_ref!=std::optional<StableId>{first->transition_id}||last->execution_source_schema!=2||
   !same_source(last->execution_source_ref,out.execution_source.source_ref)||!same_moment(last->occurred_at,*out.ended_at,x.budget)||
   !same_moment(e->last_settled_at,*out.ended_at,x.budget)||compare(last->credited_at.value,e->credited_units.value,x.budget)||
   last->terminal_disposition!=std::optional<FixedTerminalDisposition>{disposition}||!std::holds_alternative<ElapsedTerminal>(e->timing))
   fail("MilitaryOccupationTerminalHistoryMismatch",id);
  const auto* terminal=std::get_if<FixedOwnerTerminated>(&last->cause);
  if(!terminal||terminal->owner_result_schema!=2||!same_source(terminal->owner_result_ref,out.result_source->source_ref))
   fail("MilitaryOccupationOriginalTerminationRequired",id);
  const ActionElapsedTerminalBinding* binding=nullptr;
  for(const auto& row:x.elapsed.terminal_bindings)if(row.action_ref==id){if(binding)fail("MilitaryOccupationDuplicateTerminalBinding",id);binding=&row;}
  const auto* begin=binding?std::get_if<MomentOwnerSource>(&binding->started_source_ref):nullptr;
  const auto* end=binding?std::get_if<MomentOwnerSource>(&binding->ended_source_ref):nullptr;
  if(!binding||binding->action_kind!=out.action_kind||binding->actor_id!=out.actor_id||
   binding->participant_actor_ids!=out.participant_actor_ids||binding->timing_kind!=0||binding->terminal_result_ref!=id||
   !same_source(binding->original_execution_source_ref,out.execution_source.source_ref)||!begin||!end||
   begin->source_schema!=2||end->source_schema!=2||!same_source(begin->source_ref,out.execution_source.source_ref)||
   !same_source(end->source_ref,out.result_source->source_ref)||binding->last_closed_gate_result_ref||!binding->retained_gate_result_refs.empty())
   fail("MilitaryOccupationOriginalTerminalBindingMismatch",id);
  if(x.common.action(id))fail("MilitaryOccupationTerminalNotReleased",id);
 }else{
  if(out.result_source||out.completion_application||out.terminal_transition||!std::holds_alternative<ElapsedRunning>(e->timing))
   fail("MilitaryOccupationActiveHistoryMismatch",id);
  const auto* a=x.common.action(id);
  if(!a||a->id!=id||a->kind!=out.action_kind||a->actor_id!=out.actor_id||a->participants!=out.participant_actor_ids||
   a->start_tick!=out.started_at.half_day_tick)fail("MilitaryOccupationActualActionRequired",id);
 }
 // Pure S0 value/history validation only. It does not call any professional
 // validator. A same-candidate full gate may await its actual result writer.
 if(!out.ended_at&&compare(e->credited_units.value,e->required_units.value,x.budget)==0)
  take(read_fixed_service_intervals_at_completion_due(x.elapsed,id,out.started_at,e->last_settled_at,now,x.scale,x.budget));
 else take(read_fixed_service_intervals(x.elapsed,id,out.started_at,e->last_settled_at,now,x.scale,x.budget));
 if(out.ended_at&&(required_half_days==1||disposition==FixedTerminalDisposition::completed)){
  const auto credited=take(fixed_duration_work_units(required_half_days,x.scale.units_per_half_day,x.budget));
  if(compare(e->credited_units.value,credited.value,x.budget))fail("MilitaryOccupationIncompleteTerminalWork",id);
 }
 out.elapsed=e;
}
}
Outcome<MilitaryActionOccupationSource> NativeMilitaryActionOccupationSourceRead::read_original_occupation_source(const StableId& id)const{return guard<MilitaryActionOccupationSource>([&]{
 const auto& x=context_;const auto* action=x.state.dynamic_action_facts.find(id);
 const auto* logistics=x.state.dynamic_logistics_facts.find(id);const auto* training=x.state.dynamic_training_facts.find(id);
 if(id.empty()||Integer(bool(action))+Integer(bool(logistics))+Integer(bool(training))!=1||x.state.naval_repair_time_sources.find(id))
  fail("MilitaryOccupationUniqueOriginalSourceRequired",id);
 legacy_collision(x,id);MilitaryActionOccupationSource out;out.action_ref=id;
 StableId duration;Integer required_half_days=1;FixedTerminalDisposition disposition=FixedTerminalDisposition::interrupted;
 if(action){
  const auto& f=*action;if(f.id!=id||f.rule_version!=2||f.elapsed_source_ref.action_ref!=id||
   (f.command!="office_accept"&&f.command!="army_form"))fail("MilitaryOccupationActionIdentityMismatch",id);
  out.action_kind=f.command;out.actor_id=f.actor_id;out.started_at=f.started_at;out.execution_source={{"military","military_action",id},2};
  if(f.command=="office_accept"){
   const auto* t=std::get_if<OfficeTerms>(&f.original_terms);
   if(!t||t->actor_id!=f.actor_id||t->provider_actor_id.empty()||t->actor_id==t->provider_actor_id||!f.original_context||
    f.original_context->provider_actor_id!=t->provider_actor_id||f.original_context->place_id!=t->place_id)
    fail("MilitaryOccupationOfficeIdentityMismatch",id);
   out.participant_actor_ids={t->actor_id,t->provider_actor_id};duration="office_accept_original_1_half_day_v2";
  }else{
   const auto* t=std::get_if<ArmyFormationTerms>(&f.original_terms);
   if(!t||t->actor_id!=f.actor_id||f.original_context||t->member_actor_ids.empty()||t->member_actor_ids.front()!=f.actor_id)
    fail("MilitaryOccupationFormationIdentityMismatch",id);
   out.participant_actor_ids=t->member_actor_ids;duration="army_form_original_1_half_day_v2";
  }
  if(f.phase=="completed"){
   if(!f.completed_at||!f.result)fail("MilitaryOccupationResultRequired",id);
   out.ended_at=f.completed_at;out.result_source=MomentOwnerSource{{"military","military_action_completed",id},2};
   disposition=f.success?FixedTerminalDisposition::completed:FixedTerminalDisposition::interrupted;
   if(f.command=="office_accept")office_exact_detail::validate_completion_header(x.common,x.state,f);
   else if(f.command=="army_form"&&f.success){formation_exact_detail::validate_completion_header(x.common,x.state,f);out.completion_application=MomentOwnerSource{*f.terminal_source_ref,2};}
   else if(f.command=="army_form"){formation_exact_detail::validate_completion_header(x.common,x.state,f);}
   else if(f.terminal_source_ref)fail("MilitaryOccupationUnexpectedCompletionHeader",id);
  }else if(f.phase!="started"||f.completed_at||f.result||f.success||f.terminal_source_ref)fail("MilitaryOccupationPhaseMismatch",id);
 }else if(logistics){
  const auto& f=*logistics;if(f.id!=id||f.rule_version!=2||f.elapsed_source_ref.action_ref!=id||f.original_terms.actor_id!=f.actor_id||f.original_context||
   (f.command!="army_return"&&f.command!="army_resupply"))fail("MilitaryOccupationLogisticsIdentityMismatch",id);
  out.action_kind=f.command;out.actor_id=f.actor_id;out.started_at=f.started_at;out.execution_source={{"military","military_action",id},2};
  if(!selected(x.selected,f.command))fail("MilitaryOccupationKindNotSelected",f.command);
  if(f.command=="army_return"){
   if(!x.returns||!x.returns->original.bound_to(x.common,x.military,x.state))fail("MilitaryOccupationReturnParticipantsRequired",id);
   const auto original=take(read_exact_return_original_participants(x.common,x.military,x.state,*x.returns,f.id));out.participant_actor_ids.reserve(original.size());for(const auto* id:original)out.participant_actor_ids.push_back(*id);duration="army_return_original_1_half_day_v2";
  }else{
   if(!x.resupply||!x.resupply->bound_to(x.common,x.military,x.state))fail("MilitaryOccupationResupplyParticipantsRequired",id);
   out.participant_actor_ids=take(x.resupply->read_resupply_original_participants(f));duration="army_resupply_original_1_half_day_v2";
  }
  if(f.phase=="completed"){
   if(!f.completed_at||!f.result)fail("MilitaryOccupationResultRequired",id);
   out.ended_at=f.completed_at;out.result_source=MomentOwnerSource{{"military","military_logistics_completed",id},2};
   disposition=f.success?FixedTerminalDisposition::completed:FixedTerminalDisposition::interrupted;
   if(f.command=="army_return"&&f.success){return_exact_detail::validate_completion_header(x.common,x.state,f);out.completion_application=MomentOwnerSource{*f.terminal_source_ref,2};}
   else if(f.terminal_source_ref)fail("MilitaryOccupationUnexpectedCompletionHeader",id);
  }else if(f.phase!="started"||f.completed_at||f.result||f.success||f.terminal_source_ref)fail("MilitaryOccupationPhaseMismatch",id);
 }else{
  const auto& f=*training;if(f.id!=id||f.rule_version!=2||f.actor_id!=f.terms.actor_id||f.terms.duration!=4||f.elapsed_source_ref.action_ref!=id||
   f.work_binding_ref.action_id!=id||f.work_binding_ref.action_kind!="military_train"||
   !same_source(f.work_binding_ref.source_owner_ref,{"military","military_training",id}))fail("MilitaryOccupationTrainingIdentityMismatch",id);
  out.action_kind="military_train";out.actor_id=f.actor_id;out.participant_actor_ids=f.terms.participants;
  out.started_at=f.started_at;out.execution_source={{"military","military_training",id},2};duration="military_train_original_4_half_days_v2";required_half_days=4;
  if(f.phase==DynamicMilitaryTrainingPhase::completed){
   if(!f.completed_at||!f.terminal_source_ref)fail("MilitaryOccupationTrainingTerminalRequired",id);
   if(!source_valid(*f.terminal_source_ref))fail("MilitaryOccupationTrainingTerminalRequired",id);
   if(f.completion_application_source)take(validate_exact_training_completion_header(x.common,x.military,x.state,f));
   out.ended_at=f.completed_at;
   out.result_source=MomentOwnerSource{{"military","military_training_completed",id},2};
   disposition=f.success?FixedTerminalDisposition::completed:f.terminal_code==std::optional<StableId>{"MilitaryTrainingCancelled"}?FixedTerminalDisposition::cancelled:FixedTerminalDisposition::interrupted;
  }else if(f.phase!=DynamicMilitaryTrainingPhase::started||f.completed_at||f.success||f.terminal_source_ref||f.completion_application_source||f.terminal_code)
   fail("MilitaryOccupationTrainingPhaseMismatch",id);
 }
 if(!selected(x.selected,out.action_kind))fail("MilitaryOccupationKindNotSelected",out.action_kind);
 header(x.common,out.execution_source.source_ref);participants(x,out);
 execution_history(x,out,duration,required_half_days,disposition);return out;
});}
Outcome<std::vector<StableId>> NativeMilitaryActionOccupationSourceRead::enumerate_original_occupation_sources()const{return guard<std::vector<StableId>>([&]{
 std::vector<StableId> out;std::set<StableId> unique;
 auto append=[&](const auto& rows){for(const auto& row:rows){if(row.first!=row.second.id||!unique.insert(row.first).second)fail("MilitaryOccupationSourceKeyMismatch",row.first);take(read_original_occupation_source(row.first));out.push_back(row.first);}};
 append(context_.state.dynamic_action_facts.rows());append(context_.state.dynamic_logistics_facts.rows());append(context_.state.dynamic_training_facts.rows());
 // Header coverage does not validate legacy payloads or claim their intervals;
 // it only prevents silently dropping a retained /2 execution from this list.
 for(const auto& row:context_.common.state().facts.rows())if(row.second.domain=="military"&&
  (row.second.kind=="military_action"||row.second.kind=="military_training")){
  Integer owners=unique.count(row.first)?1:0;
  if(row.second.kind=="military_action"){
   for(const auto& f:context_.military.action_facts)if(f.id==row.first)++owners;
   for(const auto& f:context_.military.logistics_facts)if(f.id==row.first)++owners;
  }else for(const auto& f:context_.military.training_facts)if(f.id==row.first)++owners;
  if(row.first!=row.second.id||owners!=1)fail("MilitaryOccupationOriginalHeaderCoverage",row.first);
 }
 return out;
});}
namespace {
template<class Emit>Status unit_activity(const MilitaryActionOccupationSourceContext& x,const MilitaryCatalog& catalog,const std::vector<StableId>& units,const SourceRef& boundary,const WorldMoment& at,Emit emit){return status([&]{
 const auto cut=take(read_actual_common_fact_position(x.common,boundary));const auto now=take(x.clock.read_current_moment());
 const auto* accepting=x.state.dynamic_action_facts.find(boundary.id);const auto* accepted=accepting?std::get_if<ArmyFormationTerms>(&accepting->original_terms):nullptr;
 if(boundary.domain!="military"||boundary.kind!="military_action"||!accepting||accepting->id!=boundary.id||accepting->command!="army_form"||accepting->rule_version!=2||!accepted||accepted->unit_ids!=units||!same_moment(accepting->started_at,at,x.budget))fail("MilitaryUnitActivityAcceptingSourceRequired",boundary.id);
 auto pin=[&](std::string_view slot,const SourceRefBuildView& source){take(emit(SourcePinBuildView{"military","formation_unit_activity",boundary.id,slot,source.domain,source.kind,source.id,SourcePinReason::dependent_state}));};
 if(take(compare_world_moments(at,now,x.budget))>0||units.empty())fail("MilitaryUnitActivityBoundaryRequired",boundary.id);
 auto touches=[&](const std::vector<StableId>& original){for(const auto& id:original)if(std::find(units.begin(),units.end(),id)!=units.end())return true;return false;};
 for(std::size_t i=0;i<units.size();++i){
  if(units[i].empty()||std::find(units.begin(),units.begin()+i,units[i])!=units.begin()+i)fail("MilitaryUnitActivityIdentityMismatch",units[i]);
  const MilitaryUnitDefinition* authored=nullptr;for(const auto& camp:catalog.garrisons)for(const auto& row:camp.units)if(row.id==units[i]){if(authored)fail("MilitaryUnitSourceMissing",units[i]);authored=&row;}
  if(!authored)fail("MilitaryUnitSourceMissing",units[i]);pin("authored_unit",{"military","catalog_military_unit",authored->id});
 }
 // Legacy terminal ticks/cause IDs do not prove the actual release occurrence.
 // A retained relevant legacy execution cannot be silently treated as free.
 for(const auto& row:x.military.action_facts)if(row.command=="army_form"){const auto* terms=std::get_if<ArmyFormationTerms>(&row.terms);if(!terms)fail("MilitaryUnitActivityOriginalTermsRequired",row.id);if(touches(terms->unit_ids))fail("MilitaryUnitActivityExactSourceRequired",row.id);}
 for(const auto& row:x.military.training_facts)if(std::find(units.begin(),units.end(),row.terms.unit_id)!=units.end())fail("MilitaryUnitActivityExactSourceRequired",row.id);
 auto before=[&](const SourceRefBuildView& source,const WorldMoment& occurred){
  const auto position=take(read_actual_common_fact_position(x.common,source));const auto order=take(compare_world_moments(occurred,at,x.budget));
  if(take(compare_world_moments(occurred,now,x.budget))>0||(order<0&&position>=cut)||(order>0&&position<cut))fail("MilitaryUnitActivitySourceOrderMismatch",boundary.id);return position<cut;
 };
 NativeMilitaryActionOccupationSourceRead original(x);
 for(const auto& item:x.state.dynamic_action_facts.rows()){
  const auto& row=item.second;if(row.command!="army_form")continue;const auto* terms=std::get_if<ArmyFormationTerms>(&row.original_terms);if(!terms||item.first!=row.id)fail("MilitaryUnitActivityOriginalTermsRequired",item.first);if(!touches(terms->unit_ids))continue;
  if(!before({"military","military_action",row.id},row.started_at))continue;
  // This source-local original branch validates the actual S0 identity/history,
  // not whole C05 acceptance and hence cannot recurse into this predicate.
  take(original.read_original_occupation_source(row.id));
  if(row.phase=="started")fail("GarrisonUnitUnavailable",row.id);
  if(row.phase!="completed"||!row.completed_at||!row.terminal_source_ref)fail("MilitaryUnitActivityCompletionRequired",row.id);
  formation_exact_detail::validate_completion_header(x.common,x.state,row);const auto& released=*row.terminal_source_ref;
  if(!before({released.domain,released.kind,released.id},*row.completed_at))fail("GarrisonUnitUnavailable",row.id);pin("prior_formation_start",{"military","military_action",row.id});pin("prior_formation_release",{released.domain,released.kind,released.id});
 }
 for(const auto& item:x.state.dynamic_training_facts.rows()){
  const auto& row=item.second;if(std::find(units.begin(),units.end(),row.terms.unit_id)==units.end())continue;if(item.first!=row.id)fail("MilitaryUnitActivityOriginalTermsRequired",item.first);
  if(!before({"military","military_training",row.id},row.started_at))continue;
  if(row.phase==DynamicMilitaryTrainingPhase::started){take(original.read_original_occupation_source(row.id));fail("GarrisonUnitUnavailable",row.id);}
  if(row.phase!=DynamicMilitaryTrainingPhase::completed||!row.completed_at||!row.completion_application_source)fail("MilitaryTrainingCompletionSourceRequired",row.id);
  const auto& released=*row.completion_application_source;
  take(read_exact_training_completion_application({x.common,x.military,x.state,x.elapsed,x.scale,x.clock,x.budget},released,2));
  if(!before({released.domain,released.kind,released.id},*row.completed_at))fail("GarrisonUnitUnavailable",row.id);pin("prior_training_start",{"military","military_training",row.id});pin("prior_training_release",{released.domain,released.kind,released.id});
 }
});}
}
Status validate_original_military_unit_activity_before(const MilitaryActionOccupationSourceContext& x,const MilitaryCatalog& catalog,const std::vector<StableId>& units,const SourceRef& boundary,const WorldMoment& at){return unit_activity(x,catalog,units,boundary,at,[](const SourcePinBuildView&){return common_success();});}
Status collect_original_military_unit_activity_pins(const MilitaryActionOccupationSourceContext& x,const MilitaryCatalog& catalog,const std::vector<StableId>& units,const SourceRef& boundary,const WorldMoment& at,SourcePinSink& sink){return unit_activity(x,catalog,units,boundary,at,[&](const SourcePinBuildView& p){return sink.contains(p)?common_success():sink.append(p);});}
Status validate_original_military_unit_activity_pin(const MilitaryActionOccupationSourceContext& x,const MilitaryCatalog& catalog,const std::vector<StableId>& units,const SourceRef& boundary,const WorldMoment& at,const SourcePin& input){return status([&]{bool found=false;take(unit_activity(x,catalog,units,boundary,at,[&](const SourcePinBuildView& p){if(p.consumer_domain==input.consumer.domain&&p.consumer_kind==input.consumer.kind&&p.consumer_id==input.consumer.id&&p.consumer_slot==input.consumer.slot&&p.source_domain==input.source.domain&&p.source_kind==input.source.kind&&p.source_id==input.source.id&&p.reason==input.reason)found=true;return common_success();}));if(!found)fail("MilitaryUnitActivityPinRequired",input.consumer.id);});}
std::vector<RequiredSourceConsumerSlot> original_military_unit_activity_consumer_slots(){return {{"military","formation_unit_activity","authored_unit"},{"military","formation_unit_activity","prior_formation_start"},{"military","formation_unit_activity","prior_formation_release"},{"military","formation_unit_activity","prior_training_start"},{"military","formation_unit_activity","prior_training_release"}};}
}
