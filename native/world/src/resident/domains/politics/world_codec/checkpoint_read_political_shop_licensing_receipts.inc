// 政治检查点恢复分派：沿原 else-if 链读取
// political_shop_licensing_receipts；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "political_shop_licensing_receipts") {
  auto state = make_state<Seed, PoliticalShopLicensingReceiptState>();
  r = visit_political_shop_licensing_receipts_checkpoint(reader, *state);
  seed.political_shop_licensing_receipts = std::move(state);
}