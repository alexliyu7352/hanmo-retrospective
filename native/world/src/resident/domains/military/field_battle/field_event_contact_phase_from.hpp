// 原事件子型独立强类型合同；只移动原值，不新增持久字段或玩法。
#pragma once
#include "field_event.hpp"
namespace hanmo::resident {
inline constexpr Integer field_event_contact_phase_from_payload_id = 5;
// 可写访问只取得本候选自己的强类型 optional；只读空槽不构造或修补状态。
std::optional<FieldBattleTime> &field_event_contact_phase_from(FieldBattleEvent &);
const std::optional<FieldBattleTime> &field_event_contact_phase_from(const FieldBattleEvent &);
} // namespace hanmo::resident
