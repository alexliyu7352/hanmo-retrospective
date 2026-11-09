#include "field_member_loss_bound_apply.hpp"
#include "field_exit_placement.hpp"
#include "resident/kernel/world_copy_plan.hpp"
namespace hanmo::resident {
namespace {
Outcome<BoundSpatialLocationWrite>
bind_loss(const FieldGroupHandoffReadContext &x, const StableId &actor,
          const FieldGroupMemberLossApplication &applied,
          ExactArithmeticBudget budget) {
  // MomentOwnerSource's temporary three strings are rented before the first
  // copy. The returned BoundSpatialLocationWrite rents its own original
  // payload.


  const auto memory = CheckpointMemoryCharge(0);
  MomentOwnerSource source{
      {checkpoint_fresh_string_copy(applied.actual_handoff.domain),
       checkpoint_fresh_string_copy(applied.actual_handoff.kind),
       checkpoint_fresh_string_copy(applied.actual_handoff.id)},
      1};
  OriginalFieldBattleExitPlacementRead original(x, budget);
  return bind_actual_field_member_loss_location_write(
      x.common, x.movement, original, actor, source, applied.capture_identity,
      applied.write_identity, applied.capture_sequence, budget);
}
} // namespace
Outcome<BoundSpatialLocationWrite>
apply_field_captured_group_member_with_capture_in_candidate(
    CommonStore &common, MovementExactState &movement,
    GroupMovementExecutionState &groups, const FieldGroupHandoffReadContext &x,
    const FieldCapturedMemberApplication &body, CheckpointMemoryCharge &owner,
    ExactArithmeticBudget budget) {
  auto adopted = adopt_field_captured_group_member_in_candidate(
      common, movement, groups, x, body, owner, budget);
  if (!adopted.ok)
    return {false, {}, adopted.error};
  const auto *row =
      groups.member_loss_handoffs.find(adopted.value.actual_handoff.id);
  if (!row)
    return {false,
            {},
            common_failure("FieldMemberLossActualHandoffRequired").error};
  return bind_loss(x, row->actor_id, adopted.value, budget);
}
Outcome<BoundSpatialLocationWrite>
apply_field_dead_group_member_with_capture_in_candidate(
    CommonStore &common, MovementExactState &movement,
    GroupMovementExecutionState &groups, const FieldGroupHandoffReadContext &x,
    const SourceRef &casualty, const StableId &actor,
    const LifeConditionApplication &life, CheckpointMemoryCharge &owner,
    ExactArithmeticBudget budget) {
  auto adopted = apply_field_dead_group_member_in_candidate(
      common, movement, groups, x, casualty, actor, life, owner, budget);
  if (!adopted.ok)
    return {false, {}, adopted.error};
  return bind_loss(x, actor, adopted.value, budget);
}
} // namespace hanmo::resident
