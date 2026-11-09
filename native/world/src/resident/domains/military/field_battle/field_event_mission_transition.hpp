// 原事件子型独立强类型合同；只移动原值，不新增持久字段或玩法。
#pragma once
#include "field_event.hpp"
namespace hanmo::resident {
enum class FieldMissionTransitionReason {
  Arrived,
  TargetUnavailable,
  PursuitLimit,
  Rallied,
  WaveReleased,
  FollowupAdopted
};
struct FieldMissionTransition {
  FieldMissionTransitionReason reason = FieldMissionTransitionReason::Arrived;
  FieldMission from = FieldHold{}, to = FieldHold{};
  bool from_ai_pursuit = false, to_ai_pursuit = false;
  Integer mission_revision = 0;
  std::optional<Integer> previous_sequence;
  std::optional<SourceRef> original_command_result;
  std::optional<StableId> original_ai_source_id, original_order_id;
  // Actual local rule evidence; not a second position or world-time account.
  PointI at_position_dm;
  std::optional<StableId> target_formation_id;
  std::optional<Integer> trigger_event_sequence;
  std::optional<Integer> original_flank_assignment_index;
};
inline constexpr Integer field_event_mission_transition_payload_id = 1;
// 可写访问只取得本候选自己的强类型 optional；只读空槽不构造或修补状态。
std::optional<FieldMissionTransition> &field_event_mission_transition(FieldBattleEvent &);
const std::optional<FieldMissionTransition> &field_event_mission_transition(const FieldBattleEvent &);
} // namespace hanmo::resident
