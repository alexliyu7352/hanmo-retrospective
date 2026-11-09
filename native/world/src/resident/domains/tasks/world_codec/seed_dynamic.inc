// 任务世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto extended = decode_dynamic_task_state(
    boundary::field(source, "dynamic_tasks", ""), *result.tasks);
if (!extended.ok)
  return {false, {}, extended.error};
result.tasks = std::make_shared<const TaskState>(std::move(extended.value));
