// 任务世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto work_version = selected_native_task_work_version(catalog.identity);
if (!work_version.ok)
  return {false, {}, std::move(work_version.error)};
if (work_version.value) {
  auto parsed =
      decode_task_work_execution(field(source, "task_work_execution", "state"));
  if (!parsed.ok)
    return {false, {}, std::move(parsed.error)};
  if (parsed.value.version != work_version.value)
    fail("TaskWorkSectionVersionMismatch", "task_work_execution");
  auto extended = checkpoint_make_shared<TaskState>(*result.tasks);
  extended->work_execution = std::move(parsed.value);
  result.tasks = std::move(extended);
} else if (source.find("task_work_execution"))
  fail("UnexpectedTaskWorkSection", "task_work_execution");
{
  CommonStore view(result.common, catalog.common);
  auto exact =
      validate_native_task_execution_layout(view, catalog, *result.tasks);
  if (!exact.ok)
    return {false, {}, std::move(exact.error)};
  auto payloads = projection_value([&](auto &out) {
    return emit_native_task_execution_fact_payloads_fields(out, view, catalog,
                                                           *result.tasks);
  });
  if (!payloads.ok)
    return {false, {}, std::move(payloads.error)};
  for (const auto &row : object(payloads.value, "task_execution_time/facts")) {
    const auto &actual =
        field(field(source, "facts", "state"), row.first, "facts");
    if (!compare_boundary(actual, row.second).exact())
      fail("TaskExecutionOriginalFactPayloadMismatch", row.first);
  }
}
auto work_layout = validate_native_task_work_layout(catalog, *result.tasks);
if (!work_layout.ok)
  return {false, {}, std::move(work_layout.error)};
{
  CommonStore view(result.common, catalog.common);
  const auto *work =
      result.tasks->work_execution ? &*result.tasks->work_execution : nullptr;
  auto headers = validate_task_work_original_fact_headers(view, work);
  if (!headers.ok)
    return {false, {}, std::move(headers.error)};
  headers = validate_task_work_original_action_headers(view, work);
  if (!headers.ok)
    return {false, {}, std::move(headers.error)};
  for (const auto &row : view.state().actions.rows())
    if (row.second.kind == "work_supervise") {
      const auto &params =
          field(field(field(source, "actions", "state"), row.first, "actions"),
                "params", row.first);
      auto checked = validate_task_work_original_action_payload(
          view, work, row.second, params);
      if (!checked.ok)
        return {false, {}, std::move(checked.error)};
      if (!checked.value)
        fail("MissingTaskWorkActionPayload", row.first);
    }
}
if (catalog.task_personal_advances) {
  const auto *input = source.find("task_personal_advances");
  if (!input)
    fail("MissingTaskAdvanceDomain", "task_personal_advances");
  auto advances = decode_task_personal_advance_state(*input);
  if (!advances.ok)
    return {false, {}, advances.error};
  auto extended = std::make_shared<TaskState>(*result.tasks);
  extended->personal_advances = std::move(advances.value);
  result.tasks = std::move(extended);
} else if (source.find("task_personal_advances"))
  fail("UnexpectedTaskAdvanceDomain", "task_personal_advances");
if (catalog.task_message_handoffs_enabled) {
  auto decoded = decode_task_message_handoffs(
      field(source, "task_message_handoffs", "state"));
  if (!decoded.ok)
    return {false, {}, decoded.error};
  auto extended = std::make_shared<TaskState>(*result.tasks);
  extended->message_handoffs = std::move(decoded.value);
  result.tasks = std::move(extended);
} else if (source.find("task_message_handoffs"))
  fail("UnexpectedTaskMessageHandoffDomain", "task_message_handoffs");
if (catalog.task_message_notices) {
  auto decoded = decode_task_message_notices(
      field(source, "task_message_notices", "state"));
  if (!decoded.ok)
    return {false, {}, decoded.error};
  auto extended = std::make_shared<TaskState>(*result.tasks);
  extended->message_notices = std::move(decoded.value);
  result.tasks = std::move(extended);
} else if (source.find("task_message_notices"))
  fail("UnexpectedTaskMessageNoticeDomain", "task_message_notices");
if (catalog.task_static_assets_enabled) {
  auto decoded =
      decode_task_static_assets(field(source, "task_static_assets", "state"));
  if (!decoded.ok)
    return {false, {}, decoded.error};
  auto extended = std::make_shared<TaskState>(*result.tasks);
  extended->static_assets = std::move(decoded.value);
  result.tasks = std::move(extended);
} else if (source.find("task_static_assets"))
  fail("UnexpectedTaskStaticAssetDomain", "task_static_assets");
if (catalog.task_extensions) {
  auto decoded =
      decode_task_extensions(field(source, "task_extensions", "state"));
  if (!decoded.ok)
    return {false, {}, decoded.error};
  auto extended = std::make_shared<TaskState>(*result.tasks);
  extended->deadline_extensions = std::move(decoded.value);
  result.tasks = std::move(extended);
} else if (source.find("task_extensions"))
  fail("UnexpectedTaskExtensionDomain", "task_extensions");
if (catalog.task_offer_responses) {
  const auto *input = source.find("task_offer_responses");
  if (!input)
    fail("MissingTaskOfferResponseDomain", "task_offer_responses");
  auto responses = decode_task_offer_response_state(*input);
  if (!responses.ok)
    return {false, {}, responses.error};
  auto extended = std::make_shared<TaskState>(*result.tasks);
  extended->offer_responses = std::move(responses.value);
  result.tasks = std::move(extended);
} else if (source.find("task_offer_responses"))
  fail("UnexpectedTaskOfferResponseDomain", "task_offer_responses");
