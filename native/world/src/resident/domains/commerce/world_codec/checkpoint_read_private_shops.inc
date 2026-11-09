// 商业检查点恢复分派：沿原 else-if 链读取
// private_shops；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "private_shops") {
  auto state = make_state<Seed, PrivateShopState>();
  r = visit_private_shop_checkpoint(reader, *state);
  seed.private_shops = std::move(state);
}