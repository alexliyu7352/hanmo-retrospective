// 商业检查点恢复分派：沿原 else-if 链读取 private_shop_daily_rent_module,
// private_shop_command_results_module；不改变段顺序、所有权或拒绝条件。
else if (section.domain == private_shop_daily_rent_module) {
  auto state = make_state<Seed, PrivateShopDailyRentState>();
  r = visit_private_shop_daily_rent_checkpoint(reader, *state);
  seed.private_shop_daily_rent = std::move(state);
}
else if (section.domain == private_shop_command_results_module) {
  auto state = make_state<Seed, PrivateShopCommandResultsState>();
  r = visit_private_shop_command_results_checkpoint(reader, *state);
  seed.private_shop_command_results = std::move(state);
}