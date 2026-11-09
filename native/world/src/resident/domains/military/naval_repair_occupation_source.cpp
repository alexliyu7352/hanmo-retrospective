#include "naval_repair_occupation_source.hpp"
#include "naval_repair_exact_detail.hpp"
namespace hanmo::resident {using namespace naval_repair_exact_detail;
Outcome<NavalRepairOccupationSourceView> NativeNavalRepairOccupationSourceRead::read_original_occupation_source(const StableId& id)const{return guard<NavalRepairOccupationSourceView>([&]{
 if(!selected_)fail("NavalRepairOccupationKindNotSelected",id);
 // This original source-local reader has no occupation/professional callback.
 // Completed rows must prove both real start and actual result before borrowing.
 const auto start=take(read_exact_naval_repair_asset_source(common_,naval_,state_,elapsed_,context_,start_ref(id),2));
 if(!start.original||start.original!=state_.naval_repair_time_sources.find(id))fail("NavalRepairOccupationOriginalRowRequired",id);
 const auto& f=*start.original;const auto& t=f.timing;
 if(f.original_result){
  const auto end=take(read_exact_naval_repair_asset_source(common_,naval_,state_,elapsed_,context_,*t.result_ref,2));
  if(end.original!=&f)fail("NavalRepairOccupationOriginalResultMismatch",id);
 }
 NavalRepairOccupationSourceView out{&f,find_fixed_elapsed(elapsed_,id),nullptr,nullptr};
 for(const auto& transition:elapsed_.fixed_service_transitions)if(transition.action_ref==id){
  if(!out.started_transition)out.started_transition=&transition;
  else if(!out.terminal_transition)out.terminal_transition=&transition;
  else fail("NavalRepairOccupationUnexpectedTransition",id);
 }
 const auto* first=out.started_transition;const auto* last=out.terminal_transition;const auto* e=out.elapsed;
 if(!e||!first||first->source_schema!=1||!first->previous_input_basis_refs.empty()||first->terminal_disposition||
  compare(first->credited_at.value,{},context_.budget))fail("NavalRepairOccupationStartedSourceMismatch",id);
 // Identity references do not imply actor survival or available service.
 for(const auto& actor:f.original_terms.participants)if(!common_.actor(actor))fail("NavalRepairOccupationParticipantIdentityRequired",actor);
 const ActionElapsedTerminalBinding* binding=nullptr;
 for(const auto& row:elapsed_.terminal_bindings)if(row.action_ref==id){if(binding)fail("NavalRepairOccupationDuplicateTerminalBinding",id);binding=&row;}
 if(f.original_result){
  if(!last||last->source_schema!=1||last->previous_transition_ref!=std::optional<StableId>{first->transition_id}||
   !last->input_basis_refs.empty()||last->previous_input_basis_refs.size()!=first->input_basis_refs.size()||
   compare(last->credited_at.value,e->credited_units.value,context_.budget)||!std::holds_alternative<ElapsedTerminal>(e->timing))
   fail("NavalRepairOccupationTerminalSourceMismatch",id);
  for(std::size_t n=0;n<last->previous_input_basis_refs.size();++n)
   if(!same_fixed_input_ref(last->previous_input_basis_refs[n],first->input_basis_refs[n]))fail("NavalRepairOccupationTerminalInputMismatch",id);
  const auto* begin=binding?std::get_if<MomentOwnerSource>(&binding->started_source_ref):nullptr;
  const auto* end=binding?std::get_if<MomentOwnerSource>(&binding->ended_source_ref):nullptr;
  if(!binding||binding->action_kind!="naval_repair"||binding->actor_id!=f.original_terms.actor_id||
   binding->participant_actor_ids!=f.original_terms.participants||binding->timing_kind!=0||
   binding->terminal_result_ref!=f.original_result->id||!same_source(binding->original_execution_source_ref,t.start_fact_ref)||
   !begin||begin->source_schema!=2||!same_source(begin->source_ref,t.start_fact_ref)||
   !end||end->source_schema!=2||!same_source(end->source_ref,*t.result_ref)||
   binding->last_closed_gate_result_ref||!binding->retained_gate_result_refs.empty())
   fail("NavalRepairOccupationTerminalBindingMismatch",id);
  if(common_.action(id))fail("NavalRepairOccupationTerminalNotReleased",id);
 }else{
  const auto* actual=common_.action(id);
  if(last||binding||!std::holds_alternative<ElapsedRunning>(e->timing)||!actual||actual->id!=id||
   actual->kind!="naval_repair"||actual->actor_id!=f.original_terms.actor_id||
   actual->participants!=f.original_terms.participants||actual->start_tick!=t.started_at.half_day_tick)
   fail("NavalRepairOccupationActualActionRequired",id);
 }
 return out;
});}
Outcome<std::vector<StableId>> NativeNavalRepairOccupationSourceRead::enumerate_original_occupation_sources()const{return guard<std::vector<StableId>>([&]{
 if(!selected_){if(!state_.naval_repair_time_sources.empty())fail("NavalRepairOccupationKindNotSelected");return std::vector<StableId>{};}
 std::vector<StableId> result;std::set<StableId> starts,ends;
 for(const auto& row:state_.naval_repair_time_sources.rows()){
  if(row.first!=row.second.timing.action_ref||!starts.insert(row.first).second)fail("NavalRepairOccupationSourceKeyMismatch",row.first);
  const auto source=take(read_original_occupation_source(row.first));
  if(source.original->original_result&&!ends.insert(source.original->original_result->id).second)fail("NavalRepairOccupationDuplicateResult",row.first);
  result.push_back(row.first);
 }
 // Legacy rows remain their own provenance reader. This only rejects a missing
 // or multiply owned Common header; it does not infer their service intervals.
 for(const auto& row:common_.state().facts.rows())if(row.second.domain=="military_naval"){
  Integer owners=0;
  if(row.second.kind=="naval_repair_start"){
   owners=starts.count(row.first)?1:0;for(const auto& old:naval_.repairs.starts)if(old.id==row.first)++owners;
  }else if(row.second.kind=="naval_repair_result"){
   owners=ends.count(row.first)?1:0;for(const auto& old:naval_.repairs.results)if(old.id==row.first)++owners;
  }else continue;
  if(row.first!=row.second.id||owners!=1)fail("NavalRepairOccupationOriginalHeaderCoverage",row.first);
 }
 return result;
});}
}
