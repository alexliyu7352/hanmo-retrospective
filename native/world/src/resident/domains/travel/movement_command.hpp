// Original command adapters for the selected exact S1 owner. The caller owns
// Common Action creation, the complete candidate capture, and publication.
#pragma once
#include "movement_control.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
Outcome<StableId> begin_exact_personal_movement_in_candidate(
 CommonStore&,TravelState&,MovementExactState&,const TravelCatalog&,const TravelServices&,
 const CurrentMomentRead&,const StableId& original_action_id,const TravelQuote&,
 const TravelCommand*,std::vector<BoundSpatialLocationWrite>&,ExactArithmeticBudget={});
struct TaskPersonalMovementFundingContext;
// This quote uses only the actual accepted Task funding. The carrier's empty
// wallet cannot reject a plan that is genuinely funded by its principal.
Outcome<TravelQuote> plan_task_funded_personal_movement(const TaskPersonalMovementFundingContext&,const SourceRef& actual_accepted_plan,const StableId& carrier_actor_id,const TravelDestination&,const std::vector<StableId>& via={});
struct MotionExactAuthorSelection;
// The enclosing accepted-transport command must post the real Task dispatch
// and run full reverse coverage before publishing this candidate.
Outcome<StableId> begin_task_funded_personal_movement_in_candidate(
 CommonStore&,TravelState&,MovementExactState&,const TaskPersonalMovementFundingContext&,
 const MotionExactAuthorSelection&,const StableId& original_action_id,
 const SourceRef& actual_accepted_plan,const TravelCommand&,
 std::vector<BoundSpatialLocationWrite>&,ExactArithmeticBudget={});
// Pause/resume/wait are original instantaneous controls. TravelWait then uses
// S0's complete advance in the same candidate, with the real held actor.
// This function never advances time, charges a step, or fabricates a terminal.
Outcome<SourceRef> apply_exact_personal_control_command_in_candidate(
 CommonStore&,TravelState&,MovementExactState&,const TravelCatalog&,const TravelServices&,
 const CurrentMomentRead&,const PreparedTravelOp&,ExactArithmeticBudget={});
}
