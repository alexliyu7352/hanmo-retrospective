#pragma once
#include "resupply_exact.hpp"
#include "resident/kernel/exact_phase_graph.hpp"
namespace hanmo::resident {
// Sub-leaf of the one military owner; no parallel calendar resupply timer.
Outcome<std::vector<ExactEventPrediction>> predict_exact_resupply_events(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,const ExactResupplyReadContext&,const WorldMoment& current,const WorldMoment& upper);
Status provide_exact_resupply_interval(CommonStore&,MilitaryState&,MilitaryActionTimeState&,ActionElapsedStore&,const ExactResupplyWriteContext&,const MomentSpan&);
Outcome<ExactEventApplied> apply_exact_resupply_event(CommonStore&,MilitaryState&,MilitaryActionTimeState&,ActionElapsedStore&,const ExactResupplyWriteContext&,const ExactEventPrediction&,ExactPhaseWriteContext&);
Status validate_exact_resupply_applied_source(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,const ActionElapsedStore&,const ExactResupplyReadContext&,const ExactEventApplied&);
}
