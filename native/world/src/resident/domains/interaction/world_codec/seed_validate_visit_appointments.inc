// 交互世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
{
  CommonStore view(result.common, catalog.common);
  auto checked = validate_visit_appointment_json_payloads(
      view,
      result.interaction && result.interaction->visit_appointment_invitations
          ? &*result.interaction->visit_appointment_invitations
          : nullptr,
      result.interaction && result.interaction->visit_appointment_meetings
          ? &*result.interaction->visit_appointment_meetings
          : nullptr,
      source);
  if (!checked.ok)
    return {false, {}, checked.error};
}
