// 原脉冲士气与已发动齐射效果；不作逃散名册选择或世界提交。
#pragma once
#include "field_pulse.hpp"
namespace hanmo::resident {
class CommonStore;
class CheckpointMemoryCharge;
struct FieldMoraleRead {
  std::vector<StableId> zero_morale, rallied;
};
Outcome<FieldMoraleRead> apply_field_pulse_morale_in_candidate(
    const CommonStore &, FieldBattleSession &, const GeneratedBattleField &,
    const FieldPulseRead &, const FieldBattleWindowSlice &,
    CheckpointMemoryCharge &actual_world_owner);
} // namespace hanmo::resident
