// 任务检查点恢复分派：沿原 else-if 链读取 task_takeover_execution,
// task_message_handoffs, task_message_notices, task_static_assets,
// task_execution_time, task_work_execution,
// task_extensions；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "task_takeover_execution") {
  tasks->takeover_execution.emplace();
  r = visit_task_public_account_execution_checkpoint(
      reader, *tasks->takeover_execution);
}
else if (section.domain == "task_message_handoffs") {
  tasks->message_handoffs.emplace();
  r = visit_task_message_handoffs_checkpoint(reader, *tasks->message_handoffs);
}
else if (section.domain == "task_message_notices") {
  tasks->message_notices.emplace();
  r = visit_task_message_notices_checkpoint(reader, *tasks->message_notices);
}
else if (section.domain == "task_static_assets") {
  tasks->static_assets.emplace();
  r = visit_task_static_assets_checkpoint(reader, *tasks->static_assets);
}
else if (section.domain == "task_execution_time") {
  auto selected = selected_native_task_execution_version(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (!selected.value || section.codec_version != selected.value ||
      tasks->exact_execution)
    return {false,
            {},
            common_failure("TaskExecutionCheckpointProfileMismatch").error};
  r = reader.prepare_default_values<TaskExactExecutionState>();
  if (!r.ok)
    return {false, {}, std::move(r.error)};
  tasks->exact_execution.emplace();
  tasks->exact_execution->version = selected.value;
  r = visit_task_exact_execution_checkpoint(reader, *tasks->exact_execution,
                                            selected.value);
}
else if (section.domain == "task_work_execution") {
  auto selected = selected_native_task_work_version(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (!selected.value || section.codec_version != selected.value ||
      tasks->work_execution)
    return {
        false, {}, common_failure("TaskWorkCheckpointProfileMismatch").error};
  tasks->work_execution.emplace();
  r = visit_task_work_execution_checkpoint(reader, *tasks->work_execution);
  if (r.ok && tasks->work_execution->version != selected.value)
    r = common_failure("TaskWorkSectionVersionMismatch");
}
else if (section.domain == "task_extensions") {
  tasks->deadline_extensions.emplace();
  r = visit_task_extensions_checkpoint(reader, *tasks->deadline_extensions);
}