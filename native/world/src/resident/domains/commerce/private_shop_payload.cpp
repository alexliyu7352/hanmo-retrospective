#include "private_shop_opening_source3.hpp"
// 持久布局严格双pair，事实精确kind/prefix/表key/实体ID；未知标签不能复用旧成功回执。
#include "hanmo/resident/command_contract.hpp"
#include "private_shop_codec.hpp"
#include "private_shop_payload.hpp"
#include "resident/domains/local_economy/economic_payload_guard.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
namespace hanmo::resident {
bool private_shop_command_kind(const StableId &k) {
  return k == "private_shop_open" || k == "private_shop_fund" ||
         k == "private_shop_set_markup" || k == "private_shop_restock" ||
         k == "private_shop_operate" || k == "private_shop_cancel_operation" ||
         k == "private_shop_employ" || k == "private_shop_staff_accept" ||
         k == "private_shop_staff_decide" ||
         k == "private_shop_request_order" || k == "private_shop_close";
}
// 未选私店不限制别域市场历史；仍拒私店/执照残留，已选私店保原配对门。
Status validate_private_shop_pair(
    bool enabled, const std::vector<std::pair<StableId, StableId>> &modules) {
  Integer shops = 0, law = 0, market = 0;
  Integer shop_version = 0, market_version = 0, law_version = 0;
  Integer sources = 0, capabilities = 0;
  for (const auto &m : modules) {
    if (m.first == "growth_market_history") {
      if (!enabled)
        continue;
      if ((m.second != "1" && m.second != "2") || ++market != 1)
        return common_failure("PrivateShopMarketHistoryProfileRequired");
      market_version = m.second == "2" ? 2 : 1;
      continue;
    }
    if (enabled && (m.first == "private_shop_operation_sources" ||
                    m.first == "private_shop_operation_capabilities")) {
      auto &count =
          m.first == "private_shop_operation_sources" ? sources : capabilities;
      if (m.second != "1" || ++count != 1)
        return common_failure("ShopOperationModuleMismatch", m.first);
      continue;
    }
    if (m.first != "private_shops" && m.first != "political_shop_licensing")
      continue;
    if ((m.first == "private_shops" &&
         (m.second != "1" && m.second != "2" && m.second != "3")) ||
        (m.first == "political_shop_licensing" && m.second != "1" &&
         m.second != "2"))
      return common_failure("UnsupportedPrivateShopPairVersion");
    if (++(m.first == "private_shops" ? shops : law) != 1)
      return common_failure("DuplicatePrivateShopPairModule");
    if (m.first == "private_shops")
      shop_version = m.second == "3" ? 3 : m.second == "2" ? 2 : 1;
    else
      law_version = m.second == "2" ? 2 : 1;
  }
  if (!enabled)
    return shops || law ? common_failure("UnexpectedPrivateShopPair")
                        : common_success();
  if ((shop_version == 3 && (law_version != 2 || market_version != 2 ||
                             sources != 1 || capabilities != 1)) ||
      (shop_version != 3 && law_version != 1))
    return common_failure("PrivateShopLicensingPairRequired");
  // History2 原 reader 已接受真 private_shop_day 边界；只接原 Shop2
  // sources/capabilities 完整配对，旧 Shop1/History1 合同不放宽。
  if (market_version == 2 && ((shop_version != 2 && shop_version != 3) ||
                              sources != 1 || capabilities != 1))
    return common_failure("PrivateShopMarketHistoryProfileRequired");
  return shops == 1 && law == 1 && market == 1
             ? common_success()
             : common_failure("PrivateShopLicensingPairRequired");
}
Status validate_private_shop_layout(
    const CommonStore &c,
    const std::vector<std::pair<StableId, StableId>> &modules,
    const PrivateShopCatalog *catalog, const PrivateShopState *state) {
  if (bool(catalog) != bool(state))
    return common_failure("PrivateShopProfileRequired");
  auto pair = validate_private_shop_pair(bool(catalog), modules);
  if (!pair.ok)
    return pair;
  if (state) {
    auto layout = validate_private_shop_opening_time_layout3(*state);
    if (!layout.ok)
      return layout;
    if (state->version != catalog->version ||
        (state->version != 1 && state->version != 2 && state->version != 3))
      return common_failure("UnsupportedPrivateShopVersion");
    for (const auto &m : modules)
      if (m.first == "private_shops" &&
          m.second != std::to_string(state->version))
        return common_failure("PrivateShopSelectedVersionMismatch");
    return common_success();
  }
  for (const auto &h : c.state().facts.rows())
    if (h.second.domain == private_shop_module ||
        h.second.kind.rfind("private_shop_", 0) == 0)
      return common_failure("PrivateShopProfileRequired", h.first);
  for (const auto &a : c.state().actions.rows())
    if (a.second.kind == "private_shop_operate")
      return common_failure("PrivateShopProfileRequired", a.first);
  for (const auto &h : c.state().history)
    if (h.kind.rfind("private_shop_", 0) == 0)
      return common_failure("PrivateShopProfileRequired", h.result_id);
  for (const auto &r : c.state().results.rows())
    if (r.second && r.second->kind.rfind("private_shop_", 0) == 0)
      return common_failure("PrivateShopProfileRequired", r.first);
  return common_success();
}
namespace {
template <class Emit>
Status visit_private_shop_original_payload(const CommonStore &c,
                                           const PrivateShopState &state,
                                           const FactRecord &h, Emit emit) {
  const auto *actual = c.state().facts.find(h.id);
  if (!actual || actual->id != h.id || actual->kind != h.kind ||
      actual->domain != h.domain || h.domain != private_shop_module)
    return common_failure("InvalidPrivateShopFactPayload", h.id);
  Integer count = 0;
  bool valid = false;
  Status result = common_success();
  // Two borrowed passes allow no output until the full original unique-table
  // gate succeeds. No std::function or copied payload is retained between the
  // passes.
  for (bool output : {false, true}) {
    auto match = [&](const auto &table, const StableId &prefix,
                     const StableId &kind) {
      const auto *record = table.find(h.id);
      if (!record)
        return;
      if (!output) {
        ++count;
        auto serial = organization_instance_sequence(h.id, prefix);
        if (record->id == h.id && h.kind == kind && serial &&
            *serial < c.state().next_instance_id)
          valid = true;
      } else if (result.ok)
        result = emit(*record);
    };
    match(state.shops, "private_shop", "private_shop_opened");
    match(state.capital, "private_shop_capital", "private_shop_capital_added");
    match(state.markups, "private_shop_markup", "private_shop_markup_changed");
    match(state.purchases, "private_shop_stock", "private_shop_stock_received");
    match(state.operations, "action", "private_shop_operating_started");
    const auto *day = state.operating_results.find(h.id);
    match(state.operating_results, "private_shop_day",
          day && day->completed ? "private_shop_operating_completed"
                                : "private_shop_operating_interrupted");
    match(state.staff.contracts, "private_shop_staff",
          "private_shop_staff_accepted");
    match(state.staff.decisions, "private_shop_staff_decision",
          "private_shop_staff_ended");
    match(state.staff.services, "private_shop_staff_service",
          "private_shop_staff_served");
    match(state.orders.orders, "private_shop_order",
          "private_shop_order_issued");
    match(state.orders.bindings, "private_shop_order_bound",
          "private_shop_order_bound");
    match(state.order_settlements, "private_shop_order_settlement",
          "private_shop_order_settled");
    match(state.closings, "private_shop_closed", "private_shop_closed");
    if (count != 1 || !valid)
      return common_failure("InvalidPrivateShopFactPayload", h.id);
  }
  return result;
}
template <class Emit> Outcome<BoundaryValue> shop_payload(Emit emit) {
  BoundaryValue out;
  BoundaryProjectionWriter writer(out);
  auto result = writer.object("", emit);
  if (result.ok)
    result = writer.finish();
  if (!result.ok)
    return {false, {}, result.error};
  return {true, std::move(out), {}};
}
} // namespace
Status validate_private_shop_fact_identity(const CommonStore &c,
                                           const PrivateShopState &state,
                                           const FactRecord &h) {
  return visit_private_shop_original_payload(
      c, state, h, [](const auto &) { return common_success(); });
}
Status emit_private_shop_fact_payload_fields(ProjectionWriter &w,
                                             const CommonStore &c,
                                             const PrivateShopState &state,
                                             const FactRecord &h) {
  return visit_private_shop_original_payload(
      c, state, h, [&](const auto &record) {
        auto valid = dynamic_economy_codec::emit_fields(w, record);
        if (!valid.ok)
          return valid;
        if constexpr (std::is_same_v<std::decay_t<decltype(record)>,
                                     PrivateShopRecord>) {
          if (state.version == 3) {
            if (!record.opened_at)
              return w.reject("PrivateShopOpeningMomentRequired", record.id);
            return dynamic_economy_codec::project(w, "opened_at",
                                                  *record.opened_at, false,
                                                  false, false, false, false);
          }
          if (record.opened_at)
            return w.reject("PrivateShopOpeningMomentSelectionMismatch",
                            record.id);
        }
        return w.status();
      });
}
Outcome<BoundaryValue> encode_private_shop_fact_payload(
    const CommonStore &c, const PrivateShopState &state, const FactRecord &h) {
  return shop_payload([&](auto &w) {
    return emit_private_shop_fact_payload_fields(w, c, state, h);
  });
}
Status validate_private_shop_fact_header(const CommonStore &c,
                                         const PrivateShopState &state,
                                         const FactRecord &h,
                                         ProjectionWriter &writer) {
  return writer.validate_detached([&](auto &w) {
    return emit_private_shop_fact_payload_fields(w, c, state, h);
  });
}
Status validate_private_shop_fact_header(const CommonStore &c,
                                         const PrivateShopState &state,
                                         const FactRecord &h) {
  auto value = encode_private_shop_fact_payload(c, state, h);
  return value.ok ? common_success() : Status{false, {}, value.error};
}
Status emit_private_shop_action_payload_fields(ProjectionWriter &w,
                                               const CommonStore &c,
                                               const PrivateShopState &state,
                                               const ActionCoreView &action) {
  const auto *actual = c.action(action.id);
  const auto *o = state.operations.find(action.id);
  const auto *head = c.state().facts.find(action.id);
  if (!actual || actual->id != action.id || actual->kind != action.kind ||
      actual->actor_id != action.actor_id ||
      actual->participants != action.participants ||
      actual->start_tick != action.start_tick ||
      actual->remaining_ticks != action.remaining_ticks ||
      action.kind != "private_shop_operate" || !o || o->id != action.id ||
      o->actor_id != action.actor_id || o->start_tick != action.start_tick ||
      !o->result_fact_id.empty() || !head)
    return common_failure("InvalidPrivateShopActionPayload", action.id);
  auto valid = validate_private_shop_fact_header(c, state, *head, w);
  if (!valid.ok)
    return valid;
  w.field("shop_id", o->shop_id);
  w.field("item_id", o->item_id);
  return w.field("staff_contract_id", o->staff_contract_id);
}
Outcome<BoundaryValue>
encode_private_shop_action_payload(const CommonStore &c,
                                   const PrivateShopState &state,
                                   const ActionCoreView &action) {
  return shop_payload([&](auto &w) {
    return emit_private_shop_action_payload_fields(w, c, state, action);
  });
}
Status validate_private_shop_action_payload(const CommonStore &c,
                                            const PrivateShopState &state,
                                            const ActionCoreView &action,
                                            const BoundaryValue &raw) {
  auto unique = validate_economic_payload_fields(raw);
  if (!unique.ok)
    return unique;
  auto expected = encode_private_shop_action_payload(c, state, action);
  if (!expected.ok)
    return {false, {}, expected.error};
  return compare_boundary(raw, expected.value).exact()
             ? common_success()
             : common_failure("InvalidPrivateShopActionPayload", action.id);
}
} // namespace hanmo::resident
