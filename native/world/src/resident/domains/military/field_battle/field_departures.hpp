// Original §7.9/10 local transitions; not a substitute for S1 exit handoff.
#pragma once
#include "field_mission_step.hpp"
namespace hanmo::resident {
class CheckpointMemoryCharge;
struct FieldBattleWorldState;
struct FieldDepartureRead {
  std::vector<StableId> exited_formations;
  bool terminal = false;
};
// Caller must consume every exit with the real same-candidate world handoff
// before publishing. Does not advance session.now or any S0 clock.
Outcome<FieldDepartureRead> apply_field_departures_in_candidate(
    const FieldBattleWorldState &, FieldBattleSession &, const GeneratedBattleField &,
    const FieldBattleWindowSlice &, CheckpointMemoryCharge &);
} // namespace hanmo::resident
