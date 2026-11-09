// 地方经济检查点恢复分派：沿原 else-if 链读取
// economic_facility_capacity_sources, production_facility_conditions,
// governance_work_expense；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "economic_facility_capacity_sources") {
  auto selected = selected_native_economic_facility_versions(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (!selected.value.facilities ||
      section.codec_version != selected.value.facilities ||
      seed.economic_facility_capacity_sources)
    return {
        false,
        {},
        common_failure("EconomicFacilityCheckpointSelectionMismatch").error};
  auto state = make_state<Seed, EconomicFacilitySourcesState>();
  r = visit_economic_facility_sources(reader, *state);
  if (r.ok && state->schema_version != selected.value.facilities)
    r = common_failure("EconomicFacilitySelectedStateVersionMismatch");
  seed.economic_facility_capacity_sources = std::move(state);
}
else if (section.domain == "production_facility_conditions") {
  auto selected = selected_native_economic_facility_versions(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (!selected.value.conditions ||
      section.codec_version != selected.value.conditions ||
      seed.production_facility_conditions)
    return {
        false,
        {},
        common_failure("EconomicFacilityCheckpointSelectionMismatch").error};
  auto state = make_state<Seed, ProductionFacilityConditionsState>();
  r = visit_production_facility_conditions(reader, *state);
  seed.production_facility_conditions = std::move(state);
}
else if (section.domain == "governance_work_expense") {
  auto selected = selected_native_economic_facility_versions(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (!selected.value.work_expense ||
      section.codec_version != selected.value.work_expense ||
      seed.governance_work_expense)
    return {
        false,
        {},
        common_failure("EconomicFacilityCheckpointSelectionMismatch").error};
  auto state = make_state<Seed, GovernanceWorkExpenseState>();
  r = visit_governance_work_expenses(reader, *state);
  if (r.ok && state->version != selected.value.work_expense)
    r = common_failure("GovernanceWorkExpenseSelectedVersionMismatch");
  seed.governance_work_expense = std::move(state);
}