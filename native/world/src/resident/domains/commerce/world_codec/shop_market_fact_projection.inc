// 商业世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
Status emit_shop_market_fact_payloads_fields(
    ProjectionWriter &out, const CommonStore &common,
    const PreparedCatalog &catalog, const GrowthMarketHistoryState *market,
    const PrivateShopState *shops, const TaskShopOrderState *orders,
    const PrivateShopOperationSourcesState *exact_sources,
    const PrivateShopOperationCapabilitiesState *capabilities) {
  auto selected =
      selected_native_private_shop_operation_versions(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (bool(exact_sources) != selected.value.exact_operations() ||
      bool(capabilities) != selected.value.exact_operations() ||
      (exact_sources && (!shops || shops->version != 2)))
    fail("ShopOperationSelectedRootMismatch", "private_shop_operation_sources");
  std::size_t market_count = 0, shop_count = 0, order_count = 0,
              accepted_count = 0, labor_count = 0, terminal_count = 0;
  // 旧路径先encode完整工单再抽事实；同writer独立分支保这全部值门及首错，无树sink不存整段。
  if (orders) {
    auto checked = out.validate_detached([&](auto &writer) {
      return emit_task_shop_orders_fields(writer, *orders);
    });
    if (!checked.ok)
      return checked;
  }
  for (const auto &row : common.state().facts.rows()) {
    const auto &h = row.second;
    if (shop_estate_fact_kind(h.kind) ||
        h.domain == private_shop_liquidation_module ||
        daily_rent_fact_kind(h.kind) ||
        h.domain == private_shop_daily_rent_module)
      continue;
    const bool market_kind = h.kind == "growth_market_movement";
    const bool shop_kind = h.kind.rfind("private_shop_", 0) == 0;
    const bool order_kind = h.kind == task_shop_order_accepted_kind ||
                            h.kind == task_shop_order_settled_kind ||
                            h.kind == task_shop_order_interrupted_kind;
    enum class Payload { market, shop, binding, settlement, interruption };
    Payload payload = Payload::market;
    Integer shop_schema = 1;
    const TaskShopOrderBinding *binding = nullptr;
    const TaskShopOrderSettlement *settlement = nullptr;
    const TaskShopOrderInterruption *interruption = nullptr;
    if (market_kind || h.domain == "growth_market_history") {
      const auto *record = market ? market->movements.find(row.first) : nullptr;
      if (!catalog.growth_market_history || !record || h.id != row.first ||
          !market_kind || h.domain != "growth_market_history" ||
          record->kind != h.kind)
        fail("GrowthMarketMovementSourceInvalid", row.first);
      ++market_count;
    } else if (shop_kind || h.domain == "private_shops") {
      if (!catalog.private_shops || !shops)
        fail("PrivateShopProfileRequired", row.first);
      payload = Payload::shop;
      ++shop_count;
      if (exact_sources) {
        auto original = read_private_shop_bootstrap_payload_schema(
            common, *shops, *exact_sources, *capabilities, h);
        if (!original.ok)
          return {false, {}, std::move(original.error)};
        shop_schema = original.value;
        if (h.kind == "private_shop_labor_applied")
          ++labor_count;
        else if (shop_schema == 2 && h.kind == "private_shop_operating_started")
          ++accepted_count;
        else if (shop_schema == 2)
          ++terminal_count;
      }
    } else if (order_kind || h.domain == task_shop_order_module) {
      if (!catalog.task_shop_orders_enabled || !orders || !order_kind ||
          h.id != row.first || h.domain != task_shop_order_module)
        fail("TaskShopOrderFactMismatch", row.first);
      auto select = [&](const auto &table, auto &target) {
        for (const auto &entry : table.rows())
          if (entry.first == row.first) {
            if (target)
              fail("TaskShopOrderFactMismatch", row.first);
            target = &entry.second;
          }
        if (!target || target->fact_id != row.first)
          fail("TaskShopOrderFactMismatch", row.first);
      };
      if (h.kind == task_shop_order_accepted_kind) {
        payload = Payload::binding;
        select(orders->bindings, binding);
      } else if (h.kind == task_shop_order_settled_kind) {
        payload = Payload::settlement;
        select(orders->settlements, settlement);
      } else {
        payload = Payload::interruption;
        select(orders->interruptions, interruption);
      }
      ++order_count;
    } else
      continue;
    auto status =
        out.object(row.first, h.field_order, [&](auto &writer) -> Status {
          Status result{true, {}, {}};
          switch (payload) {
          case Payload::market:
            break;
          case Payload::shop:
            result = exact_sources && (shop_schema == 2 ||
                                       h.kind == "private_shop_labor_applied")
                         ? emit_private_shop_operation_source_payload_fields(
                               writer, common, *shops, *exact_sources, h,
                               shop_schema)
                         : emit_private_shop_fact_payload_fields(writer, common,
                                                                 *shops, h);
            break;
          case Payload::binding:
            result = emit_task_shop_order_binding_fields(writer, *binding);
            break;
          case Payload::settlement:
            result =
                emit_task_shop_order_settlement_fields(writer, *settlement);
            break;
          case Payload::interruption:
            result =
                emit_task_shop_order_interruption_fields(writer, *interruption);
            break;
          }
          if (!result.ok)
            return result;
          writer.field("kind", h.kind);
          return writer.field("domain", h.domain);
        });
    if (!status.ok)
      return status;
  }
  if (market_count != (market ? market->movements.size() : 0))
    fail("GrowthMarketMovementSourceInvalid", "growth_market_history");
  if (shops &&
      shop_count - labor_count !=
          shops->shops.size() + shops->capital.size() + shops->markups.size() +
              shops->purchases.size() + shops->operations.size() +
              shops->operating_results.size() + shops->staff.contracts.size() +
              shops->staff.decisions.size() + shops->staff.services.size() +
              shops->orders.orders.size() + shops->orders.bindings.size() +
              shops->order_settlements.size() + shops->closings.size())
    fail("PrivateShopFactCoverageMismatch", "private_shops");
  // Acceptance/terminal appendices share their real original headers. Only
  // labor adds a Common fact; no annex is counted as a fabricated second one.
  if (exact_sources &&
      (accepted_count != exact_sources->accepted_operations.size() ||
       labor_count != exact_sources->labor_applied_sources.size() ||
       terminal_count != exact_sources->terminal_sources.size() ||
       capabilities->operation_profiles.size() != shops->operations.size()))
    fail("PrivateShopOriginalPayloadCoverageMismatch",
         "private_shop_operation_sources");
  if (orders && order_count != orders->bindings.size() +
                                   orders->settlements.size() +
                                   orders->interruptions.size())
    fail("TaskShopOrderFactMismatch", "task_shop_orders");
  return out.status();
}
Outcome<BoundaryValue> shop_market_fact_payloads(
    const CommonStore &common, const PreparedCatalog &catalog,
    const GrowthMarketHistoryState *market, const PrivateShopState *shops,
    const TaskShopOrderState *orders,
    const PrivateShopOperationSourcesState *exact_sources,
    const PrivateShopOperationCapabilitiesState *capabilities) {
  return projection_value([&](auto &out) {
    return emit_shop_market_fact_payloads_fields(out, common, catalog, market,
                                                 shops, orders, exact_sources,
                                                 capabilities);
  });
}
// 只为仍真实存在的Common行动投影原业务params；结束后保留的工单不能复活Action。
Status emit_shop_action_payloads_fields(ProjectionWriter &out,
                                        const CommonStore &common,
                                        const PreparedCatalog &catalog,
                                        const PrivateShopState *shops,
                                        const TaskShopOrderState *orders) {
  for (const auto &row : common.state().actions.rows()) {
    if (row.second.kind == "private_shop_operate") {
      if (!catalog.private_shops || !shops)
        fail("PrivateShopProfileRequired", row.first);
      auto status = out.object(row.first, [&](auto &writer) {
        return emit_private_shop_action_payload_fields(writer, common, *shops,
                                                       row.second);
      });
      if (!status.ok)
        return status;
    } else if (row.second.kind == "task_shop_order_deliver") {
      const auto *original = orders ? orders->actions.find(row.first) : nullptr;
      if (!catalog.task_shop_orders_enabled || !original ||
          original->action_id != row.first ||
          original->started_tick != row.second.start_tick)
        fail("TaskShopOrderActionMismatch", row.first);
      auto status = out.object(row.first, [&](auto &writer) {
        return emit_task_shop_order_action_fields(writer, *original);
      });
      if (!status.ok)
        return status;
    }
  }
  return out.status();
}
Outcome<BoundaryValue> shop_action_payloads(const CommonStore &common,
                                            const PreparedCatalog &catalog,
                                            const PrivateShopState *shops,
                                            const TaskShopOrderState *orders) {
  return projection_value([&](auto &out) {
    return emit_shop_action_payloads_fields(out, common, catalog, shops,
                                            orders);
  });
}
