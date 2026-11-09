// 地方经济检查点恢复分派：沿原 else-if 链读取
// economic_supply_policies；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "economic_supply_policies") {
  auto state = make_state<Seed, EconomicSupplyPolicyState>();
  r = visit_economic_supply_policy_checkpoint(reader, *state);
  seed.economic_supply_policies = std::move(state);
}