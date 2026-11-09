// 政治世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (catalog.envoy_tasks_enabled) {
  if (!result.diplomacy || !result.organization || !catalog.diplomacy ||
      !catalog.organization)
    fail("MissingEnvoyCatalog", "envoy_tasks");
  auto decoded = decode_envoy_task_state(field(source, "envoy_tasks", "state"));
  if (!decoded.ok)
    return {false, {}, decoded.error};
  if (decoded.value.version != catalog.diplomacy->version)
    fail("EnvoyTaskVersionMismatch", "envoy_tasks");
  result.envoy_tasks =
      std::make_shared<const EnvoyTaskState>(std::move(decoded.value));
  CommonStore view(result.common, catalog.common);
  DiplomaticMissionAccess missions(view, *result.organization,
                                   *catalog.organization, *catalog.diplomacy,
                                   *result.diplomacy);
  std::optional<DiplomaticReportAccess> reports;
  if (result.diplomacy->v2)
    reports.emplace(
        DiplomaticHistoryRead{view, *result.organization, *catalog.organization,
                              *catalog.diplomacy, *result.diplomacy},
        *result.diplomacy->v2);
  auto payloads = envoy_task_payload_registry(
      view, *result.tasks, *result.envoy_tasks, *catalog.common, missions,
      missions, reports ? &*reports : nullptr);
  if (!payloads.ok)
    return {false, {}, payloads.error};
  auto valid = validate_envoy_task_cold_payloads(source, payloads.value);
  if (!valid.ok)
    return {false, {}, valid.error};
} else if (source.find("envoy_tasks"))
  fail("UnexpectedEnvoyTasksState", "envoy_tasks");
