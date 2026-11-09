// 原 field_exposure_v1 实际脉冲后果；不造 Family 死亡或新的世界时钟。
#pragma once
#include "field_pulse.hpp"
namespace hanmo::resident {
class CheckpointMemoryCharge;
struct FieldCommanderPulseRead {
  std::vector<StableId> changed_formations, lost_formations;
};
// 出口 changed 必须由完整 driver 重读真正现任/副手能力，不能沿用逝者技能。
Outcome<FieldCommanderPulseRead> apply_field_commander_pulse_in_candidate(
    FieldBattleSession &, const GeneratedBattleField &, const FieldPulseRead &,
    const FieldBattleWindowSlice &, CheckpointMemoryCharge &actual_world_owner);
} // namespace hanmo::resident
