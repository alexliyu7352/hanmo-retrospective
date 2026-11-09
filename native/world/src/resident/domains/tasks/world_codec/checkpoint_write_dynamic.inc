// 任务世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
r = section("dynamic_tasks",
            [&](auto &c) { return visit_dynamic_task_checkpoint(c, w.tasks); });
if (!r.ok)
  return {false, {}, r.error};
