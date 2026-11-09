// 地方经济世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto governance_catalog = validate_native_governance_program_catalog(catalog);
if (!governance_catalog.ok)
  return {false, {}, std::move(governance_catalog.error)};
auto governance_versions =
    selected_native_governance_program_versions(catalog.identity);
if (!governance_versions.ok)
  return {false, {}, std::move(governance_versions.error)};
const BoundaryValue *governance_source = nullptr;
for (const auto &row : object(source, "state"))
  if (row.first == "governance_programs") {
    if (governance_source)
      fail("DuplicateGovernanceProgramsSection", "governance_programs");
    governance_source = &row.second;
  }
if (governance_versions.value.state) {
  if (!governance_source)
    fail("MissingGovernanceProgramsSection", "governance_programs");
  ScopedCheckpointJsonDecode original(
      CheckpointWorldJsonSection::governance_programs);
  original.default_values<GovernanceProgramState>();
  auto state = decode_native_governance_programs(
      *governance_source, governance_versions.value.state);
  if (!state.ok)
    return {false, {}, std::move(state.error)};
  result.governance_programs =
      checkpoint_make_shared<const GovernanceProgramState>(
          std::move(state.value));
} else if (governance_source)
  fail("UnexpectedGovernanceProgramsSection", "governance_programs");
