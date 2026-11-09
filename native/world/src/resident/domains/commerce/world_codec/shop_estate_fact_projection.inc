// 商业世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
Status emit_shop_estate_fact_payloads_fields(
    ProjectionWriter &out, const CommonStore &common,
    const PreparedCatalog &catalog,
    const PrivateShopLiquidationState *liquidations,
    const TaskShopOrderState *orders) {
  const bool enabled = bool(catalog.private_shop_liquidations);
  auto pair = validate_private_shop_liquidation_pair(
      enabled, catalog.identity.module_versions);
  if (!pair.ok)
    return pair;
  if (bool(liquidations) != enabled)
    fail("PrivateShopLiquidationProfileMismatch", "private_shop_liquidations");
  auto own = validate_private_shop_liquidation_layout(common, liquidations);
  if (!own.ok)
    return own;
  auto layout = validate_task_shop_order_estate_layout(
      common, enabled, catalog.identity.module_versions, orders);
  if (!layout.ok)
    return layout;
  for (const auto &row : common.state().facts.rows()) {
    const auto &h = row.second;
    const bool shop = shop_estate_fact_kind(h.kind) ||
                      h.domain == private_shop_liquidation_module;
    if (shop) {
      if (!liquidations || !shop_estate_fact_kind(h.kind) ||
          h.domain != private_shop_liquidation_module || h.id != row.first)
        fail("PrivateShopLiquidationFactMismatch", row.first);
    } else if (h.kind == task_shop_order_estate_cancelled_kind ||
               h.domain == task_shop_order_estate_module) {
      if (!orders || !orders->estate_cancellations ||
          h.kind != task_shop_order_estate_cancelled_kind ||
          h.domain != task_shop_order_estate_module || h.id != row.first)
        fail("TaskShopOrderEstateFactMismatch", row.first);
    } else
      continue;
    auto status =
        out.object(row.first, h.field_order, [&](auto &writer) -> Status {
          if (shop) {
            auto encoded = emit_private_shop_liquidation_fact_payload_fields(
                writer, common, *liquidations, h);
            if (!encoded.ok)
              return encoded;
            writer.field("kind", h.kind);
            return writer.field("domain", h.domain);
          }
          // 此原Task
          // writer已带末尾kind/domain；原set是相等值覆盖，保原字段位置。
          return emit_task_shop_order_estate_fact_fields(
              writer, *orders->estate_cancellations, row.first);
        });
    if (!status.ok)
      return status;
  }
  return out.status();
}
Outcome<BoundaryValue>
shop_estate_fact_payloads(const CommonStore &common,
                          const PreparedCatalog &catalog,
                          const PrivateShopLiquidationState *liquidations,
                          const TaskShopOrderState *orders) {
  return projection_value([&](auto &out) {
    return emit_shop_estate_fact_payloads_fields(out, common, catalog,
                                                 liquidations, orders);
  });
}
