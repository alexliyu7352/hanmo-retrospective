#include "household_retail.hpp"
#include "private_shop_accounting.hpp"
#include "private_shop_market_contract.hpp"
#include "resident/domains/growth/growth_internal.hpp"
#include <map>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
template <class T>
Outcome<T> fail(const StableId &code, const StableId &id = {}) {
  return {false, {}, common_failure(code, id).error};
}
bool identity(const WorldIdentity &a, const WorldIdentity &b) {
  return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch &&
         a.revision == b.revision && a.tick == b.tick;
}
bool selected(const std::vector<StableId> &values, const StableId &id) {
  return std::find(values.begin(), values.end(), id) != values.end();
}
bool same_source_optional(const std::optional<SourceRef> &a,
                          const std::optional<SourceRef> &b) {
  return bool(a) == bool(b) && (!a || same_source_ref(*a, *b));
}
bool same_buyer(const HouseholdBuyer &a, const HouseholdBuyer &b) {
  if (a.index() != b.index())
    return false;
  if (const auto *x = std::get_if<NamedHouseholdBuyer>(&a)) {
    const auto &y = std::get<NamedHouseholdBuyer>(b);
    return x->actor_id == y.actor_id &&
           same_source_optional(x->accepted_service_ref,
                                y.accepted_service_ref);
  }
  const auto &x = std::get<CohortHouseholdBuyer>(a);
  const auto &y = std::get<CohortHouseholdBuyer>(b);
  return std::tie(x.cohort_id, x.population_selection_ref,
                  x.labor_allocation_id, x.workers) ==
             std::tie(y.cohort_id, y.population_selection_ref,
                      y.labor_allocation_id, y.workers) &&
         same_source_ref(x.accepted_household_policy_ref,
                         y.accepted_household_policy_ref);
}
bool same_seller(const HouseholdRetailSeller &a,
                 const HouseholdRetailSeller &b) {
  if (a.index() != b.index())
    return false;
  if (const auto *x = std::get_if<PublicHouseholdRetailSeller>(&a))
    return x->market_shop_id ==
           std::get<PublicHouseholdRetailSeller>(b).market_shop_id;
  const auto &x = std::get<PrivateHouseholdRetailSeller>(a);
  const auto &y = std::get<PrivateHouseholdRetailSeller>(b);
  return x.shop_id == y.shop_id && x.operation_id == y.operation_id;
}
bool same_selections(const std::vector<CargoSelection> &a,
                     const std::vector<CargoSelection> &b) {
  if (a.size() != b.size())
    return false;
  for (std::size_t i = 0; i < a.size(); ++i)
    if (a[i].inventory_id != b[i].inventory_id || a[i].lot_id != b[i].lot_id ||
        a[i].quantity != b[i].quantity)
      return false;
  return true;
}
bool same_terms(const HouseholdRetailTerms &a, const HouseholdRetailTerms &b) {
  auto time = compare_world_moments(a.quoted_at, b.quoted_at);
  return time.ok && time.value == 0 &&
         std::tie(a.purchase_id, a.household_group_id, a.item_id,
                  a.buyer_wallet_id, a.seller_wallet_id, a.source_inventory_id,
                  a.custody_inventory_id, a.destination_inventory_id,
                  a.place_id, a.quantity, a.total, a.market_revision,
                  a.market_stock, a.base_price, a.midpoint, a.unit_price,
                  a.terms_version) ==
             std::tie(b.purchase_id, b.household_group_id, b.item_id,
                      b.buyer_wallet_id, b.seller_wallet_id,
                      b.source_inventory_id, b.custody_inventory_id,
                      b.destination_inventory_id, b.place_id, b.quantity,
                      b.total, b.market_revision, b.market_stock, b.base_price,
                      b.midpoint, b.unit_price, b.terms_version) &&
         owner_equal(a.buyer, b.buyer) && owner_equal(a.seller, b.seller) &&
         same_buyer(a.executor, b.executor) &&
         same_seller(a.seller_ref, b.seller_ref) &&
         same_selections(a.selected_cargo, b.selected_cargo) &&
         same_source_ref(a.accepted_terms_ref, b.accepted_terms_ref) &&
         same_source_ref(a.custody_terms_ref, b.custody_terms_ref);
}
VerifiedTradeTerms trade_terms(const HouseholdRetailTerms &t) {
  return {t.source_inventory_id,
          t.custody_inventory_id,
          t.item_id,
          t.buyer_wallet_id,
          t.seller_wallet_id,
          t.seller,
          t.buyer,
          t.quantity,
          t.total,
          t.selected_cargo};
}
bool add(Integer &out, Integer n) {
  return n >= 0 && !__builtin_add_overflow(out, n, &out);
}
Outcome<std::vector<PrivateShopRetailLot>>
costs(const HouseholdRetailReadContext &x, const HouseholdRetailTerms &t,
      const StableId &before = {}) {
  std::vector<PrivateShopRetailLot> out;
  const auto *seller = std::get_if<PrivateHouseholdRetailSeller>(&t.seller_ref);
  if (!seller)
    return {true, {}, {}};
  if (!x.shops_catalog)
    return fail<std::vector<PrivateShopRetailLot>>(
        "HouseholdRetailPrivateCatalogRequired");
  std::map<StableId, PrivateShopCostBasis> basis;
  for (const auto &row : x.shops.purchases.rows())
    if (row.second.shop_id == seller->shop_id)
      basis.emplace(row.first,
                    PrivateShopCostBasis{row.first, row.second.quantity,
                                         row.second.purchase_cash, 0, 0, 0});
  auto account = [&](const std::vector<PrivateShopRetailLot> &rows) -> Status {
    for (const auto &lot : rows) {
      auto at = basis.find(lot.purchase_fact_id);
      if (at == basis.end())
        continue;
      auto cost = quote_private_shop_cost_slice(at->second, lot.quantity);
      if (!cost.ok)
        return {false, {}, cost.error};
      if (cost.value.cost != lot.cost)
        return common_failure("HouseholdRetailCostHistoryMismatch",
                              lot.purchase_fact_id);
      at->second.sold_quantity = cost.value.sold_quantity_after;
      at->second.recognized_cost = cost.value.recognized_cost_after;
    }
    return common_success();
  };
  for (const auto &fact : x.common.state().facts.rows()) {
    if (fact.first == before)
      break;
    if (const auto *row = x.state.settlements.find(fact.first)) {
      auto ok = account(row->consumed_lots);
      if (!ok.ok)
        return {false, {}, ok.error};
    }
    if (const auto *row = x.shops.operating_results.find(fact.first)) {
      const auto *operation = x.shops.operations.find(row->operation_id);
      const auto *shop =
          operation ? x.shops.shops.find(operation->shop_id) : nullptr;
      if (shop && !selected(x.catalog.premises_ids, shop->terms.premises_id)) {
        auto ok = account(row->consumed_lots);
        if (!ok.ok)
          return {false, {}, ok.error};
      }
    }
    if (const auto *row = x.shops.order_settlements.find(fact.first)) {
      auto ok = account(row->consumed_lots);
      if (!ok.ok)
        return {false, {}, ok.error};
    }
  }
  for (const auto &chosen : t.selected_cargo) {
    const PrivateShopPurchaseFact *original = nullptr;
    for (const auto &row : x.shops.purchases.rows())
      if (row.second.shop_id == seller->shop_id &&
          row.second.item_id == t.item_id)
        for (const auto &lot : row.second.stocked_lots)
          if (lot.lot_id == chosen.lot_id) {
            if (original)
              return fail<std::vector<PrivateShopRetailLot>>(
                  "DuplicatePrivateShopStockSource", chosen.lot_id);
            original = &row.second;
          }
    if (!original)
      return fail<std::vector<PrivateShopRetailLot>>(
          "PrivateShopStockSourceRequired", chosen.lot_id);
    auto &b = basis.at(original->id);
    auto slice = quote_private_shop_cost_slice(b, chosen.quantity);
    if (!slice.ok)
      return {false, {}, slice.error};
    out.push_back(
        {original->id, chosen.lot_id, chosen.quantity, slice.value.cost});
    b.sold_quantity = slice.value.sold_quantity_after;
    b.recognized_cost = slice.value.recognized_cost_after;
  }
  return {true, std::move(out), {}};
}
} // namespace
bool household_retail_selected(const PrivateShopContext &x,
                               const StableId &id) {
  const auto *shop = x.state.shops.find(id);
  return x.retail_catalog && x.retail && shop &&
         selected(x.retail_catalog->premises_ids, shop->terms.premises_id);
}
bool household_retail_operation_selected(const PrivateShopContext &x,
                                         const StableId &id) {
  const auto *operation = x.state.operations.find(id);
  return operation && household_retail_selected(x, operation->shop_id);
}
Outcome<HouseholdRetailQuote>
preview_household_retail(const HouseholdRetailReadContext &x,
                         const HouseholdRetailTerms &requested) {
  auto terms = requested;
  auto quoted = compare_world_moments(terms.quoted_at, x.now);
  if (!quoted.ok || quoted.value || x.catalog.version != 1 ||
      terms.purchase_id.empty() || terms.household_group_id.empty() ||
      terms.quantity < 1 || terms.quantity > 500)
    return fail<HouseholdRetailQuote>("InvalidHouseholdRetailRequest");
  const auto *payer = x.common.wallet(terms.buyer_wallet_id);
  const auto *holding = x.common.inventory(terms.custody_inventory_id);
  const auto *destination = x.common.inventory(terms.destination_inventory_id);
  if (!payer || !holding || !destination ||
      !owner_equal(payer->owner,
                   {Owner::Kind::household, terms.household_group_id, {}}) ||
      !owner_equal(destination->owner, payer->owner) ||
      !owner_equal(terms.buyer, payer->owner))
    return fail<HouseholdRetailQuote>("HouseholdRetailBuyerAssetsMismatch");
  GrowthMarketMidpoint market;
  if (const auto *public_seller =
          std::get_if<PublicHouseholdRetailSeller>(&terms.seller_ref)) {
    if (!selected(x.catalog.public_market_shop_ids,
                  public_seller->market_shop_id))
      return fail<HouseholdRetailQuote>("HouseholdPublicMarketNotSelected");
    const auto *shop = x.markets.shops.find(public_seller->market_shop_id);
    if (!shop)
      return fail<HouseholdRetailQuote>("UnknownHouseholdPublicMarket");
    auto current = read_growth_market_midpoint(
        x.common, x.markets, x.common_catalog, shop->id, terms.item_id);
    if (!current.ok)
      return {false, {}, current.error};
    market = current.value;
    const auto *stock = x.common.inventory(shop->inventory_id);
    const auto *payee = x.common.wallet(shop->wallet_id);
    if (!stock || !payee || !owner_equal(stock->owner, payee->owner))
      return fail<HouseholdRetailQuote>("HouseholdMarketOwnerMismatch");
    terms.source_inventory_id = stock->id;
    terms.seller_wallet_id = payee->id;
    terms.seller = stock->owner;
    terms.place_id = shop->place_id;
    terms.unit_price = 0;
    terms.total = growth::trade_price(
        market.base_price,
        std::min<Integer>(market.available_stock, 200 + terms.quantity),
        terms.quantity, true, 0);
  } else {
    const auto &seller =
        std::get<PrivateHouseholdRetailSeller>(terms.seller_ref);
    const auto *shop = x.shops.shops.find(seller.shop_id);
    const auto *operation = x.shops.operations.find(seller.operation_id);
    const auto *action = operation ? x.common.action(operation->id) : nullptr;
    if (!shop || !operation || operation->shop_id != shop->id ||
        operation->item_id != terms.item_id ||
        !selected(x.catalog.premises_ids, shop->terms.premises_id) ||
        shop->status != "open" || !operation->result_fact_id.empty() ||
        !action || action->kind != "private_shop_operate" ||
        action->actor_id != operation->actor_id)
      return fail<HouseholdRetailQuote>("HouseholdRetailSellerNotOperating");
    const auto *actor = x.common.actor(operation->actor_id);
    auto position = x.common.resolved_location(operation->actor_id);
    const auto *place = position.ok
                            ? std::get_if<SettlementLocation>(&position.value)
                            : nullptr;
    if (!actor || !actor->alive || !actor->available ||
        actor->action_id != action->id || !place ||
        place->place_id != shop->terms.place_id)
      return fail<HouseholdRetailQuote>("HouseholdRetailSellerNotPresent");
    if (!x.market_history)
      return fail<HouseholdRetailQuote>("HouseholdRetailMarketHistoryRequired");
    auto current = x.market_history->private_shop_market_history_current(
        shop->terms.market_shop_id, terms.item_id);
    if (!current.ok)
      return {false, {}, current.error};
    market = current.value;
    auto rate = quote_private_shop_retail_rate(market.midpoint,
                                               operation->markup_basis_points);
    if (!rate.ok)
      return {false, {}, rate.error};
    terms.unit_price = rate.value.unit_price;
    if (__builtin_mul_overflow(terms.quantity, terms.unit_price, &terms.total))
      return fail<HouseholdRetailQuote>("HouseholdRetailPriceOverflow");
    terms.source_inventory_id = shop->inventory_id;
    terms.seller_wallet_id = shop->wallet_id;
    terms.seller = {Owner::Kind::actor, shop->terms.proprietor_actor_id, {}};
    terms.place_id = shop->terms.place_id;
  }
  terms.market_revision = market.market_revision;
  terms.market_stock = market.available_stock;
  terms.base_price = market.base_price;
  terms.midpoint = market.midpoint;
  auto prepared = prepare_verified_trade_recipe(x.common, trade_terms(terms));
  if (!prepared.ok)
    return {false, {}, prepared.error};
  auto cost = costs(x, terms);
  if (!cost.ok)
    return {false, {}, cost.error};
  return {true,
          {x.common.state().identity, std::move(terms),
           prepared.value.expected_versions},
          {}};
}
Outcome<StableId> settle_household_retail_in_candidate(
    CommonStore &common, HouseholdRetailState &state,
    const HouseholdRetailReadContext &x, const HouseholdRetailQuote &quote) {
  if (&common != &x.common || &state != &x.state ||
      !identity(quote.identity, common.state().identity))
    return fail<StableId>("HouseholdRetailCandidateMismatch");
  for (const auto &row : state.settlements.rows())
    if (row.second.terms.purchase_id == quote.terms.purchase_id)
      return same_terms(row.second.terms, quote.terms)
                 ? Outcome<StableId>{true, row.first, {}}
                 : fail<StableId>("HouseholdRetailReplayMismatch", row.first);
  auto current = preview_household_retail(x, quote.terms);
  if (!current.ok)
    return {false, {}, current.error};
  if (!same_terms(current.value.terms, quote.terms) ||
      quote.versions.size() != current.value.versions.size())
    return fail<StableId>("StaleHouseholdRetailQuote");
  for (std::size_t i = 0; i < quote.versions.size(); ++i) {
    const auto &a = quote.versions[i];
    const auto &b = current.value.versions[i];
    if (a.resource.kind != b.resource.kind || a.resource.id != b.resource.id ||
        a.version != b.version)
      return fail<StableId>("StaleHouseholdRetailResources");
  }
  auto accepted =
      x.acceptances.read_household_retail_acceptance(quote.terms.purchase_id);
  if (!accepted.ok)
    return {false, {}, accepted.error};
  const auto &a = accepted.value;
  const auto &t = quote.terms;
  auto moment = compare_world_moments(a.accepted_at, t.quoted_at);
  if (a.purchase_id != t.purchase_id ||
      a.household_group_id != t.household_group_id ||
      a.wallet_id != t.buyer_wallet_id ||
      a.custody_inventory_id != t.custody_inventory_id ||
      a.destination_inventory_id != t.destination_inventory_id ||
      !owner_equal(a.owner, t.buyer) || !same_buyer(a.executor, t.executor) ||
      !same_source_ref(a.accepted_terms_ref, t.accepted_terms_ref) ||
      !same_source_ref(a.custody_terms_ref, t.custody_terms_ref) ||
      a.terms_version != t.terms_version || a.merchandise_limit < t.total ||
      !moment.ok || moment.value)
    return fail<StableId>("HouseholdRetailAcceptanceMismatch");
  auto cost = costs(x, t);
  if (!cost.ok)
    return {false, {}, cost.error};
  HouseholdRetailSettlement receipt;
  receipt.terms = t;
  receipt.settled_at = t.quoted_at;
  receipt.consumed_lots = cost.value;
  for (const auto &row : cost.value)
    if (!add(receipt.cost_of_goods, row.cost))
      return fail<StableId>("HouseholdRetailCostOverflow");
  receipt.payer_balance_before = common.wallet(t.buyer_wallet_id)->balance;
  receipt.payee_balance_before = common.wallet(t.seller_wallet_id)->balance;
  Outcome<VerifiedTradeExecution> moved;
  if (std::holds_alternative<PublicHouseholdRetailSeller>(t.seller_ref)) {
    if (!x.market_capture)
      return fail<StableId>("HouseholdRetailMarketCaptureRequired");
    moved = x.market_capture->apply_household_retail_recipe_in_candidate(
        common, trade_terms(t), t);
  } else
    moved = execute_verified_trade_recipe_in_candidate(common, trade_terms(t));
  if (!moved.ok)
    return {false, {}, moved.error};
  receipt.lots = std::move(moved.value.lots);
  receipt.payer_balance_after = common.wallet(t.buyer_wallet_id)->balance;
  receipt.payee_balance_after = common.wallet(t.seller_wallet_id)->balance;
  receipt.market_version_after =
      std::holds_alternative<PublicHouseholdRetailSeller>(t.seller_ref)
          ? common.inventory(t.source_inventory_id)->revision
          : t.market_revision;
  auto id = common.next_id("household_retail");
  if (!id.ok)
    return id;
  receipt.id = id.value;
  receipt.payment_ref = {"commerce", "household_retail_payment", id.value};
  for (std::size_t i = 0; i < receipt.lots.size(); ++i)
    receipt.transfer_refs.push_back({"commerce", "household_retail_transfer",
                                     id.value + "/" + std::to_string(i)});
  auto head =
      common.append_fact({id.value, "household_retail_settled", "commerce"});
  if (!head.ok)
    return {false, {}, head.error};
  state.settlements.insert(id.value, receipt);
  if (std::holds_alternative<PublicHouseholdRetailSeller>(t.seller_ref)) {
    auto bound = x.market_capture->bind_household_retail_source(receipt);
    if (!bound.ok)
      return {false, {}, bound.error};
  }
  return id;
}
Outcome<const HouseholdRetailSettlement *>
read_household_retail_settlement(const CommonStore &common,
                                 const HouseholdRetailState &state,
                                 const StableId &id) {
  const auto *row = state.settlements.find(id);
  const auto *fact = common.state().facts.find(id);
  if (!row || row->id != id || !fact ||
      fact->kind != "household_retail_settled" || fact->domain != "commerce")
    return fail<const HouseholdRetailSettlement *>(
        "HouseholdRetailSourceMissing", id);
  return {true, row, {}};
}
Status validate_household_retail(const HouseholdRetailReadContext &x) {
  if (!x.shops_catalog && (!x.catalog.premises_ids.empty() ||
                           !is_default_private_shop_state(x.shops)))
    return common_failure("HouseholdRetailPrivateCatalogRequired");
  if (x.catalog.version != 1 || x.state.version != 1)
    return common_failure("HouseholdRetailProfileMismatch");
  std::set<StableId> purchases;
  for (const auto &row : x.state.settlements.rows()) {
    auto actual =
        read_household_retail_settlement(x.common, x.state, row.first);
    if (!actual.ok)
      return {false, {}, actual.error};
    const auto &r = *actual.value;
    const auto &t = r.terms;
    auto at = compare_world_moments(r.settled_at, t.quoted_at);
    Integer payer = 0, payee = 0;
    if (!purchases.insert(t.purchase_id).second || t.quantity <= 0 ||
        t.total < 0 || !at.ok || at.value || ([&] {
          auto current = compare_world_moments(r.settled_at, x.now);
          return !current.ok || current.value > 0;
        })() ||
        __builtin_sub_overflow(r.payer_balance_before, t.total, &payer) ||
        __builtin_add_overflow(r.payee_balance_before, t.total, &payee) ||
        payer != r.payer_balance_after || payer < 0 ||
        payee != r.payee_balance_after ||
        !same_source_ref(r.payment_ref,
                         {"commerce", "household_retail_payment", r.id}) ||
        r.lots.size() != t.selected_cargo.size() ||
        r.transfer_refs.size() != r.lots.size())
      return common_failure("InvalidHouseholdRetailSource", row.first);
    Integer quantity = 0;
    for (std::size_t i = 0; i < r.lots.size(); ++i) {
      const auto &lot = r.lots[i];
      if (lot.original_lot_id != t.selected_cargo[i].lot_id ||
          lot.quantity != t.selected_cargo[i].quantity ||
          lot.destination_lot_id.empty() || !add(quantity, lot.quantity) ||
          !same_source_ref(r.transfer_refs[i],
                           {"commerce", "household_retail_transfer",
                            r.id + "/" + std::to_string(i)}))
        return common_failure("HouseholdRetailTransferMismatch", row.first);
    }
    if (quantity != t.quantity)
      return common_failure("HouseholdRetailQuantityMismatch", row.first);
    auto accepted = x.acceptances.validate_household_retail_settlement(r);
    if (!accepted.ok)
      return accepted;
    auto expected = costs(x, t, row.first);
    if (!expected.ok)
      return {false, {}, expected.error};
    if (expected.value.size() != r.consumed_lots.size())
      return common_failure("HouseholdRetailCostMismatch", row.first);
    Integer total = 0;
    for (std::size_t i = 0; i < expected.value.size(); ++i) {
      const auto &a = expected.value[i];
      const auto &b = r.consumed_lots[i];
      if (std::tie(a.purchase_fact_id, a.lot_id, a.quantity, a.cost) !=
              std::tie(b.purchase_fact_id, b.lot_id, b.quantity, b.cost) ||
          !add(total, b.cost))
        return common_failure("HouseholdRetailCostMismatch", row.first);
    }
    if (total != r.cost_of_goods)
      return common_failure("HouseholdRetailCostMismatch", row.first);
  }
  for (const auto &fact : x.common.state().facts.rows())
    if (fact.second.domain == "commerce" &&
        fact.second.kind == "household_retail_settled" &&
        !x.state.settlements.find(fact.first))
      return common_failure("OrphanHouseholdRetailFact", fact.first);
  return common_success();
}
Status summarize_household_retail_operation(const PrivateShopContext &x,
                                            const StableId &id,
                                            PrivateShopDayResult &result) {
  if (!household_retail_operation_selected(x, id))
    return common_failure("HouseholdRetailOperationProfileRequired", id);
  result.quantity = result.income = result.cost_of_goods = 0;
  result.consumed_lots.clear();
  result.market_observed_tick = result.market_revision = result.market_stock =
      result.base_price = result.midpoint = 0;
  for (const auto &row : x.retail->settlements.rows()) {
    const auto *seller =
        std::get_if<PrivateHouseholdRetailSeller>(&row.second.terms.seller_ref);
    if (!seller || seller->operation_id != id)
      continue;
    if (!add(result.quantity, row.second.terms.quantity) ||
        !add(result.income, row.second.terms.total) ||
        !add(result.cost_of_goods, row.second.cost_of_goods))
      return common_failure("HouseholdRetailSummaryOverflow", id);
    for (const auto &sold : row.second.consumed_lots) {
      auto old =
          std::find_if(result.consumed_lots.begin(), result.consumed_lots.end(),
                       [&](const auto &l) {
                         return l.purchase_fact_id == sold.purchase_fact_id &&
                                l.lot_id == sold.lot_id;
                       });
      if (old == result.consumed_lots.end())
        result.consumed_lots.push_back(sold);
      else if (!add(old->quantity, sold.quantity) || !add(old->cost, sold.cost))
        return common_failure("HouseholdRetailSummaryOverflow", id);
    }
  }
  return common_success();
}
Status
validate_household_retail_operation_result(const PrivateShopContext &x,
                                           const PrivateShopOperation &o,
                                           const PrivateShopDayResult &r) {
  auto expected = r;
  auto ok = summarize_household_retail_operation(x, o.id, expected);
  if (!ok.ok)
    return ok;
  if (expected.quantity != r.quantity || expected.income != r.income ||
      expected.cost_of_goods != r.cost_of_goods || r.market_observed_tick ||
      r.market_revision || r.market_stock || r.base_price || r.midpoint ||
      expected.consumed_lots.size() != r.consumed_lots.size())
    return common_failure("HouseholdRetailSummaryMismatch", r.id);
  for (std::size_t i = 0; i < r.consumed_lots.size(); ++i) {
    const auto &a = expected.consumed_lots[i];
    const auto &b = r.consumed_lots[i];
    if (std::tie(a.purchase_fact_id, a.lot_id, a.quantity, a.cost) !=
        std::tie(b.purchase_fact_id, b.lot_id, b.quantity, b.cost))
      return common_failure("HouseholdRetailSummaryMismatch", r.id);
  }
  return common_success();
}
const std::vector<PrivateShopRetailLot> *
private_shop_actual_cost_lots(const PrivateShopContext &x, const StableId &id) {
  if (x.retail)
    if (const auto *r = x.retail->settlements.find(id))
      return &r->consumed_lots;
  if (const auto *day = x.state.operating_results.find(id))
    return household_retail_operation_selected(x, day->operation_id)
               ? nullptr
               : &day->consumed_lots;
  if (const auto *order = x.state.order_settlements.find(id))
    return &order->consumed_lots;
  return nullptr;
}
} // namespace hanmo::resident
