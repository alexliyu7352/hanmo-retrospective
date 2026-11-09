#pragma once
#include "field_deployment.hpp"
namespace hanmo::resident {
// Original deployment rectangle/wing only; no arbitrary point setter or
// authored fallback. The returned MoveTo includes the exact arrival facing.
Outcome<FieldMoveTo> read_field_flank_target(
    const FieldBattleSession &, const GeneratedBattleField &, Integer side,
    const FieldDeploymentPlacement &actual_original_placement);
} // namespace hanmo::resident
namespace hanmo::resident {
struct FieldAttackPrefixRead;
struct FieldFlankFollowupRead {
  const FieldCouncilAssignment *assignment = nullptr;
  const FieldFormation *target = nullptr;
  PointI position_dm;
  Integer facing_mdeg = 0;
};
// Borrow the actual adopted plan and derive a target at the actual arrival
// prefix. A null target is the genuine no-archer/no-melee Hold case.
Outcome<FieldFlankFollowupRead> read_field_flank_followup(
    const FieldBattleSession &, const GeneratedBattleField &, const StableId &,
    Integer actual_assignment_index, const FieldAttackPrefixRead * = nullptr);
} // namespace hanmo::resident
