// 同一原Action源序列，不用技术片/重放缓存作长期状态转换身份。
#include "fixed_service_runtime.hpp"
#include "fixed_service_history.hpp"
#include "world_copy_plan.hpp"
#include <type_traits>
#include <limits>
#include <stdexcept>
namespace hanmo::resident {
namespace {
struct Failure{RuleError error;};
void fail(const char* code){RuleError e;e.original={code,"fixed_service_runtime"};throw Failure{std::move(e)};}
template<class T>T take(Outcome<T> r){if(!r.ok)throw Failure{std::move(r.error)};return std::move(r.value);}
template<class T,class F>Outcome<T> guard(F fn){try{return {true,fn(),{}};}catch(Failure& e){return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){RuleError e;e.original={"FixedServiceResourceExhausted","fixed_service_runtime"};e.kind=ErrorKind::internal;return {false,{},std::move(e)};}catch(const std::length_error&){RuleError e;e.original={"FixedServiceResourceExhausted","fixed_service_runtime"};e.kind=ErrorKind::internal;return {false,{},std::move(e)};}}
bool same(const WorldMoment& a,const WorldMoment& z,ExactArithmeticBudget b){return take(compare_world_moments(a,z,b))==0;}
void install_terminal_binding(PreparedFixedServiceChange& change,const ActionTimeOwnerProof& proof,const WorldMoment& at,const ActionTimeOriginalSourceRead& source,ExactArithmeticBudget b){
    if(proof.phase!=ActionTimeOwnerPhase::terminal||!proof.ended_source||!proof.terminal_result_ref||proof.terminal_result_ref->empty()||!proof.retained_gate_result_refs.empty()||!same(take(source.read_action_time_source(*proof.ended_source)),at,b))fail("MissingFixedOriginalTerminal");
    const std::optional<StableId> no_gate;
    ActionElapsedTerminalBuildView view{proof.action_ref,proof.action_kind,proof.actor_id,proof.participant_actor_ids,proof.original_execution_source,0,proof.started_source,*proof.ended_source,*proof.terminal_result_ref,no_gate,proof.retained_gate_result_refs};
     (change.copied_fields).grow(0);
    static_assert(std::is_nothrow_move_constructible_v<ActionElapsedTerminalBinding> && std::is_nothrow_swappable_v<ActionElapsedTerminalBinding>);
    {std::optional<ActionElapsedTerminalBinding> replacement(std::in_place,ActionElapsedTerminalBinding{view.action_ref,view.action_kind,view.actor_id,view.participant_actor_ids,view.original_execution_source_ref,view.timing_kind,view.started_source_ref,view.ended_source_ref,view.terminal_result_ref,view.last_closed_gate_result_ref,view.retained_gate_result_refs});change.terminal.swap(replacement);}
}
void reserve_history_view(ActionElapsedStoreBuildView& view,std::size_t entries,std::size_t gates,std::size_t terminals,std::size_t transitions){view.entries.reserve(entries);view.gate_results.reserve(gates);view.terminal_bindings.reserve(terminals);view.fixed_service_transitions.reserve(transitions);}
ActionElapsedEntryBuildRef borrow_entry(const ActionElapsedStore::Entry& row){return std::visit([](const auto& value)->ActionElapsedEntryBuildRef{return &value;},row);}

void identity(const ActionTimeRegistration& reg,const ActionTimeOwnerProof& proof,const ActionElapsedState& entry,const FixedServiceTransition& t){take(validate_action_time_owner_proof(reg,proof));if(reg.mode!=ActionTimeMode::fixed_duration||proof.action_ref!=entry.action_ref||proof.action_kind!=reg.action_kind||proof.original_execution_source.domain!=reg.owner_domain||proof.original_execution_source.id!=entry.execution_source_ref||t.action_ref!=entry.action_ref||!same_source_ref(t.execution_source_ref,proof.original_execution_source)||t.execution_source_schema!=proof.source_schema||proof.source_schema!=reg.source_schema)fail("FixedServiceOwnerIdentityMismatch");}
void validate_retained_action(const ActionElapsedStore& store,const ActionElapsedState& entry,const ActionTimeRegistration& registration,const TimeScaleRef& scale,const ActionTimeOriginalSourceRead& source,ExactArithmeticBudget b){
    ActionElapsedStoreBuildView view;reserve_history_view(view,1,0,store.terminal_bindings.size(),store.fixed_service_transitions.size());view.entries.emplace_back(&entry);
    for(const auto& row:store.fixed_service_transitions)if(row.action_ref==entry.action_ref)view.fixed_service_transitions.push_back(&row);
    for(const auto& row:store.terminal_bindings)if(row.action_ref==entry.action_ref)view.terminal_bindings.push_back(&row);
    ActionElapsedStore retained(view);take(validate_fixed_history_values(retained,entry.last_settled_at,scale,b));auto proof=take(source.read_action_time_owner(registration,entry.action_ref));take(source.validate_fixed_action_sources_in_candidate(registration,proof,*find_fixed_elapsed(retained,entry.action_ref),retained.fixed_service_transitions,retained));
}
void check_history(const ActionElapsedStore& prior,const PreparedFixedServiceChange& change,const ActionTimeRegistration& registration,const TimeScaleRef& scale,const ActionTimeOriginalSourceRead& source,ExactArithmeticBudget budget){
    if(prior.fixed_service_transitions.size()==SIZE_MAX)throw std::bad_alloc();
    ActionElapsedStoreBuildView view;reserve_history_view(view,1,0,change.terminal?1:0,prior.fixed_service_transitions.size()+1);view.entries.emplace_back(&change.entry);
    for(const auto& row:prior.fixed_service_transitions)if(row.action_ref==change.entry.action_ref)view.fixed_service_transitions.push_back(&row);
    view.fixed_service_transitions.push_back(&change.transition);if(change.terminal)view.terminal_bindings.push_back(&*change.terminal);
    ActionElapsedStore check(view);take(validate_fixed_history_values(check,change.transition.occurred_at,scale,budget));auto proof=take(source.read_action_time_owner(registration,change.entry.action_ref));identity(registration,proof,change.entry,change.transition);take(source.validate_fixed_action_sources_in_candidate(registration,proof,*find_fixed_elapsed(check,change.entry.action_ref),check.fixed_service_transitions,check));
}
CheckpointMemoryCharge adoption_input_memory(const SourceRefBuildView& source,const CatalogIdentity& identity,std::string_view rule){return CheckpointMemoryCharge(0);}
CheckpointMemoryCharge transition_input_memory(const FixedServiceTerminalTransitionBuildView& value){return CheckpointMemoryCharge(0);}
CheckpointMemoryCharge transition_input_memory(const FixedServiceAdoptionTransitionBuildView& value){return CheckpointMemoryCharge(0);}
SourceRef transition_input_source(const SourceRefBuildView& value){return {StableId(value.domain),StableId(value.kind),StableId(value.id)};}
FixedServiceTransitionCause transition_input_cause(const FixedOwnerTerminatedBuildView& value){return FixedOwnerTerminated{transition_input_source(value.owner_result_ref),value.owner_result_schema};}
FixedServiceTransitionCause transition_input_cause(const FixedOwnerAdoptedBuildView& value){return FixedOwnerAdopted{transition_input_source(value.owner_source_ref),value.owner_source_schema};}
CheckpointMemoryCharge prepared_memory(const PreparedFixedServiceChangeBuildView& value){return CheckpointMemoryCharge(0);}

}
FixedServiceAdoptionInputs::FixedServiceAdoptionInputs(const SourceRefBuildView& source,Integer schema,const CatalogIdentity& identity,std::string_view rule)
 :copied_fields_(adoption_input_memory(source,identity,rule)){
    values_.reserve(2);
    values_.emplace_back(FixedAppliedInput{transition_input_source(source),schema});
    values_.emplace_back(FixedLockedRuleInput{identity,StableId(rule)});
}
template<class OriginalBuildView>FixedServiceTransitionInput::FixedServiceTransitionInput(const OriginalBuildView& value,int)
 :copied_fields_(transition_input_memory(value)),
  row_{StableId(value.transition_id),value.source_schema,StableId(value.action_ref),value.transition_ordinal,
   value.previous_transition_ref?std::optional<StableId>{std::in_place,*value.previous_transition_ref}:std::nullopt,
   value.kind,value.occurred_at,value.credited_at,transition_input_source(value.execution_source_ref),value.execution_source_schema,
   transition_input_cause(value.cause),value.input_basis_refs,value.previous_input_basis_refs,value.terminal_disposition}{}
FixedServiceTransitionInput::FixedServiceTransitionInput(const FixedServiceAdoptionTransitionBuildView& value):FixedServiceTransitionInput(value,0){}
FixedServiceTransitionInput::FixedServiceTransitionInput(const FixedServiceTerminalTransitionBuildView& value):FixedServiceTransitionInput(value,0){}
PreparedFixedServiceChange::PreparedFixedServiceChange(const PreparedFixedServiceChangeBuildView& value)
    :copied_fields(prepared_memory(value)),entry(value.entry),transition(value.transition),terminal(value.terminal?std::optional<ActionElapsedTerminalBinding>{*value.terminal}:std::nullopt),replay(value.replay){}
PreparedFixedServiceChange::PreparedFixedServiceChange(const PreparedFixedServiceChange& value)
    :PreparedFixedServiceChange(PreparedFixedServiceChangeBuildView{value.entry,value.transition,value.terminal?&*value.terminal:nullptr,value.replay}){}
Outcome<PreparedFixedServiceChange> prepare_fixed_service_adoption(const ActionCoreView& action,const ActionElapsedState& entry,const FixedServiceTransition& transition,const ActionTimeRegistration& registration,const TimeScaleRef& scale,const ActionTimeOriginalSourceRead& source,ExactArithmeticBudget b){return guard<PreparedFixedServiceChange>([&]{if(transition.kind!=FixedTransitionKind::started||transition.transition_ordinal!=0||transition.previous_transition_ref||action.id!=entry.action_ref||action.kind!=registration.action_kind||!same(entry.started_at,transition.occurred_at,b)||!same(entry.last_settled_at,transition.occurred_at,b)||take(compare_fractions(entry.credited_units.value,CanonicalFraction{},b))!=0||action.start_tick!=entry.started_at.half_day_tick||action.remaining_ticks!=take(fixed_elapsed_remaining_ticks(entry,scale,b)))fail("InvalidFixedServiceAdoption");auto proof=take(source.read_action_time_owner(registration,entry.action_ref));identity(registration,proof,entry,transition);if(proof.phase!=ActionTimeOwnerPhase::working||proof.actor_id!=action.actor_id||proof.participant_actor_ids!=action.participants||!same(take(source.read_action_time_source(proof.started_source)),entry.started_at,b))fail("FixedServiceStartSourceMismatch");PreparedFixedServiceChange change(PreparedFixedServiceChangeBuildView{entry,transition});check_history({},change,registration,scale,source,b);return change;});}
Outcome<PreparedFixedServiceChange> prepare_fixed_service_change(const ActionElapsedStore& store,const StableId& action,const FixedServiceTransition& transition,const std::optional<StableId>& suspension,const ActionTimeRegistration& registration,const TimeScaleRef& scale,const ActionTimeOriginalSourceRead& source,ExactArithmeticBudget b){return guard<PreparedFixedServiceChange>([&]{
    for(const auto& retained:store.fixed_service_transitions)if(retained.transition_id==transition.transition_id){
        if(retained.action_ref!=action||!take(same_fixed_service_transition(retained,transition,b)))fail("FixedServiceReplayPayloadMismatch");
        const auto* existing=find_fixed_elapsed(store,action);if(!existing)fail("MissingFixedServiceEntry");
        auto proof=take(source.read_action_time_owner(registration,action));identity(registration,proof,*existing,retained);validate_retained_action(store,*existing,registration,scale,source,b);
        const ActionElapsedTerminalBinding* ended=nullptr;for(const auto& terminal:store.terminal_bindings)if(terminal.action_ref==action)ended=&terminal;return PreparedFixedServiceChange(PreparedFixedServiceChangeBuildView{*existing,retained,ended,true});
    }
    const auto* original=find_fixed_elapsed(store,action);if(!original||transition.action_ref!=action||transition.kind==FixedTransitionKind::started||std::holds_alternative<ElapsedTerminal>(original->timing))fail("InvalidFixedServiceTransition");
    const FixedServiceTransition* previous=nullptr;for(const auto& row:store.fixed_service_transitions)if(row.action_ref==action&&(!previous||row.transition_ordinal>previous->transition_ordinal))previous=&row;if(!previous||previous->transition_ordinal==std::numeric_limits<Integer>::max()||transition.transition_ordinal!=previous->transition_ordinal+1||!transition.previous_transition_ref||*transition.previous_transition_ref!=previous->transition_id)fail("FixedServiceTransitionPredecessorMismatch");
    PreparedFixedServiceChange change(PreparedFixedServiceChangeBuildView{*original,transition});auto& next=change.entry;if(take(compare_world_moments(next.last_settled_at,transition.occurred_at,b))>0)fail("FixedServiceTransitionInPast");
    if(std::holds_alternative<ElapsedRunning>(next.timing)&&!same(next.last_settled_at,transition.occurred_at,b)){auto slice=take(prepare_fixed_elapsed_slice(next,{next.last_settled_at,transition.occurred_at},scale,b));next.credited_units=std::move(slice.credited_units);next.last_settled_at=transition.occurred_at;}
    if(take(compare_fractions(next.credited_units.value,transition.credited_at.value,b))!=0)fail("FixedServiceTransitionCreditMismatch");next.last_settled_at=transition.occurred_at;
    auto remaining=take(subtract_work_units(next.required_units,next.credited_units,b));const bool full=take(compare_fractions(remaining.value,CanonicalFraction{},b))==0;
    switch(transition.kind){
    case FixedTransitionKind::suspended:if(!std::holds_alternative<ElapsedRunning>(original->timing)||full||!suspension||suspension->empty())fail("InvalidFixedServiceSuspend");next.replace_timing(ElapsedSuspendedBuildView{*suspension});break;
    case FixedTransitionKind::resumed:if(!std::holds_alternative<ElapsedSuspended>(original->timing)||full||suspension)fail("InvalidFixedServiceResume");{ElapsedRunning running{take(add_world_duration(transition.occurred_at,take(scale_units_to_duration(remaining,scale.units_per_half_day,b)),b))};next.replace_timing(&running);}break;
    case FixedTransitionKind::input_rebound:
        if(full||suspension)fail("InvalidFixedServiceInputRebind");
        // An actual input change does not grant service or resume a paused owner.
        // Preserve its original suspension source; only Resumed changes that state.
        if(std::holds_alternative<ElapsedRunning>(original->timing)){ElapsedRunning running{take(add_world_duration(transition.occurred_at,take(scale_units_to_duration(remaining,scale.units_per_half_day,b)),b))};next.replace_timing(&running);}
        else if(!std::holds_alternative<ElapsedSuspended>(original->timing))fail("InvalidFixedServiceInputRebind");
        break;
    case FixedTransitionKind::terminal:{if(!transition.terminal_disposition||suspension||(*transition.terminal_disposition==FixedTerminalDisposition::completed&&!full))fail("InvalidFixedServiceTerminal");auto proof=take(source.read_action_time_owner(registration,action));install_terminal_binding(change,proof,transition.occurred_at,source,b);next.replace_timing(ElapsedTerminalBuildView{*proof.terminal_result_ref});break;}
    default:fail("InvalidFixedServiceTransitionKind");}
    check_history(store,change,registration,scale,source,b);return change;
});}
Status apply_fixed_service_change_in_candidate(ActionElapsedStore& store,PreparedFixedServiceChange change,const ActionTimeRegistration& registration,const TimeScaleRef& scale,const ActionTimeOriginalSourceRead& source,ExactArithmeticBudget b){return guard<Done>([&]{for(const auto& prior:store.fixed_service_transitions)if(prior.transition_id==change.transition.transition_id){if(!take(same_fixed_service_transition(prior,change.transition,b)))fail("FixedServiceReplayPayloadMismatch");const auto* current=find_fixed_elapsed(store,prior.action_ref);if(!current)fail("MissingFixedServiceEntry");auto proof=take(source.read_action_time_owner(registration,prior.action_ref));identity(registration,proof,*current,prior);validate_retained_action(store,*current,registration,scale,source,b);return Done{};}if(change.replay)fail("MissingFixedServiceReplayHistory");check_history(store,change,registration,scale,source,b);const auto* entry=find_fixed_elapsed(store,change.entry.action_ref);
    const bool started=change.transition.kind==FixedTransitionKind::started;
    if(started){for(const auto& candidate:store.entries)if(action_elapsed_entry_ref(candidate)==change.entry.action_ref)fail("DuplicateActionTimeEntry");}else if(!entry)fail("MissingFixedServiceEntry");
    if(store.entries.size()==SIZE_MAX||store.fixed_service_transitions.size()==SIZE_MAX||store.terminal_bindings.size()==SIZE_MAX)throw std::bad_alloc();
    ActionElapsedStoreBuildView view;reserve_history_view(view,store.entries.size()+(started?1:0),store.gate_results.size(),store.terminal_bindings.size()+(change.terminal?1:0),store.fixed_service_transitions.size()+1);
    for(const auto& row:store.entries){if(!started&&std::get_if<ActionElapsedState>(&row)==entry)view.entries.emplace_back(&change.entry);else view.entries.push_back(borrow_entry(row));}
    if(started)view.entries.emplace_back(&change.entry);
    for(const auto& row:store.gate_results)view.gate_results.push_back(&row);
    for(const auto& row:store.terminal_bindings)view.terminal_bindings.push_back(&row);
    if(change.terminal)view.terminal_bindings.push_back(&*change.terminal);
    for(const auto& row:store.fixed_service_transitions)view.fixed_service_transitions.push_back(&row);view.fixed_service_transitions.push_back(&change.transition);
    ActionElapsedStore replacement(view);store.swap(replacement);
    return Done{};});}
}
