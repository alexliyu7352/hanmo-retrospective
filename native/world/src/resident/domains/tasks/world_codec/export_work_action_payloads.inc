// 任务世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto work_actions = validate_task_work_original_action_headers(
    w.common, w.tasks.work_execution ? &*w.tasks.work_execution : nullptr);
if (!work_actions.ok)
  return work_actions;
for (const auto &row : w.common.state().actions.rows())
  if (row.second.kind == "work_supervise") {
    if (composition.has_action_payload(row.first))
      fail("DuplicateTaskWorkActionPayload", row.first);
    WORLD_FIELDS(
        P::action_payloads, "task_work_execution",
        out.object(row.first, [&](auto &fields) -> Status {
          auto result = emit_task_work_original_action_payload(
              fields, w.common,
              w.tasks.work_execution ? &*w.tasks.work_execution : nullptr,
              row.second);
          if (!result.ok)
            return {false, {}, std::move(result.error)};
          return result.value ? common_success()
                              : common_failure("MissingTaskWorkActionPayload",
                                               row.first);
        }));
  }
