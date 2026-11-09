// 固定工期精确计量只生成未发布数值；原owner完成源和全世界原子发布由真实调用者负责。
#include "world_timeline.hpp"
#include "world_copy_plan.hpp"
#include <charconv>
#include <limits>
#include <new>
#include <stdexcept>
namespace hanmo::resident {
namespace {
// 统一传播原精确算术错误，不把资源不足降格成输入错或空成功。
struct TimelineFailure { RuleError error; };
// 数值和结构出口使用同一错误域，缺真实引用不能用默认字符串补齐。
void fail(const char* code){RuleError e;e.original={code,"world_timeline"};throw TimelineFailure{std::move(e)};}
// 每步失败停止当前纯计算，调用方输入保持不变。
template<class T> T take(Outcome<T> value){if(!value.ok)throw TimelineFailure{std::move(value.error)};return std::move(value.value);}
// 异常只损失本次拥有值；没有WorldBundle部分写入。
template<class T,class F> Outcome<T> guard(F&& f){try{return {true,f(),{}};}catch(const TimelineFailure& e){return {false,{},e.error};}catch(const std::bad_alloc&){RuleError e;e.original={"ExactTimelineResourceExhausted","world_timeline"};e.kind=ErrorKind::internal;return {false,{},std::move(e)};}catch(const std::length_error&){RuleError e;e.original={"ExactTimelineResourceExhausted","world_timeline"};e.kind=ErrorKind::internal;return {false,{},std::move(e)};}}
// 原required是正整数工量，读档必须保同一范围而不是接受任意分数工期。
void required_integer(const WorkUnits& units,ExactArithmeticBudget b){auto wire=take(encode_canonical_fraction(units.value,b));Integer value=0;auto parsed=std::from_chars(wire.numerator.data(),wire.numerator.data()+wire.numerator.size(),value);if(wire.denominator!="1"||parsed.ec!=std::errc{}||parsed.ptr!=wire.numerator.data()+wire.numerator.size()||value<=0)fail("InvalidFixedRequiredUnits");}
// 非空原source只作结构门，真实反读与费用once仍由Growth/WorldBundle验证。
void references(const ActionElapsedState& s){if(s.action_ref.empty()||s.execution_source_ref.empty()||s.duration_definition_ref.empty()||s.owner_cost_state_ref.empty())fail("MissingFixedElapsedSource");if(s.owner_work_remainder<0)fail("InvalidOwnerWorkRemainder");}
// 字面Moment相等必须经精确比较，不能比较浮点或兼容tick。
bool same(const WorldMoment& a,const WorldMoment& b,ExactArithmeticBudget budget){return take(compare_world_moments(a,b,budget))==0;}
}
namespace {
ElapsedTimingBuildView borrowed_timing(const ElapsedTiming& value){return std::visit([](const auto& row)->ElapsedTimingBuildView{using T=std::decay_t<decltype(row)>;if constexpr(std::is_same_v<T,ElapsedRunning>)return &row;else if constexpr(std::is_same_v<T,ElapsedSuspended>)return ElapsedSuspendedBuildView{row.reason_source_ref};else return ElapsedTerminalBuildView{row.outcome_ref};},value);}
ActionElapsedStateBuildView borrowed_elapsed(const ActionElapsedState& value,const ElapsedTimingBuildView* replacement=nullptr){return {value.action_ref,value.execution_source_ref,value.duration_definition_ref,value.started_at,value.required_units,value.credited_units,value.last_settled_at,replacement?*replacement:borrowed_timing(value.timing),value.owner_work_remainder,value.owner_cost_state_ref};}
CheckpointCopyLease elapsed_row_memory(const ActionElapsedStateBuildView& value){return CheckpointCopyLease::reserve();}
ElapsedTiming copied_timing(const ElapsedTimingBuildView& value){return std::visit([](const auto& row)->ElapsedTiming{using T=std::decay_t<decltype(row)>;if constexpr(std::is_same_v<T,const ElapsedRunning*>){if(!row)throw std::bad_alloc();return *row;}else if constexpr(std::is_same_v<T,ElapsedSuspendedBuildView>)return ElapsedSuspended{checkpoint_fresh_string_copy(row.reason_source_ref)};else return ElapsedTerminal{checkpoint_fresh_string_copy(row.outcome_ref)};},value);}
}
ActionElapsedState::ActionElapsedState(const ActionElapsedStateBuildView& value)
    :copied_fields(elapsed_row_memory(value)),action_ref(checkpoint_fresh_string_copy(value.action_ref)),execution_source_ref(checkpoint_fresh_string_copy(value.execution_source_ref)),duration_definition_ref(checkpoint_fresh_string_copy(value.duration_definition_ref)),started_at(value.started_at),required_units(value.required_units),credited_units(value.credited_units),last_settled_at(value.last_settled_at),timing(copied_timing(value.timing)),owner_work_remainder(value.owner_work_remainder),owner_cost_state_ref(checkpoint_fresh_string_copy(value.owner_cost_state_ref)){}
ActionElapsedState::ActionElapsedState(const ActionElapsedState& value):ActionElapsedState(borrowed_elapsed(value)){}
ActionElapsedState::ActionElapsedState(ActionElapsedState&& value):ActionElapsedState(borrowed_elapsed(value)){}
ActionElapsedState& ActionElapsedState::operator=(const ActionElapsedState& value){if(this!=&value){ActionElapsedState fresh(value);swap(fresh);}return *this;}
ActionElapsedState& ActionElapsedState::operator=(ActionElapsedState&& value){if(this!=&value){ActionElapsedState fresh(std::move(value));swap(fresh);}return *this;}
void ActionElapsedState::swap(ActionElapsedState& value){
    if(this==&value)return;auto left=copied_fields,right=value.copied_fields;auto both=CheckpointCopyLease::hold_both(left,right);copied_fields=both;value.copied_fields=both;
    using std::swap;swap(action_ref,value.action_ref);swap(execution_source_ref,value.execution_source_ref);swap(duration_definition_ref,value.duration_definition_ref);swap(started_at,value.started_at);swap(required_units,value.required_units);swap(credited_units,value.credited_units);swap(last_settled_at,value.last_settled_at);swap(timing,value.timing);swap(owner_work_remainder,value.owner_work_remainder);swap(owner_cost_state_ref,value.owner_cost_state_ref);
    copied_fields=std::move(right);value.copied_fields=std::move(left);
}
void ActionElapsedState::replace_timing(const ElapsedTimingBuildView& timing_value){ActionElapsedState fresh(borrowed_elapsed(*this,&timing_value));swap(fresh);}
namespace {
CheckpointMemoryCharge elapsed_store_memory(const ActionElapsedStore& value){return CheckpointMemoryCharge(0);}
CheckpointMemoryCharge elapsed_store_memory(const ActionElapsedStoreBuildView& value){return CheckpointMemoryCharge(0);}
}
ActionElapsedStore::ActionElapsedStore(const ActionElapsedStore& value)
    :copied_fields(elapsed_store_memory(value)),entries(value.entries),gate_results(value.gate_results),terminal_bindings(value.terminal_bindings),fixed_service_transitions(value.fixed_service_transitions){}
ActionElapsedStore::ActionElapsedStore(const ActionElapsedStoreBuildView& value):copied_fields(elapsed_store_memory(value)){
    entries.reserve(value.entries.size());
    for(const auto& row:value.entries)std::visit([&](const auto* original){if(!original)throw std::bad_alloc();entries.emplace_back(*original);},row);
    gate_results.reserve(value.gate_results.size());for(const auto* row:value.gate_results){if(!row)throw std::bad_alloc();gate_results.emplace_back(*row);}
    terminal_bindings.reserve(value.terminal_bindings.size());for(const auto* row:value.terminal_bindings){if(!row)throw std::bad_alloc();terminal_bindings.emplace_back(*row);}
    fixed_service_transitions.reserve(value.fixed_service_transitions.size());for(const auto* row:value.fixed_service_transitions){if(!row)throw std::bad_alloc();fixed_service_transitions.emplace_back(*row);}
}
void ActionElapsedStore::swap(ActionElapsedStore& value)noexcept{
    copied_fields.swap(value.copied_fields);entries.swap(value.entries);gate_results.swap(value.gate_results);terminal_bindings.swap(value.terminal_bindings);fixed_service_transitions.swap(value.fixed_service_transitions);
}
// TimeScale为原内容固定值；此处不生成默认N或默认版本。
Status validate_time_scale(const TimeScaleRef& scale){return guard<Done>([&]{if(scale.definition_id.empty()||scale.version<=0||scale.units_per_half_day<=0)fail("InvalidTimeScaleRef");return Done{};});}
// current只从Common整部和S0余量临时合成，不持久复制整钟。
Outcome<WorldMoment> read_world_moment(Integer common_tick,const WorldTimelineState& timeline,ExactArithmeticBudget budget){return guard<WorldMoment>([&]{take(validate_time_scale(timeline.scale_ref));WorldMoment current{common_tick,timeline.sub_half_day};take(encode_world_moment(current,budget));return current;});}
// Ready只在整数稳定界；Optional源是否必需由已选择真实macro-provider闭包反验，不伪造空供给回执。
Status validate_timeline_value(Integer common_tick,const WorldTimelineState& s,ExactArithmeticBudget budget){return guard<Done>([&]{auto current=take(read_world_moment(common_tick,s,budget));if(s.period_id.empty())fail("MissingTimelinePeriod");if((s.opening_receipt_ref&&s.opening_receipt_ref->empty())||(s.close_receipt_ref&&s.close_receipt_ref->empty()))fail("EmptyTimelineReceiptRef");switch(s.boundary_state){case PeriodBoundaryState::ready:if(take(compare_fractions(current.sub_half_day,CanonicalFraction{},budget))!=0)fail("ReadyTimelineNotBoundary");break;case PeriodBoundaryState::open:break;default:fail("InvalidPeriodBoundaryState");}return Done{};});}
// 开工只接受已建立的原source与真实时刻；H*N溢出与due越Integer都拒绝。
Outcome<ActionElapsedState> begin_fixed_elapsed(const StableId& action,const StableId& execution,const StableId& duration_definition,const StableId& cost_state,const WorldMoment& at,Integer half_days,const TimeScaleRef& scale,ExactArithmeticBudget budget){return guard<ActionElapsedState>([&]{take(validate_time_scale(scale));WorkUnits initial_units;ElapsedRunning initial_timing;ActionElapsedState out(ActionElapsedStateBuildView{action,execution,duration_definition,at,initial_units,initial_units,at,&initial_timing,0,cost_state});references(out);out.started_at=at;out.last_settled_at=at;out.required_units=take(fixed_duration_work_units(half_days,scale.units_per_half_day,budget));auto duration=take(scale_units_to_duration(out.required_units,scale.units_per_half_day,budget));out.timing=ElapsedRunning{take(add_world_duration(at,duration,budget))};take(validate_fixed_elapsed_value(out,at,scale,budget));return out;});}
// 稳定世界所有Running已结至current，满额必须由原owner同候选产生真实Terminal。
static Status validate_fixed_elapsed_phase_value(const ActionElapsedState& s,const WorldMoment& current,const TimeScaleRef& scale,ExactArithmeticBudget budget,bool completion_due){return guard<Done>([&]{take(validate_time_scale(scale));references(s);required_integer(s.required_units,budget);take(encode_world_moment(current,budget));if(take(compare_world_moments(s.started_at,s.last_settled_at,budget))>0||take(compare_world_moments(s.last_settled_at,current,budget))>0)fail("InvalidFixedElapsedTimeOrder");auto remaining=take(subtract_work_units(s.required_units,s.credited_units,budget));auto remaining_duration=take(scale_units_to_duration(remaining,scale.units_per_half_day,budget));const auto outstanding=take(compare_fractions(remaining.value,CanonicalFraction{},budget))>0;if(completion_due&&(!std::holds_alternative<ElapsedRunning>(s.timing)||outstanding))fail("FixedCompletionDuePhaseRequired");if(const auto* running=std::get_if<ElapsedRunning>(&s.timing)){if(completion_due){if(!same(s.last_settled_at,current,budget)||!same(current,running->completion_due_at,budget))fail("FixedCompletionDuePhaseMismatch");return Done{};}if(!outstanding)fail("FullFixedElapsedStillRunning");if(!same(s.last_settled_at,current,budget))fail("UnsettledFixedElapsed");if(take(compare_world_moments(current,running->completion_due_at,budget))>=0)fail("UnconsumedFixedElapsedDue");auto expected=take(add_world_duration(current,remaining_duration,budget));if(!same(expected,running->completion_due_at,budget))fail("FixedElapsedDueMismatch");}else if(const auto* suspended=std::get_if<ElapsedSuspended>(&s.timing)){if(!outstanding||suspended->reason_source_ref.empty())fail("InvalidFixedElapsedSuspension");}else if(const auto* terminal=std::get_if<ElapsedTerminal>(&s.timing)){if(terminal->outcome_ref.empty())fail("MissingFixedElapsedOutcome");}else fail("InvalidFixedElapsedTiming");return Done{};});}
Status validate_fixed_elapsed_value(const ActionElapsedState& s,const WorldMoment& current,const TimeScaleRef& scale,ExactArithmeticBudget b){return validate_fixed_elapsed_phase_value(s,current,scale,b,false);}
// 只用于原完工writer前的同候选实窗读取；不会建立终态、改变信用或允许稳定发布满额Running。
Status validate_fixed_elapsed_at_completion_due(const ActionElapsedState& s,const WorldMoment& current,const TimeScaleRef& scale,ExactArithmeticBudget b){return validate_fixed_elapsed_phase_value(s,current,scale,b,true);}
// 兼容字段由真实剩余量单向投影；不可反过来用于scheduler或复原过去。
Outcome<Integer> fixed_elapsed_remaining_ticks(const ActionElapsedState& s,const TimeScaleRef& scale,ExactArithmeticBudget budget){return guard<Integer>([&]{take(validate_time_scale(scale));auto remaining=take(subtract_work_units(s.required_units,s.credited_units,budget));return take(ceil_half_day_duration(take(scale_units_to_duration(remaining,scale.units_per_half_day,budget)),budget));});}
Outcome<FixedServiceGateBoundary> next_fixed_service_halfday_gate(const ActionElapsedState& s,const TimeScaleRef& scale,ExactArithmeticBudget b){return guard<FixedServiceGateBoundary>([&]{
    take(validate_fixed_elapsed_value(s,s.last_settled_at,scale,b));if(!std::holds_alternative<ElapsedRunning>(s.timing))fail("FixedServiceGateNotRunning");
    auto supplied=take(scale_units_to_duration(s.credited_units,scale.units_per_half_day,b));Integer ordinal=take(ceil_half_day_duration(supplied,b));auto integer=take(normalize_fraction(std::to_string(ordinal),"1",b));if(take(compare_fractions(supplied.value,integer,b))==0){if(ordinal==std::numeric_limits<Integer>::max())fail("FixedServiceGateOverflow");++ordinal;}
    auto boundary=take(fixed_duration_work_units(ordinal,scale.units_per_half_day,b));if(take(compare_fractions(boundary.value,s.required_units.value,b))>0)fail("FixedServiceGateExceedsRequired");auto remaining=take(subtract_work_units(boundary,s.credited_units,b));auto due=take(add_world_duration(s.last_settled_at,take(scale_units_to_duration(remaining,scale.units_per_half_day,b)),b));return FixedServiceGateBoundary{ordinal,std::move(boundary),std::move(due)};
});}
// due片只返回尚未发布的数值；真实完工失败时原世界及RNG仍由候选边界保留。
Outcome<FixedElapsedSlice> prepare_fixed_elapsed_slice(const ActionElapsedState& s,const MomentSpan& span,const TimeScaleRef& scale,ExactArithmeticBudget budget){return guard<FixedElapsedSlice>([&]{take(make_moment_span(span.from,span.to,budget));take(validate_fixed_elapsed_value(s,span.from,scale,budget));const auto* running=std::get_if<ElapsedRunning>(&s.timing);if(!running)fail("FixedElapsedNotRunning");if(take(compare_world_moments(span.to,running->completion_due_at,budget))>0)fail("FixedElapsedCrossesDue");auto delta=take(duration_to_scale_units(take(elapsed_world_duration(span.from,span.to,budget)),scale.units_per_half_day,budget));FixedElapsedSlice result{take(add_work_units(s.credited_units,delta,budget)),span.to,same(span.to,running->completion_due_at,budget)};auto full=take(compare_fractions(result.credited_units.value,s.required_units.value,budget));if(full>0||(result.due!=(full==0)))fail("FixedElapsedCreditMismatch");return result;});}
} // 没有provider默认成功，没有按墙钟补时，也没有第二份remaining余额。
