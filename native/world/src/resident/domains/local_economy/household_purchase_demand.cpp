#include "household_purchase_demand.hpp"
#include "household_purchase_cargo.hpp"
#include "resident/domains/growth/growth_model.hpp"
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace reserve_demand {
template <class T>
Outcome<T> fail(const StableId &code, const StableId &id = {}) {
  return {false, {}, common_failure(code, id).error};
}
bool add(Integer &value, Integer amount) {
  return amount >= 0 && !__builtin_add_overflow(value, amount, &value);
}
bool same_versions(const std::vector<ReadVersion> &a,
                   const std::vector<ReadVersion> &b) {
  if (a.size() != b.size())
    return false;
  for (std::size_t i = 0; i < a.size(); ++i)
    if (a[i].resource.kind != b[i].resource.kind ||
        a[i].resource.id != b[i].resource.id || a[i].version != b[i].version)
      return false;
  return true;
}
bool same_known(const HouseholdKnownMarketEvidence &a,
                const HouseholdKnownMarketEvidence &b) {
  if (a.selection != b.selection ||
      a.knowledge.index() != b.knowledge.index() ||
      !same_source_ref(a.report_ref, b.report_ref) ||
      !same_source_ref(a.observation_source, b.observation_source))
    return false;
  auto observed = compare_world_moments(a.observed_at, b.observed_at);
  if (!observed.ok || observed.value)
    return false;
  if (const auto *x = std::get_if<KnowledgeReceiptV2>(&a.knowledge)) {
    const auto &y = std::get<KnowledgeReceiptV2>(b.knowledge);
    auto learned = compare_world_moments(x->learned_at, y.learned_at);
    return std::tie(x->receiver.kind, x->receiver.id, x->report_id,
                    x->source_message_id, x->learned_tick,
                    x->source_subject.kind, x->source_subject.id) ==
               std::tie(y.receiver.kind, y.receiver.id, y.report_id,
                        y.source_message_id, y.learned_tick,
                        y.source_subject.kind, y.source_subject.id) &&
           learned.ok && !learned.value;
  }
  const auto &x = std::get<HouseholdPurchaseScopedKnowledgeRef>(a.knowledge);
  const auto &y = std::get<HouseholdPurchaseScopedKnowledgeRef>(b.knowledge);
  auto learned = compare_world_moments(x.learned_at, y.learned_at);
  return x.actual_delivery.source_schema == y.actual_delivery.source_schema &&
         same_source_ref(x.actual_delivery.source_ref,
                         y.actual_delivery.source_ref) &&
         same_source_ref(x.purchase_plan_ref, y.purchase_plan_ref) &&
         x.cohort_id == y.cohort_id && x.report_id == y.report_id &&
         learned.ok && !learned.value;
}
bool same_demand(const HouseholdReserveDemand &a,
                 const HouseholdReserveDemand &b) {
  if (!same_source_ref(a.reserve_rule_ref, b.reserve_rule_ref) ||
      std::tie(a.plan_id, a.household_id, a.item_id, a.destination_inventory_id,
               a.available_stock, a.owned_pending_delivery, a.minimum_stock,
               a.quantity_needed, a.minimum_cash_reserve,
               a.available_cash_after_reserve) !=
          std::tie(b.plan_id, b.household_id, b.item_id,
                   b.destination_inventory_id, b.available_stock,
                   b.owned_pending_delivery, b.minimum_stock, b.quantity_needed,
                   b.minimum_cash_reserve, b.available_cash_after_reserve) ||
      !same_versions(a.versions, b.versions) ||
      a.pending_retail_sources.size() != b.pending_retail_sources.size())
    return false;
  for (std::size_t i = 0; i < a.pending_retail_sources.size(); ++i)
    if (!same_source_ref(a.pending_retail_sources[i],
                         b.pending_retail_sources[i]))
      return false;
  return true;
}
struct SellerStock {
  const InventoryView *inventory = nullptr;
  StableId place_id;
  Owner owner;
};
Outcome<SellerStock> seller_stock(const HouseholdPurchaseContext &x,
                                  const HouseholdPurchasePlan &plan,
                                  const HouseholdRetailSeller &seller,
                                  const StableId &item) {
  if (const auto *public_market =
          std::get_if<PublicHouseholdRetailSeller>(&seller)) {
    const auto *market =
        x.retail.markets.shops.find(public_market->market_shop_id);
    if (!market || market->id != plan.known_market_ref ||
        std::find(x.retail.catalog.public_market_shop_ids.begin(),
                  x.retail.catalog.public_market_shop_ids.end(),
                  market->id) == x.retail.catalog.public_market_shop_ids.end())
      return fail<SellerStock>("HouseholdDemandMarketOutsidePlan");
    const auto *inventory = x.common.inventory(market->inventory_id);
    if (!inventory)
      return fail<SellerStock>("HouseholdDemandStockMissing");
    return {true, {inventory, market->place_id, inventory->owner}, {}};
  }
  const auto &private_market = std::get<PrivateHouseholdRetailSeller>(seller);
  const auto *shop = x.retail.shops.shops.find(private_market.shop_id);
  const auto *operation =
      x.retail.shops.operations.find(private_market.operation_id);
  if (!shop || !operation || operation->shop_id != shop->id ||
      operation->item_id != item ||
      shop->terms.market_shop_id != plan.known_market_ref)
    return fail<SellerStock>("HouseholdDemandMarketOutsidePlan");
  const auto *inventory = x.common.inventory(shop->inventory_id);
  if (!inventory)
    return fail<SellerStock>("HouseholdDemandStockMissing");
  return {true,
          {inventory,
           shop->terms.place_id,
           {Owner::Kind::actor, shop->terms.proprietor_actor_id, {}}},
          {}};
}
std::vector<CargoSelection> select_lots(const SellerStock &stock,
                                        const StableId &item, Integer amount) {
  std::vector<CargoSelection> selected;
  for (const auto &lot : stock.inventory->lots) {
    if (!amount)
      break;
    if (lot.item_id != item || !owner_equal(lot.owner, stock.owner) ||
        !lot.reserved_by.empty())
      continue;
    const auto count = std::min(amount, lot.quantity);
    if (count > 0) {
      selected.push_back({stock.inventory->id, lot.id, count});
      amount -= count;
    }
  }
  if (amount)
    selected.clear();
  return selected;
}
} // namespace reserve_demand
using namespace reserve_demand;
Status validate_household_reserve_rules(
    const CommonStore &common, const CommonCatalog &,
    const LocalEconomyState &economy, const HouseholdPurchaseCatalog &catalog) {
  std::set<StableId> ids;
  std::set<std::tuple<StableId, StableId, StableId>> scopes;
  for (const auto &rule : catalog.household_reserve_rules) {
    const auto *household = economy.households.find(rule.household_id);
    const auto *inventory = common.inventory(rule.destination_inventory_id);
    if (rule.id.empty() || !ids.insert(rule.id).second ||
        !scopes
             .emplace(rule.household_id, rule.destination_inventory_id,
                      rule.item_id)
             .second ||
        !household || !inventory || !common.item_definition(rule.item_id) ||
        !owner_equal(inventory->owner,
                     {Owner::Kind::household, rule.household_id, {}}) ||
        rule.minimum_stock < 0 || rule.minimum_cash_reserve < 0)
      return common_failure("InvalidHouseholdReserveRule", rule.id);
  }
  return common_success();
}
Outcome<std::vector<HouseholdReserveDemand>>
read_household_reserve_demand(const HouseholdPurchaseContext &x,
                              const StableId &plan_id) {
  auto rules = validate_household_reserve_rules(x.common, x.shared, x.economy,
                                                x.catalog);
  if (!rules.ok)
    return {false, {}, rules.error};
  const auto *plan = x.state.plans.find(plan_id);
  const auto *fact = x.common.state().facts.find(plan_id);
  if (!plan || !fact || fact->domain != "local_economy" ||
      fact->kind != "household_purchase_planned")
    return fail<std::vector<HouseholdReserveDemand>>(
        "HouseholdPurchasePlanMissing", plan_id);
  const auto *household = x.economy.households.find(plan->household_id);
  const auto *wallet =
      household ? x.common.wallet(household->wallet_id) : nullptr;
  if (!wallet || !owner_equal(wallet->owner,
                              {Owner::Kind::household, plan->household_id, {}}))
    return fail<std::vector<HouseholdReserveDemand>>(
        "HouseholdDemandWalletMismatch");
  auto spendable = x.common.spendable(wallet->id);
  if (!spendable.ok)
    return {false, {}, spendable.error};
  std::vector<HouseholdReserveDemand> result;
  for (const auto &rule : x.catalog.household_reserve_rules) {
    if (rule.household_id != plan->household_id ||
        rule.destination_inventory_id != plan->destination_inventory)
      continue;
    const auto cap = std::find_if(
        plan->allowed_items.begin(), plan->allowed_items.end(),
        [&](const auto &row) { return row.item_id == rule.item_id; });
    if (cap == plan->allowed_items.end())
      continue;
    auto stock = x.common.count(rule.destination_inventory_id, rule.item_id,
                                {wallet->owner});
    if (!stock.ok)
      return {false, {}, stock.error};
    HouseholdReserveDemand demand;
    demand.reserve_rule_ref = {"local_economy", "household_reserve_rule",
                               rule.id};
    demand.plan_id = plan_id;
    demand.household_id = rule.household_id;
    demand.item_id = rule.item_id;
    demand.destination_inventory_id = rule.destination_inventory_id;
    demand.available_stock = stock.value;
    demand.minimum_stock = rule.minimum_stock;
    demand.minimum_cash_reserve = rule.minimum_cash_reserve;
    demand.available_cash_after_reserve =
        spendable.value > rule.minimum_cash_reserve
            ? spendable.value - rule.minimum_cash_reserve
            : 0;
    demand.versions = {
        x.common.read_version(
            {ResourceKey::Kind::inventory, rule.destination_inventory_id}),
        x.common.read_version({ResourceKey::Kind::wallet, wallet->id})};
    for (const auto &entry : x.state.purchases.rows()) {
      const auto &purchase = entry.second;
      if (purchase.state == HouseholdPurchasePhase::lost)
        continue;
      const auto *accepted =
          x.state.accepted_terms.find(purchase.accepted_terms_ref.id);
      if (!accepted || accepted->plan.household_id != rule.household_id ||
          accepted->plan.destination_inventory !=
              rule.destination_inventory_id ||
          accepted->item_id != rule.item_id)
        continue;
      auto receipt = read_household_retail_settlement(
          x.common, x.retail.state, purchase.retail_receipt_ref);
      if (!receipt.ok)
        return {false, {}, receipt.error};
      auto valid = x.retail.acceptances.validate_household_retail_settlement(
          *receipt.value);
      if (!valid.ok)
        return {false, {}, valid.error};
      auto cargo = read_household_purchase_cargo(x, purchase.purchase_id);
      if (!cargo.ok)
        return {false, {}, cargo.error};
      if (cargo.value.purchased_quantity != receipt.value->terms.quantity ||
          cargo.value.received_quantity > receipt.value->terms.quantity)
        return fail<std::vector<HouseholdReserveDemand>>(
            "HouseholdDemandHandoverMismatch", purchase.purchase_id);
      const auto pending =
          receipt.value->terms.quantity - cargo.value.received_quantity;
      if (pending) {
        if (!add(demand.owned_pending_delivery, pending))
          return fail<std::vector<HouseholdReserveDemand>>(
              "HouseholdDemandQuantityOverflow");
        demand.pending_retail_sources.push_back(
            {"commerce", "household_retail_settled", receipt.value->id});
      }
    }
    Integer owned = demand.available_stock;
    if (!add(owned, demand.owned_pending_delivery))
      return fail<std::vector<HouseholdReserveDemand>>(
          "HouseholdDemandQuantityOverflow");
    demand.quantity_needed =
        owned < rule.minimum_stock ? rule.minimum_stock - owned : 0;
    result.push_back(std::move(demand));
  }
  return {true, std::move(result), {}};
}
Outcome<std::optional<HouseholdReservePurchaseProposal>>
preview_household_reserve_purchase(
    const HouseholdPurchaseContext &x,
    const HouseholdPurchaseKnownMarketRead &knowledge, const StableId &plan_id,
    const HouseholdRetailSeller &seller) {
  auto demands = read_household_reserve_demand(x, plan_id);
  if (!demands.ok)
    return {false, {}, demands.error};
  const auto &plan = *x.state.plans.find(plan_id);
  auto allowance = read_household_purchase_allowance(
      x, plan.accepted_policy_ref, plan.accepted_policy_revision);
  if (!allowance.ok)
    return {false, {}, allowance.error};
  for (const auto &demand : demands.value) {
    if (const auto *chosen =
            std::get_if<PrivateHouseholdRetailSeller>(&seller)) {
      const auto *operation =
          x.retail.shops.operations.find(chosen->operation_id);
      if (!operation)
        return fail<std::optional<HouseholdReservePurchaseProposal>>(
            "HouseholdDemandOperationMissing");
      if (operation->item_id != demand.item_id)
        continue;
    }
    if (!demand.quantity_needed || !demand.available_cash_after_reserve)
      continue;
    auto stock = seller_stock(x, plan, seller, demand.item_id);
    if (!stock.ok)
      return {false, {}, stock.error};
    auto known = knowledge.read_known_household_market(
        plan, plan.known_market_ref, stock.value.inventory->id,
        stock.value.place_id, demand.item_id, x.now);
    if (!known.ok)
      return {false, {}, known.error};
    if (!valid_source_ref(known.value.report_ref) ||
        !valid_source_ref(known.value.observation_source) ||
        !household_market_knowledge_belongs_to_plan(known.value, plan))
      return fail<std::optional<HouseholdReservePurchaseProposal>>(
          "HouseholdDemandKnownSourceMissing");
    auto available = x.common.count(stock.value.inventory->id, demand.item_id,
                                    {stock.value.owner});
    if (!available.ok)
      return {false, {}, available.error};
    const auto cap = std::find_if(
        plan.allowed_items.begin(), plan.allowed_items.end(),
        [&](const auto &row) { return row.item_id == demand.item_id; });
    Integer quantity = std::min({demand.quantity_needed, cap->quantity_cap,
                                 available.value, Integer{500}});
    const auto money =
        std::min({demand.available_cash_after_reserve,
                  allowance.value.merchandise_available, plan.spend_cap_qian});
    if (money <= 0)
      continue;
    for (; quantity > 0; --quantity) {
      HouseholdRetailTerms request;
      request.seller_ref = seller;
      request.item_id = demand.item_id;
      request.quantity = quantity;
      request.selected_cargo =
          select_lots(stock.value, demand.item_id, quantity);
      if (request.selected_cargo.empty())
        return fail<std::optional<HouseholdReservePurchaseProposal>>(
            "HouseholdDemandSelectionMismatch");
      auto quote = preview_household_purchase(x, plan_id, request);
      if (!quote.ok) {
        const auto &code = quote.error.original.code;
        if (code == "InsufficientFunds" ||
            code == "HouseholdPurchaseBudgetExceeded" || code == "OverCapacity")
          continue;
        return {false, {}, quote.error};
      }
      if (quote.value.retail.terms.total > money)
        continue;
      return {true,
              HouseholdReservePurchaseProposal{demand, std::move(known.value),
                                               std::move(quote.value)},
              {}};
    }
  }
  return {true, std::nullopt, {}};
}
Outcome<StableId> accept_household_reserve_purchase_in_candidate(
    CommonStore &common, LocalEconomyState &economy,
    HouseholdPurchaseState &purchases, FacilityClaimsState &claims,
    ActionElapsedStore &elapsed, HouseholdRetailState &retail,
    const HouseholdPurchaseContext &x,
    const HouseholdPurchaseKnownMarketRead &knowledge,
    const HouseholdReservePurchaseProposal &proposal) {
  auto current = preview_household_reserve_purchase(
      x, knowledge, proposal.demand.plan_id,
      proposal.quote.retail.terms.seller_ref);
  if (!current.ok)
    return {false, {}, current.error};
  if (!current.value || !same_demand(current.value->demand, proposal.demand) ||
      !same_known(current.value->known_market, proposal.known_market) ||
      current.value->quote.retail.terms.item_id !=
          proposal.quote.retail.terms.item_id ||
      current.value->quote.retail.terms.quantity !=
          proposal.quote.retail.terms.quantity ||
      current.value->quote.retail.terms.total !=
          proposal.quote.retail.terms.total)
    return fail<StableId>("StaleHouseholdReserveProposal");
  return accept_household_purchase_in_candidate(
      common, economy, purchases, claims, elapsed, retail, x, proposal.quote);
}
} // namespace hanmo::resident

