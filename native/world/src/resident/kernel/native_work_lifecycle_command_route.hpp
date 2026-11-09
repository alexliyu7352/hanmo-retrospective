// Original Work reassignment commands use the sole TaskWork graph and Common seal.
#pragma once
#include "command_types.hpp"
namespace hanmo::resident {
struct WorldBundle;
bool native_work_lifecycle_command_kind(const StableId&)noexcept;
bool native_work_lifecycle_command_payload(const CommandPayload&)noexcept;
Outcome<CommandPayload> decode_native_work_lifecycle_command(const StableId&,const BoundaryValue&);
Status preview_native_work_lifecycle_command(const WorldBundle&,const StableId& authenticated_actor,const CommandPayload&);
Outcome<ReceiptDetails> apply_native_work_lifecycle_command(WorldBundle&,const StableId& authenticated_actor,const PreparedDomainOp&);
// The original Common result is still the only command receipt. This selected
// profile gate does not infer source authority from an administrative Done.
Status validate_native_work_lifecycle_command_history(const WorldBundle&);
}
