// 五种原任务与原波次只推进场内位置；实际出口返回给原 S1 交接，绝不瞬移世界锚。
#pragma once
#include "field_navigation.hpp"
#include <functional>
namespace hanmo::resident {
class CheckpointMemoryCharge;
struct FieldBattleWorldState;
struct FieldMissionStepRead {
  std::vector<StableId> reached_exits;
};
Outcome<FieldMissionStepRead> apply_field_mission_step_in_candidate(
    const FieldBattleWorldState &, FieldBattleSession &, const GeneratedBattleField &,
    const FieldBattleWindowSlice &, CheckpointMemoryCharge &,
    const std::function<Status()> &before_actual_motion);
} // namespace hanmo::resident
