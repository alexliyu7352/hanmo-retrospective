// 军事世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。 协商整编新增独立域及回执版本；旧配置仍选原目录，不自动迁移旧格式。
template <class DomainList> DomainList siege_domains(DomainList domains) {
  for (auto &d : domains) {
    if (d.name == "common")
      d.codec_version = 6;
    else if (d.name == "population_military")
      d.codec_version = 4;
  }
  domains.push_back({"siege_integration", 1});
  domains.push_back({"military_integration", 1});
  return domains;
}
const auto siege_checkpoint_domains = siege_domains(dynamic_checkpoint_domains);
const auto siege_population_checkpoint_domains =
    siege_domains(population_checkpoint_domains);
const auto siege_regional_checkpoint_domains =
    siege_domains(regional_checkpoint_domains);
const auto siege_regional_population_checkpoint_domains =
    siege_domains(regional_population_checkpoint_domains);
bool siege_enabled(const PreparedCatalog &catalog) {
  return catalog.military && catalog.military->siege_integration.enabled;
}
bool population_enabled(const PreparedCatalog &catalog) {
  return catalog.population_military && catalog.population_military->enabled;
}
Integer population_version(const PreparedCatalog &catalog) {
  if (!population_enabled(catalog))
    boundary::fail("PopulationMilitaryCapabilityRequired",
                   "population_military");
  auto selected = selected_population_military_version(
      *catalog.population_military, siege_enabled(catalog));
  if (!selected.ok)
    boundary::fail(selected.error.original.code, selected.error.original.path);
  if (selected.value == 4 &&
      (!catalog.exact_time || !exact_complete_selected(*catalog.exact_time)))
    boundary::fail("PopulationMilitaryExactProfileRequired",
                   "population_military");
  return selected.value;
}
