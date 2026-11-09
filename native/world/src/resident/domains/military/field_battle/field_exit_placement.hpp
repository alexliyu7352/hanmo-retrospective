#pragma once
#include "field_active_membership.hpp"
#include "field_group_handoff.hpp"
#include "resident/domains/travel/field_battle_exit_placement.hpp"
namespace hanmo::resident {
class OriginalFieldBattleExitPlacementRead final
    : public FieldBattleExitPlacementRead,
      public GroupContactMemberReleaseRead,
      public FieldBattleActiveMembershipRead {
  FieldGroupHandoffReadContext context_;
  ExactArithmeticBudget budget_;

public:
  explicit OriginalFieldBattleExitPlacementRead(FieldGroupHandoffReadContext c,
                                                ExactArithmeticBudget b = {})
      : context_(c), budget_(b) {}
  bool bound_to(const CommonStore &,
                const MovementExactState &) const noexcept override;
  bool bound_to(const CommonStore &, const MovementExactState &,
                const GroupMovementExecutionState &) const noexcept override;
  Outcome<ActualActorSpatialWriteRead>
  read_field_exit_placement(const MomentOwnerSource &,
                            const StableId &) const override;
  Outcome<CheckpointVector<ActualActorSpatialWriteRead>>
  read_field_exit_placement_history(const StableId &) const override;
  Outcome<FieldActorMemberLossPlacementRead>
  read_field_member_loss_placement(const MomentOwnerSource &,
                                   const StableId &) const override;
  Outcome<CheckpointVector<FieldActorMemberLossPlacementRead>>
  read_field_member_loss_placement_history(const StableId &) const override;
  Outcome<FieldActorArmyExitStablePlacementRead>
  read_field_army_exit_stable_placement(const MomentOwnerSource &,
                                       const StableId &) const override;
  Outcome<CheckpointVector<ActualActorSpatialWriteRead>>
  read_field_army_exit_stable_placement_history(const StableId &) const override;
  Outcome<CheckpointVector<FieldActorEngagementRead>>
  read_field_actor_engagement_history(const StableId &) const override;
  bool bound_to(const CommonStore &,
                const FieldBattleWorldState &) const noexcept override;
  Outcome<FieldActorEngagementRead>
  read_field_active_member_engagement(const StableId &,
                                      const StableId &) const override;
  Status validate_actual_group_member_loss(
      const GroupMemberLossHandoffSource &) const override;
  Status validate_actual_group_member_release(
      const GroupContactMemberReleaseSource &) const override;
};
} // namespace hanmo::resident
