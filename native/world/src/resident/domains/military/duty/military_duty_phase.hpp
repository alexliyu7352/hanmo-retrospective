#pragma once
#include "military_duty_contract.hpp"
#include "resident/kernel/exact_phase_graph.hpp"
namespace hanmo::resident {
Outcome<std::vector<ExactEventPrediction>> predict_military_duty_events(const MilitaryDutyState&,const MilitaryDutyContext&,const WorldMoment&,const WorldMoment&);
Status provide_military_duty_interval(CommonStore&,MilitaryDutyState&,const MilitaryDutyContext&,const MomentSpan&);
Outcome<ExactEventApplied> apply_military_duty_event(CommonStore&,MilitaryDutyState&,const MilitaryDutyContext&,const ExactEventPrediction&,ExactPhaseWriteContext&);
// Called by the original condition/contact/authority producer after its actual
// same-candidate write. It closes only duties whose actual reader names that
// cause, without adding an earlier-stage same-Moment back edge.
Outcome<std::vector<ExactEventApplied>> invalidate_military_duties_after_original_source(CommonStore&,MilitaryDutyState&,const MilitaryDutyContext&,const MomentOwnerSource&,const WorldMoment&);
Status validate_military_duty_applied_source(const CommonStore&,const MilitaryDutyState&,const MilitaryDutyContext&,const ExactEventApplied&);
}
