// 同一ActorAction的一次真实参与→一个gate；不按师生人数、床数或技术切片重复领取工时。
#include "shared_work_runtime.hpp"
#include "world_copy_plan.hpp"
#include <algorithm>
#include <set>
#include <string_view>
#include <stdexcept>
namespace hanmo::resident {
namespace {
struct Failure{RuleError error;};
using BorrowedWorkIdSet=std::set<std::string_view,std::less<std::string_view>,CheckpointAllocator<std::string_view>>;
void fail(const char* code){RuleError e;e.original={code,"shared_work"};throw Failure{std::move(e)};}
// 已写原source后的结构/前像冲突属于候选不变量失败，不能吞成普通业务拒绝。
[[noreturn]]void fail_pace(const char* code){RuleError e;e.original={code,"shared_work"};e.kind=ErrorKind::internal;throw Failure{std::move(e)};}
template<class T>T take(Outcome<T> r){if(!r.ok)throw Failure{std::move(r.error)};return std::move(r.value);}
template<class T,class F>Outcome<T> guard(F fn){try{return {true,fn(),{}};}catch(Failure& e){return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){RuleError e;e.original={"SharedWorkResourceExhausted","shared_work"};e.kind=ErrorKind::internal;return {false,{},std::move(e)};}catch(const std::length_error&){RuleError e;e.original={"SharedWorkResourceExhausted","shared_work"};e.kind=ErrorKind::internal;return {false,{},std::move(e)};}}
CheckpointMemoryCharge slice_memory(const SharedWorkElapsedState& before,const SharedWorkElapsedState& entry){
    return CheckpointMemoryCharge(0);
}
bool same(const WorldMoment& a,const WorldMoment& z,ExactArithmeticBudget b){return take(compare_world_moments(a,z,b))==0;}
const ActorActionWorkBinding& actor_binding(const WorkExecutionBinding& v,const StableId& action){take(validate_work_binding(v));const auto* actor=std::get_if<ActorActionWorkBinding>(&v);if(!actor||actor->action_id!=action)fail("SharedWorkActorBindingRequired");return *actor;}
bool same_state(const SharedWorkElapsedState& a,const SharedWorkElapsedState& z,ExactArithmeticBudget budget){
    if(a.action_ref!=z.action_ref||a.gate.index()!=z.gate.index())return false;const auto& x=a.execution;const auto& y=z.execution;if(x.work_unit_id!=y.work_unit_id||x.total_work_units!=y.total_work_units||x.completed_work_units!=y.completed_work_units||x.phase!=y.phase||x.running_policy_ref!=y.running_policy_ref||x.last_processed_period.has_value()!=y.last_processed_period.has_value()||(x.last_processed_period&&!same_work_period_ref(*x.last_processed_period,*y.last_processed_period)))return false;
    if(const auto* g=std::get_if<CraftWorkGate>(&a.gate)){const auto& h=std::get<CraftWorkGate>(z.gate);return same_work_binding(g->execution_binding,h.execution_binding)&&g->gate_ordinal==h.gate_ordinal&&same(g->gate_opened_at,h.gate_opened_at,budget)&&same(g->last_accounted_at,h.last_accounted_at,budget)&&take(compare_fractions(g->earned_service_units.value,h.earned_service_units.value,budget))==0&&g->pace_for_gate==h.pace_for_gate&&g->accepted_pace_source_ref==h.accepted_pace_source_ref&&g->rush_units_committed==h.rush_units_committed&&g->last_closed_gate_result_ref==h.last_closed_gate_result_ref&&g->pending_stage_ordinal==h.pending_stage_ordinal&&g->terminal_result_ref==h.terminal_result_ref;}
    if(const auto* g=std::get_if<OriginalHalfDayGate>(&a.gate)){const auto& h=std::get<OriginalHalfDayGate>(z.gate);return same_work_binding(g->original_execution_binding,h.original_execution_binding)&&g->original_gate_profile_ref==h.original_gate_profile_ref&&g->next_period_ordinal==h.next_period_ordinal&&g->last_consumed_period_ref.has_value()==h.last_consumed_period_ref.has_value()&&(!g->last_consumed_period_ref||same_work_period_ref(*g->last_consumed_period_ref,*h.last_consumed_period_ref));}
    const auto& g=std::get<LifeHalfDayStageGateState>(a.gate);const auto& h=std::get<LifeHalfDayStageGateState>(z.gate);return g.gate_profile_ref==h.gate_profile_ref&&same_source_ref(g.stage_rule_source_ref,h.stage_rule_source_ref)&&take(compare_fractions(g.earned_service_units.value,h.earned_service_units.value,budget))==0;
}
SharedWorkElapsedState* mutable_shared(ActionElapsedStore& store,const StableId& id){SharedWorkElapsedState* found=nullptr;for(auto& entry:store.entries)if(action_elapsed_entry_ref(entry)==id){if(found||entry.index()!=1)fail("MissingUniqueSharedWorkEntry");found=&std::get<SharedWorkElapsedState>(entry);}return found;}

// Keep the existing source checks outside this adoption boundary. The actual
// final four-vector graph is copied under the sole Store owner before any
// retained row changes; both the old Store and all borrowed inputs stay alive.
// This does not cover the earlier Prepared/source output construction.
void replace_shared_store(ActionElapsedStore& store,const SharedWorkElapsedState* previous,
    const SharedWorkElapsedState& next,const SharedWorkGateResult* gate=nullptr,
    std::size_t gate_position=0,const ActionElapsedTerminalBinding* terminal=nullptr){
    if((!previous&&store.entries.size()==SIZE_MAX)||
       (gate&&store.gate_results.size()==SIZE_MAX)||
       (terminal&&store.terminal_bindings.size()==SIZE_MAX))throw std::bad_alloc();
    if(gate&&gate_position>store.gate_results.size())fail("InvalidSharedWorkGatePosition");
    ActionElapsedStoreBuildView view;
    view.entries.reserve(store.entries.size()+(previous?0:1));
    view.gate_results.reserve(store.gate_results.size()+(gate?1:0));
    view.terminal_bindings.reserve(store.terminal_bindings.size()+(terminal?1:0));
    view.fixed_service_transitions.reserve(store.fixed_service_transitions.size());
    bool replaced=false;
    for(const auto& row:store.entries){
        if(previous&&std::get_if<SharedWorkElapsedState>(&row)==previous){
            view.entries.emplace_back(&next);replaced=true;
        }else std::visit([&](const auto& value){view.entries.emplace_back(&value);},row);
    }
    if(previous&&!replaced)fail("MissingSharedWorkReplacementTarget");
    if(!previous)view.entries.emplace_back(&next);
    for(std::size_t i=0;i<store.gate_results.size();++i){
        if(gate&&i==gate_position)view.gate_results.push_back(gate);
        view.gate_results.push_back(&store.gate_results[i]);
    }
    if(gate&&gate_position==store.gate_results.size())view.gate_results.push_back(gate);
    for(const auto& row:store.terminal_bindings)view.terminal_bindings.push_back(&row);
    if(terminal)view.terminal_bindings.push_back(terminal);
    for(const auto& row:store.fixed_service_transitions)view.fixed_service_transitions.push_back(&row);
    ActionElapsedStore replacement(view);store.swap(replacement);
}

// owner真实phase映射唯一；等待结算是有原保留源的等待，不能当作terminal。
WorkExecutionPhase original_work_phase(ActionTimeOwnerPhase phase){switch(phase){case ActionTimeOwnerPhase::working:return WorkExecutionPhase::working;case ActionTimeOwnerPhase::suspended:return WorkExecutionPhase::suspended;case ActionTimeOwnerPhase::awaiting_process_choice:return WorkExecutionPhase::awaiting_process_choice;case ActionTimeOwnerPhase::awaiting_output_space:return WorkExecutionPhase::awaiting_output_space;case ActionTimeOwnerPhase::awaiting_settlement:return WorkExecutionPhase::awaiting_settlement;case ActionTimeOwnerPhase::terminal:return WorkExecutionPhase::terminal;}fail("UnknownOriginalWorkPhase");return WorkExecutionPhase::terminal;}
// 真实终局引用逐字段比较，不依赖JSON复制或结果ID排序来判断因果。
bool same_original_time(const ElapsedTimeSource& a,const ElapsedTimeSource& z){if(a.index()!=z.index())return false;return std::visit([&](const auto& x){using T=std::decay_t<decltype(x)>;const auto& y=std::get<T>(z);if(x.source_schema!=y.source_schema||!same_source_ref(x.source_ref,y.source_ref))return false;if constexpr(std::is_same_v<T,OriginalPeriodSource>)return x.processed_tick==y.processed_tick;return true;},a);}
bool same_terminal_source(const ActionElapsedTerminalBinding& a,const ActionElapsedTerminalBinding& z){return a.action_ref==z.action_ref&&a.action_kind==z.action_kind&&a.actor_id==z.actor_id&&a.participant_actor_ids==z.participant_actor_ids&&same_source_ref(a.original_execution_source_ref,z.original_execution_source_ref)&&a.timing_kind==z.timing_kind&&same_original_time(a.started_source_ref,z.started_source_ref)&&same_original_time(a.ended_source_ref,z.ended_source_ref)&&a.terminal_result_ref==z.terminal_result_ref&&a.last_closed_gate_result_ref==z.last_closed_gate_result_ref&&a.retained_gate_result_refs==z.retained_gate_result_refs;}
// 原owner必须独立证明采用后的phase与原输入/余量；不能从请求的phase反造证明。
ActionTimeOwnerProof original_changed_owner(const SharedWorkElapsedState& state,const ActionTimeRegistration& registration,const ActionTimeOriginalSourceRead& source,const WorldMoment& at,ExactArithmeticBudget b){auto owner=take(source.read_action_time_owner(registration,state.action_ref));take(validate_action_time_owner_proof(registration,owner));if(registration.mode!=ActionTimeMode::shared_work||!registration.shared_gate_kind||*registration.shared_gate_kind!=state.gate.index()||owner.action_ref!=state.action_ref||owner.action_kind!=registration.action_kind||owner.actor_id.empty()||!valid_source_ref(owner.original_execution_source)||owner.original_execution_source.domain!=registration.owner_domain||owner.source_schema!=registration.source_schema)fail("SharedWorkOwnerChangeIdentityMismatch");std::visit([&](const auto& value){if(!valid_source_ref(value.source_ref)||value.source_schema<=0)fail("InvalidSharedWorkStartSource");},owner.started_source);auto started=take(source.read_action_time_source(owner.started_source));take(encode_world_moment(started,b));if(const auto* original=std::get_if<OriginalPeriodSource>(&owner.started_source))if(original->processed_tick<0||started.half_day_tick!=original->processed_tick||take(compare_fractions(started.sub_half_day,CanonicalFraction{},b))!=0)fail("SharedWorkOriginalStartBoundaryMismatch");if(take(compare_world_moments(started,at,b))>0)fail("SharedWorkOwnerChangeBeforeStart");return owner;}

}
PreparedSharedWorkSlice::PreparedSharedWorkSlice(const SharedWorkElapsedState& before_value,const SharedWorkElapsedState& entry_value,bool due)
    :copied_fields_(slice_memory(before_value,entry_value)),before(before_value),entry(entry_value),gate_due(due){}
PreparedSharedWorkSlice::PreparedSharedWorkSlice(const PreparedSharedWorkSlice& value)
    :PreparedSharedWorkSlice(value.before,value.entry,value.gate_due){}
Status adopt_shared_work_execution_in_candidate(
    ActionElapsedStore &store, const ActionCoreView &action,
    const SharedWorkElapsedState &initial,
    const ActionTimeRegistration &registration, const WorldMoment &at,
    const TimeScaleRef &scale, const ActionTimeOriginalSourceRead &source,
    ExactArithmeticBudget budget) {
  return guard<Done>([&] {
    take(validate_action_time_registration(registration));
    take(validate_time_scale(scale));
    take(validate_work_execution_state(initial.execution));
    if (registration.mode != ActionTimeMode::shared_work ||
        !registration.shared_gate_kind ||
        *registration.shared_gate_kind != initial.gate.index() ||
        action.id != initial.action_ref ||
        action.kind != registration.action_kind ||
        initial.execution.completed_work_units != 0 ||
        initial.execution.last_processed_period ||
        initial.execution.phase != WorkExecutionPhase::working)
      fail("InvalidSharedWorkAdoption");
    auto owner = take(source.read_action_time_owner(registration, action.id));
    auto began = take(source.read_action_time_source(owner.started_source));
    if (owner.action_ref != action.id || owner.action_kind != action.kind ||
        owner.actor_id != action.actor_id ||
        owner.participant_actor_ids != action.participants ||
        owner.phase != ActionTimeOwnerPhase::working || owner.ended_source ||
        owner.terminal_result_ref || !owner.retained_gate_result_refs.empty() ||
        !same(began, at, budget) || action.start_tick != at.half_day_tick)
      fail("SharedWorkAdoptionSourceMismatch");
    if (const auto *gate = std::get_if<CraftWorkGate>(&initial.gate)) {
      const auto &binding = actor_binding(gate->execution_binding, action.id);
      if (binding.action_kind != action.kind ||
          !same_source_ref(binding.source_owner_ref,
                           owner.original_execution_source) ||
          gate->gate_ordinal != 0 || gate->last_closed_gate_result_ref ||
          gate->pending_stage_ordinal || gate->terminal_result_ref ||
          !same(gate->gate_opened_at, at, budget) ||
          !same(gate->last_accounted_at, at, budget) ||
          take(compare_fractions(gate->earned_service_units.value,
                                 CanonicalFraction{}, budget)) != 0)
        fail("InvalidInitialCraftGate");
      take(validate_craft_work_gate(initial.execution, *gate,
                                    scale.units_per_half_day, budget));
    } else if (const auto *gate =
                   std::get_if<OriginalHalfDayGate>(&initial.gate)) {
      const auto &binding =
          actor_binding(gate->original_execution_binding, action.id);
      if (binding.action_kind != action.kind ||
          !same_source_ref(binding.source_owner_ref,
                           owner.original_execution_source) ||
          gate->next_period_ordinal != 0 || gate->last_consumed_period_ref ||
          take(compare_fractions(at.sub_half_day, CanonicalFraction{},
                                 budget)) != 0)
        fail("InvalidInitialOriginalGate");
    } else {
      const auto &life_gate = std::get<LifeHalfDayStageGateState>(initial.gate);
      if (take(compare_fractions(life_gate.earned_service_units.value,
                                 CanonicalFraction{}, budget)) != 0)
        fail("InvalidInitialLifeGate");
      take(validate_life_half_day_stage_gate(initial.execution, life_gate,
                                             scale.units_per_half_day, budget));
    }
    take(source.validate_shared_action_sources(registration, owner, initial));
    for (const auto &terminal : store.terminal_bindings)
      if (terminal.action_ref == action.id)
        fail("SharedWorkAdoptionAfterTerminal");
    if (auto *existing = mutable_shared(store, action.id)) {
      if (!same_state(*existing, initial, budget))
        fail("SharedWorkAdoptionReplayMismatch");
      return Done{};
    }
    replace_shared_store(store, nullptr, initial);
    return Done{};
  });
}
Status apply_shared_work_slice_in_candidate(ActionElapsedStore& store,PreparedSharedWorkSlice prepared,ExactArithmeticBudget budget){return guard<Done>([&]{auto* existing=mutable_shared(store,prepared.entry.action_ref);if(!existing)fail("MissingSharedWorkEntry");if(same_state(*existing,prepared.entry,budget))return Done{};if(!same_state(*existing,prepared.before,budget))fail("StaleSharedWorkSlice");if(prepared.before.action_ref!=prepared.entry.action_ref||prepared.before.gate.index()!=prepared.entry.gate.index()||prepared.before.execution.completed_work_units!=prepared.entry.execution.completed_work_units||prepared.before.execution.total_work_units!=prepared.entry.execution.total_work_units)fail("InvalidSharedWorkSlicePayload");replace_shared_store(store,existing,prepared.entry);return Done{};});}
Outcome<PreparedSharedWorkSlice> prepare_shared_work_slice(const SharedWorkElapsedState& state,const ActionCoreView& action,const WorkExecutionBinding& binding,const MomentSpan& span,const WorldMoment& current,const TimeScaleRef& scale,const WorkServiceIntervalRead& source,ExactArithmeticBudget b){return guard<PreparedSharedWorkSlice>([&]{
    take(validate_time_scale(scale));take(make_moment_span(span.from,span.to,b));take(validate_work_execution_state(state.execution));const auto& actor=actor_binding(binding,state.action_ref);
    if(state.action_ref!=action.id||actor.action_kind!=action.kind||!same(current,span.from,b)||state.execution.phase!=WorkExecutionPhase::working)fail("InvalidSharedWorkSliceBoundary");
    if(state.gate.index()==1)fail("OriginalHalfDayGateCannotAccumulateSlice");
    const auto* borrowed_proof=take(source.read_actual_work_service_interval(actor,span));
    if(!borrowed_proof)fail("MissingActualWorkServiceCoverage");
    const auto& proof=*borrowed_proof;
    if(!same_work_binding(proof.execution_binding,binding)||!same(proof.actual_span.from,span.from,b)||!same(proof.actual_span.to,span.to,b)||proof.participation_sources.empty())fail("MissingActualWorkServiceCoverage");
    BorrowedWorkIdSet actors;for(const auto& role:proof.actual_actor_roles)if(role.actor_id.empty()||role.role.empty()||!actors.insert(role.actor_id).second||std::find(action.participants.begin(),action.participants.end(),role.actor_id)==action.participants.end())fail("InvalidWorkServiceParticipant");
    if(!actors.count(action.actor_id))fail("MissingPrimaryWorkServiceParticipant");
    BorrowedWorkIdSet labor,claims;for(const auto& id:proof.actual_labor_allocations)if(id.empty()||!labor.insert(id).second)fail("DuplicateWorkServiceLabor");for(const auto& id:proof.facility_claim_refs)if(id.empty()||!claims.insert(id).second)fail("DuplicateWorkServiceFacility");
    for(std::size_t i=0;i<proof.participation_sources.size();++i){const auto& ref=proof.participation_sources[i];if(!valid_source_ref(ref.source_ref)||ref.source_schema<=0)fail("InvalidWorkServiceSource");for(std::size_t j=0;j<i;++j)if(ref.source_schema==proof.participation_sources[j].source_schema&&same_source_ref(ref.source_ref,proof.participation_sources[j].source_ref))fail("DuplicateWorkServiceSource");take(source.validate_work_service_participation_source(proof,ref));}
    PreparedSharedWorkSlice out{state,state,false};
    if(auto* craft=std::get_if<CraftWorkGate>(&out.entry.gate)){if(!same_work_binding(craft->execution_binding,binding))fail("CraftSliceBindingMismatch");auto prepared=take(prepare_craft_work_slice(state.execution,*craft,span,scale.units_per_half_day,b));craft->earned_service_units=std::move(prepared.earned_service_units);craft->last_accounted_at=prepared.last_accounted_at;out.gate_due=prepared.gate_due;}
    else{auto& gate=std::get<LifeHalfDayStageGateState>(out.entry.gate);gate.earned_service_units=take(prepare_life_half_day_stage_slice(state.execution,gate,span,scale.units_per_half_day,b));out.gate_due=take(compare_fractions(gate.earned_service_units.value,take(fixed_duration_work_units(1,scale.units_per_half_day,b)).value,b))==0;}
    return out;
});}
Outcome<PreparedSharedWorkGate> prepare_shared_work_gate(const SharedWorkElapsedState& state,const SharedWorkGateResult& result,const SharedWorkGateDisposition& disposition,const ActionTimeRegistration& registration,const TimeScaleRef& scale,const WorkPeriodSourceRead& periods,const ActionTimeOriginalSourceRead& source,ExactArithmeticBudget b){return guard<PreparedSharedWorkGate>([&]{
    take(validate_time_scale(scale));if(registration.mode!=ActionTimeMode::shared_work||!registration.shared_gate_kind||*registration.shared_gate_kind!=state.gate.index()||state.gate.index()!=result.index())fail("UnregisteredSharedGateConsumer");
    // 必须原owner已应用结果，时间到门本身不能构造一个空success。
    take(source.validate_original_gate_result(result));auto result_id=take(source.read_gate_result_id(result));if(result_id.empty())fail("MissingOriginalSharedGateResultId");auto canonical=take(source.read_original_gate_result(result_id));if(!same_shared_work_gate_result(result,canonical))fail("SharedGateOriginalResultMismatch");
    PreparedSharedWorkGate out{state,state,result,std::move(result_id)};
    bool old=false;if(const auto* gate=std::get_if<CraftWorkGate>(&state.gate)){const auto& r=std::get<CraftWorkGateResult>(result);old=r.gate_ordinal<gate->gate_ordinal||(gate->last_closed_gate_result_ref&&*gate->last_closed_gate_result_ref==out.original_result_id);}
    else if(const auto* gate=std::get_if<OriginalHalfDayGate>(&state.gate))old=std::get<OriginalHalfDayGateResult>(result).gate_ordinal<gate->next_period_ordinal;
    else old=std::get<LifeHalfDayStageGateResult>(result).completed_halfday_ordinal<state.execution.completed_work_units/scale.units_per_half_day;
    if(old){auto proof=take(source.read_action_time_owner(registration,state.action_ref));if(std::find(proof.retained_gate_result_refs.begin(),proof.retained_gate_result_refs.end(),out.original_result_id)==proof.retained_gate_result_refs.end())fail("SharedGateReplaySourceMissing");take(source.validate_shared_action_sources(registration,proof,state));out.replay=true;return out;}
    if(const auto* craft=std::get_if<CraftWorkGate>(&state.gate)){auto next=take(close_craft_work_gate(state.execution,*craft,std::get<CraftWorkGateResult>(result),disposition.phase,disposition.pending_stage_ordinal,disposition.terminal_result_ref,scale.units_per_half_day,b));out.entry.execution=std::move(next.execution);out.entry.gate=std::move(next.gate);}
    else if(const auto* original=std::get_if<OriginalHalfDayGate>(&state.gate)){if(disposition.pending_stage_ordinal)fail("OriginalGateUnexpectedCraftStage");auto next=take(consume_original_half_day_gate(state.execution,*original,std::get<OriginalHalfDayGateResult>(result),periods,scale.units_per_half_day));next.execution.phase=disposition.phase;take(validate_work_execution_state(next.execution));out.entry.execution=std::move(next.execution);out.entry.gate=std::move(next.gate);}
    else{if(disposition.pending_stage_ordinal)fail("LifeGateUnexpectedCraftStage");const auto& life=std::get<LifeHalfDayStageGateResult>(result);const auto* life_reader=source.life_work_period_sources(life.work_period_ref);if(!life_reader)fail("MissingOriginalLifeWorkPeriodReader");auto actual=take(read_verified_life_work_period(life.work_period_ref,*life_reader,scale.units_per_half_day,b));const auto& binding=actor_binding(actual.execution_binding,state.action_ref);auto proof=take(source.read_action_time_owner(registration,state.action_ref));if(binding.action_kind!=registration.action_kind||!same_source_ref(binding.source_owner_ref,proof.original_execution_source))fail("LifeGateExecutionBindingMismatch");auto next=take(consume_life_half_day_stage_gate(state.execution,std::get<LifeHalfDayStageGateState>(state.gate),life,*life_reader,disposition.phase,scale.units_per_half_day,b));out.entry.execution=std::move(next.execution);out.entry.gate=std::move(next.gate);}
    auto owner=take(source.read_action_time_owner(registration,state.action_ref));take(validate_action_time_owner_proof(registration,owner));if(original_work_phase(owner.phase)!=disposition.phase)fail("SharedGateOwnerPhaseMismatch");take(source.validate_shared_action_sources(registration,owner,out.entry));
    if((disposition.phase==WorkExecutionPhase::terminal)!=owner.terminal_result_ref.has_value()||disposition.terminal_result_ref!=owner.terminal_result_ref)fail("SharedGateTerminalSourceMismatch");
    return out;
});}
Status apply_shared_work_gate_in_candidate(ActionElapsedStore& store,PreparedSharedWorkGate prepared,const ActionTimeOriginalSourceRead& source,ExactArithmeticBudget budget){return guard<Done>([&]{
    SharedWorkElapsedState* entry=nullptr;for(auto& candidate:store.entries)if(action_elapsed_entry_ref(candidate)==prepared.entry.action_ref){if(entry||candidate.index()!=1)fail("MissingUniqueSharedWorkEntry");entry=&std::get<SharedWorkElapsedState>(candidate);}
    if(!entry)fail("MissingSharedWorkEntry");
    auto position=store.gate_results.end();for(auto i=store.gate_results.begin();i!=store.gate_results.end();++i){auto id=take(source.read_gate_result_id(*i));if(id==prepared.original_result_id){if(!same_shared_work_gate_result(*i,prepared.result))fail("SharedGateReplayMismatch");take(source.validate_original_gate_result(*i));return Done{};}if(id>prepared.original_result_id){position=i;break;}}
    if(prepared.replay)fail("SharedGateReplayHistoryMissing");
    if(!same_state(*entry,prepared.before,budget))fail("StaleSharedWorkGate");
    if(entry->execution.phase==WorkExecutionPhase::terminal||prepared.entry.execution.completed_work_units<entry->execution.completed_work_units)fail("TerminalOrRewoundSharedGate");
    // 这是未发布候选：任一后续原资源/身体/组合核失败必须整体丢弃，不可发布半个gate。
    replace_shared_store(store,entry,prepared.entry,&prepared.result,
        static_cast<std::size_t>(position-store.gate_results.begin()));return Done{};
});}
Outcome<ActionElapsedTerminalBinding> prepare_shared_work_terminal_binding(const SharedWorkElapsedState& entry,const ActionTimeRegistration& registration,const ActionTimeOriginalSourceRead& source,const WorldMoment& current,ExactArithmeticBudget b){return guard<ActionElapsedTerminalBinding>([&]{
    if(entry.execution.phase!=WorkExecutionPhase::terminal||registration.mode!=ActionTimeMode::shared_work)fail("SharedWorkNotTerminal");auto owner=original_changed_owner(entry,registration,source,current,b);take(source.validate_shared_action_sources(registration,owner,entry));if(owner.phase!=ActionTimeOwnerPhase::terminal||!owner.ended_source||!owner.terminal_result_ref)fail("MissingSharedWorkTerminalSource");auto started=take(source.read_action_time_source(owner.started_source));auto ended=take(source.read_action_time_source(*owner.ended_source));if(take(compare_world_moments(started,ended,b))>0||take(compare_world_moments(ended,current,b))>0)fail("SharedWorkTerminalTimeMismatch");ActionElapsedTerminalBinding result;result.action_ref=entry.action_ref;result.action_kind=owner.action_kind;result.actor_id=owner.actor_id;result.participant_actor_ids=owner.participant_actor_ids;result.original_execution_source_ref=owner.original_execution_source;result.timing_kind=1;result.started_source_ref=owner.started_source;result.ended_source_ref=*owner.ended_source;result.terminal_result_ref=*owner.terminal_result_ref;result.retained_gate_result_refs=owner.retained_gate_result_refs;Integer last=-1;for(const auto& id:owner.retained_gate_result_refs){auto gate=take(source.read_original_gate_result(id));const auto ordinal=std::visit([](const auto& value){using T=std::decay_t<decltype(value)>;if constexpr(std::is_same_v<T,LifeHalfDayStageGateResult>)return value.completed_halfday_ordinal;else return value.gate_ordinal;},gate);if(ordinal>last){last=ordinal;result.last_closed_gate_result_ref=id;}}return result;
});}
// 先结原服务、原owner写真实状态，再采用；本函数只变既有gate状态和可追溯终局绑定。
Outcome<PreparedSharedWorkOwnerChange> prepare_shared_work_owner_change(const SharedWorkElapsedState& before,const SharedWorkGateDisposition& requested,const ActionTimeRegistration& registration,const TimeScaleRef& scale,const ActionTimeOriginalSourceRead& source,const WorldMoment& at,ExactArithmeticBudget b){return guard<PreparedSharedWorkOwnerChange>([&]{
    take(validate_action_time_registration(registration));take(validate_time_scale(scale));take(encode_world_moment(at,b));take(validate_work_execution_state(before.execution));auto owner=original_changed_owner(before,registration,source,at,b);
    const auto actual_phase=original_work_phase(owner.phase);if(requested.phase!=actual_phase||requested.terminal_result_ref!=owner.terminal_result_ref)fail("SharedWorkOwnerPhaseSourceMismatch");
    if(before.execution.phase==WorkExecutionPhase::terminal&&actual_phase!=WorkExecutionPhase::terminal)fail("TerminalSharedWorkCannotResume");
    PreparedSharedWorkOwnerChange out{before,before,at,{}};
    if(const auto* old=std::get_if<CraftWorkGate>(&before.gate)){
        if(before.execution.phase==WorkExecutionPhase::working&&actual_phase!=WorkExecutionPhase::working&&!same(old->last_accounted_at,at,b))fail("SharedWorkServiceNotSettledAtChange");
        if(actual_phase==WorkExecutionPhase::working&&before.execution.phase!=WorkExecutionPhase::working){auto resumed=take(resume_craft_work_gate(before.execution,*old,at,old->gate_ordinal,old->last_closed_gate_result_ref,scale.units_per_half_day,b));out.entry.execution=std::move(resumed.execution);out.entry.gate=std::move(resumed.gate);}
        else {out.entry.execution.phase=actual_phase;auto& next=std::get<CraftWorkGate>(out.entry.gate);next.pending_stage_ordinal=requested.pending_stage_ordinal;next.terminal_result_ref=requested.terminal_result_ref;}
        auto& next=std::get<CraftWorkGate>(out.entry.gate);if(next.pending_stage_ordinal!=requested.pending_stage_ordinal||next.terminal_result_ref!=requested.terminal_result_ref)fail("SharedWorkOwnerDispositionMismatch");take(validate_craft_work_gate(out.entry.execution,next,scale.units_per_half_day,b));
    }else {
        if(requested.pending_stage_ordinal)fail("NonCraftWorkUnexpectedPendingStage");out.entry.execution.phase=actual_phase;
        if(const auto* life=std::get_if<LifeHalfDayStageGateState>(&out.entry.gate))take(validate_life_half_day_stage_gate(out.entry.execution,*life,scale.units_per_half_day,b));
        else {const auto& gate=std::get<OriginalHalfDayGate>(out.entry.gate);actor_binding(gate.original_execution_binding,before.action_ref);if(out.entry.execution.completed_work_units%scale.units_per_half_day||out.entry.execution.total_work_units%scale.units_per_half_day||(actual_phase==WorkExecutionPhase::working&&out.entry.execution.completed_work_units==out.entry.execution.total_work_units))fail("InvalidOriginalWorkOwnerChange");take(validate_work_execution_state(out.entry.execution));}
    }
    // 它仍是原唯一累计；专业reader核同一原暂停/恢复/取消/物化结果及保留责任。
    take(source.validate_shared_action_sources(registration,owner,out.entry));
    if(actual_phase==WorkExecutionPhase::terminal){if(!owner.ended_source||!same(take(source.read_action_time_source(*owner.ended_source)),at,b))fail("SharedWorkTerminalChangeTimeMismatch");out.terminal=take(prepare_shared_work_terminal_binding(out.entry,registration,source,at,b));}
    else if(owner.ended_source||owner.terminal_result_ref)fail("ActiveSharedWorkHasTerminalSource");
    return out;
});}
// 前像与原source同检；重复终局必须同payload，不分新ID、不再释放或结算原资源。
Status apply_shared_work_owner_change_in_candidate(ActionElapsedStore& store,PreparedSharedWorkOwnerChange prepared,const ActionTimeRegistration& registration,const TimeScaleRef& scale,const ActionTimeOriginalSourceRead& source,ExactArithmeticBudget b){return guard<Done>([&]{
    auto* entry=mutable_shared(store,prepared.entry.action_ref);if(!entry)fail("MissingSharedWorkEntry");if(!same_state(*entry,prepared.before,b)&&!same_state(*entry,prepared.entry,b))fail("StaleSharedWorkOwnerChange");
    SharedWorkGateDisposition disposition;disposition.phase=prepared.entry.execution.phase;if(const auto* craft=std::get_if<CraftWorkGate>(&prepared.entry.gate)){disposition.pending_stage_ordinal=craft->pending_stage_ordinal;disposition.terminal_result_ref=craft->terminal_result_ref;}else if(prepared.terminal)disposition.terminal_result_ref=prepared.terminal->terminal_result_ref;
    auto canonical=take(prepare_shared_work_owner_change(prepared.before,disposition,registration,scale,source,prepared.actual_at,b));if(!same_state(canonical.entry,prepared.entry,b)||canonical.terminal.has_value()!=prepared.terminal.has_value()||(canonical.terminal&&!same_terminal_source(*canonical.terminal,*prepared.terminal)))fail("SharedWorkOwnerChangePayloadMismatch");
    const ActionElapsedTerminalBinding* retained=nullptr;for(const auto& row:store.terminal_bindings)if(row.action_ref==prepared.entry.action_ref){if(retained)fail("DuplicateActionTerminalBinding");retained=&row;}
    if(retained&&(!prepared.terminal||!same_terminal_source(*retained,*prepared.terminal)))fail("SharedWorkTerminalReplayMismatch");
    replace_shared_store(store,entry,prepared.entry,nullptr,0,
        prepared.terminal&&!retained?&*prepared.terminal:nullptr);return Done{};
});}

// 变速来源不产生服务；前后同一个Craft gate，只沿实际owner已采用的变更反核。
Outcome<PreparedSharedCraftPaceChange> prepare_shared_craft_pace_change(
    const SharedWorkElapsedState& before,CraftPace pace,const MomentOwnerSource& actual,
    Integer ordinal,const std::optional<StableId>& previous,const ActionTimeRegistration& registration,
    const TimeScaleRef& scale,const ActionTimeOriginalSourceRead& source,const WorldMoment& at,
    ExactArithmeticBudget budget){return guard<PreparedSharedCraftPaceChange>([&]{
    take(validate_action_time_registration(registration));take(validate_time_scale(scale));take(encode_world_moment(at,budget));
    const auto* old=std::get_if<CraftWorkGate>(&before.gate);
    if(!old||registration.mode!=ActionTimeMode::shared_work||registration.shared_gate_kind!=std::optional<std::size_t>{0})
        fail_pace("SharedCraftPaceOwnerRequired");
    if(!valid_source_ref(actual.source_ref)||actual.source_ref.domain!=registration.owner_domain||actual.source_schema!=registration.source_schema)
        fail_pace("SharedCraftPaceOriginalSourceRequired");
    auto owner=original_changed_owner(before,registration,source,at,budget);
    const auto& binding=actor_binding(old->execution_binding,before.action_ref);
    if(binding.action_kind!=owner.action_kind||!same_source_ref(binding.source_owner_ref,owner.original_execution_source)||
       owner.phase!=ActionTimeOwnerPhase::working||owner.ended_source||owner.terminal_result_ref)
        fail_pace("SharedCraftPaceOriginalOwnerMismatch");
    if(!same(take(source.read_action_time_source(ElapsedTimeSource{actual})),at,budget))
        fail_pace("SharedCraftPaceActualMomentMismatch");
    if(!same(old->last_accounted_at,at,budget))fail_pace("SharedCraftPaceUnsettledService");
    auto changed=take(change_craft_pace(before.execution,*old,pace,actual.source_ref.id,ordinal,previous,scale.units_per_half_day,budget));
    PreparedSharedCraftPaceChange out{before,before,actual,at};out.entry.execution=std::move(changed.execution);out.entry.gate=std::move(changed.gate);
    take(source.validate_shared_action_sources(registration,owner,out.entry));
    return out;
});}
Status apply_shared_craft_pace_change_in_candidate(ActionElapsedStore& store,PreparedSharedCraftPaceChange prepared,
    const ActionTimeRegistration& registration,const TimeScaleRef& scale,const ActionTimeOriginalSourceRead& source,
    ExactArithmeticBudget budget){return guard<Done>([&]{
    auto* entry=mutable_shared(store,prepared.entry.action_ref);
    if(!entry)fail_pace("MissingSharedCraftPaceEntry");
    if(!same_state(*entry,prepared.before,budget)&&!same_state(*entry,prepared.entry,budget))fail_pace("StaleSharedCraftPaceChange");
    const auto* old=std::get_if<CraftWorkGate>(&prepared.before.gate);const auto* next=std::get_if<CraftWorkGate>(&prepared.entry.gate);
    if(!old||!next)fail_pace("SharedCraftPaceGateRequired");
    auto actual=take(prepare_shared_craft_pace_change(prepared.before,next->pace_for_gate,prepared.actual_source,
        old->gate_ordinal,old->last_closed_gate_result_ref,registration,scale,source,prepared.actual_at,budget));
    if(!same_state(actual.entry,prepared.entry,budget))fail_pace("SharedCraftPacePayloadMismatch");
    for(const auto& terminal:store.terminal_bindings)if(terminal.action_ref==prepared.entry.action_ref)fail_pace("SharedCraftPaceAfterTerminal");
    replace_shared_store(store,entry,prepared.entry);return Done{};
});}

}
