// 军事世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (const auto *population = dynamic->find("population_military")) {
  const auto &capabilities = required_capabilities();
  if (std::find(capabilities.begin(), capabilities.end(),
                "population_military_v1") == capabilities.end())
    fail("PopulationMilitaryCapabilityRequired",
         "dynamic_world/population_military");
  auto decoded = decode_population_military_catalog(*population);
  if (!decoded.ok)
    return {false, {}, decoded.error};
  if (!decoded.value.enabled)
    fail("PopulationMilitaryCapabilityRequired",
         "dynamic_world/population_military");
  result->population_military =
      checkpoint_make_shared<const PopulationMilitaryCatalog>(
          std::move(decoded.value));
}
