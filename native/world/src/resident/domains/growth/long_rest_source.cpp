#include "resident/domains/growth/growth_model.hpp"
#include "long_rest_source.hpp"
#include "long_rest_mouth_sources.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include "resident/checkpoint/projection_writer.hpp"
#include <limits>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
template<class T,class F>Outcome<T> guarded(F fn){try{return {true,fn(),{}};}catch(Failure& e){return {false,{},std::move(e.error)};}}
const ActionTimeRegistration& registration(const LongRestPeriodSources& s){const ActionTimeRegistration* result=nullptr;for(const auto& r:s.registrations)if(r.owner_domain=="growth"&&r.action_kind=="long_rest"){if(result)fail("DuplicateLongRestRegistration");result=&r;}if(!result)fail("LongRestRegistrationRequired");return *result;}
void same_moment(const WorldMoment& a,const WorldMoment& b,const StableId& id){if(take(compare_world_moments(a,b)))fail("LongRestPeriodMomentMismatch",id);}
}
Outcome<LongRestActualPeriodRead> read_long_rest_actual_period(const LongRestPeriodSources& s,const StableId& id,Integer ordinal){return guarded<LongRestActualPeriodRead>([&]{
 if(!s.catalog.complete_exact_period_sources||!s.catalog.complete_study_sources||&s.estates.common!=&s.common)fail("LongRestCompleteSelectedSourcesRequired",id);
 const auto* raw=s.growth.facts.find(id);const auto* rest=raw?std::get_if<RestFact>(raw):nullptr;const auto* header=s.common.state().facts.find(id);
 if(!rest||!header||header->id!=id||header->domain!="services"||header->kind!="long_rest"||ordinal<1||ordinal>static_cast<Integer>(rest->actual_periods.size())||rest->duration<1||rest->duration>720||rest->ration_per_person<0)fail("LongRestActualPeriodRequired",id);
 const auto& period=rest->actual_periods[static_cast<std::size_t>(ordinal-1)];if(period.ordinal!=ordinal||ordinal>rest->duration||take(compare_world_moments(period.applied_at,s.actual_now))>0)fail("LongRestActualPeriodOrdinalMismatch",id);
 const auto& reg=registration(s);GrowthActionTimeRead original(s.common,s.growth,s.catalog,s.shared,s.elapsed,s.scale,s.identity,s.actual_now);auto proof=take(original.read_action_time_owner(reg,id));const auto* entry=find_fixed_elapsed(s.elapsed,id);if(!entry)fail("LongRestElapsedRequired",id);take(original.validate_retained_fixed_action_sources(reg,proof,*entry));
 auto before=ordinal==1?WorkUnits{}:take(fixed_duration_work_units(ordinal-1,s.scale.units_per_half_day));auto after=take(fixed_duration_work_units(ordinal,s.scale.units_per_half_day));same_moment(period.provided_from,take(read_fixed_service_credit_boundary(s.elapsed,id,before,s.actual_now,s.scale)),id);same_moment(period.applied_at,take(read_fixed_service_credit_boundary(s.elapsed,id,after,s.actual_now,s.scale)),id);
 const bool completion_due=std::holds_alternative<ElapsedRunning>(entry->timing)&&take(compare_fractions(entry->credited_units.value,entry->required_units.value))==0&&take(compare_world_moments(entry->last_settled_at,s.actual_now))==0;const auto intervals=take(completion_due?read_fixed_service_intervals_at_completion_due(s.elapsed,id,period.provided_from,period.applied_at,s.actual_now,s.scale):read_fixed_service_intervals(s.elapsed,id,period.provided_from,period.applied_at,s.actual_now,s.scale));const auto amount=take(subtract_work_units(intervals.credited_after,intervals.credited_before));auto one=take(fixed_duration_work_units(1,s.scale.units_per_half_day));if(!intervals.complete||intervals.intervals.empty()||take(compare_fractions(amount.value,one.value)))fail("LongRestActualServiceIncomplete",id);
 // Read the original snapshots in service order; a repeated snapshot across
 // adjacent fragments is one input, never a new period or a second timer.
 std::size_t input_index=0;const StableId* previous=nullptr;
 for(const auto& span:intervals.intervals){const StableId* actual=nullptr;for(const auto& input:span.input_basis_refs)if(const auto* applied=std::get_if<FixedAppliedInput>(&input);applied&&applied->source_ref.domain=="growth"&&applied->source_ref.kind=="growth_fixed_service_boundary"){if(actual||applied->source_schema!=1)fail("LongRestInputBasisMismatch",id);actual=&applied->source_ref.id;}if(!actual)fail("LongRestInputBasisMissing",id);if(previous&&*previous==*actual)continue;if(input_index>=period.input_boundary_refs.size()||period.input_boundary_refs[input_index++]!=*actual)fail("LongRestInputCoverageMismatch",id);previous=actual;
  const auto* boundary=s.growth.fixed_service_boundaries.find(*actual);if(!boundary||boundary->action_ref!=id||boundary->action_kind!="long_rest"||boundary->owner_ref.kind!=0||boundary->owner_ref.fact_kind!=GrowthFixedFactKind::rest)fail("LongRestInputOwnerMismatch",id);
  // Each actually fed/healed person must be represented in the actual adopted
  // inputs. Complete FSL source validation separately certifies those inputs.
  for(const auto& actor:period.mouth_actor_ids){if(s.catalog.complete_exact_rest_mouth_sources&&actor!=rest->actor_id)continue;std::size_t matches=0;for(const auto& row:boundary->input_snapshot.actors)if(row.actor_id==actor)++matches;if(matches!=1)fail("LongRestMouthInputCoverageMismatch",actor);}
 }
 if(input_index!=period.input_boundary_refs.size()||period.estate_basis.estate_id!=rest->estate_id)fail("LongRestInputCoverageMismatch",id);
 same_moment(period.estate_basis.adopted_at,period.applied_at,id);auto estate=take(read_estate_rest_at_adoption(s.estates,period.estate_basis));
 if(estate.view.owner_actor_id!=rest->actor_id||estate.view.inventory_id!=period.inventory_id||estate.view.food_item_id!=period.food_item_id||period.food_item_id!=rest->food_item_id||estate.view.ration_per_person!=rest->ration_per_person||period.mouth_actor_ids.empty()||period.condition_receipt_refs.size()!=period.mouth_actor_ids.size()||period.mouth_actor_ids.size()>static_cast<std::size_t>(std::numeric_limits<Integer>::max()))fail("LongRestEstateBasisMismatch",id);
 if(rest->ration_per_person&&static_cast<Integer>(period.mouth_actor_ids.size())>std::numeric_limits<Integer>::max()/rest->ration_per_person)fail("LongRestQuantityOverflow",id);
 if(period.consumed_quantity!=static_cast<Integer>(period.mouth_actor_ids.size())*rest->ration_per_person)fail("LongRestConsumptionMismatch",id);
 bool owner=false;for(std::size_t i=0;i<period.mouth_actor_ids.size();++i){const auto& actor=period.mouth_actor_ids[i];if(actor.empty()||!s.common.actor(actor)||period.condition_receipt_refs[i].result_id.empty()||(i&&period.mouth_actor_ids[i-1]>=actor))fail("LongRestMouthIdentityMismatch",id);if(actor==rest->actor_id)owner=true;else for(const auto& resident:estate.view.resident_actor_ids)if(actor==resident)fail("LongRestResidentDoubleProvision",actor);for(std::size_t j=0;j<i;++j)if(period.condition_receipt_refs[j].result_id==period.condition_receipt_refs[i].result_id)fail("LongRestReceiptDuplicate",id);}
 if(!owner)fail("LongRestOwnerMouthMissing",id);if(s.catalog.complete_exact_rest_mouth_sources){if(!s.condition||&s.condition->common!=&s.common)fail("LongRestMouthConditionSourcesRequired",id);take(validate_long_rest_mouth_inputs(*s.condition,*rest,period,estate.view));}else if(!period.mouth_inputs.empty())fail("GrowthExactRestMouthPhysicalSelectionRequired",id);return LongRestActualPeriodRead{rest,&period,std::move(estate)};
});}
Status validate_long_rest_actual_periods(const LongRestPeriodSources& s){return guarded<Done>([&]{for(const auto& row:s.growth.facts.rows())if(const auto* rest=std::get_if<RestFact>(&row.second)){
 if(rest->elapsed<0||rest->actual_periods.size()!=static_cast<std::size_t>(rest->elapsed))fail("LongRestPeriodReverseCoverageMismatch",row.first);Integer consumed=0;for(std::size_t i=0;i<rest->actual_periods.size();++i){auto actual=take(read_long_rest_actual_period(s,row.first,static_cast<Integer>(i+1)));if(actual.period->consumed_quantity>std::numeric_limits<Integer>::max()-consumed)fail("LongRestConsumptionOverflow",row.first);consumed+=actual.period->consumed_quantity;}if(consumed!=rest->consumed)fail("LongRestConsumptionProjectionMismatch",row.first);
 if(rest->phase=="active"&&!rest->actual_periods.empty()&&rest->last_mouths!=rest->actual_periods.back().mouth_actor_ids)fail("LongRestLastMouthProjectionMismatch",row.first);
 }return Done{};});}
}
