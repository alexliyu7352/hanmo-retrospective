// Borrowed read of real occupation intervals in one original ActionTime owner.
// No persistent row, alternate clock, permission or publication is created.
#pragma once
#include "hanmo/resident/world_moment.hpp"
#include <string_view>
namespace hanmo::resident {
struct FactRecord;
struct CommitResult;
// Each interval covers one actual acquisition through release. Fixed suspension
// ends an interval; resume starts another. Shared waits keep the original
// occupancy. The owner filters the requested actor using original participants.
// An application header is usable only when that owner proves its writer made
// the actual acquisition/release at that Common cut, not merely its cause.
struct ActionTimeActorOccupancyInterval {
  std::string_view action_ref;
  const WorldMoment &acquired_at;
  const FactRecord *acquired_application = nullptr;
  const WorldMoment *released_at = nullptr;
  const FactRecord *released_application = nullptr;
  // Optional original command receipts, validated by this same domain owner.
  // Only an owner whose actual application finishes before the next command
  // seal may supply these. A seal alone is never a same-candidate cut.
  const CommitResult *acquired_command = nullptr;
  const CommitResult *released_command = nullptr;
};
// The owner enumerates independently from ActionElapsed, including retained
// terminal executions, and retains all borrowed values for this synchronous
// call. A missing application head may prove strict-M ordering only; same-M
// queries must reject until the original writer supplies a genuine cut.
class ActionTimeActorHistorySink {
public:
  virtual ~ActionTimeActorHistorySink() = default;
  // Enumerate each genuine original execution exactly once before filtering
  // its participants. The borrowed ID survives this whole synchronous visit.
  // This structural callback must not invoke unrelated actors' business/P10
  // validation. It prevents deleted owner payloads from hiding S0 entries.
  virtual Status original_action_owner(std::string_view actual_action_id) = 0;
  virtual Status
  original_actor_occupancy(const ActionTimeActorOccupancyInterval &) = 0;
};
} // namespace hanmo::resident
