// 任务世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
result->governance_tasks_enabled = read_politics_governance_profile(*dynamic);
if (result->governance_tasks_enabled) {
  if (!politics_enabled(*result))
    fail("GovernanceRequiresPolitics", "dynamic_world/politics_governance");
  auto decoded = decode_politics_governance_catalog(
      field(*dynamic, "politics_governance", "dynamic_world"));
  if (!decoded.ok)
    return {false, {}, decoded.error};
  result->politics_governance =
      checkpoint_make_shared<const PoliticsGovernanceCatalog>(
          std::move(decoded.value));
}
