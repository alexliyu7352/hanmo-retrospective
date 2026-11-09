// 军事世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto m = decode_military_catalog(data, get_string(state, "scenario_id", ""));
if (!m.ok)
  return {false, {}, m.error};
if (const auto *dynamic = scenario.find("dynamic_world"))
  if (const auto *integration = dynamic->find("siege_integration")) {
    const auto &caps = required_capabilities();
    if (std::find(caps.begin(), caps.end(), "siege_integration_v1") ==
        caps.end())
      fail("SiegeIntegrationCapabilityRequired",
           "dynamic_world/siege_integration");
    auto decoded = decode_siege_integration_catalog(*integration);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    if (!decoded.value.enabled)
      fail("SiegeIntegrationCapabilityRequired",
           "dynamic_world/siege_integration");
    m.value.siege_integration = std::move(decoded.value);
  }
result->military =
    checkpoint_make_shared<const MilitaryCatalog>(std::move(m.value));
auto naval = decode_naval_combat_profile(data, scenario,
                                         result->military->battle_catalog);
if (!naval.ok)
  return {false, {}, naval.error};
if (naval.value) {
  if (naval.value->catalog.repair_charge_policy) {
    auto charges = validate_naval_repair_charge_policy(
        *naval.value->catalog.repair_charge_policy, naval.value->catalog,
        *result->military, result->common.get());
    if (!charges.ok)
      return {false, {}, charges.error};
  }
  if (naval.value->catalog.repair_policy) {
    auto repair = validate_naval_repair_policy(naval.value->catalog,
                                               result->common.get());
    if (!repair.ok)
      return {false, {}, repair.error};
  }
  auto plans =
      validate_naval_fleet_plan_sources(naval.value->catalog, *result->military,
                                        *result->common, *result->travel);
  if (!plans.ok)
    return {false, {}, plans.error};
  result->naval = checkpoint_make_shared<const NavalCatalog>(
      std::move(naval.value->catalog));
}
