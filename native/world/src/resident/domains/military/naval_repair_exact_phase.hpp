#pragma once
#include "naval_repair_exact.hpp"
#include "resident/kernel/exact_phase_graph.hpp"
namespace hanmo::resident {
Outcome<std::vector<ExactEventPrediction>> predict_exact_naval_repair_events(const CommonStore&,const MilitaryState&,const NavalState&,const MilitaryActionTimeState&,const ExactNavalRepairReadContext&,const WorldMoment& current,const WorldMoment& upper);
Status provide_exact_naval_repair_interval(CommonStore&,const MilitaryState&,const NavalState&,MilitaryActionTimeState&,ActionElapsedStore&,const ExactNavalRepairWriteContext&,const MomentSpan&);
Outcome<ExactEventApplied> apply_exact_naval_repair_event(CommonStore&,const MilitaryState&,NavalState&,MilitaryActionTimeState&,ActionElapsedStore&,const ExactNavalRepairWriteContext&,const ExactEventPrediction&,ExactPhaseWriteContext&);
Status validate_exact_naval_repair_applied_source(const CommonStore&,const MilitaryState&,const NavalState&,const MilitaryActionTimeState&,const ActionElapsedStore&,const ExactNavalRepairReadContext&,const ExactEventApplied&);
// Call only at the original cause writer's successful same-candidate tail.
// Returns only true results and routes their existing ActionCompletion once.
Status consume_exact_naval_repair_interruptions(CommonStore&,const MilitaryState&,NavalState&,MilitaryActionTimeState&,ActionElapsedStore&,const ExactNavalRepairWriteContext&,ExactPhaseWriteContext&);
}
