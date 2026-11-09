// The 31ef queue belongs to the original boundary, not a second army or world.
// Only completed actual admissions become session.formations. These records
// are original immutable sources for their own initial mission/ability cut.
#pragma once
#include "field_session.hpp"
namespace hanmo::resident {
struct FieldReinforcementAdmission {
  SourceRef application_source;
  StableId formation_id;
  FieldBattleTime at;
  WorldMoment applied_at;
  Integer event_sequence = 0;
  PointI position_dm;
  Integer facing_mdeg = 0;
  FieldFormationControl initial_control = FieldAutonomousControl{};
  FieldMission initial_mission = FieldHold{};
  // No synthetic player result is used for the automatic AI arrival mission.
  bool ai_mission = false;
  bool pending_player_entry_order = false;
};
struct FieldReinforcementQueue {
  SourceRef initialized_source;
  WorldMoment initialized_at;
  // Complete original deterministic ordering; admitted is exactly its prefix.
  // Unadmitted identities have no local position or simulated formation.
  std::vector<StableId> formation_ids;
  std::vector<FieldReinforcementAdmission> admitted;
};
} // namespace hanmo::resident
