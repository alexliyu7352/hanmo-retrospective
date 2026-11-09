// 任务世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
r = validate_native_task_work_world(w);
if (!r.ok)
  return r;
if (w.tasks.work_execution) {
  r = section("task_work_execution", [&](auto &c) {
    return visit_task_work_execution_checkpoint(c, *w.tasks.work_execution);
  });
  if (!r.ok)
    return r;
}
