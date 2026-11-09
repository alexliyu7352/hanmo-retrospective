#pragma once
#include "field_member_loss_group_apply.hpp"
#include "resident/domains/travel/spatial_location_capture.hpp"
namespace hanmo::resident {
// Full private loss handoff: original body/source adoption followed immediately
// by the original S1 typed capture binder. Outer still owns atomic publication.
Outcome<BoundSpatialLocationWrite>
apply_field_captured_group_member_with_capture_in_candidate(
    CommonStore &, MovementExactState &, GroupMovementExecutionState &,
    const FieldGroupHandoffReadContext &,
    const FieldCapturedMemberApplication &, CheckpointMemoryCharge &,
    ExactArithmeticBudget = {});
Outcome<BoundSpatialLocationWrite>
apply_field_dead_group_member_with_capture_in_candidate(
    CommonStore &, MovementExactState &, GroupMovementExecutionState &,
    const FieldGroupHandoffReadContext &, const SourceRef &actual_casualty,
    const StableId &actor, const LifeConditionApplication &,
    CheckpointMemoryCharge &, ExactArithmeticBudget = {});
} // namespace hanmo::resident
