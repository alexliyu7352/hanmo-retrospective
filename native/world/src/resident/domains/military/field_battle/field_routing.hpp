// 2026-10-27 固定澄清：Routing 未俘成员留场内，真正出口/终局才落逃散。
#pragma once
#include "field_session.hpp"
namespace hanmo::resident {
class CheckpointMemoryCharge;
struct FieldRoutingRead {
  std::vector<StableId> started;
};
Outcome<FieldRoutingRead> apply_field_routing_in_candidate(
    FieldBattleSession &, const GeneratedBattleField &,
    const FieldBattleWindowSlice &, CheckpointMemoryCharge &actual_world_owner);
// 仅真实离场/终局写者调用；不为仍在途中或已恢复 Hold 的编队转逃散。
Status settle_field_routing_deserters_at_exit(FieldFormation &);
} // namespace hanmo::resident
