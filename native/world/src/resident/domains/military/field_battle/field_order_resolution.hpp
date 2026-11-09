#pragma once
#include "field_session.hpp"
namespace hanmo::resident {
class CheckpointMemoryCharge;
Status record_field_order_resolution(FieldBattleSession &, FieldOrderRecord &,
                                     CheckpointMemoryCharge &);
// Source-local bidirectional identity only. Original historical military
// authority is additionally checked by the complete command-source validator.
Status validate_field_order_resolution_sources(const FieldBattleSession &);
} // namespace hanmo::resident
