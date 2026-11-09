#pragma once
#include "field_group_handoff.hpp"
#include "field_member_loss_application.hpp"
namespace hanmo::resident {
struct ActorLocationCaptureIdentity;
struct ActorLocationWriteIdentity;
struct LifeConditionApplication;
struct FieldGroupMemberLossApplication {
  SourceRef actual_handoff;
  std::shared_ptr<const ActorLocationCaptureIdentity> capture_identity;
  std::shared_ptr<const ActorLocationWriteIdentity> write_identity;
  Integer capture_sequence = 0;
};
// After the original captured body application, transfer only its existing S1
// carrier binding to the actual captor. Never adds active workers/Army members
// or performs a second Common body/location mutation. Failure discards
// candidate.
Outcome<FieldGroupMemberLossApplication>
adopt_field_captured_group_member_in_candidate(
    CommonStore &, MovementExactState &, GroupMovementExecutionState &,
    const FieldGroupHandoffReadContext &,
    const FieldCapturedMemberApplication &, CheckpointMemoryCharge &,
    ExactArithmeticBudget = {});
Outcome<FieldGroupMemberLossApplication>
apply_field_dead_group_member_in_candidate(
    CommonStore &, MovementExactState &, GroupMovementExecutionState &,
    const FieldGroupHandoffReadContext &, const SourceRef &actual_casualty,
    const StableId &actor, const LifeConditionApplication &,
    CheckpointMemoryCharge &, ExactArithmeticBudget = {});
} // namespace hanmo::resident
