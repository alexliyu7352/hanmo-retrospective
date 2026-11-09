#include "field_event_mission_transition.hpp"
#pragma once
#include "field_session.hpp"
namespace hanmo::resident {
struct GeneratedBattleField;
class CheckpointMemoryCharge;
class CommonStore;
struct FieldBattleWorldState;
// Only original rule predicates may derive a mission. No caller-supplied to
// mission, position, revision, command source, or world moment is accepted.
Status apply_field_rule_mission_transition(
    FieldBattleSession &, FieldFormation &, const GeneratedBattleField &,
    FieldMissionTransitionReason, const FieldBattleTime &,
    CheckpointMemoryCharge &,
    std::optional<Integer> actual_trigger_event = std::nullopt);
struct FieldMissionOriginRead {
  const FieldMission *mission = nullptr;
  bool ai_pursuit = false;
};
// Returns the original accepted mission through the actual retained chain;
// the existing command/AI/order/council reader must still verify that origin.
Outcome<FieldMissionOriginRead>
read_field_current_mission_origin(const FieldBattleSession &,
                                  const FieldFormation &);
Status validate_field_mission_transition_events(const CommonStore &,
                                                const FieldBattleWorldState &,
                                                const FieldBattleSession &,
                                                const GeneratedBattleField &);
} // namespace hanmo::resident
