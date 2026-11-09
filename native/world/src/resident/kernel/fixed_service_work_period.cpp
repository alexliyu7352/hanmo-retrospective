// 完整期只投影唯一 Fixed 信用和 Running 历史；不写 Action、gate、费用或身体。
#include "fixed_service_work_period.hpp"
#include "resident/model/common_model.hpp"
#include <limits>
#include <new>
#include <stdexcept>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
template<class T>T take(Outcome<T> r){if(!r.ok)throw Failure{std::move(r.error)};return std::move(r.value);}
[[noreturn]]void fail(const char* code){throw Failure{common_failure(code,"work_period/2",ErrorKind::internal).error};}
template<class T,class F>Outcome<T> guard(F f){try{return {true,f(),{}};}catch(Failure& e){return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){return {false,{},common_failure("FixedPeriodResourceExhausted","work_period/2",ErrorKind::internal).error};}catch(const std::length_error&){return {false,{},common_failure("FixedPeriodResourceExhausted","work_period/2",ErrorKind::internal).error};}}
bool same_units(const WorkUnits& a,const WorkUnits& b,ExactArithmeticBudget budget){return take(compare_fractions(a.value,b.value,budget))==0;}
void ids(const std::vector<StableId>& values){for(std::size_t i=0;i<values.size();++i){if(values[i].empty())fail("EmptyFixedPeriodReference");for(std::size_t j=0;j<i;++j)if(values[i]==values[j])fail("DuplicateFixedPeriodReference");}}
void refs(const std::vector<SourceRef>& values){for(std::size_t i=0;i<values.size();++i){if(!valid_source_ref(values[i]))fail("InvalidFixedPeriodSource");for(std::size_t j=0;j<i;++j)if(same_source_ref(values[i],values[j]))fail("DuplicateFixedPeriodSource");}}
}
Status validate_work_period_ref2(const WorkPeriodRef2& r,ExactArithmeticBudget budget){return guard<Done>([&]{
 take(validate_work_binding(r.binding));if(r.period_ordinal<0||r.source_schema_version!=2)fail("InvalidWorkPeriodRef2");
 take(make_moment_span(r.from,r.until,budget));return Done{};
});}
Outcome<bool> same_work_period_ref2(const WorkPeriodRef2& a,const WorkPeriodRef2& b,ExactArithmeticBudget budget){return guard<bool>([&]{
 return same_work_binding(a.binding,b.binding)&&a.period_ordinal==b.period_ordinal&&a.source_schema_version==b.source_schema_version&&take(compare_world_moments(a.from,b.from,budget))==0&&take(compare_world_moments(a.until,b.until,budget))==0;
});}
Status validate_work_period_view2(const WorkPeriodView2& v,ExactArithmeticBudget budget){return guard<Done>([&]{
 take(validate_work_period_ref2(v.ref,budget));if(v.work_unit_id.empty()||static_cast<unsigned>(v.disposition)>2)fail("InvalidWorkPeriodView2");
 if(take(compare_fractions(v.units_before.value,CanonicalFraction{},budget))<0||take(compare_fractions(v.units_added.value,CanonicalFraction{},budget))<0||take(compare_fractions(v.units_after.value,CanonicalFraction{},budget))<0)fail("NegativeWorkPeriodUnits2");
 if(!same_units(take(add_work_units(v.units_before,v.units_added,budget)),v.units_after,budget))fail("WorkPeriodUnitsMismatch2");
 if(v.disposition==WorkPeriodDisposition::provided){if(take(compare_fractions(v.units_added.value,CanonicalFraction{},budget))<=0||v.reason)fail("InvalidProvidedWorkPeriod2");}
 else if(!same_units(v.units_added,WorkUnits{},budget)||!v.reason||!valid_source_ref(*v.reason))fail("InvalidUnprovidedWorkPeriod2");
 for(std::size_t i=0;i<v.actual_actor_roles.size();++i){const auto& r=v.actual_actor_roles[i];if(r.actor_id.empty()||r.role.empty())fail("InvalidFixedPeriodWorker");for(std::size_t j=0;j<i;++j)if(v.actual_actor_roles[j].actor_id==r.actor_id)fail("DuplicateFixedPeriodWorker");}
 ids(v.actual_labor_allocations);ids(v.facility_claim_refs);refs(v.resource_effect_refs);refs(v.exertion_source_refs);return Done{};
});}
Outcome<FixedServiceWorkPeriodRead2> read_fixed_service_work_period2(const ActionElapsedStore& store,const ActorActionWorkBinding& binding,Integer ordinal,Integer h,const WorldMoment& now,const TimeScaleRef& scale,ExactArithmeticBudget budget){return guard<FixedServiceWorkPeriodRead2>([&]{
 take(validate_work_binding(WorkExecutionBinding{binding}));
 // Action kind and the owner record kind are independently registered. Care's
 // actual execution is care_handover_execution; it is not a Common Fact alias.
 const bool handover=binding.action_kind=="care_handover"&&binding.source_owner_ref.kind=="care_handover_execution";
 const bool clearance=binding.action_kind=="facility_clearance"&&binding.source_owner_ref.kind=="facility_clearance";
 const bool care=binding.source_owner_ref.domain=="care_requirements"&&(handover||clearance);
 // The selected original Shop/2 service has its own accepted-operation source
 // and the fixed H2 rule. Only its genuine S7 producer may supply this binding;
 // this shared reader still grants neither staffing nor shop authority.
 const bool shop=binding.action_kind=="private_shop_operate"&&binding.source_owner_ref.domain=="private_shops"&&
     binding.source_owner_ref.kind=="private_shop_operating_started"&&h==2;
 if(!care&&!shop)fail("UnregisteredFixedWorkPeriodOwner2");
 if(h<=0||ordinal<0||ordinal>=h||ordinal==std::numeric_limits<Integer>::max())fail("InvalidFixedWorkPeriodOrdinal2");
 const auto* entry=find_fixed_elapsed(store,binding.action_id);if(!entry)fail("FixedWorkPeriodElapsedRequired");
 const auto required=take(fixed_duration_work_units(h,scale.units_per_half_day,budget));if(!same_units(required,entry->required_units,budget))fail("FixedWorkPeriodTermsMismatch");
 const auto before=ordinal==0?WorkUnits{}:take(fixed_duration_work_units(ordinal,scale.units_per_half_day,budget));
 const auto added=take(fixed_duration_work_units(1,scale.units_per_half_day,budget));const auto after=take(fixed_duration_work_units(ordinal+1,scale.units_per_half_day,budget));
 // 两信用边界均只可读已发生服务。nN 后若暂停，首片在恢复处，不能把暂停起点记作 from。
 const auto earliest=take(read_fixed_service_credit_boundary(store,binding.action_id,before,now,scale,budget));
 const auto until=take(read_fixed_service_credit_boundary(store,binding.action_id,after,now,scale,budget));
 const bool due=std::holds_alternative<ElapsedRunning>(entry->timing)&&same_units(entry->credited_units,entry->required_units,budget);
 auto service=take(due?read_fixed_service_intervals_at_completion_due(store,binding.action_id,earliest,until,now,scale,budget):read_fixed_service_intervals(store,binding.action_id,earliest,until,now,scale,budget));
 if(!service.complete||service.intervals.empty()||!same_units(service.credited_before,before,budget)||!same_units(service.credited_after,after,budget))fail("IncompleteFixedWorkPeriod2");
 for(const auto& row:store.fixed_service_transitions)if(row.action_ref==binding.action_id&&!same_source_ref(row.execution_source_ref,binding.source_owner_ref))fail("FixedWorkPeriodExecutionMismatch");
 const auto from=service.intervals.front().from;
 if(take(compare_world_moments(service.intervals.back().until,until,budget))!=0)fail("FixedWorkPeriodEndMismatch");
 service.covered_from=from;
 WorkPeriodRef2 ref{WorkExecutionBinding{binding},ordinal,from,until,2};take(validate_work_period_ref2(ref,budget));
 return FixedServiceWorkPeriodRead2{std::move(ref),before,added,after,std::move(service)};
});}
}
