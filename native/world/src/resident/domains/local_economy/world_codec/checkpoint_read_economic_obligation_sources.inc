// 地方经济检查点恢复分派：沿原 else-if 链读取 economic_obligation_sources,
// governance_programs；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "economic_obligation_sources") {
  auto selected =
      selected_native_economic_obligation_versions(catalog.identity);
  if (!selected.ok || selected.value.sources != 1 ||
      seed.economic_obligation_sources)
    return {false,
            {},
            common_failure("EconomicObligationSelectedRootMismatch").error};
  auto state = make_state<Seed, EconomicObligationSourcesState>();
  r = visit_economic_obligation_sources(reader, *state);
  seed.economic_obligation_sources = std::move(state);
}
else if (section.domain == "governance_programs") {
  auto selected = selected_native_governance_program_versions(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (!selected.value.state || section.codec_version != selected.value.state ||
      seed.governance_programs)
    return {false,
            {},
            common_failure("GovernanceProgramSelectedRootMismatch").error};
  auto state = make_state<Seed, GovernanceProgramState>();
  r = visit_native_governance_programs(reader, *state, selected.value.state);
  seed.governance_programs = std::move(state);
}