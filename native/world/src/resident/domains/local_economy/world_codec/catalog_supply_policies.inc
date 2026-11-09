// 地方经济世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (const auto *config = dynamic->find("economic_supply_policies")) {
  const auto &caps = required_capabilities();
  if (std::find(caps.begin(), caps.end(), "economic_supply_policies_v1") ==
      caps.end())
    unsupported("EconomicSupplyPolicyCapabilityRequired",
                "economic_supply_policies_v1");
  auto parsed = decode_economic_supply_policy_catalog(*config);
  if (!parsed.ok)
    return {false, {}, parsed.error};
  result->economic_supply_policies_enabled = true;
  result->economic_supply_policies =
      checkpoint_make_shared<const EconomicSupplyPolicyCatalog>(
          std::move(parsed.value));
}
