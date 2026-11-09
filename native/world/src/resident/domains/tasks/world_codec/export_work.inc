// 任务世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto task_work = validate_native_task_work_world(w);
if (!task_work.ok)
  return task_work;
if (w.tasks.work_execution)
  WORLD_WHOLE(P::section, "task_work_execution",
              emit_task_work_execution_projection(out, "",
                                                  *w.tasks.work_execution));
