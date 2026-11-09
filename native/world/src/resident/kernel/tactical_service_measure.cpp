#include "tactical_service_measure.hpp"
namespace hanmo::resident {namespace {
struct MeasureFailure {RuleError error;};
template<class T>T take(Outcome<T> value){if(!value.ok)throw MeasureFailure{std::move(value.error)};return std::move(value.value);}
[[noreturn]] void fail(const char* code){throw MeasureFailure{common_failure(code,"military_tactical_service").error};}
template<class T,class F>Outcome<T> measure(F fn){try{return {true,fn(),{}};}catch(MeasureFailure& e){return {false,{},std::move(e.error)};}}
void value(const MilitaryTacticalService2& service,const TimeScaleRef& scale,ExactArithmeticBudget budget){
    take(validate_time_scale(scale));
    const auto& binding=service.work_binding_ref;
    if(binding.session_ref.empty()||binding.generation<1||binding.action_ref.empty())fail("ActualMilitaryTacticalBindingRequired");
    switch(binding.stage){case MilitaryTacticalWorkStage::prepare:case MilitaryTacticalWorkStage::recovery:break;default:fail("InvalidMilitaryTacticalStage");}
    take(encode_world_moment(service.started_at,budget));
    take(encode_world_moment(service.last_settled_at,budget));
    if(take(compare_fractions(service.required_units.value,CanonicalFraction{},budget))<=0)fail("PositiveMilitaryTacticalServiceRequired");
    take(subtract_work_units(service.required_units,service.credited_units,budget));
    const auto wall=take(elapsed_world_duration(service.started_at,service.last_settled_at,budget));
    const auto maximum=take(duration_to_scale_units(wall,scale.units_per_half_day,budget));
    if(take(compare_fractions(service.credited_units.value,maximum.value,budget))>0)fail("MilitaryTacticalCreditExceedsElapsed");
    if(service.closed_by_source&&(service.closed_by_source->domain.empty()||service.closed_by_source->kind.empty()||service.closed_by_source->id.empty()))fail("ActualMilitaryTacticalClosureRequired");
}
WorldMoment due(const MilitaryTacticalService2& service,const TimeScaleRef& scale,ExactArithmeticBudget budget){
    value(service,scale,budget);
    if(service.closed_by_source)fail("ClosedMilitaryTacticalServiceHasNoDue");
    const auto remaining=take(subtract_work_units(service.required_units,service.credited_units,budget));
    return take(add_world_duration(service.last_settled_at,take(scale_units_to_duration(remaining,scale.units_per_half_day,budget)),budget));
}
}
Status validate_military_tactical_service_measure(const MilitaryTacticalService2& service,const TimeScaleRef& scale,ExactArithmeticBudget budget){return measure<Done>([&]{value(service,scale,budget);return Done{};});}
Outcome<MilitaryTacticalService2> make_military_tactical_service(const MilitaryTacticalWorkBinding& binding,const HalfDayDuration& duration,const WorldMoment& at,const TimeScaleRef& scale,ExactArithmeticBudget budget){return measure<MilitaryTacticalService2>([&]{
    take(validate_time_scale(scale));
    if(take(compare_fractions(duration.value,CanonicalFraction{},budget))<=0)fail("PositiveMilitaryTacticalDurationRequired");
    MilitaryTacticalService2 service;service.work_binding_ref=binding;service.started_at=at;service.last_settled_at=at;
    service.required_units=take(duration_to_scale_units(duration,scale.units_per_half_day,budget));
    // Reject a time overflow before the original writer adopts this new stage.
    take(add_world_duration(at,duration,budget));value(service,scale,budget);return service;
});}
Outcome<WorldMoment> read_military_tactical_service_due(const MilitaryTacticalService2& service,const TimeScaleRef& scale,ExactArithmeticBudget budget){return measure<WorldMoment>([&]{return due(service,scale,budget);});}
Outcome<MilitaryTacticalServiceMeasure> advance_military_tactical_service(const MilitaryTacticalService2& service,const MomentSpan& span,const TimeScaleRef& scale,ExactArithmeticBudget budget){return measure<MilitaryTacticalServiceMeasure>([&]{
    const auto gate=due(service,scale,budget);
    if(take(compare_world_moments(span.from,service.last_settled_at,budget))!=0)fail("MilitaryTacticalServiceSliceNotCurrent");
    if(take(compare_world_moments(span.from,span.to,budget))>=0)fail("PositiveMilitaryTacticalServiceSliceRequired");
    if(take(compare_world_moments(span.to,gate,budget))>0)fail("MilitaryTacticalServiceCrossesDue");
    const auto elapsed=take(elapsed_world_duration(span.from,span.to,budget));
    const auto supplied=take(duration_to_scale_units(elapsed,scale.units_per_half_day,budget));
    auto credited=take(add_work_units(service.credited_units,supplied,budget));
    const auto full=take(compare_fractions(credited.value,service.required_units.value,budget));
    const bool at_gate=take(compare_world_moments(span.to,gate,budget))==0;
    if(full>0||(full==0)!=at_gate)fail("MilitaryTacticalServiceCreditMismatch");
    return MilitaryTacticalServiceMeasure{std::move(credited),span.to};
});}
}
