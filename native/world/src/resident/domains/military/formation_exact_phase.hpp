#pragma once
#include "formation_exact.hpp"
#include "resident/kernel/exact_phase_graph.hpp"
namespace hanmo::resident {
// Sub-leaf of the one military owner, not a parallel calendar formation timer.
Outcome<std::vector<ExactEventPrediction>> predict_exact_formation_events(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,const ExactFormationReadContext&,const WorldMoment& current,const WorldMoment& upper);
Status provide_exact_formation_interval(CommonStore&,MilitaryState&,MilitaryActionTimeState&,ActionElapsedStore&,const ExactFormationWriteContext&,const MomentSpan&);
Outcome<ExactEventApplied> apply_exact_formation_event(CommonStore&,MilitaryState&,MilitaryActionTimeState&,ActionElapsedStore&,const ExactFormationWriteContext&,const ExactEventPrediction&,ExactPhaseWriteContext&);
Status validate_exact_formation_applied_source(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,const ActionElapsedStore&,const ExactFormationReadContext&,const ExactEventApplied&);
}
