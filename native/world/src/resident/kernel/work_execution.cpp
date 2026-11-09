// 共享工作数学与来源消费；所有公开写口返回独立候选，由原owner与Common资源一起原子采用。
#include "work_execution.hpp"
#include "life_work_period.hpp"
#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>
#include <tuple>
namespace hanmo::resident {
namespace {
struct WorkFailure { RuleError error; };
// 留首个真实来源/算术失败，资源耗尽归技术故障，不转成业务no_work。
void fail(const char* code){RuleError e;e.original={code,"work_execution"};throw WorkFailure{std::move(e)};}
template<class T> T take(Outcome<T> r){if(!r.ok)throw WorkFailure{std::move(r.error)};return std::move(r.value);}
template<class T,class F> Outcome<T> guard(F f){try{return {true,f(),{}};}catch(WorkFailure& e){return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){RuleError e;e.original={"WorkResourceExhausted","work_execution"};e.kind=ErrorKind::internal;return {false,{},std::move(e)};}catch(const std::length_error&){RuleError e;e.original={"WorkResourceExhausted","work_execution"};e.kind=ErrorKind::internal;return {false,{},std::move(e)};}}
Integer add(Integer a,Integer b){if(a<0||b<0||b>std::numeric_limits<Integer>::max()-a)fail("WorkUnitsOverflow");return a+b;}
// ID集合只做重复检查，不改变原业务/source采用次序。
void ids(const std::vector<StableId>& values){std::set<StableId> seen;for(const auto& v:values)if(v.empty()||!seen.insert(v).second)fail("DuplicateWorkSource");}
void refs(const std::vector<SourceRef>& values){std::set<std::tuple<StableId,StableId,StableId>> seen;for(const auto& v:values)if(!valid_source_ref(v)||!seen.emplace(v.domain,v.kind,v.id).second)fail("DuplicateWorkSource");}
bool same_optional_period(const std::optional<WorkPeriodRef>& a,const std::optional<WorkPeriodRef>& b){return a.has_value()==b.has_value()&&(!a||same_work_period_ref(*a,*b));}
// 原工艺工作量是normal_thirds整数；N只计经过服务，二者绝不能当同量纲相加。
void craft_value(const WorkExecutionState& s,const CraftWorkGate& g,Integer n,ExactArithmeticBudget b,bool closing){
    take(validate_work_execution_state(s));take(validate_work_binding(g.execution_binding));
    if(!std::holds_alternative<ActorActionWorkBinding>(g.execution_binding)||n<=0||g.gate_ordinal<0||g.accepted_pace_source_ref.empty()||g.rush_units_committed<0||g.rush_units_committed>s.completed_work_units)fail("InvalidCraftGate");
    if(g.pace_for_gate!=CraftPace::normal&&g.pace_for_gate!=CraftPace::rush)fail("InvalidCraftPace");
    if(take(compare_world_moments(g.gate_opened_at,g.last_accounted_at,b))>0)fail("CraftGateTimeOrder");
    const auto full=take(fixed_duration_work_units(1,n,b));const int earned=take(compare_fractions(g.earned_service_units.value,full.value,b));
    if(earned>0||(s.phase==WorkExecutionPhase::working&&!closing&&earned==0))fail("UnconsumedCraftGate");
    if(s.phase==WorkExecutionPhase::working&&s.completed_work_units==s.total_work_units)fail("CompletedCraftStillWorking");
    if(s.phase==WorkExecutionPhase::working&&g.gate_ordinal==0&&g.last_closed_gate_result_ref)fail("FirstCraftGateHasPrevious");
    if(g.gate_ordinal>0&&!g.last_closed_gate_result_ref)fail("MissingCraftGatePredecessor");
    if((s.phase==WorkExecutionPhase::awaiting_process_choice)!=g.pending_stage_ordinal.has_value()||(g.pending_stage_ordinal&&*g.pending_stage_ordinal<0))fail("CraftPendingStageMismatch");
    if((s.phase==WorkExecutionPhase::terminal)!=g.terminal_result_ref.has_value()||(g.terminal_result_ref&&g.terminal_result_ref->empty()))fail("CraftTerminalSourceMismatch");
}
}
// 受信枚举只许可合同已有三类OwnerPeriod；实际kind/profile仍交静态owner核验。
Status validate_work_binding(const WorkExecutionBinding &b) {
  return guard<Done>([&] {
    if (const auto *a = std::get_if<ActorActionWorkBinding>(&b)) {
      if (a->action_id.empty() || a->action_kind.empty() ||
          !valid_source_ref(a->source_owner_ref))
        fail("InvalidActorWorkBinding");
    } else {
      const auto &period_binding = std::get<OwnerPeriodWorkBinding>(b);
      if (period_binding.domain != WorkPeriodOwner::local_economy &&
          period_binding.domain != WorkPeriodOwner::estate &&
          period_binding.domain != WorkPeriodOwner::organization)
        fail("UnknownWorkPeriodOwner");
      if (period_binding.activity_kind.empty() ||
          period_binding.activity_id.empty() ||
          period_binding.registered_period_profile.empty())
        fail("InvalidOwnerPeriodBinding");
    }
    return Done{};
  });
}
bool same_work_binding(const WorkExecutionBinding& a,const WorkExecutionBinding& b){if(a.index()!=b.index())return false;if(const auto* x=std::get_if<ActorActionWorkBinding>(&a)){const auto& y=std::get<ActorActionWorkBinding>(b);return x->action_id==y.action_id&&x->action_kind==y.action_kind&&same_source_ref(x->source_owner_ref,y.source_owner_ref);}const auto& x=std::get<OwnerPeriodWorkBinding>(a);const auto& y=std::get<OwnerPeriodWorkBinding>(b);return x.domain==y.domain&&x.activity_kind==y.activity_kind&&x.activity_id==y.activity_id&&x.registered_period_profile==y.registered_period_profile;}
bool same_work_period_ref(const WorkPeriodRef& a,const WorkPeriodRef& b){return same_work_binding(a.binding,b.binding)&&a.period_ordinal==b.period_ordinal&&a.from_tick==b.from_tick&&a.to_tick==b.to_tick&&a.source_schema_version==b.source_schema_version;}
// 原完整期间不能为负/零；只接受已定义/1来源，不把未知schema当当前版。
Status validate_work_period_value(const WorkPeriodView& v){return guard<Done>([&]{take(validate_work_binding(v.ref.binding));if(v.ref.source_schema_version!=1||v.ref.period_ordinal<0||v.ref.from_tick<0||v.ref.to_tick<=v.ref.from_tick||v.work_unit_id.empty()||v.units_before<0||v.units_added<0||v.units_after!=add(v.units_before,v.units_added))fail("InvalidWorkPeriod");std::set<StableId> actors;for(const auto& a:v.actual_actor_roles)if(a.actor_id.empty()||a.role.empty()||!actors.insert(a.actor_id).second)fail("DuplicateWorkActor");ids(v.actual_labor_allocations);ids(v.facility_claim_refs);refs(v.resource_effect_refs);refs(v.exertion_source_refs);if(v.reason&&!valid_source_ref(*v.reason))fail("InvalidWorkReason");switch(v.disposition){case WorkPeriodDisposition::provided:if(v.units_added==0||(v.actual_actor_roles.empty()&&v.actual_labor_allocations.empty()))fail("WorkProvidedWithoutLabor");break;case WorkPeriodDisposition::interrupted:if(!v.reason||(v.units_added>0&&v.actual_actor_roles.empty()&&v.actual_labor_allocations.empty()))fail("InvalidInterruptedWorkDisposition");break;case WorkPeriodDisposition::no_work:if(v.units_added!=0||!v.reason)fail("InvalidNoWorkDisposition");break;default:fail("UnknownWorkDisposition");}return Done{};});}
Status validate_work_execution_state(const WorkExecutionState& s){return guard<Done>([&]{if(s.work_unit_id.empty()||s.running_policy_ref.empty()||s.total_work_units<=0||s.completed_work_units<0||s.completed_work_units>s.total_work_units)fail("InvalidWorkExecutionState");if(static_cast<unsigned>(s.phase)>static_cast<unsigned>(WorkExecutionPhase::awaiting_settlement))fail("UnknownWorkExecutionPhase");if(s.last_processed_period){take(validate_work_binding(s.last_processed_period->binding));if(s.last_processed_period->source_schema_version!=1||s.last_processed_period->period_ordinal<0||s.last_processed_period->from_tick<0||s.last_processed_period->to_tick<=s.last_processed_period->from_tick)fail("InvalidLastWorkPeriod");}return Done{};});}
// 全部参与/材料/用力/设施腿均从同一原期间反验，缺任一接口不产生成功值。
Outcome<WorkPeriodView> read_verified_work_period(const WorkPeriodRef& ref,const WorkPeriodSourceRead& owner){return guard<WorkPeriodView>([&]{auto v=take(owner.read_work_period(ref));if(!same_work_period_ref(v.ref,ref))fail("WorkPeriodSourceMismatch");take(validate_work_period_value(v));for(const auto& x:v.actual_actor_roles)take(owner.validate_actor_role(ref,x));for(const auto& x:v.actual_labor_allocations)take(owner.validate_labor_allocation(ref,x));for(const auto& x:v.facility_claim_refs)take(owner.validate_facility_service(ref,x));for(const auto& x:v.resource_effect_refs)take(owner.validate_resource_effect(ref,x));for(const auto& x:v.exertion_source_refs)take(owner.validate_exertion_source(ref,x));return v;});}
// 重读同一真实末期间为幂等；源键相同但数值不一致或跳期明确拒绝。
Outcome<WorkExecutionState> apply_verified_work_period(const WorkExecutionState& s,const WorkPeriodRef& ref,const WorkPeriodSourceRead& owner){return guard<WorkExecutionState>([&]{take(validate_work_execution_state(s));auto v=take(read_verified_work_period(ref,owner));if(v.work_unit_id!=s.work_unit_id)fail("WorkUnitMismatch");if(s.last_processed_period&&same_work_period_ref(*s.last_processed_period,ref)){if(v.units_after!=s.completed_work_units)fail("WorkPeriodReplayMismatch");return s;}if(s.phase==WorkExecutionPhase::terminal)fail("TerminalWorkAdvance");if(s.last_processed_period){if(!same_work_binding(s.last_processed_period->binding,ref.binding)||s.last_processed_period->period_ordinal==std::numeric_limits<Integer>::max()||ref.period_ordinal!=s.last_processed_period->period_ordinal+1||ref.from_tick<s.last_processed_period->to_tick)fail("WorkPeriodSequenceMismatch");}else if(ref.period_ordinal!=0)fail("MissingFirstWorkPeriod");if(v.units_before!=s.completed_work_units||v.units_after>s.total_work_units)fail("WorkPeriodCumulativeMismatch");WorkExecutionState out=s;out.completed_work_units=v.units_after;out.last_processed_period=ref;if(v.disposition!=WorkPeriodDisposition::provided)out.phase=WorkExecutionPhase::suspended;return out;});}
Status validate_craft_work_gate(const WorkExecutionState& s,const CraftWorkGate& g,Integer n,ExactArithmeticBudget b){return guard<Done>([&]{craft_value(s,g,n,b,false);return Done{};});}
Outcome<WorldMoment> craft_next_gate_due(const WorkExecutionState& s,const CraftWorkGate& g,Integer n,ExactArithmeticBudget b){return guard<WorldMoment>([&]{craft_value(s,g,n,b,false);if(s.phase!=WorkExecutionPhase::working)fail("CraftGateNotWorking");auto remaining=take(subtract_work_units(take(fixed_duration_work_units(1,n,b)),g.earned_service_units,b));return take(add_world_duration(g.last_accounted_at,take(scale_units_to_duration(remaining,n,b)),b));});}
Outcome<CraftGateSlice> prepare_craft_work_slice(const WorkExecutionState& s,const CraftWorkGate& g,const MomentSpan& span,Integer n,ExactArithmeticBudget b){return guard<CraftGateSlice>([&]{take(make_moment_span(span.from,span.to,b));const auto due=take(craft_next_gate_due(s,g,n,b));if(take(compare_world_moments(span.from,g.last_accounted_at,b))!=0||take(compare_world_moments(span.to,due,b))>0)fail("CraftSliceCrossesGate");auto delta=take(duration_to_scale_units(take(elapsed_world_duration(span.from,span.to,b)),n,b));return CraftGateSlice{take(add_work_units(g.earned_service_units,delta,b)),span.to,take(compare_world_moments(span.to,due,b))==0};});}
// 估算先以原整窗ceil求窗数，再减本窗已供；不先乘N造成许可无关溢出。
Outcome<Integer> craft_remaining_half_days(const WorkExecutionState& s,const CraftWorkGate& g,Integer n,ExactArithmeticBudget b){return guard<Integer>([&]{craft_value(s,g,n,b,false);const auto remain=s.total_work_units-s.completed_work_units;if(remain==0)return Integer{0};const Integer rate=g.pace_for_gate==CraftPace::normal?3:4;const Integer windows=remain/rate+(remain%rate!=0);auto duration=HalfDayDuration{take(normalize_fraction(std::to_string(windows),"1",b))};auto all=take(duration_to_scale_units(duration,n,b));// 等工序保N代表已经结入done的闭门，不可再减一次而把未来工作假写0。
    const bool closed=take(compare_fractions(g.earned_service_units.value,take(fixed_duration_work_units(1,n,b)).value,b))==0;
    auto outstanding=closed?all:take(subtract_work_units(all,g.earned_service_units,b));return take(ceil_half_day_duration(take(scale_units_to_duration(outstanding,n,b)),b));});}
Outcome<Integer> craft_gate_work_increment(const WorkExecutionState& s,const CraftWorkGate& g){return guard<Integer>([&]{take(validate_work_execution_state(s));if(g.pace_for_gate!=CraftPace::normal&&g.pace_for_gate!=CraftPace::rush)fail("UnknownCraftPace");if(s.phase!=WorkExecutionPhase::working)fail("CraftGateNotWorking");return std::min<Integer>(g.pace_for_gate==CraftPace::normal?3:4,s.total_work_units-s.completed_work_units);});}
// 所有结果均由专业owner完成后传入；此处只核唯一gate量与实际source列表，不补默认材料或终局。
Outcome<ClosedCraftGate> close_craft_work_gate(const WorkExecutionState& s,const CraftWorkGate& g,const CraftWorkGateResult& r,WorkExecutionPhase phase,std::optional<Integer> pending,std::optional<StableId> terminal,Integer n,ExactArithmeticBudget b){return guard<ClosedCraftGate>([&]{craft_value(s,g,n,b,true);if(s.phase!=WorkExecutionPhase::working||take(compare_fractions(g.earned_service_units.value,take(fixed_duration_work_units(1,n,b)).value,b))!=0)fail("CraftGateNotDue");if(r.result_id.empty()||!same_work_binding(r.execution_binding,g.execution_binding)||r.gate_ordinal!=g.gate_ordinal||r.work_units_before!=s.completed_work_units||r.work_units_added!=take(craft_gate_work_increment(s,g))||r.work_units_after!=add(r.work_units_before,r.work_units_added)||r.actual_service_source_refs.empty())fail("CraftGateResultMismatch");ids(r.actual_service_source_refs);ids(r.material_effect_refs);ids(r.fee_receipt_refs);ids(r.exertion_source_refs);Integer prior=-1;for(auto ordinal:r.reached_stage_ordinals){if(ordinal<0||ordinal<=prior)fail("CraftStageOrderMismatch");prior=ordinal;}ClosedCraftGate out{s,g};out.execution.completed_work_units=r.work_units_after;out.execution.phase=phase;out.gate.last_closed_gate_result_ref=r.result_id;out.gate.pending_stage_ordinal=pending;out.gate.terminal_result_ref=terminal;if(g.pace_for_gate==CraftPace::rush)out.gate.rush_units_committed=add(g.rush_units_committed,r.work_units_added);if(phase==WorkExecutionPhase::working){if(r.work_units_after==s.total_work_units||g.gate_ordinal==std::numeric_limits<Integer>::max())fail("InvalidNextCraftGate");out.gate.gate_ordinal++;out.gate.gate_opened_at=g.last_accounted_at;out.gate.earned_service_units=WorkUnits{};}craft_value(out.execution,out.gate,n,b,false);return out;});}
Outcome<ClosedCraftGate> resume_craft_work_gate(const WorkExecutionState& s,const CraftWorkGate& g,const WorldMoment& at,Integer ordinal,const std::optional<StableId>& previous,Integer n,ExactArithmeticBudget b){return guard<ClosedCraftGate>([&]{
    craft_value(s,g,n,b,false);
    if((s.phase!=WorkExecutionPhase::suspended&&s.phase!=WorkExecutionPhase::awaiting_process_choice&&s.phase!=WorkExecutionPhase::awaiting_output_space&&s.phase!=WorkExecutionPhase::awaiting_settlement)||s.completed_work_units==s.total_work_units||ordinal!=g.gate_ordinal||previous!=g.last_closed_gate_result_ref||take(compare_world_moments(at,g.last_accounted_at,b))<0)fail("InvalidCraftResumeBoundary");
    ClosedCraftGate out{s,g};out.execution.phase=WorkExecutionPhase::working;out.gate.pending_stage_ordinal.reset();out.gate.last_accounted_at=at;
    const bool closed=take(compare_fractions(g.earned_service_units.value,take(fixed_duration_work_units(1,n,b)).value,b))==0;
    if(closed){if(!g.last_closed_gate_result_ref||g.gate_ordinal==std::numeric_limits<Integer>::max())fail("MissingClosedCraftResumeGate");++out.gate.gate_ordinal;out.gate.gate_opened_at=at;out.gate.earned_service_units=WorkUnits{};}
    craft_value(out.execution,out.gate,n,b,false);return out;
});}
Outcome<ClosedCraftGate> finish_craft_gate_wait(const WorkExecutionState& s,const CraftWorkGate& g,const StableId& terminal,Integer n,ExactArithmeticBudget b){return guard<ClosedCraftGate>([&]{
    craft_value(s,g,n,b,false);
    if((s.phase!=WorkExecutionPhase::awaiting_process_choice&&s.phase!=WorkExecutionPhase::awaiting_output_space&&s.phase!=WorkExecutionPhase::suspended&&s.phase!=WorkExecutionPhase::awaiting_settlement)||s.completed_work_units!=s.total_work_units||terminal.empty()||!g.last_closed_gate_result_ref)fail("CraftWorkNotReadyToFinish");
    ClosedCraftGate out{s,g};out.execution.phase=WorkExecutionPhase::terminal;out.gate.pending_stage_ordinal.reset();out.gate.terminal_result_ref=terminal;craft_value(out.execution,out.gate,n,b,false);return out;
});}
Outcome<ClosedCraftGate> change_craft_pace(const WorkExecutionState& s,const CraftWorkGate& g,CraftPace pace,const StableId& source,Integer ordinal,const std::optional<StableId>& previous,Integer n,ExactArithmeticBudget b){return guard<ClosedCraftGate>([&]{craft_value(s,g,n,b,false);if(s.phase!=WorkExecutionPhase::working||take(compare_fractions(g.earned_service_units.value,CanonicalFraction{},b))!=0||ordinal!=g.gate_ordinal||previous!=g.last_closed_gate_result_ref||source.empty()||(pace!=CraftPace::normal&&pace!=CraftPace::rush))fail("CraftPaceBoundaryRequired");ClosedCraftGate out{s,g};out.gate.pace_for_gate=pace;out.gate.accepted_pace_source_ref=source;return out;});}
Outcome<ClosedOriginalGate> consume_original_half_day_gate(const WorkExecutionState& s,const OriginalHalfDayGate& g,const OriginalHalfDayGateResult& result,const WorkPeriodSourceRead& reader,Integer n){return guard<ClosedOriginalGate>([&]{
    const auto& ref=result.period_source_ref;
    if(n<=0||g.next_period_ordinal<0||static_cast<unsigned>(g.original_gate_profile_ref)>4||!std::holds_alternative<ActorActionWorkBinding>(g.original_execution_binding)||!same_work_binding(ref.binding,g.original_execution_binding)||!same_work_binding(result.execution_binding,g.original_execution_binding))fail("InvalidOriginalHalfDayGate");
    auto v=take(read_verified_work_period(ref,reader));
    if(result.gate_ordinal!=ref.period_ordinal||result.provided_work_units!=v.units_added||result.completed_before!=v.units_before||result.completed_after!=v.units_after||result.processed_tick!=ref.to_tick)fail("OriginalGateResultSourceMismatch");
    refs(result.stage_result_refs);for(const auto& stage:result.stage_result_refs)take(reader.validate_stage_result(ref,stage));
    if(g.last_consumed_period_ref&&same_work_period_ref(*g.last_consumed_period_ref,ref)){if(s.completed_work_units!=v.units_after)fail("OriginalGateReplayMismatch");return ClosedOriginalGate{s,g,result};}
    if(ref.period_ordinal!=g.next_period_ordinal||!same_optional_period(s.last_processed_period,g.last_consumed_period_ref))fail("OriginalGateSequenceMismatch");
    const auto periods=ref.to_tick-ref.from_tick;if(periods>std::numeric_limits<Integer>::max()/n)fail("OriginalGateUnitsOverflow");
    if(v.disposition==WorkPeriodDisposition::provided&&v.units_added!=periods*n)fail("OriginalGateMustProvideWholePeriod");
    auto next=take(apply_verified_work_period(s,ref,reader));if(g.next_period_ordinal==std::numeric_limits<Integer>::max())fail("OriginalGateOrdinalOverflow");
    OriginalHalfDayGate gate=g;gate.last_consumed_period_ref=ref;++gate.next_period_ordinal;return ClosedOriginalGate{std::move(next),std::move(gate),result};
});}


const char* life_half_day_stage_profile_name(LifeHalfDayStageGateProfile p) noexcept {static const char* names[]={"medicine_batch_halfday_gate_v1","work_composition_halfday_gate_v1","copy_production_halfday_gate_v1","elixir_halfday_gate_v1","talisman_halfday_gate_v1","infusion_halfday_gate_v1","ritual_execution_halfday_gate_v1"};const auto i=static_cast<unsigned>(p);return i<7?names[i]:nullptr;}
const char* original_half_day_gate_profile_name(OriginalHalfDayGateProfile p) noexcept {static const char* names[]={"copy","infusion","recharge","maintain","appraise"};const auto i=static_cast<unsigned>(p);return i<5?names[i]:nullptr;}
// 固定七profile字面由原锁内定义引用；不存在通用默认职业或自动赶工。
Status validate_life_half_day_stage_gate(const WorkExecutionState& s,const LifeHalfDayStageGateState& g,Integer n,ExactArithmeticBudget b){return guard<Done>([&]{take(validate_work_execution_state(s));if(n<=0||static_cast<unsigned>(g.gate_profile_ref)>6||!valid_source_ref(g.stage_rule_source_ref)||s.total_work_units%n||s.completed_work_units%n||s.last_processed_period)fail("InvalidLifeHalfDayStageGate");if(s.running_policy_ref!=life_half_day_stage_profile_name(g.gate_profile_ref))fail("LifeGatePolicyMismatch");auto full=take(fixed_duration_work_units(1,n,b));if(take(compare_fractions(g.earned_service_units.value,full.value,b))>=0)fail("UnconsumedLifeHalfDayGate");if(s.completed_work_units==s.total_work_units&&(s.phase==WorkExecutionPhase::working||take(compare_fractions(g.earned_service_units.value,CanonicalFraction{},b))!=0))fail("FullLifeWorkStillRunning");return Done{};});}
// from/to来自唯一协调器实际已验证参与片；只能积到下一完整门，完整门的原owner效果在随后同候选采用。
Outcome<WorkUnits> prepare_life_half_day_stage_slice(const WorkExecutionState& s,const LifeHalfDayStageGateState& g,const MomentSpan& span,Integer n,ExactArithmeticBudget b){return guard<WorkUnits>([&]{take(validate_life_half_day_stage_gate(s,g,n,b));if(s.phase!=WorkExecutionPhase::working)fail("LifeGateNotWorking");take(make_moment_span(span.from,span.to,b));auto delta=take(duration_to_scale_units(take(elapsed_world_duration(span.from,span.to,b)),n,b));auto earned=take(add_work_units(g.earned_service_units,delta,b));if(take(compare_fractions(earned.value,take(fixed_duration_work_units(1,n,b)).value,b))>0)fail("LifeSliceCrossesGate");return earned;});}
Outcome<Integer> life_half_day_stage_remaining(const WorkExecutionState& s,const LifeHalfDayStageGateState& g,Integer n,ExactArithmeticBudget b){return guard<Integer>([&]{take(validate_life_half_day_stage_gate(s,g,n,b));WorkUnits remaining{take(normalize_fraction(std::to_string(s.total_work_units-s.completed_work_units),"1",b))};remaining=take(subtract_work_units(remaining,g.earned_service_units,b));return take(ceil_half_day_duration(take(scale_units_to_duration(remaining,n,b)),b));});}
// stage_result仅真实原期间已应用结果引用；本层给N一次，不能顺手制造材料/药效/XP。
Outcome<ClosedLifeStageGate> consume_life_half_day_stage_gate(const WorkExecutionState& s,const LifeHalfDayStageGateState& g,const LifeHalfDayStageGateResult& result,const LifeWorkPeriodSourceRead& reader,WorkExecutionPhase phase,Integer n,ExactArithmeticBudget b){return guard<ClosedLifeStageGate>([&]{
    if(n<=0||s.phase!=WorkExecutionPhase::working||take(compare_fractions(g.earned_service_units.value,take(fixed_duration_work_units(1,n,b)).value,b))!=0)fail("LifeHalfDayGateNotDue");
    LifeHalfDayStageGateState cleared=g;cleared.earned_service_units=WorkUnits{};take(validate_life_half_day_stage_gate(s,cleared,n,b));auto period=take(read_verified_life_work_period(result.work_period_ref,reader,n,b));
    if(period.gate_profile!=g.gate_profile_ref||!same_source_ref(period.stage_rule_source_ref,g.stage_rule_source_ref)||result.completed_halfday_ordinal!=s.completed_work_units/n||period.completed_halfday_ordinal!=result.completed_halfday_ordinal||period.completed_stage_ordinals!=result.completed_stage_ordinals||period.original_stage_result_refs.size()!=result.original_stage_result_refs.size())fail("LifeHalfDayPeriodMismatch");
    for(std::size_t i=0;i<period.original_stage_result_refs.size();++i)if(!same_source_ref(period.original_stage_result_refs[i],result.original_stage_result_refs[i]))fail("LifeStageResultSourceMismatch");
    auto next=s;next.completed_work_units=add(s.completed_work_units,n);if(next.completed_work_units>s.total_work_units)fail("LifeHalfDayWorkOverflow");next.phase=phase;
    // 新Life精确源不污染原WorkPeriodRef/1；历史末键只从实际gate_results读取。
    next.last_processed_period.reset();take(validate_life_half_day_stage_gate(next,cleared,n,b));return ClosedLifeStageGate{std::move(next),std::move(cleared)};
});}
static bool same_gate_sources(const std::vector<SourceRef>& a,const std::vector<SourceRef>& z){if(a.size()!=z.size())return false;for(std::size_t i=0;i<a.size();++i)if(!same_source_ref(a[i],z[i]))return false;return true;}
bool same_shared_work_gate_result(const SharedWorkGateResult& a,const SharedWorkGateResult& z){if(a.index()!=z.index())return false;return std::visit([&](const auto& x){using T=std::decay_t<decltype(x)>;const auto& y=std::get<T>(z);if constexpr(std::is_same_v<T,CraftWorkGateResult>)return x.result_id==y.result_id&&same_work_binding(x.execution_binding,y.execution_binding)&&x.gate_ordinal==y.gate_ordinal&&x.actual_service_source_refs==y.actual_service_source_refs&&x.work_units_before==y.work_units_before&&x.work_units_added==y.work_units_added&&x.work_units_after==y.work_units_after&&x.reached_stage_ordinals==y.reached_stage_ordinals&&x.material_effect_refs==y.material_effect_refs&&x.fee_receipt_refs==y.fee_receipt_refs&&x.exertion_source_refs==y.exertion_source_refs;else if constexpr(std::is_same_v<T,OriginalHalfDayGateResult>)return same_work_binding(x.execution_binding,y.execution_binding)&&same_work_period_ref(x.period_source_ref,y.period_source_ref)&&x.gate_ordinal==y.gate_ordinal&&x.provided_work_units==y.provided_work_units&&x.completed_before==y.completed_before&&x.completed_after==y.completed_after&&x.processed_tick==y.processed_tick&&same_gate_sources(x.stage_result_refs,y.stage_result_refs);else return same_life_work_period_ref(x.work_period_ref,y.work_period_ref)&&x.completed_halfday_ordinal==y.completed_halfday_ordinal&&x.completed_stage_ordinals==y.completed_stage_ordinals&&same_gate_sources(x.original_stage_result_refs,y.original_stage_result_refs);},a);}

}

