// 地方经济世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
r = section("local_economy", [&](auto &c) {
  auto selected =
      selected_native_economic_obligation_versions(w.catalog->identity);
  if (!selected.ok)
    return Status{false, {}, std::move(selected.error)};
  return visit_local_economy_checkpoint(
      c, w.local_economy, w.population_military_enabled(),
      w.named_membership_enabled(), w.production_management_enabled(),
      w.household_provisioning_enabled(), selected.value.local_economy);
});
if (!r.ok)
  return {false, {}, r.error};
