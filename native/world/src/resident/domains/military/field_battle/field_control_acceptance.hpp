#pragma once
#include "hanmo/resident/world_moment.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
namespace hanmo::resident {
struct FieldBattleSession;
class CurrentMomentRead;
class OriginalWorldReadBoundary;
// The lease is runtime ownership metadata, never a serialized permission.
// Every value-copy pre-rents its own fresh payload; moves preserve that owner.
struct FieldControlAcceptanceBoundary {
  CheckpointCopyLease copied_fields;
  CommonActualFactPrefix common_prefix;
  Integer last_local_event_sequence = 0;
  WorldMoment observed_at;
  FieldControlAcceptanceBoundary() = default;
  FieldControlAcceptanceBoundary(const FieldControlAcceptanceBoundary &);
  FieldControlAcceptanceBoundary &
  operator=(const FieldControlAcceptanceBoundary &);
  FieldControlAcceptanceBoundary(FieldControlAcceptanceBoundary &&) = default;
  FieldControlAcceptanceBoundary &operator=(FieldControlAcceptanceBoundary &&);
};
Outcome<FieldControlAcceptanceBoundary> capture_field_control_acceptance(
    const CommonStore &, const FieldBattleSession &, const CurrentMomentRead &,
    const OriginalWorldReadBoundary &, bool committed_command);
Status
validate_field_control_acceptance(const CommonStore &,
                                  const FieldBattleSession &,
                                  const FieldControlAcceptanceBoundary &);
} // namespace hanmo::resident
