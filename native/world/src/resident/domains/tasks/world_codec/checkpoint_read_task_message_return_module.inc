// 任务检查点恢复分派：沿原 else-if 链读取 task_message_return_module,
// task_personal_message_module, task_personal_report_module,
// task_personal_acceptance_module,
// task_personal_cancellation_module；不改变段顺序、所有权或拒绝条件。
else if (section.domain == task_message_return_module) {
  tasks->message_returns.emplace();
  r = visit_task_message_returns_checkpoint(reader, *tasks->message_returns);
}
else if (section.domain == task_personal_message_module) {
  tasks->personal_messages.emplace();
  r = visit_task_personal_messages_checkpoint(reader,
                                              *tasks->personal_messages);
}
else if (section.domain == task_personal_report_module) {
  tasks->personal_reports.emplace();
  r = visit_task_personal_reports_checkpoint(reader, *tasks->personal_reports);
}
else if (section.domain == task_personal_acceptance_module) {
  tasks->personal_acceptances.emplace();
  r = visit_task_personal_acceptances_checkpoint(reader,
                                                 *tasks->personal_acceptances);
}
else if (section.domain == task_personal_cancellation_module) {
  tasks->personal_cancellations.emplace();
  r = visit_task_personal_cancellations_checkpoint(
      reader, *tasks->personal_cancellations);
}