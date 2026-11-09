// 地方经济世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
// 目录尚未安装，按本情景显式section传解码开关；旧缺省仍关闭，字段与版本继续严格验证。
const auto &economy_input = field(*dynamic, "local_economy", "dynamic_world");
const bool economy_management =
               economy_input.find("production_management") != nullptr,
           economy_household =
               economy_input.find("household_provisioning") != nullptr;
auto economy_version = read_native_economic_obligation_selection(
    scenario, economy_household             ? 5
              : economy_management          ? 4
              : result->named_membership    ? 3
              : population_enabled(*result) ? 2
                                            : 1);
if (!economy_version.ok)
  return {false, {}, std::move(economy_version.error)};
auto population_version =
    read_native_born_population_selection(scenario, economy_version.value);
if (!population_version.ok)
  return {false, {}, std::move(population_version.error)};
auto economy = [&]() {
  ScopedCheckpointJsonDecode original(CheckpointCatalogJsonKind::local_economy);
  return decode_local_economy_catalog(
      economy_input, population_enabled(*result), result->named_membership,
      economy_management, economy_household, population_version.value);
}();
if (!economy.ok)
  return {false, {}, economy.error};
result->local_economy =
    checkpoint_make_shared<const LocalEconomyCatalog>(std::move(economy.value));
if (const auto *config = dynamic->find("production_disruptions")) {
  const auto &caps = required_capabilities();
  if (std::find(caps.begin(), caps.end(), "production_disruptions_v1") ==
      caps.end())
    unsupported("ProductionDisruptionCapabilityRequired",
                "dynamic_world/production_disruptions");
  auto parsed = decode_production_disruption_catalog(*config);
  if (!parsed.ok)
    return {false, {}, parsed.error};
  auto valid = validate_production_disruption_catalog(*result->local_economy,
                                                      parsed.value);
  if (!valid.ok)
    return {false, {}, valid.error};
  result->production_disruptions =
      checkpoint_make_shared<const ProductionDisruptionCatalog>(
          std::move(parsed.value));
}
if (const auto *config = dynamic->find("household_provision_management")) {
  if (!household_control_enabled(*result) || !household_enabled(*result))
    fail("HouseholdProvisionManagementDependencies",
         "dynamic_world/household_provision_management");
  auto decoded = decode_household_provision_management_catalog(*config);
  if (!decoded.ok)
    return {false, {}, decoded.error};
  result->household_provision_management =
      checkpoint_make_shared<const HouseholdProvisionManagementCatalog>(
          std::move(decoded.value));
}
