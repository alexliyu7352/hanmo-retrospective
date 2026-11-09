// 原事件子型独立强类型合同；只移动原值，不新增持久字段或玩法。
#pragma once
#include "field_event.hpp"
namespace hanmo::resident {
struct FieldMotionApplication {
  FieldBattleTime evaluated_at;
  Integer navigation_sequence = 0;
  std::optional<Integer> previous_motion_sequence;
  PointI from_dm, to_dm;
  bool reached = false, blocked = false;
};
inline constexpr Integer field_event_motion_application_payload_id = 3;
// 可写访问只取得本候选自己的强类型 optional；只读空槽不构造或修补状态。
std::optional<FieldMotionApplication> &field_event_motion_application(FieldBattleEvent &);
const std::optional<FieldMotionApplication> &field_event_motion_application(const FieldBattleEvent &);
} // namespace hanmo::resident
