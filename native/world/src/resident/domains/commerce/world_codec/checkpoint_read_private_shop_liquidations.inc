// 商业检查点恢复分派：沿原 else-if 链读取
// private_shop_liquidations；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "private_shop_liquidations") {
  auto state = make_state<Seed, PrivateShopLiquidationState>();
  r = visit_private_shop_liquidations_checkpoint(reader, *state);
  seed.private_shop_liquidations = std::move(state);
}