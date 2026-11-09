#pragma once
#include "field_session.hpp"
namespace hanmo::resident {
class CommonStore;
struct MilitaryState;
struct FieldBattleWorldState;
struct FieldBattleAbilityReadContext;
struct FieldDeploymentApplied;
struct FieldReinforcementAdmission;
struct FieldInitialPlacementRead {
  PointI position_dm;
  Integer facing_mdeg = 0;
  // Exactly one actual accepted source; borrowed from the original owners.
  const FieldCouncilFact *council = nullptr;
  const FieldDeploymentApplied *deployment = nullptr;
  const SourceRef *deployment_source = nullptr;
  const FieldMission *immediate_mission = nullptr, *wave_mission = nullptr;
  std::optional<FieldHold> original_second_wave_hold;
  const FieldReinforcementAdmission *entry = nullptr;
};
// Historical initial placement, never today's reduced-fit geometry/current
// position. Actual entry formations need their own source, not this reader.
Outcome<FieldInitialPlacementRead> read_field_original_initial_placement(
    const CommonStore &, const MilitaryState &, const FieldBattleWorldState &,
    const FieldBattleSession &, const FieldFormation &,
    const GeneratedBattleField &, const FieldBattleAbilityReadContext &);
} // namespace hanmo::resident
