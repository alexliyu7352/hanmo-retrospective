// 只允许无原责任的私铺退出，归还原本人容器；关闭空壳保留，历史不由今日空集合倒填。
#include "private_shop_closing_assets.hpp"
#include "resident/domains/local_economy/local_economy_internal.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include "resident/domains/tasks/shop_orders/task_shop_asset_responsibilities.hpp"
#include "resident/storage/money_transfer_recipe.hpp"
#include <map>
#include <set>
namespace hanmo::resident {
namespace {
template <class T> Outcome<T> fail(const StableId &code) {
  return {false, {}, common_failure(code).error};
}
bool identity(const WorldIdentity &a, const WorldIdentity &b) {
  return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch &&
         a.revision == b.revision && a.tick == b.tick;
}
bool before(const CommonStore &c, const StableId &a, const StableId &b) {
  const auto &order = c.state().facts.order();
  auto x = std::find(order.begin(), order.end(), a),
       y = std::find(order.begin(), order.end(), b);
  return x != order.end() && (b.empty() || y != order.end()) && x < y;
}
bool same_lot(const LotView &a, const LotView &b) {
  return a.id == b.id && a.item_id == b.item_id && a.quantity == b.quantity &&
         owner_equal(a.owner, b.owner) && a.reserved_by == b.reserved_by &&
         a.extra == b.extra && a.field_order == b.field_order;
}
bool same_quote(const PrivateShopClosingQuote &a,
                const PrivateShopClosingQuote &b) {
  if (!identity(a.source_identity, b.source_identity) ||
      a.actor_id != b.actor_id || a.shop_id != b.shop_id ||
      a.to_wallet_id != b.to_wallet_id ||
      a.to_inventory_id != b.to_inventory_id ||
      a.returned_cash != b.returned_cash ||
      a.stock_lots.size() != b.stock_lots.size() ||
      a.versions.size() != b.versions.size())
    return false;
  for (std::size_t i = 0; i < a.stock_lots.size(); ++i)
    if (!same_lot(a.stock_lots[i], b.stock_lots[i]))
      return false;
  for (std::size_t i = 0; i < a.versions.size(); ++i)
    if (a.versions[i].resource.kind != b.versions[i].resource.kind ||
        a.versions[i].resource.id != b.versions[i].resource.id ||
        a.versions[i].version != b.versions[i].version)
      return false;
  return true;
}
} // namespace
Status validate_private_shop_closing_responsibilities(
    const PrivateShopContext &x, const PrivateShopRecord &shop, Integer tick,
    const StableId &boundary) {
  if (tick < shop.opened_tick || tick > x.common.tick())
    return common_failure("PrivateShopClosingBoundaryMismatch");
  if (!boundary.empty()) {
    const auto *h = x.common.state().facts.find(boundary);
    const auto n =
        organization_instance_sequence(boundary, "private_shop_closed");
    if (!h || h->id != boundary || h->domain != private_shop_module ||
        h->kind != "private_shop_closed" || !n ||
        *n >= x.common.state().next_instance_id ||
        !before(x.common, shop.id, boundary))
      return common_failure("PrivateShopClosingBoundaryMismatch");
  }
  std::set<StableId> obligations{shop.rent_obligation_id};
  for (const auto &row : x.state.staff.contracts.rows())
    if (row.second.terms.shop_id == shop.id &&
        before(x.common, row.first, boundary)) {
      const auto &c = row.second;
      const auto *ended = x.state.staff.decisions.find(c.terminal_fact_id);
      if (!ended || ended->tick > tick ||
          !before(x.common, ended->id, boundary))
        return common_failure("PrivateShopStaffStillEngaged", row.first);
      obligations.insert(c.obligation_id);
    }
  for (const auto &row : x.state.operations.rows())
    if (row.second.shop_id == shop.id &&
        before(x.common, row.first, boundary)) {
      const auto *done =
          x.state.operating_results.find(row.second.result_fact_id);
      if (!done || done->tick > tick || !before(x.common, done->id, boundary))
        return common_failure("PrivateShopOperationOutstanding", row.first);
    }
  for (const auto &row : x.state.orders.orders.rows())
    if (row.second.terms.shop_id == shop.id &&
        before(x.common, row.first, boundary)) {
      const auto &order = row.second;
      if (order.status == "pending_order") {
        if (order.valid_through_tick >= tick)
          return common_failure("PrivateShopOrderOfferOutstanding", row.first);
      } else {
        const PrivateShopOrderSettlement *done = nullptr;
        for (const auto &settled : x.state.order_settlements.rows())
          if (settled.second.order_id == row.first)
            done = &settled.second;
        if (!done || done->tick > tick || !before(x.common, done->id, boundary))
          return common_failure("PrivateShopOrderOutstanding", row.first);
      }
    }
  // 私铺生成容器不借给另一产业；没有完整起停史的额外绑定在今日及历史均明确不可证明。
  for (const auto &row : x.economy.production_units.rows()) {
    const auto &u = row.second;
    if (u.output_inventory_id == shop.inventory_id ||
        std::find(u.input_inventory_ids.begin(), u.input_inventory_ids.end(),
                  shop.inventory_id) != u.input_inventory_ids.end())
      return common_failure("PrivateShopExternalEconomicHistoryUnavailable",
                            row.first);
  }
  for (const auto &row : x.economy.provision_bindings.rows())
    if (row.second.payer_inventory == shop.inventory_id)
      return common_failure("PrivateShopExternalEconomicHistoryUnavailable",
                            row.first);
  for (const auto &row : x.economy_catalog.supply_sources.rows())
    if (row.second.inventory_id == shop.inventory_id)
      return common_failure("PrivateShopExternalEconomicHistoryUnavailable",
                            row.first);
  for (const auto &row : x.economy.obligations.rows()) {
    const auto &o = row.second;
    if (o.source_wallet_or_inventory == shop.wallet_id ||
        o.destination == shop.wallet_id ||
        o.source_wallet_or_inventory == shop.inventory_id ||
        o.destination == shop.inventory_id) {
      if (!obligations.count(row.first))
        return common_failure("PrivateShopExternalEconomicHistoryUnavailable",
                              row.first);
      if (o.active)
        return common_failure("PrivateShopActiveObligation", row.first);
    }
  }
  for (const auto &row : x.economy.arrears.rows())
    if (obligations.count(row.second.obligation_id) &&
        before(x.common, row.first, boundary)) {
      const auto &due = row.second;
      if (due.due_tick > tick || due.original_due < 0)
        return common_failure("PrivateShopClosingArrearMismatch", row.first);
      Integer paid = 0;
      for (const auto &p : x.economy.payments.rows())
        if (p.second.arrear_id == row.first &&
            before(x.common, p.first, boundary)) {
          if (p.second.tick > tick || p.second.amount <= 0 ||
              !before(x.common, row.first, p.first) ||
              __builtin_add_overflow(paid, p.second.amount, &paid))
            return common_failure("PrivateShopClosingPaymentMismatch", p.first);
        }
      if (paid != due.original_due)
        return common_failure("PrivateShopUnpaidObligation", row.first);
    }
  if (!x.closing_tasks)
    return common_failure("PrivateShopTaskAssetHistoryUnavailable");
  TaskPrivateShopAssetScope scope{shop.terms.proprietor_actor_id, shop.id,
                                  shop.wallet_id, shop.inventory_id};
  auto tasks = boundary.empty()
                   ? x.closing_tasks->current(scope)
                   : x.closing_tasks->before_close(scope, boundary, tick);
  if (!tasks.ok)
    return {false, {}, tasks.error};
  const auto &s = tasks.value;
  if (!identity(s.source_identity, x.common.state().identity) ||
      s.scope.proprietor_actor_id != scope.proprietor_actor_id ||
      s.scope.shop_id != scope.shop_id ||
      s.scope.wallet_id != scope.wallet_id ||
      s.scope.inventory_id != scope.inventory_id || s.as_of_tick != tick ||
      s.closing_fact_id != boundary)
    return common_failure("PrivateShopTaskAssetSourceMismatch");
  return s.pending_source_ids.empty()
             ? common_success()
             : common_failure("PrivateShopTaskAssetsOutstanding",
                              s.pending_source_ids.front());
}
Outcome<PrivateShopClosingQuote>
preview_private_shop_closing(const PrivateShopContext &x, const StableId &actor,
                             const PrivateShopClose &command) {
  if (x.phase || !validate_private_shop_committed_read3(x).ok)
    return fail<PrivateShopClosingQuote>(
        "PrivateShopCommandRequiresCommittedClock");
  auto valid = validate_private_shops(x);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto *shop = x.state.shops.find(command.shop_id);
  const auto *person = x.common.actor(actor);
  if (!shop || !person || shop->terms.proprietor_actor_id != actor)
    return fail<PrivateShopClosingQuote>("PrivateShopProprietorRequired");
  if (shop->status == "closed")
    return fail<PrivateShopClosingQuote>("PrivateShopAlreadyClosed");
  auto place = x.common.resolved_location(actor);
  if (!place.ok)
    return {false, {}, place.error};
  const auto *at = std::get_if<SettlementLocation>(&place.value);
  if (!person->alive || !person->available || !person->action_id.empty() ||
      x.activity.in_open_battle(actor) || x.activity.in_interaction(actor) ||
      !at || at->place_id != shop->terms.place_id)
    return fail<PrivateShopClosingQuote>("PrivateShopPresenceRequired");
  valid =
      validate_private_shop_closing_responsibilities(x, *shop, x.common.tick());
  if (!valid.ok)
    return {false, {}, valid.error};
  return private_shop_closing_assets::preview(x, shop->id);
}
Outcome<PrivateShopClosingQuote>
private_shop_closing_assets::preview(const PrivateShopContext &x,
                                     const StableId &shop_id) {
  if (x.phase || !validate_private_shop_committed_read3(x).ok)
    return fail<PrivateShopClosingQuote>(
        "PrivateShopCommandRequiresCommittedClock");
  const auto *shop = x.state.shops.find(shop_id);
  if (!shop || shop->status == "closed")
    return fail<PrivateShopClosingQuote>("PrivateShopAlreadyClosed");
  const auto actor = shop->terms.proprietor_actor_id;
  const auto *person = x.common.actor(actor);
  if (!person)
    return fail<PrivateShopClosingQuote>("PrivateShopProprietorRequired");
  auto valid =
      validate_private_shop_closing_responsibilities(x, *shop, x.common.tick());
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto *wallet = x.common.wallet(shop->wallet_id);
  const auto *target = x.common.wallet(person->wallet_id);
  const auto *stock = x.common.inventory(shop->inventory_id);
  const auto *bag = x.common.inventory(person->inventory_id);
  const Owner owner{Owner::Kind::actor, actor, {}};
  if (!wallet || !target || !stock || !bag || wallet->id == target->id ||
      stock->id == bag->id || !owner_equal(wallet->owner, owner) ||
      !owner_equal(target->owner, owner) || !owner_equal(stock->owner, owner) ||
      !owner_equal(bag->owner, owner) || !wallet->reserved.empty())
    return fail<PrivateShopClosingQuote>(
        "PrivateShopClosingPrivateAssetsRequired");
  Integer balance = 0;
  if (wallet->balance < 0 ||
      __builtin_add_overflow(target->balance, wallet->balance, &balance))
    return fail<PrivateShopClosingQuote>("PrivateShopClosingCashOverflow");
  auto existing = x.common.inventory_weight(bag->id),
       remaining = x.common.inventory_weight(stock->id);
  if (!existing.ok)
    return {false, {}, existing.error};
  if (!remaining.ok)
    return {false, {}, remaining.error};
  Integer weight = 0;
  if (__builtin_add_overflow(existing.value, remaining.value, &weight) ||
      weight > bag->capacity)
    return fail<PrivateShopClosingQuote>("PrivateShopClosingBagCapacity");
  for (const auto &lot : stock->lots)
    if (!owner_equal(lot.owner, owner) || !lot.reserved_by.empty())
      return fail<PrivateShopClosingQuote>("PrivateShopClosingReservedGoods");
  PrivateShopClosingQuote q{
      x.common.state().identity, actor,       shop->id, target->id, bag->id,
      wallet->balance,           stock->lots, {}};
  for (const auto &key : {ResourceKey{ResourceKey::Kind::actor, actor},
                          ResourceKey{ResourceKey::Kind::wallet, wallet->id},
                          ResourceKey{ResourceKey::Kind::wallet, target->id},
                          ResourceKey{ResourceKey::Kind::inventory, stock->id},
                          ResourceKey{ResourceKey::Kind::inventory, bag->id}})
    q.versions.push_back(x.common.read_version(key));
  return {true, std::move(q), {}};
}
Outcome<StableId>
close_private_shop_in_candidate(CommonStore &c, PrivateShopState &state,
                                const PrivateShopContext &x,
                                const PrivateShopClosingQuote &quote) {
  if (&c != &x.common || &state != &x.state)
    return fail<StableId>("PrivateShopCandidateMismatch");
  auto current =
      preview_private_shop_closing(x, quote.actor_id, {quote.shop_id});
  if (!current.ok)
    return {false, {}, current.error};
  if (!same_quote(quote, current.value))
    return fail<StableId>("StalePrivateShopClosing");
  local_economy_detail::CandidateStateGuard<PrivateShopState> guard(state);
  auto closed =
      private_shop_closing_assets::commit_in_candidate(c, state, x, quote);
  if (!closed.ok)
    return closed;
  auto valid = validate_private_shops(x);
  if (!valid.ok)
    return {false, {}, valid.error};
  guard.commit();
  return closed;
}
Outcome<StableId> private_shop_closing_assets::commit_in_candidate(
    CommonStore &c, PrivateShopState &state, const PrivateShopContext &x,
    const PrivateShopClosingQuote &quote) {
  if (&c != &x.common || &state != &x.state)
    return fail<StableId>("PrivateShopCandidateMismatch");
  auto fresh = preview(x, quote.shop_id);
  if (!fresh.ok)
    return {false, {}, fresh.error};
  if (!same_quote(quote, fresh.value))
    return fail<StableId>("StalePrivateShopClosing");
  local_economy_detail::CandidateStateGuard<PrivateShopState> guard(state);
  const auto shop = *state.shops.find(quote.shop_id);
  const auto old_bag = c.inventory(quote.to_inventory_id)->lots;
  const Integer next_before = c.state().next_instance_id;
  std::set<StableId> previous;
  for (const auto &lot : old_bag)
    previous.insert(lot.id);
  std::map<StableId, Integer> quantities;
  for (const auto &lot : quote.stock_lots)
    if (__builtin_add_overflow(quantities[lot.item_id], lot.quantity,
                               &quantities[lot.item_id]))
      return fail<StableId>("PrivateShopClosingStockOverflow");
  ResourceRecipe recipe;
  recipe.expected_versions = quote.versions;
  auto built = resource_recipe::append_money_transfer(
      recipe, shop.wallet_id, quote.to_wallet_id, quote.returned_cash,
      resource_recipe::MoneyTransferPolicy::spendable_debit_original_credit);
  if (!built.ok)
    return {false, {}, built.error};
  const Owner owner{Owner::Kind::actor, quote.actor_id, {}};
  for (const auto &item : quantities)
    recipe.operations.push_back(TransferLots{shop.inventory_id,
                                             quote.to_inventory_id,
                                             item.first,
                                             item.second,
                                             {owner},
                                             {},
                                             owner});
  auto moved = c.apply_resources(recipe);
  if (!moved.ok)
    return {false, {}, moved.error};
  const auto *bag = c.inventory(quote.to_inventory_id);
  for (const auto &original : old_bag) {
    auto found =
        std::find_if(bag->lots.begin(), bag->lots.end(),
                     [&](const auto &l) { return l.id == original.id; });
    if (found == bag->lots.end() || !same_lot(original, *found))
      return fail<StableId>("PrivateShopClosingOldBagChanged");
  }
  std::vector<PrivateShopClosingLot> returned;
  std::set<StableId> used;
  for (const auto &item : quantities) {
    std::vector<const LotView *> fresh;
    for (const auto &l : bag->lots)
      if (!previous.count(l.id) && l.item_id == item.first)
        fresh.push_back(&l);
    std::size_t i = 0;
    for (const auto &lot : quote.stock_lots)
      if (lot.item_id == item.first) {
        if (i >= fresh.size())
          return fail<StableId>("PrivateShopClosingTransferMismatch");
        const auto &dest = *fresh[i++];
        const auto n = organization_instance_sequence(dest.id, "lot");
        if (!n || *n < next_before || dest.quantity != lot.quantity ||
            !owner_equal(dest.owner, owner) || !dest.reserved_by.empty() ||
            !used.insert(dest.id).second)
          return fail<StableId>("PrivateShopClosingTransferMismatch");
        returned.push_back({lot.id, dest.id, lot.item_id, lot.quantity});
      }
    if (i != fresh.size())
      return fail<StableId>("PrivateShopClosingTransferMismatch");
  }
  if (bag->lots.size() != old_bag.size() + used.size() ||
      !c.inventory(shop.inventory_id)->lots.empty() ||
      c.wallet(shop.wallet_id)->balance != 0)
    return fail<StableId>("PrivateShopClosingTransferMismatch");
  auto id = c.next_id("private_shop_closed");
  if (!id.ok)
    return id;
  auto written =
      c.append_fact({id.value, "private_shop_closed", private_shop_module});
  if (!written.ok)
    return {false, {}, written.error};
  PrivateShopClosingRecord done{id.value,
                                shop.id,
                                quote.actor_id,
                                shop.wallet_id,
                                quote.to_wallet_id,
                                shop.inventory_id,
                                quote.to_inventory_id,
                                c.tick(),
                                quote.returned_cash,
                                std::move(returned)};
  if (!state.closings.insert(id.value, std::move(done)))
    return fail<StableId>("PrivateShopDuplicateClosing");
  state.shops.find(shop.id)->status = "closed";
  guard.commit();
  return id;
}
Status validate_private_shop_closings(const PrivateShopContext &x) {
  std::set<StableId> shops;
  for (const auto &row : x.state.closings.rows()) {
    const auto &r = row.second;
    const auto *shop = x.state.shops.find(r.shop_id);
    const auto *actor = x.common.actor(r.actor_id);
    const auto *head = x.common.state().facts.find(row.first);
    const auto n =
        organization_instance_sequence(row.first, "private_shop_closed");
    if (row.first != r.id || !shop || !actor || !head || head->id != r.id ||
        head->kind != "private_shop_closed" ||
        head->domain != private_shop_module || !n ||
        *n >= x.common.state().next_instance_id ||
        !shops.insert(r.shop_id).second || shop->status != "closed" ||
        r.actor_id != shop->terms.proprietor_actor_id ||
        r.from_wallet_id != shop->wallet_id ||
        r.from_inventory_id != shop->inventory_id ||
        r.to_wallet_id != actor->wallet_id ||
        r.to_inventory_id != actor->inventory_id || r.returned_cash < 0 ||
        r.tick < shop->opened_tick || r.tick > x.common.tick())
      return common_failure(
          "InvalidPrivateShopClosing",
          r.id); // 关闭后任何新的签约/订单/服务都不允许借空壳继续；旧载荷只按事实前界读取。
    for (const auto &c : x.state.staff.contracts.rows())
      if (c.second.terms.shop_id == shop->id &&
          !before(x.common, c.first, r.id))
        return common_failure("PrivateShopStaffAfterClosing", c.first);
    for (const auto &o : x.state.orders.orders.rows())
      if (o.second.terms.shop_id == shop->id &&
          !before(x.common, o.first, r.id))
        return common_failure("PrivateShopOrderAfterClosing", o.first);
    auto valid =
        validate_private_shop_closing_responsibilities(x, *shop, r.tick, r.id);
    if (!valid.ok)
      return valid;
  }
  for (const auto &row : x.state.shops.rows())
    if ((row.second.status == "closed") != (shops.count(row.first) != 0))
      return common_failure("PrivateShopClosingReferenceMismatch", row.first);
  return common_success();
}
} // namespace hanmo::resident
