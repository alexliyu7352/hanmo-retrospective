#pragma once
#include "return_exact.hpp"
#include "resident/kernel/exact_phase_graph.hpp"
namespace hanmo::resident {
// Sub-leaf of the one military owner; no parallel calendar return timer.
Outcome<std::vector<ExactEventPrediction>> predict_exact_return_events(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,const ExactReturnReadContext&,const WorldMoment& current,const WorldMoment& upper);
Status provide_exact_return_interval(CommonStore&,MilitaryState&,MilitaryActionTimeState&,ActionElapsedStore&,const ExactReturnWriteContext&,const MomentSpan&);
Outcome<ExactEventApplied> apply_exact_return_event(CommonStore&,MilitaryState&,MilitaryActionTimeState&,ActionElapsedStore&,const ExactReturnWriteContext&,const ExactEventPrediction&,ExactPhaseWriteContext&);
Status validate_exact_return_applied_source(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,const ActionElapsedStore&,const ExactReturnReadContext&,const ExactEventApplied&);
}
