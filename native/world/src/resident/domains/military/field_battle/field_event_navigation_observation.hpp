// 原事件子型独立强类型合同；只移动原值，不新增持久字段或玩法。
#pragma once
#include "field_event.hpp"
namespace hanmo::resident {
struct FieldNavigationObservation {
  FieldBattleTime evaluated_at;
  FieldMission mission = FieldHold{};
  Integer mission_revision = 0;
  std::optional<Integer> mission_transition_sequence;
  std::optional<SourceRef> original_command_result;
  std::optional<StableId> original_ai_source_id, original_order_id;
  bool routing = false, ai_pursuit = false;
  PointI from_dm, target_dm;
  std::optional<StableId> target_formation_id;
  std::optional<Integer> target_motion_sequence;
};
inline constexpr Integer field_event_navigation_observation_payload_id = 2;
// 可写访问只取得本候选自己的强类型 optional；只读空槽不构造或修补状态。
std::optional<FieldNavigationObservation> &field_event_navigation_observation(FieldBattleEvent &);
const std::optional<FieldNavigationObservation> &field_event_navigation_observation(const FieldBattleEvent &);
} // namespace hanmo::resident
