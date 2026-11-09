#pragma once
#include "training_exact.hpp"
#include "resident/kernel/exact_phase_graph.hpp"
namespace hanmo::resident {
// Original military provider's actual C06 sub-leaf. These functions do not
// register a second military owner or run the legacy calendar training timer.
Outcome<std::vector<ExactEventPrediction>> predict_exact_military_training_events(
 const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,
 const ExactTrainingReadContext&,const WorldMoment& current,const WorldMoment& upper);
Status provide_exact_military_training_interval(CommonStore&,MilitaryState&,
 MilitaryActionTimeState&,ActionElapsedStore&,const ExactTrainingWriteContext&,
 const MomentSpan&,ExactPhaseWriteContext&);
Outcome<ExactEventApplied> apply_exact_military_training_event(CommonStore&,MilitaryState&,
 MilitaryActionTimeState&,ActionElapsedStore&,const ExactTrainingWriteContext&,
 const ExactEventPrediction&,ExactPhaseWriteContext&);
// A later same-M body/roster/authority producer invokes this directly. There is
// no scheduled back-edge to immediate_consequences after action_completions.
Outcome<std::vector<ExactEventApplied>> apply_pending_exact_training_consequences_in_candidate(
 CommonStore&,MilitaryState&,MilitaryActionTimeState&,ActionElapsedStore&,
 const ExactTrainingWriteContext&,const MomentOwnerSource& actual_cause,
 ExactPhaseWriteContext&);
Status validate_exact_military_training_applied_source(const CommonStore&,const MilitaryState&,
 const MilitaryActionTimeState&,const ActionElapsedStore&,const ExactTrainingReadContext&,
 const ExactEventApplied&);
}
