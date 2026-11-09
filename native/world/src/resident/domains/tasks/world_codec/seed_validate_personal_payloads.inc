// 任务世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
{
  CommonStore view(result.common, catalog.common);
  auto checked = validate_task_message_return_json_payloads(
      view,
      result.tasks && result.tasks->message_returns
          ? &*result.tasks->message_returns
          : nullptr,
      source);
  if (!checked.ok)
    return {false, {}, checked.error};
}
{
  CommonStore view(result.common, catalog.common);
  auto checked = validate_task_personal_message_json_payloads(
      view,
      result.tasks && result.tasks->personal_messages
          ? &*result.tasks->personal_messages
          : nullptr,
      source);
  if (!checked.ok)
    return {false, {}, checked.error};
}
{
  CommonStore view(result.common, catalog.common);
  auto checked = validate_task_personal_report_json_payloads(
      view,
      result.tasks && result.tasks->personal_reports
          ? &*result.tasks->personal_reports
          : nullptr,
      source);
  if (!checked.ok)
    return {false, {}, checked.error};
}
{
  CommonStore view(result.common, catalog.common);
  auto checked = validate_task_personal_acceptance_json_payloads(
      view,
      result.tasks && result.tasks->personal_acceptances
          ? &*result.tasks->personal_acceptances
          : nullptr,
      source);
  if (!checked.ok)
    return {false, {}, checked.error};
}
{
  CommonStore view(result.common, catalog.common);
  auto checked = validate_task_personal_cancellation_json_payloads(
      view,
      result.tasks && result.tasks->personal_cancellations
          ? &*result.tasks->personal_cancellations
          : nullptr,
      source);
  if (!checked.ok)
    return {false, {}, checked.error};
}
