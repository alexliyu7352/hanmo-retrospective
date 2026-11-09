// 地方经济世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto obligation_versions =
    selected_native_economic_obligation_versions(w.catalog->identity);
if (!obligation_versions.ok)
  return {false, {}, std::move(obligation_versions.error)};
WORLD_FIELDS(P::section, "local_economy",
             emit_local_economy_state_fields(
                 out, w.local_economy, w.population_military_enabled(),
                 w.named_membership_enabled(),
                 w.production_management_enabled(),
                 w.household_provisioning_enabled(),
                 obligation_versions.value.local_economy));
if (w.economic_obligation_sources) {
  WORLD_FIELDS(P::section, "economic_obligation_sources",
               emit_economic_obligation_sources_fields(
                   out, *w.economic_obligation_sources));
}
