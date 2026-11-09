// 任务世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (catalog.envoy_tasks_enabled) {
  auto decoded = decode_diplomacy_state(field(source, "diplomacy", "state"));
  if (!decoded.ok)
    return {false, {}, decoded.error};
  if (decoded.value.version != catalog.diplomacy->version)
    fail("DiplomacyVersionMismatch", "diplomacy");
  result.diplomacy =
      std::make_shared<const DiplomacyState>(std::move(decoded.value));
} else if (source.find("diplomacy"))
  fail("UnexpectedDiplomacyState", "diplomacy");
if (catalog.governance_tasks_enabled) {
  auto decoded = decode_politics_governance_state(
      field(source, "politics_governance", "state"));
  if (!decoded.ok)
    return {false, {}, decoded.error};
  result.politics_governance =
      std::make_shared<const PoliticsGovernanceState>(std::move(decoded.value));
} else if (source.find("politics_governance"))
