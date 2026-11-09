// 交互世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。 本profile的wait没有业务params；其它未实现动作不能丢弃载荷。
if (!result.interaction)
  return {false, {}, common_failure("InteractionDecodedStateRequired").error};
{
  auto layout =
      validate_native_interaction_layout(catalog, *result.interaction);
  if (!layout.ok)
    return {false, {}, std::move(layout.error)};
}
{
  CommonStore view(result.common, catalog.common);
  auto checked = validate_personal_appointment_json_payloads(
      view,
      result.interaction && result.interaction->personal_appointments
          ? &*result.interaction->personal_appointments
          : nullptr,
      source);
  if (!checked.ok)
    return {false, {}, checked.error};
}
{
  CommonStore view(result.common, catalog.common);
  auto checked = validate_contact_inquiry_json_payloads(
      view,
      result.interaction && result.interaction->contact_inquiries
          ? &*result.interaction->contact_inquiries
          : nullptr,
      source);
  if (!checked.ok)
    return {false, {}, checked.error};
}
{
  CommonStore view(result.common, catalog.common);
  auto checked = validate_visit_reply_json_payloads(
      view,
      result.interaction && result.interaction->visit_message_replies
          ? &*result.interaction->visit_message_replies
          : nullptr,
      source);
  if (!checked.ok)
    return {false, {}, checked.error};
}
{
  CommonStore view(result.common, catalog.common);
  auto checked = validate_born_personal_dialogue_json_payloads(
      view,
      result.interaction && result.interaction->born_personal_dialogues
          ? &*result.interaction->born_personal_dialogues
          : nullptr,
      source);
  if (!checked.ok)
    return {false, {}, checked.error};
}
