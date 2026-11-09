// 地方经济世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (catalog.economic_supply_policies_enabled) {
  auto economic = decode_economic_supply_policy_state(
      field(source, "economic_supply_policies", "state"));
  if (!economic.ok)
    return {false, {}, economic.error};
  auto political = decode_political_supply_adoptions(
      field(source, "political_supply_adoptions", "state"));
  if (!political.ok)
    return {false, {}, political.error};
  result.economic_supply_policies =
      std::make_shared<const EconomicSupplyPolicyState>(
          std::move(economic.value));
  result.political_supply_adoptions =
      std::make_shared<const PoliticalSupplyAdoptionState>(
          std::move(political.value));
} else if (source.find("economic_supply_policies") ||
           source.find("political_supply_adoptions"))
  fail("UnexpectedEconomicSupplyPolicyState", "state");
