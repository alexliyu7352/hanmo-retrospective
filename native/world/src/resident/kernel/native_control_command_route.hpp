#pragma once
#include "command_types.hpp"
namespace hanmo::resident {
struct WorldBundle;struct CompleteExactWorldReadBindings;
bool native_control_cancellation_command_kind(const StableId&);
// These two commands only manage the original continuation preparation/tail.
// Their original owner, not ordinary Action occupancy, supplies the permission.
Status guard_native_control_cancellation(const WorldBundle&,const StableId& actor);
Status preview_native_control_cancellation(const WorldBundle&,const StableId& actor,
    const CommandPayload&,const CompleteExactWorldReadBindings* = nullptr);
Outcome<ReceiptDetails> apply_native_control_cancellation(WorldBundle&,const StableId& actor,
    const PreparedDomainOp&,const CompleteExactWorldReadBindings* = nullptr);
// Only after the real Common result/history insertion. No second receipt or
// generation ledger is installed by this shared adapter.
Status bind_native_control_command_result(WorldBundle&,const CommitResult&);
}
