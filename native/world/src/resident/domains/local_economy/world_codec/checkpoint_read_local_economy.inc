// 地方经济检查点恢复分派：沿原 else-if 链读取
// local_economy；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "local_economy") {
  auto selected =
      selected_native_economic_obligation_versions(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  r = visit_local_economy_checkpoint(
      reader, *economy, population_enabled(catalog), catalog.named_membership,
      management_enabled(catalog), household_enabled(catalog),
      selected.value.local_economy);
}