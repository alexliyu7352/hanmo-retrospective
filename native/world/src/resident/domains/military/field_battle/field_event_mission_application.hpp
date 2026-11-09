// 原事件子型独立强类型合同；只移动原值，不新增持久字段或玩法。
#pragma once
#include "field_event.hpp"
namespace hanmo::resident {
struct FieldMissionApplication {
  FieldMission mission = FieldHold{};
  Integer mission_revision = 0;
  bool ai_pursuit = false;
  std::optional<SourceRef> command_result;
  std::optional<StableId> ai_source_id, order_id;
};
inline constexpr Integer field_event_mission_application_payload_id = 4;
// 可写访问只取得本候选自己的强类型 optional；只读空槽不构造或修补状态。
std::optional<FieldMissionApplication> &field_event_mission_application(FieldBattleEvent &);
const std::optional<FieldMissionApplication> &field_event_mission_application(const FieldBattleEvent &);
} // namespace hanmo::resident
