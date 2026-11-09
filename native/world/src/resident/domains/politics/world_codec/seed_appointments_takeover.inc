// 政治世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (political_military_appointments_enabled(catalog)) {
  auto state = decode_political_military_appointments(
      field(source, military_officer_political_module, "state"),
      political_field_appointments_enabled(catalog));
  if (!state.ok)
    return {false, {}, state.error};
  result.political_military_appointments =
      std::make_shared<const PoliticalMilitaryAppointmentState>(
          std::move(state.value));
} else if (source.find(military_officer_political_module))
  fail("UnexpectedPoliticalMilitaryAppointmentState", "state");
if (political_field_appointments_enabled(catalog)) {
  auto state = decode_political_field_appointments(
      field(source, military_field_command_political_module, "state"));
  if (!state.ok)
    return {false, {}, state.error};
  result.political_field_appointments =
      std::make_shared<const PoliticalFieldAppointmentState>(
          std::move(state.value));
} else if (source.find(military_field_command_political_module))
  fail("UnexpectedPoliticalFieldAppointmentState", "state");
if (takeover_enabled(catalog)) {
  auto selected = read_political_locked_body_version(catalog.identity,
                                                     "political_takeover");
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (!selected.value || selected.value != catalog.political_takeover->version)
    fail("PoliticalTakeoverPhysicalVersionMismatch", "political_takeover");
  auto political = selected.value == 2
                       ? decode_political_takeover_state2(
                             field(source, "political_takeover", "state"))
                       : decode_political_takeover_state(
                             field(source, "political_takeover", "state"));
  if (!political.ok)
    return {false, {}, political.error};
  result.political_takeover = std::make_shared<const PoliticalTakeoverState>(
      std::move(political.value));
  auto economic = decode_economic_takeover_execution(
      field(source, "economic_takeover_execution", "state"));
  if (!economic.ok)
    return {false, {}, economic.error};
  result.economic_takeover_execution =
      std::make_shared<const EconomicTakeoverExecutionState>(
          std::move(economic.value));
  auto task = decode_task_public_account_execution_state(
      field(source, "task_takeover_execution", "state"));
  if (!task.ok)
    return {false, {}, task.error};
  if (task.value.version != catalog.political_takeover->task_execution_version)
    fail("TaskPublicAccountExecutionVersion", "task_takeover_execution");
  auto extended = std::make_shared<TaskState>(*result.tasks);
  extended->takeover_execution = std::move(task.value);
  result.tasks = std::move(extended);
} else
  for (const auto *name : {"political_takeover", "economic_takeover_execution",
                           "task_takeover_execution"})
    if (source.find(name))
      fail("UnexpectedPoliticalTakeoverState", name);
