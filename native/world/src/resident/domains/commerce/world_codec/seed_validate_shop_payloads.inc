// 商业世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
// 三新域所有Common头/活动params均按本候选永久原件复取；不忽略额外JSON。
{
  CommonStore view(result.common, catalog.common);
  const auto *orders = result.tasks && result.tasks->shop_orders
                           ? &*result.tasks->shop_orders
                           : nullptr;
  auto facts = shop_market_fact_payloads(
      view, catalog, result.growth_market_history.get(),
      result.private_shops.get(), orders,
      result.private_shop_operation_sources.get(),
      result.private_shop_operation_capabilities.get());
  if (!facts.ok)
    return {false, {}, facts.error};
  auto estate_facts = shop_estate_fact_payloads(
      view, catalog, result.private_shop_liquidations.get(), orders);
  if (!estate_facts.ok)
    return {false, {}, estate_facts.error};
  merge_unique(facts.value, estate_facts.value);
  for (const auto &row : object(facts.value, "shop_market/facts")) {
    const auto &actual =
        field(field(source, "facts", "state"), row.first, "facts");
    if (!compare_boundary(actual, row.second).exact())
      fail("ShopMarketFactPayloadMismatch", row.first);
  }
  auto actions =
      shop_action_payloads(view, catalog, result.private_shops.get(), orders);
  if (!actions.ok)
    return {false, {}, actions.error};
  for (const auto &row : object(actions.value, "shop/actions")) {
    const auto &actual =
        field(field(field(source, "actions", "state"), row.first, "actions"),
              "params", row.first);
    if (!compare_boundary(actual, row.second).exact())
      fail("ShopActionPayloadMismatch", row.first);
  }
}
