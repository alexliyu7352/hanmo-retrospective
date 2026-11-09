// 当前本人合同保留真实日净收益及暂停原因；开设本金、补资本和未售库存不进入收入。
#include "private_shop_read.hpp"
#include "daily_rent/private_shop_daily_rent_financial.hpp"
#include "private_shop_accounting.hpp"
#include <map>
namespace hanmo::resident {
namespace {
template <class T> Outcome<T> fail(const StableId &code) {
  return {false, {}, common_failure(code).error};
}
Status visible_observer(const PrivateShopContext &x, const StableId &actor) {
  if (x.phase || !validate_private_shop_committed_read3(x).ok)
    return common_failure("PrivateShopReadRequiresCommittedClock");
  if (actor.empty() || actor != x.common.state().metadata.player_actor_id ||
      !x.common.actor(actor))
    return common_failure("PrivateShopObserverRequired");
  return validate_private_shops(x);
}
} // namespace
Outcome<bool> private_shop_observer_is_local(const CommonStore &common,
                                             const StableId &actor,
                                             const StableId &place) {
  if (actor.empty() || place.empty() || !common.actor(actor))
    return fail<bool>("PrivateShopObserverRequired");
  auto location = common.resolved_location(actor);
  if (!location.ok)
    return {false, {}, location.error};
  const auto *at = std::get_if<SettlementLocation>(&location.value);
  return {true, at && at->place_id == place, {}};
}
bool private_shop_history_is_firsthand(const PrivateShopState &s,
                                       const StableId &actor,
                                       const StableId &fact) {
  if (actor.empty() || fact.empty())
    return false;
  if (const auto *v = s.shops.find(fact))
    return v->terms.proprietor_actor_id == actor;
  if (const auto *v = s.capital.find(fact))
    return v->actor_id == actor;
  if (const auto *v = s.markups.find(fact))
    return v->actor_id == actor;
  if (const auto *v = s.purchases.find(fact)) {
    const auto *shop = s.shops.find(v->shop_id);
    return shop && shop->terms.proprietor_actor_id == actor;
  }
  if (const auto *v = s.operating_results.find(fact)) {
    const auto *op = s.operations.find(v->operation_id);
    return op && op->shop_id == v->shop_id && op->actor_id == actor &&
           v->actor_id == actor;
  }
  if (const auto *v = s.order_settlements.find(fact)) {
    const auto *order = s.orders.orders.find(v->order_id);
    return order && order->terms.proprietor_actor_id == actor &&
           v->outcome == "delivered";
  }
  if (const auto *v = s.closings.find(fact))
    return v->actor_id == actor;
  if (const auto *v = s.staff.decisions.find(fact))
    return v->actor_id == actor &&
           (v->decision == "left" || v->decision == "dismissed");
  if (const auto *v = s.staff.services.find(fact)) {
    const auto *contract = s.staff.contracts.find(v->contract_id);
    const auto *op = s.operations.find(v->operation_id);
    return contract && op && op->staff_contract_id == contract->id &&
           op->actor_id == actor &&
           (actor == contract->terms.worker_actor_id ||
            actor == contract->terms.employer_actor_id);
  }
  return false;
}
// 只汇总原零售/店单实际消耗，分摊仍委托唯一cost_slice；关店归还没有销售分录。
Outcome<PrivateShopPurchaseCostView> derive_private_shop_purchase_cost_view(
    const PrivateShopState &state, const PrivateShopPurchaseFact &purchase) {
  if (state.purchases.find(purchase.id) != &purchase ||
      purchase.quantity <= 0 || purchase.purchase_cash < 0)
    return fail<PrivateShopPurchaseCostView>("PrivateShopCostSourceMismatch");
  PrivateShopCostBasis basis{
      purchase.id, purchase.quantity, purchase.purchase_cash, 0, 0, 0};
  auto add = [&](const PrivateShopRetailLot &lot,
                 const StableId &shop) -> Status {
    if (lot.purchase_fact_id != purchase.id)
      return common_success();
    if (shop != purchase.shop_id || lot.quantity <= 0 || lot.cost < 0 ||
        __builtin_add_overflow(basis.sold_quantity, lot.quantity,
                               &basis.sold_quantity) ||
        __builtin_add_overflow(basis.recognized_cost, lot.cost,
                               &basis.recognized_cost))
      return common_failure("PrivateShopCostSourceMismatch");
    return common_success();
  };
  for (const auto &day : state.operating_results.rows())
    for (const auto &lot : day.second.consumed_lots) {
      auto valid = add(lot, day.second.shop_id);
      if (!valid.ok)
        return {false, {}, valid.error};
    }
  for (const auto &settlement : state.order_settlements.rows()) {
    const auto *order = state.orders.orders.find(settlement.second.order_id);
    for (const auto &lot : settlement.second.consumed_lots) {
      auto valid = add(lot, order ? order->terms.shop_id : StableId{});
      if (!valid.ok)
        return {false, {}, valid.error};
    }
  }
  if (basis.sold_quantity > purchase.quantity ||
      basis.recognized_cost > purchase.purchase_cash)
    return fail<PrivateShopPurchaseCostView>("PrivateShopCostHistoryMismatch");
  const auto left = purchase.quantity - basis.sold_quantity;
  Integer cost = 0;
  if (left) {
    auto slice = quote_private_shop_cost_slice(basis, left);
    if (!slice.ok)
      return {false, {}, slice.error};
    cost = slice.value.cost;
  } else if (basis.recognized_cost != purchase.purchase_cash)
    return fail<PrivateShopPurchaseCostView>("PrivateShopCostHistoryMismatch");
  return {true, {basis.sold_quantity, basis.recognized_cost, left, cost}, {}};
}
Outcome<std::vector<StableId>>
PrivateShopAccess::private_shop_ids(const StableId &observer, Integer offset,
                                    Integer limit) const {
  auto valid = visible_observer(context_, observer);
  if (!valid.ok)
    return {false, {}, valid.error};
  if (offset < 0 || limit < 1 || limit > 16)
    return fail<std::vector<StableId>>("PrivateShopReadLimit");
  Integer index = 0;
  std::vector<StableId> ids;
  for (const auto &row : context_.state.shops.rows())
    if (row.second.terms.proprietor_actor_id == observer) {
      if (index++ < offset)
        continue;
      if (ids.size() == static_cast<std::size_t>(limit))
        break;
      ids.push_back(row.first);
    }
  if (offset > index)
    return fail<std::vector<StableId>>("PrivateShopReadCursor");
  return {true, std::move(ids), {}};
}
Outcome<std::optional<PrivateShopSource>>
PrivateShopAccess::private_shop_source(const StableId &observer,
                                       const StableId &id) const {
  auto valid = visible_observer(context_, observer);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto *shop = context_.state.shops.find(id);
  if (!shop || shop->terms.proprietor_actor_id != observer)
    return {true, {}, {}};
  PrivateShopSource out;
  out.source_identity = context_.common.state().identity;
  out.observer_actor_id = observer;
  out.shop_id = shop->id;
  out.premises_id = shop->terms.premises_id;
  out.proprietor_actor_id = observer;
  out.place_id = shop->terms.place_id;
  out.wallet_id = shop->wallet_id;
  out.inventory_id = shop->inventory_id;
  out.opening_fact_id = shop->id;
  out.rent_obligation_id = shop->rent_obligation_id;
  out.opened_tick = shop->opened_tick;
  auto local =
      private_shop_observer_is_local(context_.common, observer, out.place_id);
  if (!local.ok)
    return {false, {}, local.error};
  if (!local.value) {
    out.unavailable_code = "PrivateShopLocalObservationRequired";
    return {true, std::move(out), {}};
  }
  out.status = shop->status;
  out.markup_basis_points = shop->markup_basis_points;
  out.accounts_proven = true;
  for (const auto &actual : context_.common.state().facts.rows())
    if (const auto *day = context_.state.operating_results.find(actual.first))
      if (day->shop_id == id) {
        out.last_operating_fact_id = day->id;
        if (day->completed &&
            __builtin_add_overflow(out.operating_days, Integer{1},
                                   &out.operating_days))
          return fail<std::optional<PrivateShopSource>>(
              "PrivateShopDayCountOverflow");
        if (__builtin_add_overflow(out.net_income, day->net_income,
                                   &out.net_income))
          return fail<std::optional<PrivateShopSource>>(
              "PrivateShopNetIncomeOverflow");
        if (!day->completed)
          out.unavailable_code = day->reason;
        else
          out.unavailable_code.clear();
      }
  // 原Action开工即付的租金、实际到岗即付的工钱已经是费用；结日后原day净额接管，同一款不重复扣。
  for (const auto &entry : context_.state.operations.rows()) {
    const auto &operation = entry.second;
    if (operation.shop_id != id || !operation.result_fact_id.empty())
      continue;
    if (!operation.rent_payment_id.empty()) {
      const auto *paid =
          context_.economy.payments.find(operation.rent_payment_id);
      if (!paid || paid->id != operation.rent_payment_id ||
          paid->obligation_id != shop->rent_obligation_id ||
          paid->from_wallet_id != shop->wallet_id ||
          paid->to_wallet_id != shop->terms.lessor_wallet_id ||
          paid->amount <= 0)
        return fail<std::optional<PrivateShopSource>>(
            "PrivateShopRentSourceMismatch");
      if (__builtin_add_overflow(out.unclosed_rent_paid, paid->amount,
                                 &out.unclosed_rent_paid))
        return fail<std::optional<PrivateShopSource>>(
            "PrivateShopNetIncomeOverflow");
    }
    for (const auto &row : context_.state.staff.services.rows())
      if (row.second.operation_id == operation.id) {
        const auto *contract =
            context_.state.staff.contracts.find(operation.staff_contract_id);
        const auto *paid =
            context_.economy.payments.find(row.second.payment_id);
        if (!contract || row.second.contract_id != contract->id || !paid ||
            paid->id != row.second.payment_id ||
            paid->obligation_id != contract->obligation_id ||
            paid->from_wallet_id != shop->wallet_id ||
            paid->to_wallet_id != contract->terms.payee_wallet_id ||
            paid->amount != row.second.amount || paid->amount <= 0)
          return fail<std::optional<PrivateShopSource>>(
              "PrivateShopStaffWageReceiptMismatch");
        if (__builtin_add_overflow(out.unclosed_wage_paid, paid->amount,
                                   &out.unclosed_wage_paid))
          return fail<std::optional<PrivateShopSource>>(
              "PrivateShopNetIncomeOverflow");
      }
  }
  if (__builtin_sub_overflow(out.net_income, out.unclosed_rent_paid,
                             &out.net_income) ||
      __builtin_sub_overflow(out.net_income, out.unclosed_wage_paid,
                             &out.net_income))
    return fail<std::optional<PrivateShopSource>>(
        "PrivateShopNetIncomeOverflow");
  for (const auto &settlement : context_.state.order_settlements.rows()) {
    const auto *order =
        context_.state.orders.orders.find(settlement.second.order_id);
    if (order && order->terms.shop_id == id) {
      if (settlement.second.outcome == "delivered" &&
          __builtin_add_overflow(out.completed_orders, Integer{1},
                                 &out.completed_orders))
        return fail<std::optional<PrivateShopSource>>(
            "PrivateShopOrderCountOverflow");
      Integer net = 0;
      if (__builtin_sub_overflow(settlement.second.income,
                                 settlement.second.cost_of_goods, &net) ||
          __builtin_add_overflow(out.net_income, net, &out.net_income))
        return fail<std::optional<PrivateShopSource>>(
            "PrivateShopNetIncomeOverflow");
    }
  }
  for (const auto &contract : context_.state.staff.contracts.rows())
    if (contract.second.terms.shop_id == id &&
        contract.second.status == "active")
      out.employee_contract_id = contract.first;
  if (context_.daily_rent) {
    auto rent = read_private_shop_rent_ledger(
        context_.common, context_.economy, context_.state,
        *context_.daily_rent_catalog, *context_.daily_rent, id);
    if (!rent.ok)
      return {false, {}, rent.error};
    if (__builtin_sub_overflow(out.net_income, rent.value.paid_cash,
                               &out.net_income))
      return fail<std::optional<PrivateShopSource>>(
          "PrivateShopNetIncomeOverflow");
    if (rent.value.suspended)
      out.unavailable_code = rent.value.pending_arrear_id.empty()
                                 ? "PrivateShopRentResumeRequired"
                                 : "PrivateShopRentFullPaymentRequired";
  }
  out.milestone_reached = out.operating_days >= 10 &&
                          out.completed_orders >= 2 && out.net_income >= 200;
  if (shop->status == "closed")
    out.unavailable_code.clear();
  else if (!context_.market_history)
    out.unavailable_code = "PrivateShopMarketHistoryUnavailable";
  else if (shop->status == "suspended" && out.unavailable_code.empty())
    out.unavailable_code = "PrivateShopDailyFundsRequired";
  return {true, std::move(out), {}};
}
Outcome<PrivateShopPage> query_private_shops(const PrivateShopContext &x,
                                             const StableId &observer,
                                             Integer offset, Integer limit) {
  PrivateShopAccess read(x);
  auto ids = read.private_shop_ids(observer, offset, limit);
  if (!ids.ok)
    return {false, {}, ids.error};
  PrivateShopPage page;
  page.source_identity = x.common.state().identity;
  page.observer_actor_id = observer;
  page.next_offset = offset;
  for (const auto &id : ids.value) {
    auto source = read.private_shop_source(observer, id);
    if (!source.ok)
      return {false, {}, source.error};
    if (!source.value)
      return fail<PrivateShopPage>("PrivateShopSourceChanged");
    PrivateShopPageRow row;
    row.source = std::move(*source.value);
    if (row.source.accounts_proven) {
      const auto *wallet = x.common.wallet(row.source.wallet_id);
      const auto *stock = x.common.inventory(row.source.inventory_id);
      auto spendable = x.common.spendable(row.source.wallet_id),
           weight = x.common.inventory_weight(row.source.inventory_id);
      if (!wallet || !stock || !spendable.ok || !weight.ok)
        return fail<PrivateShopPage>("PrivateShopAssetsMismatch");
      row.cash = wallet->balance;
      row.spendable = spendable.value;
      row.inventory_capacity = stock->capacity;
      row.inventory_weight = weight.value;
      std::map<StableId, Integer> items;
      for (const auto &lot : stock->lots)
        if (__builtin_add_overflow(items[lot.item_id], lot.quantity,
                                   &items[lot.item_id]))
          return fail<PrivateShopPage>("PrivateShopStockOverflow");
      for (const auto &item : items)
        row.stock.push_back({item.first, item.second});
      for (const auto &operation : x.state.operations.rows())
        if (operation.second.shop_id == id &&
            operation.second.result_fact_id.empty()) {
          const auto *action = x.common.action(operation.first);
          if (!action)
            return fail<PrivateShopPage>("PrivateShopActionMismatch");
          row.active_action_id = action->id;
          row.remaining_ticks = action->remaining_ticks;
        }
    }
    page.shops.push_back(std::move(row));
    if (__builtin_add_overflow(page.next_offset, Integer{1}, &page.next_offset))
      return fail<PrivateShopPage>("PrivateShopReadCursor");
  }
  auto more = read.private_shop_ids(observer, page.next_offset, 1);
  if (!more.ok)
    return {false, {}, more.error};
  page.has_more = !more.value.empty();
  return {true, std::move(page), {}};
}
} // namespace hanmo::resident
