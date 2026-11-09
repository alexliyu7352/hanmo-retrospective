// 地方经济世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto facility_catalogs = validate_native_economic_facility_catalogs(catalog);
if (!facility_catalogs.ok)
  return {false, {}, std::move(facility_catalogs.error)};
auto facility_versions =
    selected_native_economic_facility_versions(catalog.identity);
if (!facility_versions.ok)
  return {false, {}, std::move(facility_versions.error)};
if (facility_versions.value.facilities) {
  {
    ScopedCheckpointJsonDecode original(
        CheckpointWorldJsonSection::economic_facility_capacity_sources);
    auto state = decode_economic_facility_sources(
        field(source, "economic_facility_capacity_sources", "state"));
    if (!state.ok)
      return {false, {}, std::move(state.error)};
    result.economic_facility_capacity_sources =
        checkpoint_make_shared<const EconomicFacilitySourcesState>(
            std::move(state.value));
  }
  {
    ScopedCheckpointJsonDecode original(
        CheckpointWorldJsonSection::production_facility_conditions);
    auto state = decode_production_facility_conditions(
        field(source, "production_facility_conditions", "state"));
    if (!state.ok)
      return {false, {}, std::move(state.error)};
    result.production_facility_conditions =
        checkpoint_make_shared<const ProductionFacilityConditionsState>(
            std::move(state.value));
  }
} else if (source.find("economic_facility_capacity_sources") ||
           source.find("production_facility_conditions"))
  fail("UnexpectedEconomicFacilityState", "economic_facility_capacity_sources");
if (facility_versions.value.work_expense) {
  ScopedCheckpointJsonDecode original(
      CheckpointWorldJsonSection::governance_work_expense);
  auto state = decode_governance_work_expenses(
      field(source, "governance_work_expense", "state"));
  if (!state.ok)
    return {false, {}, std::move(state.error)};
  if (state.value.version != facility_versions.value.work_expense)
    fail("GovernanceWorkExpenseSelectedVersionMismatch",
         "governance_work_expense");
  result.governance_work_expense =
      checkpoint_make_shared<const GovernanceWorkExpenseState>(
          std::move(state.value));
} else if (source.find("governance_work_expense"))
  fail("UnexpectedGovernanceWorkExpenseState", "governance_work_expense");
