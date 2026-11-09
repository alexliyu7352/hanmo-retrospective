// Pure measure for the original S2 actions/2 service. No Common Action or clock.
#pragma once
#include "world_timeline.hpp"
#include "resident/domains/military/dynamic_action_model.hpp"
namespace hanmo::resident {
struct MilitaryTacticalServiceMeasure {
    WorkUnits credited_units;
    WorldMoment last_settled_at;
};
Outcome<MilitaryTacticalService2> make_military_tactical_service(
    const MilitaryTacticalWorkBinding&,const HalfDayDuration&,const WorldMoment&,
    const TimeScaleRef&,ExactArithmeticBudget budget={});
// The original S2 owner proves participation for the entire positive interval
// before adopting this measure and its real service sources in one candidate.
Outcome<MilitaryTacticalServiceMeasure> advance_military_tactical_service(
    const MilitaryTacticalService2&,const MomentSpan&,const TimeScaleRef&,
    ExactArithmeticBudget budget={});
// A prediction from the one retained measure, not an applied effect or permission.
Outcome<WorldMoment> read_military_tactical_service_due(
    const MilitaryTacticalService2&,const TimeScaleRef&,ExactArithmeticBudget budget={});
// Structural/numeric only. S2 validates the actual source closure and definition.
Status validate_military_tactical_service_measure(
    const MilitaryTacticalService2&,const TimeScaleRef&,ExactArithmeticBudget budget={});
}
