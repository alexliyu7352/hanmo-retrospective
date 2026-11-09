// 任务世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto task_execution_layout = validate_native_task_execution_layout(w);
if (!task_execution_layout.ok)
  return task_execution_layout;
auto task_execution =
    selected_native_task_execution_version(w.catalog->identity);
if (!task_execution.ok)
  return {false, {}, std::move(task_execution.error)};
if (task_execution.value)
  WORLD_WHOLE(P::section, "task_execution_time",
              emit_task_exact_execution_projection(out, "",
                                                   *w.tasks.exact_execution,
                                                   task_execution.value));
