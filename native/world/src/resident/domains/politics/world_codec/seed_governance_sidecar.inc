// 政治世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
fail("UnexpectedPoliticsGovernanceState", "politics_governance");
// 冷JSON仅恢复本情景显式治理sidecar；临时共同只读封装只校验原源，不发布另一个世界。
if (catalog.governance_tasks_enabled) {
  auto decoded =
      decode_governance_task_state(field(source, "governance_tasks", "state"));
  if (!decoded.ok)
    return {false, {}, decoded.error};
  result.governance_tasks =
      std::make_shared<const GovernanceTaskState>(std::move(decoded.value));
  CommonStore view(result.common, catalog.common);
  auto payloads = governance_task_payload_registry(view, *result.tasks,
                                                   *result.governance_tasks);
  if (!payloads.ok)
    return {false, {}, payloads.error};
  auto valid = validate_governance_task_cold_payloads(source, payloads.value);
  if (!valid.ok)
    return {false, {}, valid.error};
} else if (source.find("governance_tasks"))
  fail("UnexpectedGovernanceTasksState", "governance_tasks");
