// 地方经济世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto obligation_catalog = validate_native_economic_obligation_catalog(catalog);
if (!obligation_catalog.ok)
  return {false, {}, std::move(obligation_catalog.error)};
auto birth_catalog = validate_native_family_birth_catalog(catalog);
if (!birth_catalog.ok)
  return {false, {}, std::move(birth_catalog.error)};
auto population_catalog = validate_native_born_population_catalog(catalog);
if (!population_catalog.ok)
  return {false, {}, std::move(population_catalog.error)};
auto obligation_versions =
    selected_native_economic_obligation_versions(catalog.identity);
if (!obligation_versions.ok)
  return {false, {}, std::move(obligation_versions.error)};
