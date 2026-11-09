#pragma once
#include "office_exact.hpp"
#include "resident/kernel/exact_phase_graph.hpp"
namespace hanmo::resident {
// Sub-leaf of the one military owner, not a parallel calendar Office timer.
Outcome<std::vector<ExactEventPrediction>> predict_exact_office_events(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,const ExactOfficeReadContext&,const WorldMoment& current,const WorldMoment& upper);
Status provide_exact_office_interval(CommonStore&,MilitaryState&,MilitaryActionTimeState&,ActionElapsedStore&,const ExactOfficeWriteContext&,const MomentSpan&);
Outcome<ExactEventApplied> apply_exact_office_event(CommonStore&,MilitaryState&,MilitaryActionTimeState&,ActionElapsedStore&,const ExactOfficeWriteContext&,const ExactEventPrediction&,ExactPhaseWriteContext&);
Status validate_exact_office_applied_source(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,const ActionElapsedStore&,const ExactOfficeReadContext&,const ExactEventApplied&);
}
