// 家庭检查点恢复分派：沿原 else-if 链读取
// family_collateral_legacy；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "family_collateral_legacy") {
  auto state = make_state<Seed, CollateralControlState>();
  r = visit_collateral_control_checkpoint(reader, *state);
  seed.family_collateral_legacy = std::move(state);
}