namespace hanmo::resident {
const StableId &
household_market_known_report_id(const HouseholdKnownMarketEvidence &v) {
  return std::visit(
      [](const auto &r) -> const StableId & { return r.report_id; },
      v.knowledge);
}
bool household_market_knowledge_belongs_to_plan(
    const HouseholdKnownMarketEvidence &v, const HouseholdPurchasePlan &p) {
  if ((v.selection != HouseholdMarketKnowledgeSelection::original_subject &&
       v.selection != HouseholdMarketKnowledgeSelection::accepted_plan_scope) ||
      v.report_ref.domain != "social" || v.report_ref.kind != "report" ||
      v.report_ref.id.empty())
    return false;
  if (const auto *named =
          std::get_if<NamedHouseholdBuyer>(&p.execution_choice)) {
    const auto *k = std::get_if<KnowledgeReceiptV2>(&v.knowledge);
    return k && k->receiver.kind == SubjectKind::actor &&
           k->receiver.id == named->actor_id && k->report_id == v.report_ref.id;
  }
  const auto &b = std::get<CohortHouseholdBuyer>(p.execution_choice);
  if (v.selection == HouseholdMarketKnowledgeSelection::original_subject) {
    const auto *k = std::get_if<KnowledgeReceiptV2>(&v.knowledge);
    return k && k->receiver.kind == SubjectKind::cohort &&
           k->receiver.id == b.cohort_id && k->report_id == v.report_ref.id;
  }
  if (v.selection != HouseholdMarketKnowledgeSelection::accepted_plan_scope)
    return false;
  const auto *k =
      std::get_if<HouseholdPurchaseScopedKnowledgeRef>(&v.knowledge);
  return k && k->actual_delivery.source_schema == 1 &&
         k->actual_delivery.source_ref.domain == "interaction" &&
         k->actual_delivery.source_ref.kind ==
             household_purchase_report_source_kind &&
         !k->actual_delivery.source_ref.id.empty() &&
         k->purchase_plan_ref.domain == "local_economy" &&
         k->purchase_plan_ref.kind == "household_purchase_planned" &&
         k->purchase_plan_ref.id == p.plan_id && k->cohort_id == b.cohort_id &&
         k->report_id == v.report_ref.id;
}
} // namespace hanmo::resident
