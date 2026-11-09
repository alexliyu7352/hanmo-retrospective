// 任务检查点恢复分派：沿原 else-if 链读取
// tasks；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "tasks") r = visit_task_checkpoint(reader, *tasks);
