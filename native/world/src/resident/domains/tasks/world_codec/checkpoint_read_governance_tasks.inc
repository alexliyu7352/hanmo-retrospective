// 任务检查点恢复分派：沿原 else-if 链读取 governance_tasks,
// task_personal_advances, task_offer_responses, envoy_tasks,
// dynamic_tasks；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "governance_tasks") {
  auto governance = make_state<Seed, GovernanceTaskState>();
  r = visit_governance_task_checkpoint(reader, *governance);
  seed.governance_tasks = std::move(governance);
}
else if (section.domain == "task_personal_advances") {
  tasks->personal_advances.emplace();
  r = visit_task_personal_advance_checkpoint(reader, *tasks->personal_advances);
}
else if (section.domain == "task_offer_responses") {
  tasks->offer_responses.emplace();
  r = visit_task_offer_response_checkpoint(reader, *tasks->offer_responses);
}
else if (section.domain == "envoy_tasks") {
  auto envoy = make_state<Seed, EnvoyTaskState>();
  r = visit_envoy_task_checkpoint(reader, *envoy);
  seed.envoy_tasks = std::move(envoy);
}
else if (section.domain == "dynamic_tasks") r =
    visit_dynamic_task_checkpoint(reader, *tasks);
