// 商业世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (catalog.private_shop_daily_rent) {
  auto parsed = decode_private_shop_daily_rent_state(
      field(source, private_shop_daily_rent_module, "state"));
  if (!parsed.ok)
    return {false, {}, parsed.error};
  result.private_shop_daily_rent =
      std::make_shared<const PrivateShopDailyRentState>(
          std::move(parsed.value));
} else if (source.find(private_shop_daily_rent_module))
  fail("UnexpectedPrivateShopDailyRentState", private_shop_daily_rent_module);
{
  CommonStore view(result.common, catalog.common);
  auto checked = validate_private_shop_daily_rent_layout(
      view, result.private_shop_daily_rent.get());
  if (!checked.ok)
    return {false, {}, checked.error};
}
if (catalog.private_shops) {
  ScopedCheckpointJsonDecode original(
      CheckpointWorldJsonSection::private_shops);
  auto parsed =
      decode_private_shop_state(field(source, "private_shops", "state"));
  if (!parsed.ok)
    return {false, {}, parsed.error};
  result.private_shops =
      std::make_shared<const PrivateShopState>(std::move(parsed.value));
} else if (source.find("private_shops"))
  fail("UnexpectedShopMarketDomain", "private_shops");
