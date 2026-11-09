// 军事世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (population_enabled(catalog)) {
  auto population = [&] {
    std::optional<ScopedCheckpointJsonDecode> section;
    if (owned_budget)
      section.emplace(*owned_budget, *owned_job, "population_military", current_checkpoint_memory());
    return decode_population_military_state(
        boundary::field(source, "population_military", ""),
        population_version(catalog), siege_enabled(catalog));
  }();
  if (!population.ok)
    return {false, {}, population.error};
  result.population_military = std::make_shared<const PopulationMilitaryState>(
      std::move(population.value));
  const auto canonical = encode_population_military_state(
      *result.population_military, population_version(catalog),
      siege_enabled(catalog));
  const auto &actions =
      boundary::field(canonical, "actions", "population_military");
  for (const auto &row :
       boundary::object(actions, "population_military/actions")) {
    const auto &actual =
        boundary::field(boundary::field(boundary::field(source, "actions", ""),
                                        row.first, "actions"),
                        "params", row.first);
    if (!compare_boundary(actual, row.second).exact())
      fail("PopulationActionPayloadMismatch", row.first);
  }
} else if (source.find("population_military"))
  fail("UnexpectedPopulationMilitaryState", "population_military");
