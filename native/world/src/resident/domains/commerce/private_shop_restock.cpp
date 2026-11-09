// 原交易新批次逐ID扣除再同recipe入店，保持实付成本与库存一一对应；不能从已有背包粮食偷换来源。
#include "private_shop_restock.hpp"
#include "private_shop_market_contract.hpp"
#include "resident/domains/local_economy/local_economy_internal.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
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
bool same_lot(const LotView &a, const LotView &b) {
  return a.id == b.id && a.item_id == b.item_id && a.quantity == b.quantity &&
         owner_equal(a.owner, b.owner) && a.reserved_by == b.reserved_by &&
         a.field_order == b.field_order && a.extra == b.extra;
}
} // namespace
Outcome<PrivateShopRestockAdmission> prepare_private_shop_restock(
    const PrivateShopContext &x, const GrowthState &growth,
    const SocialState &social, const GrowthAccess &access,
    const StableId &actor, const PrivateShopRestock &command) {
  if (x.phase || !validate_private_shop_committed_read3(x).ok)
    return {false,
            {},
            common_failure("PrivateShopCommandRequiresCommittedClock").error};
  auto valid = validate_private_shops(x);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto *shop = x.state.shops.find(command.shop_id);
  if (!shop || shop->terms.proprietor_actor_id != actor)
    return fail<PrivateShopRestockAdmission>("PrivateShopProprietorRequired");
  if (shop->status == "closed")
    return fail<PrivateShopRestockAdmission>("PrivateShopAlreadyClosed");
  if (!x.markets || !x.markets->regional_life || !x.trades)
    return fail<PrivateShopRestockAdmission>("PrivateShopTradeHistoryRequired");
  const auto *market = x.markets->shops.find(shop->terms.market_shop_id);
  const auto *person = x.common.actor(actor);
  if (!market || market->place_id != shop->terms.place_id || !person)
    return fail<PrivateShopRestockAdmission>("PrivateShopLocalMarketRequired");
  auto location = x.common.resolved_location(actor);
  if (!location.ok)
    return {false, {}, location.error};
  const auto *at = std::get_if<SettlementLocation>(&location.value);
  if (!at || at->place_id != shop->terms.place_id)
    return fail<PrivateShopRestockAdmission>("PrivateShopPresenceRequired");
  GrowthCommand trade;
  trade.kind = "trade";
  trade.shop_id = market->id;
  trade.item_id = command.item_id;
  trade.side = "buy";
  trade.payer_wallet_id = shop->wallet_id;
  trade.quantity = command.quantity;
  trade.context = command.context;
  auto quoted = preview_growth(x.common, growth, *x.markets, x.common_catalog,
                               social, access, actor, trade);
  if (!quoted.ok)
    return {false, {}, quoted.error};
  const auto *terms = std::get_if<TradeTerms>(&quoted.value.data);
  if (!terms || terms->quantity <= 0 || terms->total <= 0 ||
      !owner_equal(terms->owner, {Owner::Kind::actor, actor, {}}))
    return fail<PrivateShopRestockAdmission>("PrivateShopTradeSourceMismatch");
  const auto *bag = x.common.inventory(person->inventory_id);
  const auto *stock = x.common.inventory(shop->inventory_id);
  const auto *payer = x.common.wallet(shop->wallet_id);
  const auto *merchant = x.common.wallet(market->wallet_id);
  const auto *market_stock = x.common.inventory(market->inventory_id);
  const auto *item = x.common_catalog.items.find(command.item_id);
  if (!bag || !stock || !payer || !merchant || !market_stock || !item ||
      !owner_equal(bag->owner, payer->owner) || bag->id == stock->id ||
      market_stock->id == stock->id)
    return fail<PrivateShopRestockAdmission>("PrivateShopTradeAssetsMismatch");
  auto weight = x.common.inventory_weight(stock->id);
  if (!weight.ok)
    return {false, {}, weight.error};
  Integer added = 0, total = 0;
  if (__builtin_mul_overflow(item->weight, terms->quantity, &added) ||
      __builtin_add_overflow(weight.value, added, &total) ||
      total > stock->capacity)
    return fail<PrivateShopRestockAdmission>("PrivateShopStockCapacity");
  auto count =
      x.common.count(market_stock->id, command.item_id, {market_stock->owner});
  if (!count.ok)
    return {false, {}, count.error};
  return {true,
          {x.common.state().identity, actor, shop->id, bag->id,
           market_stock->id, merchant->id, std::move(quoted.value),
           payer->balance, merchant->balance, count.value,
           x.common.state().next_instance_id, bag->lots,
           x.common.read_version({ResourceKey::Kind::inventory, stock->id})},
          {}};
}
Outcome<StableId> capture_private_shop_restock_in_candidate(
    CommonStore &c, PrivateShopState &s, const PrivateShopContext &x,
    const PrivateShopRestockAdmission &before, const StableId &trade_id) {
  if (&c != &x.common || &s != &x.state)
    return fail<StableId>("PrivateShopCandidateMismatch");
  if (!identity(before.source_identity, c.state().identity) || !x.trades ||
      !x.markets)
    return fail<StableId>("StalePrivateShopRestock");
  const auto *shop = s.shops.find(before.shop_id);
  const auto *person = c.actor(before.actor_id);
  const auto *terms = std::get_if<TradeTerms>(&before.trade.data);
  const auto *action = c.action(trade_id);
  const auto *head = c.state().facts.find(trade_id);
  const auto sequence = organization_instance_sequence(trade_id, "action");
  auto original = x.trades->life_result(trade_id);
  if (!shop || shop->status == "closed" || !person ||
      shop->terms.proprietor_actor_id != person->id || !terms || !sequence ||
      *sequence < before.next_instance_before || !action ||
      action->kind != "trade" || action->actor_id != person->id ||
      person->action_id != trade_id || action->start_tick != c.tick() ||
      !head || head->kind != "life_activity" || head->domain != "growth" ||
      c.state().facts.order().empty() || !original ||
      original->fact_id != trade_id || original->action_id != trade_id ||
      original->kind != LifeActivityKind::trade ||
      original->phase != LifeActivityPhase::active ||
      original->activity != "buy" || original->actor_id != person->id ||
      original->start_tick != c.tick() ||
      original->settlement_tick != c.tick() ||
      original->item_id != terms->item_id ||
      original->quantity != terms->quantity ||
      original->amount != terms->total ||
      original->payer_wallet_id != shop->wallet_id ||
      original->payee_wallet_id != before.merchant_wallet_id ||
      original->from_inventory_id != before.market_inventory_id ||
      original->to_inventory_id != before.bag_id)
    return fail<StableId>("PrivateShopImmediateTradeRequired");
  if (c.state().facts.order().back() != trade_id) {
    if (!x.market_history)
      return fail<StableId>("PrivateShopImmediateTradeRequired");
    auto adjacent =
        x.market_history->private_shop_immediate_market_trade(trade_id);
    if (!adjacent.ok)
      return {false, {}, adjacent.error};
  }

  for (const auto &row : s.purchases.rows())
    if (row.second.trade_fact_id == trade_id)
      return fail<StableId>("PrivateShopTradeAlreadyStocked");
  const auto *market = x.markets->shops.find(shop->terms.market_shop_id);
  if (!market || market->id != terms->shop_id ||
      market->place_id != shop->terms.place_id ||
      market->inventory_id != before.market_inventory_id ||
      market->wallet_id != before.merchant_wallet_id || terms->side != "buy" ||
      terms->quantity <= 0 || terms->quantity > 500 || terms->total <= 0 ||
      before.shop_cash_before < 0 || before.merchant_cash_before < 0 ||
      terms->payer_wallet_id != shop->wallet_id ||
      !owner_equal(terms->owner, {Owner::Kind::actor, person->id, {}}))
    return fail<StableId>("PrivateShopTradeSourceMismatch");
  const auto *payer = c.wallet(shop->wallet_id);
  const auto *merchant = c.wallet(market->wallet_id);
  const auto *source = c.inventory(before.market_inventory_id);
  const auto *bag = c.inventory(before.bag_id);
  const auto *stock = c.inventory(shop->inventory_id);
  if (!payer || !merchant || !source || !bag || !stock ||
      person->inventory_id != bag->id)
    return fail<StableId>("PrivateShopTradeAssetsMismatch");
  Integer paid = 0, received = 0;
  if (__builtin_sub_overflow(before.shop_cash_before, terms->total, &paid) ||
      __builtin_add_overflow(before.merchant_cash_before, terms->total,
                             &received) ||
      payer->balance != paid || merchant->balance != received)
    return fail<StableId>("PrivateShopTradeCashMismatch");
  auto count = c.count(source->id, terms->item_id, {source->owner});
  if (!count.ok)
    return {false, {}, count.error};
  Integer remaining = 0;
  if (before.market_stock_before < 0 ||
      __builtin_sub_overflow(before.market_stock_before, terms->quantity,
                             &remaining) ||
      count.value != remaining)
    return fail<StableId>("PrivateShopTradeStockMismatch");
  const auto current_version =
      c.read_version(before.shop_inventory_version.resource);
  if (before.shop_inventory_version.resource.kind !=
          ResourceKey::Kind::inventory ||
      before.shop_inventory_version.resource.id != stock->id ||
      current_version.version != before.shop_inventory_version.version)
    return fail<StableId>("PrivateShopStockChangedDuringTrade");
  std::set<StableId> old_ids;
  for (const auto &old : before.bag_lots_before) {
    if (!old_ids.insert(old.id).second)
      return fail<StableId>("PrivateShopBagSnapshotInvalid");
    const auto found =
        std::find_if(bag->lots.begin(), bag->lots.end(),
                     [&](const auto &lot) { return lot.id == old.id; });
    if (found == bag->lots.end() || !same_lot(old, *found))
      return fail<StableId>("PrivateShopOldBagLotsChanged");
  }
  Integer quantity = 0;
  std::vector<PrivateShopStockLot> purchased;
  AdjustLots debit;
  debit.inventory_id = bag->id;
  for (const auto &lot : bag->lots)
    if (!old_ids.count(lot.id)) {
      const auto id = organization_instance_sequence(lot.id, "lot");
      if (!id || *id <= *sequence || *id >= c.state().next_instance_id ||
          lot.quantity <= 0 || lot.item_id != terms->item_id ||
          !owner_equal(lot.owner, terms->owner) || !lot.reserved_by.empty() ||
          __builtin_add_overflow(quantity, lot.quantity, &quantity))
        return fail<StableId>("PrivateShopNewTradeLotsRequired");
      purchased.push_back({lot.id, lot.quantity});
      debit.lots.push_back({lot.id, -lot.quantity});
    }
  if (quantity != terms->quantity)
    return fail<StableId>("PrivateShopNewTradeLotsRequired");
  local_economy_detail::CandidateStateGuard<PrivateShopState> guard(s);
  auto id = c.next_id("private_shop_stock");
  if (!id.ok)
    return id;
  auto appended = c.append_fact(
      {id.value, "private_shop_stock_received", private_shop_module});
  if (!appended.ok)
    return {false, {}, appended.error};
  std::set<StableId> previous;
  for (const auto &lot : stock->lots)
    previous.insert(lot.id);
  const auto target = stock->id;
  ResourceRecipe recipe;
  recipe.expected_versions = {
      c.read_version({ResourceKey::Kind::inventory, bag->id}), current_version};
  recipe.operations = {
      debit, AddLot{target, terms->item_id, quantity, terms->owner, {}}};
  auto moved = c.apply_resources(recipe);
  if (!moved.ok)
    return {false, {}, moved.error};
  std::vector<PrivateShopStockLot> stocked;
  for (const auto &lot : c.inventory(target)->lots)
    if (!previous.count(lot.id))
      stocked.push_back({lot.id, lot.quantity});
  if (!s.purchases.insert(id.value, {id.value, shop->id, trade_id,
                                     terms->item_id, market->id, before.bag_id,
                                     target, c.tick(), quantity, terms->total,
                                     std::move(purchased), std::move(stocked)}))
    return fail<StableId>("PrivateShopDuplicatePurchase");
  auto valid = validate_private_shops(x);
  if (!valid.ok)
    return {false, {}, valid.error};
  guard.commit();
  return id;
}
} // namespace hanmo::resident
