// 任务世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto tasks = decode_task_seed(source);
if (!tasks.ok)
  return {false, {}, tasks.error};
auto task_execution = selected_native_task_execution_version(catalog.identity);
if (!task_execution.ok)
  return {false, {}, std::move(task_execution.error)};
if (task_execution.value) {
  ScopedCheckpointJsonDecode original(
      CheckpointWorldJsonSection::task_execution_time);
  auto exact = decode_task_exact_execution(
      field(source, "task_execution_time", "state"), task_execution.value);
  if (!exact.ok)
    return {false, {}, std::move(exact.error)};
  tasks.value.exact_execution.emplace(std::move(exact.value));
} else if (source.find("task_execution_time"))
  fail("UnexpectedTaskExecutionTimeState", "task_execution_time");
result.tasks = std::make_shared<const TaskState>(std::move(tasks.value));
