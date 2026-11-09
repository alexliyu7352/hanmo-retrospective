// Military owns the actual takeover source; Growth owns its skill prefix.
#pragma once
#include "field_world.hpp"
namespace hanmo::resident {
class CommonStore;
class CheckpointMemoryCharge;
class FieldSegmentApplicationRead;
struct FieldCommanderChangeSourceRead {
  const FieldBattleSession *session = nullptr;
  const FieldFormation *formation = nullptr;
  const FieldCommanderChange *change = nullptr;
  const FieldBattleEvent *lost = nullptr;
};
// Cold source lookup does not require today's predecessor/deputy to be alive.
Outcome<FieldCommanderChangeSourceRead> read_field_commander_change_source(
    const CommonStore &, const FieldBattleWorldState &, const SourceRef &);
// Records only the true takeover boundary in the unpublished candidate.
// Caller MUST complete the real Growth/tactic rebind before publishing; this
// source producer does not claim that ability/current-tactic state is ready.
Outcome<std::optional<SourceRef>>
record_field_commander_change_source_in_candidate(
    CommonStore &, const MilitaryState &, FieldBattleWorldState &,
    const StableId &battle_id, const StableId &formation_id,
    const FieldBattleTime &actual_lost_boundary,
    const FieldSegmentApplicationRead &, CheckpointMemoryCharge &);
} // namespace hanmo::resident
