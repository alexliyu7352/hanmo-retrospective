// 任务世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (catalog.task_message_returns) {
  if (!result.tasks)
    fail("TaskMessageReturnOriginalSourcesRequired",
         task_message_return_module);
  auto parsed = decode_task_message_returns(
      field(source, task_message_return_module, "state"));
  if (!parsed.ok)
    return {false, {}, parsed.error};
  auto extended = std::make_shared<TaskState>(*result.tasks);
  extended->message_returns = std::move(parsed.value);
  result.tasks = std::move(extended);
} else if (source.find(task_message_return_module))
  fail("UnexpectedTaskMessageReturnDomain", task_message_return_module);
if (catalog.task_personal_messages) {
  if (!result.tasks)
    fail("TaskPersonalMessageOriginalSourcesRequired",
         task_personal_message_module);
  auto parsed = decode_task_personal_messages(
      field(source, task_personal_message_module, "state"));
  if (!parsed.ok)
    return {false, {}, parsed.error};
  auto extended = std::make_shared<TaskState>(*result.tasks);
  extended->personal_messages = std::move(parsed.value);
  result.tasks = std::move(extended);
} else if (source.find(task_personal_message_module))
  fail("UnexpectedTaskPersonalMessageDomain", task_personal_message_module);
if (catalog.task_personal_reports) {
  if (!result.tasks)
    fail("TaskPersonalReportOriginalSourcesRequired",
         task_personal_report_module);
  auto parsed = decode_task_personal_reports(
      field(source, task_personal_report_module, "state"));
  if (!parsed.ok)
    return {false, {}, parsed.error};
  auto extended = std::make_shared<TaskState>(*result.tasks);
  extended->personal_reports = std::move(parsed.value);
  result.tasks = std::move(extended);
} else if (source.find(task_personal_report_module))
  fail("UnexpectedTaskPersonalReportDomain", task_personal_report_module);
if (catalog.task_personal_acceptances) {
  if (!result.tasks)
    fail("TaskPersonalAcceptanceOriginalSourcesRequired",
         task_personal_acceptance_module);
  auto parsed = decode_task_personal_acceptances(
      field(source, task_personal_acceptance_module, "state"));
  if (!parsed.ok)
    return {false, {}, parsed.error};
  auto extended = std::make_shared<TaskState>(*result.tasks);
  extended->personal_acceptances = std::move(parsed.value);
  result.tasks = std::move(extended);
} else if (source.find(task_personal_acceptance_module))
  fail("UnexpectedTaskPersonalAcceptanceDomain",
       task_personal_acceptance_module);
if (catalog.task_personal_cancellations) {
  if (!result.tasks)
    fail("TaskPersonalCancellationOriginalSourcesRequired",
         task_personal_cancellation_module);
  auto parsed = decode_task_personal_cancellations(
      field(source, task_personal_cancellation_module, "state"));
  if (!parsed.ok)
    return {false, {}, parsed.error};
  auto extended = std::make_shared<TaskState>(*result.tasks);
  extended->personal_cancellations = std::move(parsed.value);
  result.tasks = std::move(extended);
} else if (source.find(task_personal_cancellation_module))
  fail("UnexpectedTaskPersonalCancellationDomain",
       task_personal_cancellation_module);
