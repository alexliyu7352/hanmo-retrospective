#include "resident/kernel/catalog_section_observation.hpp"
#include "household_retail.hpp"
#include "private_shop_age3.hpp"
#include "private_shop_opening_source3.hpp"
// 每份租约沿本域实际头重新读取开设前资格、本人声明和原LE租金，未知动态状态保持硬拒。
#include "daily_rent/private_shop_daily_rent_financial.hpp"
#include "private_shop_closing.hpp"
#include "private_shop_operation.hpp"
#include "private_shop_operation_sources.hpp"
#include "private_shop_orders.hpp"
#include "private_shop_staff.hpp"
#include "resident/domains/growth/growth_model.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <map>
#include <set>
namespace hanmo::resident {
namespace {
const DialogueNode *node(const InteractionCatalog &c, const StableId &dialogue,
                         const StableId &id) {
  for (const auto &d : c.dialogues)
    if (d.id == dialogue)
      for (const auto &n : d.nodes)
        if (n.id == id)
          return &n;
  return nullptr;
}
bool same_client(const PrivateShopClientQualification &a,
                 const TaskFulfilledClientSource &b) {
  return owner_equal(a.original_client, b.original_client) &&
         a.delivery_beneficiary.kind == b.delivery_beneficiary.kind &&
         a.delivery_beneficiary.id == b.delivery_beneficiary.id &&
         a.task_id == b.task_id && a.settlement_id == b.settlement_id &&
         a.template_id == b.template_id &&
         a.payment_fact_id == b.payment_fact_id &&
         a.completed_tick == b.completed_tick &&
         a.objective_fact_ids == b.objective_fact_ids;
}
} // namespace
Status validate_private_shop_catalog(const PrivateShopContext &x) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 16, "PrivateShopContext", "validate_private_shop_catalog", &x);

  if (x.catalog.version == 3) {
    auto actual = read_private_shop_opening_context_moment3(x);
    if (!actual.ok)
      return {false, {}, actual.error};
  }
  if ((x.catalog.version != 1 && x.catalog.version != 2 &&
       x.catalog.version != 3) ||
      x.catalog.premises.empty() || !x.economy_catalog.enabled || !x.markets ||
      !x.trades || !x.markets->regional_life)
    return common_failure("PrivateShopProfileMismatch");
  for (const auto &row : x.catalog.premises.rows()) {
    const auto &p = row.second;
    const auto *role = x.common_catalog.roles.find(p.lessor_role_id);
    const auto *person = x.common.actor(p.lessor_actor_id);
    const auto *wallet = x.common.wallet(p.lessor_wallet_id);
    const auto *rate = x.economy_catalog.rates.find(p.rent_rate_profile);
    const auto *market = x.markets->shops.find(p.market_shop_id);
    const auto *agreement = node(x.interaction_catalog, p.agreement_dialogue_id,
                                 p.agreement_node_id);
    if (p.id != row.first || p.id.empty() ||
        !x.common_catalog.places.find(p.place_id) || !role ||
        role->initial_actor != p.lessor_actor_id ||
        role->place_id != p.place_id || role->organization_id ||
        !role->dialogue_id || *role->dialogue_id != p.agreement_dialogue_id ||
        !person || !wallet ||
        !owner_equal(wallet->owner,
                     {Owner::Kind::actor, p.lessor_actor_id, {}}) ||
        !market || market->place_id != p.place_id || p.opening_fee != 900 ||
        p.initial_capital != 300 || p.inventory_capacity != 200 ||
        p.daily_rent != 2 || !rate || rate->per_capita ||
        rate->numerator != p.daily_rent || rate->denominator != 1)
      return common_failure("InvalidPrivateShopPremises", row.first);
    if (!agreement || agreement->kind != "say" ||
        (agreement->speaker != "$provider" &&
         agreement->speaker != p.lessor_actor_id))
      return common_failure("PrivateShopLeaseDeclarationRequired", row.first);
  }
  return common_success();
}
Outcome<const PrivateShopRecord *>
read_original_private_shop_opening(const PrivateShopContext &x,
                                   const SourceRef &source) {
  auto rejected = [](const char *code,
                     const StableId &id) -> Outcome<const PrivateShopRecord *> {
    return {false, nullptr, common_failure(code, id).error};
  };
  if (source.domain != private_shop_module ||
      source.kind != private_shop_open_kind || source.id.empty())
    return rejected("PrivateShopOpeningSourceRequired", source.id);
  auto valid = validate_private_shop_catalog(x);
  if (!valid.ok)
    return {false, nullptr, valid.error};
  valid = validate_private_shop_opening_time_layout3(x.state);
  if (!valid.ok)
    return {false, nullptr, valid.error};
  if (x.state.version == 3) {
    auto actual = read_private_shop_opening_actual3(x, source);
    if (!actual.ok)
      return {false, nullptr, actual.error};
  }
  auto observed = private_shop_observed_tick(x);
  if (!observed.ok)
    return {false, nullptr, observed.error};
  const auto *original = x.state.shops.find(source.id);
  if (!original)
    return rejected("PrivateShopOpeningSourceRequired", source.id);
  const auto &s = *original;
  const auto &t = s.terms;
  const auto *p = x.catalog.premises.find(t.premises_id);
  const auto *head = x.common.state().facts.find(s.id);
  const auto id =
                 organization_instance_sequence(s.id, private_shop_open_prefix),
             rent_id = organization_instance_sequence(s.rent_obligation_id,
                                                      "private_shop_rent");
  const auto &order = x.common.state().facts.order();
  if (s.id != source.id || !p || !head || head->id != s.id ||
      head->kind != source.kind || head->domain != source.domain ||
      std::count(order.begin(), order.end(), s.id) != 1 || !id || !rent_id ||
      *id >= x.common.state().next_instance_id ||
      *rent_id >= x.common.state().next_instance_id || *rent_id != *id + 1 ||
      s.opened_tick < x.state.initialized_tick ||
      s.opened_tick > observed.value)
    return rejected("InvalidPrivateShopOpening", s.id);
  const auto *actor = x.common.actor(t.proprietor_actor_id);
  if (!actor)
    return rejected("PrivateShopProprietorMissing", s.id);
  PrivateShopOpeningTerms expected{p->id,
                                   p->place_id,
                                   actor->id,
                                   p->lessor_actor_id,
                                   actor->wallet_id,
                                   p->lessor_wallet_id,
                                   p->market_shop_id,
                                   p->opening_fee,
                                   p->initial_capital,
                                   p->inventory_capacity,
                                   p->daily_rent,
                                   p->rent_rate_profile};
  if (!private_shop_opening_terms_equal(t, expected) ||
      actor->id == p->lessor_actor_id)
    return rejected("PrivateShopTermsMismatch", s.id);
  const auto &m = s.meeting;
  if (m.actor_id != actor->id || m.provider_actor_id != p->lessor_actor_id ||
      m.session_id.empty() || m.dialogue_id != p->agreement_dialogue_id ||
      m.node_id != p->agreement_node_id || m.place_id != p->place_id ||
      m.role_id != p->lessor_role_id)
    return rejected("PrivateShopMeetingMismatch", s.id);
  auto age = read_private_shop_party_adult_before_fact(x, p->lessor_actor_id,
                                                       s.opened_tick, s.id);
  if (!age.ok)
    return {false, {}, age.error};
  if (!age.value.adult)
    return rejected("PrivateShopAdultRequired", s.id);
  PrivateShopOpeningBoundary boundary{s.id, s.opened_tick};
  auto history = read_private_shop_opening_eligibility(
      x.common, x.common_catalog, x.family_catalog, x.clients, x.permissions,
      *p, actor->id, &boundary, x.study_history, x.skill_origins, &x);
  if (!history.ok)
    return {false, {}, history.error};
  const auto &q = s.qualification;
  const auto &h = history.value;
  if (q.minimum_commerce_xp != h.minimum_commerce_xp ||
      q.minimum_commerce_level != h.minimum_commerce_level ||
      q.commerce_study_source_ids != h.commerce_study_source_ids ||
      q.permit_fact_id != h.permission.source().permit_fact_id ||
      q.clearance_fact_id != h.permission.source().clearance_fact_id ||
      q.original_clients.size() != h.original_clients.size())
    return rejected("PrivateShopQualificationMismatch", s.id);
  for (std::size_t i = 0; i < q.original_clients.size(); ++i) {
    auto completion = validate_private_shop_client_completion_source(
        x.common, h.original_clients[i], s.opened_tick, &boundary);
    if (!completion.ok)
      return {false, {}, completion.error};
    if (!same_client(q.original_clients[i], h.original_clients[i]))
      return rejected("PrivateShopQualificationMismatch", s.id);
  }
  return {true, original, {}};
}
Status validate_private_shops(const PrivateShopContext &x) {
  auto valid = validate_private_shop_catalog(x);
  if (!valid.ok)
    return valid;
  valid = validate_private_shop_opening_sources3(x);
  if (!valid.ok)
    return valid;
  valid = validate_private_shop_daily_rent_selection(x);
  if (!valid.ok)
    return valid;
  auto observed = private_shop_observed_tick(x);
  if (!observed.ok)
    return {false, {}, observed.error};
  const Integer limit = observed.value;
  if (x.state.version != x.catalog.version ||
      (x.exact_operations && x.state.version != 2 && x.state.version != 3) ||
      x.state.initialized_tick < 0 ||
      x.state.initialized_tick > x.common.tick() ||
      x.state.last_step_tick < x.state.initialized_tick ||
      x.state.last_step_tick > limit)
    return common_failure("PrivateShopClockMismatch");
  std::set<StableId> premises, proprietors, rents;
  std::set<Integer> allocated;
  std::set<StableId> seen_capital, seen_markups, seen_purchases, seen_trades,
      seen_closings, seen_labor;
  std::set<StableId> live_lots;
  for (const auto &inventory : x.common.state().inventories.rows())
    for (const auto &lot : inventory.second.lots)
      live_lots.insert(lot.id);
  for (const auto &row : x.state.shops.rows()) {
    const auto &s = row.second;
    const auto &t = s.terms;
    const auto *p = x.catalog.premises.find(t.premises_id);
    const auto *head = x.common.state().facts.find(s.id);
    const auto id =
        organization_instance_sequence(s.id, private_shop_open_prefix);
    const auto rent_id = organization_instance_sequence(s.rent_obligation_id,
                                                        "private_shop_rent");
    if (row.first != s.id || !p || !head || head->id != s.id ||
        head->kind != private_shop_open_kind ||
        head->domain != private_shop_module || !id || !rent_id ||
        *id >= x.common.state().next_instance_id ||
        *rent_id >= x.common.state().next_instance_id || *rent_id != *id + 1 ||
        !allocated.insert(*id).second || !allocated.insert(*rent_id).second ||
        s.opened_tick < x.state.initialized_tick || s.opened_tick > limit ||
        (s.status != "preparing" && s.status != "open" &&
         s.status != "suspended" && s.status != "closed"))
      return common_failure("InvalidPrivateShopOpening", s.id);
    if (!rents.insert(s.rent_obligation_id).second ||
        (s.status != "closed" &&
         (!premises.insert(t.premises_id).second ||
          !proprietors.insert(t.proprietor_actor_id).second)))
      return common_failure("PrivateShopAlreadyOccupied", s.id);
    for (const auto &prior : x.state.shops.rows())
      if (prior.first != s.id &&
          (prior.second.terms.proprietor_actor_id == t.proprietor_actor_id ||
           prior.second.terms.premises_id == t.premises_id)) {
        const auto &order = x.common.state().facts.order();
        const auto a = std::find(order.begin(), order.end(), prior.first),
                   b = std::find(order.begin(), order.end(), s.id);
        if (a != order.end() && b != order.end() && a < b) {
          const PrivateShopClosingRecord *closed = nullptr;
          for (const auto &row : x.state.closings.rows())
            if (row.second.shop_id == prior.first)
              closed = &row.second;
          if (!closed || std::find(order.begin(), order.end(), closed->id) >= b)
            return common_failure("PrivateShopOverlappingLeaseHistory", s.id);
        }
      }

    auto opening = read_original_private_shop_opening(
        x, {private_shop_module, private_shop_open_kind, s.id});
    if (!opening.ok)
      return {false, {}, opening.error};
    const auto *actor = x.common.actor(t.proprietor_actor_id);
    Integer capital = t.initial_capital, markup = 0, last_tick = s.opened_tick;
    StableId status = "preparing", last_rent;
    bool opened = false;
    std::map<StableId, std::pair<StableId, Integer>> expected_stock;
    std::map<StableId, PrivateShopCostBasis> costs;
    for (const auto &actual : x.common.state().facts.rows()) {
      if (actual.first == s.id) {
        opened = true;
        continue;
      }
      if (x.daily_rent) {
        auto rent_event =
            private_shop_rent_cash_effect(x, s, actual.first, capital);
        if (!rent_event.ok)
          return {false, {}, rent_event.error};
        if (rent_event.value) {
          const auto &f = *rent_event.value;
          if (!opened || f.tick < last_tick || f.tick > limit ||
              __builtin_add_overflow(capital, f.shop_cash_delta, &capital) ||
              capital < 0)
            return common_failure("PrivateShopRentCashHistoryMismatch",
                                  actual.first);
          for (const auto &source : f.allocated_sources) {
            const auto sequence =
                organization_instance_sequence(source.first, source.second);
            if (!sequence || *sequence >= x.common.state().next_instance_id ||
                !allocated.insert(*sequence).second)
              return common_failure("PrivateShopRentIdCollision", source.first);
          }
          if (f.status)
            status = *f.status;
          if (!f.last_local_payment_id.empty())
            last_rent = f.last_local_payment_id;
          last_tick = f.tick;
        }
      }
      if (const auto *funding = x.state.capital.find(actual.first)) {
        const auto &f = *funding;
        if (f.shop_id == s.id) {
          const auto sequence =
              organization_instance_sequence(f.id, "private_shop_capital");
          if (!opened || actual.first != f.id ||
              actual.second.kind != "private_shop_capital_added" ||
              actual.second.domain != private_shop_module || !sequence ||
              *sequence >= x.common.state().next_instance_id ||
              !allocated.insert(*sequence).second ||
              !seen_capital.insert(f.id).second ||
              f.actor_id != t.proprietor_actor_id ||
              f.from_wallet_id != t.payer_wallet_id ||
              f.to_wallet_id != s.wallet_id || f.tick < last_tick ||
              f.tick > limit || f.amount <= 0 ||
              __builtin_add_overflow(capital, f.amount, &capital))
            return common_failure("InvalidPrivateShopCapital", f.id);
          last_tick = f.tick;
        }
      }
      if (const auto *purchase = x.state.purchases.find(actual.first)) {
        const auto &f = *purchase;
        if (f.shop_id == s.id) {
          const auto number =
              organization_instance_sequence(f.id, "private_shop_stock");
          const auto trade_number =
              organization_instance_sequence(f.trade_fact_id, "action");
          const auto *head = x.common.state().facts.find(f.trade_fact_id);
          const auto source = x.trades->life_result(f.trade_fact_id);
          const auto *market = x.markets->shops.find(t.market_shop_id);
          const auto &order = x.common.state().facts.order();
          const auto origin =
                         std::find(order.begin(), order.end(), f.trade_fact_id),
                     edge = std::find(order.begin(), order.end(), f.id),
                     opening = std::find(order.begin(), order.end(), s.id);
          if (!opened || actual.first != f.id ||
              actual.second.kind != "private_shop_stock_received" ||
              actual.second.domain != private_shop_module || !number ||
              *number >= x.common.state().next_instance_id ||
              !allocated.insert(*number).second ||
              !seen_purchases.insert(f.id).second ||
              !seen_trades.insert(f.trade_fact_id).second || !trade_number ||
              *trade_number >= *number || !head || head->domain != "growth" ||
              head->kind != "life_activity" || origin <= opening ||
              origin >= edge || !source || source->fact_id != f.trade_fact_id ||
              source->action_id != f.trade_fact_id ||
              source->kind != LifeActivityKind::trade ||
              source->activity != "buy" ||
              source->actor_id != t.proprietor_actor_id ||
              source->start_tick != f.tick ||
              source->settlement_tick != f.tick ||
              source->quantity != f.quantity ||
              source->amount != f.purchase_cash ||
              source->item_id != f.item_id ||
              source->payer_wallet_id != s.wallet_id || !market ||
              f.market_shop_id != market->id ||
              source->payee_wallet_id != market->wallet_id ||
              source->from_inventory_id != market->inventory_id ||
              source->to_inventory_id != f.from_inventory_id ||
              f.from_inventory_id != actor->inventory_id ||
              f.to_inventory_id != s.inventory_id || f.tick < last_tick ||
              f.tick > limit || f.quantity <= 0 || f.purchase_cash <= 0 ||
              capital < f.purchase_cash)
            return common_failure("InvalidPrivateShopPurchase", f.id);
          Integer bought = 0, stocked = 0;
          for (const auto &lot : f.purchased_lots) {
            const auto serial =
                organization_instance_sequence(lot.lot_id, "lot");
            if (!serial || *serial <= *trade_number || *serial >= *number ||
                !allocated.insert(*serial).second ||
                live_lots.count(lot.lot_id) || lot.quantity <= 0 ||
                __builtin_add_overflow(bought, lot.quantity, &bought))
              return common_failure("PrivateShopPurchaseLotHistoryMismatch",
                                    f.id);
          }
          for (const auto &lot : f.stocked_lots) {
            const auto serial =
                organization_instance_sequence(lot.lot_id, "lot");
            if (!serial || *serial <= *number ||
                *serial >= x.common.state().next_instance_id ||
                !allocated.insert(*serial).second || lot.quantity <= 0 ||
                !expected_stock
                     .emplace(lot.lot_id,
                              std::make_pair(f.item_id, lot.quantity))
                     .second ||
                __builtin_add_overflow(stocked, lot.quantity, &stocked))
              return common_failure("PrivateShopStockLotHistoryMismatch", f.id);
          }
          if (bought != f.quantity || stocked != f.quantity)
            return common_failure("PrivateShopPurchaseQuantityMismatch", f.id);
          capital -= f.purchase_cash;
          last_tick = f.tick;
          costs.emplace(f.id, PrivateShopCostBasis{f.id, f.quantity,
                                                   f.purchase_cash, 0, 0, 0});
        }
      }
      if (const auto *operation = x.state.operations.find(actual.first)) {
        const auto &f = *operation;
        if (f.shop_id == s.id) {
          const auto sequence = organization_instance_sequence(f.id, "action"),
                     due = organization_instance_sequence(f.rent_arrear_id,
                                                          "local_due"),
                     paid = organization_instance_sequence(f.rent_payment_id,
                                                           "local_payment");
          if (!opened || !sequence || !allocated.insert(*sequence).second ||
              f.start_tick < last_tick || f.markup_basis_points != markup)
            return common_failure("PrivateShopOperationHistoryMismatch", f.id);
          if (x.daily_rent) {
            if (!f.rent_arrear_id.empty() || !f.rent_payment_id.empty())
              return common_failure("PrivateShopDuplicateDailyRent", f.id);
          } else {
            if (!due || !paid || !allocated.insert(*due).second ||
                !allocated.insert(*paid).second || capital < t.daily_rent)
              return common_failure("PrivateShopOperationHistoryMismatch",
                                    f.id);
            capital -= t.daily_rent;
            last_rent = f.rent_payment_id;
          }
          last_tick = f.start_tick;
          status = "open";
        }
      }
      if (const auto *service = x.state.staff.services.find(actual.first)) {
        const auto *contract =
            x.state.staff.contracts.find(service->contract_id);
        if (contract && contract->terms.shop_id == s.id) {
          const auto id = organization_instance_sequence(
                         service->id, "private_shop_staff_service"),
                     due = organization_instance_sequence(service->arrear_id,
                                                          "local_due"),
                     paid = organization_instance_sequence(service->payment_id,
                                                           "local_payment");
          if (!opened || !id || !due || !paid ||
              !allocated.insert(*id).second || !allocated.insert(*due).second ||
              !allocated.insert(*paid).second || service->tick < last_tick ||
              service->tick > limit || service->amount <= 0 ||
              capital < service->amount)
            return common_failure("PrivateShopStaffCashHistoryMismatch",
                                  service->id);
          capital -= service->amount;
          last_tick = service->tick;
        }
      }
      if (const auto *result = x.state.operating_results.find(actual.first)) {
        const auto &f = *result;
        if (f.shop_id == s.id) {
          const auto sequence =
              organization_instance_sequence(f.id, "private_shop_day");
          if (!opened || !sequence || !allocated.insert(*sequence).second ||
              f.tick < last_tick || f.tick > limit || f.income < 0 ||
              (!household_retail_operation_selected(x, f.operation_id) &&
               __builtin_add_overflow(capital, f.income, &capital)))
            return common_failure("PrivateShopOperatingHistoryMismatch", f.id);
          if (!household_retail_operation_selected(x, f.operation_id))
            for (const auto &sold : f.consumed_lots) {
              auto stock = expected_stock.find(sold.lot_id);
              auto basis = costs.find(sold.purchase_fact_id);
              const auto *purchase =
                  x.state.purchases.find(sold.purchase_fact_id);
              bool original = false;
              if (purchase)
                for (const auto &lot : purchase->stocked_lots)
                  original |= lot.lot_id == sold.lot_id;
              if (!purchase || purchase->shop_id != s.id ||
                  purchase->item_id != f.item_id || !original ||
                  basis == costs.end() || stock == expected_stock.end() ||
                  stock->second.first != f.item_id || sold.quantity <= 0 ||
                  sold.quantity > stock->second.second)
                return common_failure("PrivateShopSoldLotSourceMismatch", f.id);
              auto cost =
                  quote_private_shop_cost_slice(basis->second, sold.quantity);
              if (!cost.ok)
                return {false, {}, cost.error};
              if (cost.value.cost != sold.cost)
                return common_failure("PrivateShopSoldCostMismatch", f.id);
              basis->second.sold_quantity = cost.value.sold_quantity_after;
              basis->second.recognized_cost = cost.value.recognized_cost_after;
              stock->second.second -= sold.quantity;
              if (stock->second.second == 0)
                expected_stock.erase(stock);
            }
          last_tick = f.tick;
          status = f.completed && (x.daily_rent || capital >= t.daily_rent)
                       ? "open"
                       : "suspended";
        }
      }
      if (x.retail)
        if (const auto *retail = x.retail->settlements.find(actual.first)) {
          const auto *seller = std::get_if<PrivateHouseholdRetailSeller>(
              &retail->terms.seller_ref);
          if (seller && seller->shop_id == s.id) {
            const auto &f = *retail;
            const auto sequence =
                organization_instance_sequence(f.id, "household_retail");
            if (!opened || !household_retail_selected(x, s.id) ||
                actual.second.domain != "commerce" ||
                actual.second.kind != "household_retail_settled" || !sequence ||
                *sequence >= x.common.state().next_instance_id ||
                !allocated.insert(*sequence).second ||
                f.settled_at.half_day_tick < last_tick ||
                f.settled_at.half_day_tick > limit ||
                f.terms.seller_wallet_id != s.wallet_id ||
                f.terms.source_inventory_id != s.inventory_id ||
                f.terms.total < 0 || f.payee_balance_before != capital ||
                __builtin_add_overflow(capital, f.terms.total, &capital) ||
                f.payee_balance_after != capital)
              return common_failure("PrivateShopHouseholdCashHistoryMismatch",
                                    f.id);
            for (const auto &sold : f.consumed_lots) {
              auto stock = expected_stock.find(sold.lot_id);
              auto basis = costs.find(sold.purchase_fact_id);
              const auto *purchase =
                  x.state.purchases.find(sold.purchase_fact_id);
              bool original = false;
              if (purchase)
                for (const auto &lot : purchase->stocked_lots)
                  original |= lot.lot_id == sold.lot_id;
              if (!purchase || purchase->shop_id != s.id ||
                  purchase->item_id != f.terms.item_id || !original ||
                  basis == costs.end() || stock == expected_stock.end() ||
                  stock->second.first != f.terms.item_id ||
                  sold.quantity <= 0 || sold.quantity > stock->second.second)
                return common_failure("PrivateShopHouseholdSoldLotMismatch",
                                      f.id);
              auto cost =
                  quote_private_shop_cost_slice(basis->second, sold.quantity);
              if (!cost.ok)
                return {false, {}, cost.error};
              if (cost.value.cost != sold.cost)
                return common_failure("PrivateShopHouseholdSoldCostMismatch",
                                      f.id);
              basis->second.sold_quantity = cost.value.sold_quantity_after;
              basis->second.recognized_cost = cost.value.recognized_cost_after;
              stock->second.second -= sold.quantity;
              if (stock->second.second == 0)
                expected_stock.erase(stock);
            }
            last_tick = f.settled_at.half_day_tick;
          }
        }
      if (const auto *settlement =
              x.state.order_settlements.find(actual.first)) {
        const auto *order = x.state.orders.orders.find(settlement->order_id);
        if (order && order->terms.shop_id == s.id) {
          const auto &f = *settlement;
          const auto id = organization_instance_sequence(
              f.id, "private_shop_order_settlement");
          if (!opened || !id || !allocated.insert(*id).second ||
              f.tick < last_tick || f.tick > limit || f.income < 0 ||
              __builtin_add_overflow(capital, f.income, &capital))
            return common_failure("PrivateShopOrderCashHistoryMismatch", f.id);
          for (const auto &sold : f.consumed_lots) {
            auto stock = expected_stock.find(sold.lot_id);
            auto basis = costs.find(sold.purchase_fact_id);
            const auto *purchase =
                x.state.purchases.find(sold.purchase_fact_id);
            bool original = false;
            if (purchase)
              for (const auto &lot : purchase->stocked_lots)
                original |= lot.lot_id == sold.lot_id;
            if (!purchase || purchase->shop_id != s.id ||
                purchase->item_id != order->terms.item_id || !original ||
                basis == costs.end() || stock == expected_stock.end() ||
                stock->second.first != order->terms.item_id ||
                sold.quantity <= 0 || sold.quantity > stock->second.second)
              return common_failure("PrivateShopOrderSoldLotSourceMismatch",
                                    f.id);
            auto cost =
                quote_private_shop_cost_slice(basis->second, sold.quantity);
            if (!cost.ok)
              return {false, {}, cost.error};
            if (cost.value.cost != sold.cost)
              return common_failure("PrivateShopOrderSoldCostMismatch", f.id);
            basis->second.sold_quantity = cost.value.sold_quantity_after;
            basis->second.recognized_cost = cost.value.recognized_cost_after;
            stock->second.second -= sold.quantity;
            if (stock->second.second == 0)
              expected_stock.erase(stock);
          }
          last_tick = f.tick;
        }
      }
      if (const auto *price = x.state.markups.find(actual.first)) {
        const auto &f = *price;
        if (f.shop_id == s.id) {
          const auto sequence =
              organization_instance_sequence(f.id, "private_shop_markup");
          if (!opened || actual.first != f.id ||
              actual.second.kind != "private_shop_markup_changed" ||
              actual.second.domain != private_shop_module || !sequence ||
              *sequence >= x.common.state().next_instance_id ||
              !allocated.insert(*sequence).second ||
              !seen_markups.insert(f.id).second ||
              f.actor_id != t.proprietor_actor_id || f.tick < last_tick ||
              f.tick > limit || f.before_basis_points != markup ||
              f.after_basis_points == markup ||
              (f.after_basis_points != 0 && f.after_basis_points != 500 &&
               f.after_basis_points != 1000))
            return common_failure("InvalidPrivateShopMarkup", f.id);
          markup = f.after_basis_points;
          last_tick = f.tick;
        }
      }
      if (const auto *closed = x.state.closings.find(actual.first)) {
        const auto &f = *closed;
        if (f.shop_id == s.id) {
          const auto number =
              organization_instance_sequence(f.id, "private_shop_closed");
          if (!opened || actual.first != f.id ||
              actual.second.kind != "private_shop_closed" ||
              actual.second.domain != private_shop_module || !number ||
              *number >= x.common.state().next_instance_id ||
              !allocated.insert(*number).second ||
              !seen_closings.insert(f.id).second || f.tick < last_tick ||
              f.tick > limit || f.returned_cash != capital ||
              f.returned_lots.size() != expected_stock.size())
            return common_failure("PrivateShopClosingAssetHistoryMismatch",
                                  f.id);
          Integer returned_index = 0;
          std::set<StableId> returned;
          for (const auto &lot : f.returned_lots) {
            const auto original = expected_stock.find(lot.original_lot_id);
            const auto n = organization_instance_sequence(
                           lot.destination_lot_id, "lot"),
                       old = organization_instance_sequence(lot.original_lot_id,
                                                            "lot");
            if (original == expected_stock.end() ||
                original->second.first != lot.item_id ||
                original->second.second != lot.quantity || lot.quantity <= 0 ||
                !returned.insert(lot.original_lot_id).second || !n || !old ||
                Integer(f.returned_lots.size()) >= *number ||
                *n != *number - Integer(f.returned_lots.size()) +
                          returned_index++ ||
                *old >= *number - Integer(f.returned_lots.size()) ||
                !allocated.insert(*n).second ||
                live_lots.count(lot.original_lot_id))
              return common_failure("PrivateShopClosingLotHistoryMismatch",
                                    f.id);
          }
          expected_stock.clear();
          capital = 0;
          last_tick = f.tick;
          status = "closed";
          opened = false;
        }
      }
    }
    if (s.status != status)
      return common_failure("PrivateShopStatusHistoryMismatch", s.id);
    if (s.markup_basis_points != markup)
      return common_failure("PrivateShopMarkupHistoryMismatch", s.id);
    const auto *wallet = x.common.wallet(s.wallet_id);
    const auto *inventory = x.common.inventory(s.inventory_id);
    if (s.wallet_id != "private_shop_wallet:" + std::to_string(*id) ||
        s.inventory_id != "private_shop_inventory:" + std::to_string(*id) ||
        !wallet || !inventory ||
        !owner_equal(wallet->owner, {Owner::Kind::actor, actor->id, {}}) ||
        !owner_equal(inventory->owner, wallet->owner) ||
        inventory->capacity != t.inventory_capacity ||
        wallet->balance != capital ||
        inventory->lots.size() != expected_stock.size())
      return common_failure("PrivateShopAssetsMismatch", s.id);
    for (const auto &hold : wallet->reserved) {
      const auto *operation = x.state.operations.find(hold.action_id.id);
      if (hold.action_id.kind != ReservationRef::Kind::action ||
          hold.amount <= 0 || !operation || operation->shop_id != s.id ||
          operation->staff_contract_id.empty() ||
          !operation->result_fact_id.empty())
        return common_failure("PrivateShopUnknownReservation", s.id);
    }
    for (const auto &lot : inventory->lots) {
      const auto expected = expected_stock.find(lot.id);
      if (expected == expected_stock.end() ||
          lot.item_id != expected->second.first ||
          lot.quantity != expected->second.second ||
          !owner_equal(lot.owner, wallet->owner) || !lot.reserved_by.empty())
        return common_failure("PrivateShopCurrentStockMismatch", s.id);
    }
    const auto *rent = x.economy.obligations.find(s.rent_obligation_id);
    const auto *remainder =
        x.economy.obligation_remainders.find(s.rent_obligation_id);
    Integer due = 0;
    if (x.common.state().facts.find(s.rent_obligation_id) ||
        __builtin_add_overflow(s.opened_tick, Integer{2}, &due) || !rent ||
        rent->id != s.rent_obligation_id ||
        rent->principal.kind != SubjectKind::actor ||
        rent->principal.id != actor->id ||
        rent->beneficiary.kind != SubjectKind::actor ||
        rent->beneficiary.id != t.lessor_actor_id ||
        rent->kind != ObligationKind::contract_payment ||
        rent->source_wallet_or_inventory != s.wallet_id ||
        rent->destination != t.lessor_wallet_id ||
        rent->rate_profile != t.rent_rate_profile ||
        rent->authority_ref != s.id || rent->period_ticks != 2 ||
        rent->next_due_tick != due || rent->active || !rent->item_id.empty() ||
        rent->last_receipt_id != last_rent || !remainder || *remainder != 0)
      return common_failure("PrivateShopRentMismatch", s.id);
  }
  if (x.exact_operations)
    for (const auto &labor :
         x.exact_operations->sources.labor_applied_sources) {
      auto original = read_original_private_shop_labor(
          *x.exact_operations,
          {private_shop_labor_applied_source(labor.id), 1});
      if (!original.ok)
        return {false, {}, original.error};
      auto id = organization_instance_sequence(labor.id, "private_shop_labor");
      if (!id || *id >= x.common.state().next_instance_id ||
          !allocated.insert(*id).second || !seen_labor.insert(labor.id).second)
        return common_failure("PrivateShopLaborIdCollision", labor.id);
    }
  if (seen_capital.size() != x.state.capital.size() ||
      seen_markups.size() != x.state.markups.size() ||
      seen_purchases.size() != x.state.purchases.size() ||
      seen_closings.size() != x.state.closings.size())
    return common_failure("PrivateShopManagementRegistryMismatch");
  for (const auto &row : x.common.state().facts.rows())
    if (row.second.domain == private_shop_module &&
        !x.state.shops.find(row.first) && !seen_capital.count(row.first) &&
        !seen_markups.count(row.first) && !seen_purchases.count(row.first) &&
        !x.state.operations.find(row.first) &&
        !x.state.operating_results.find(row.first) &&
        !x.state.staff.contracts.find(row.first) &&
        !x.state.staff.decisions.find(row.first) &&
        !x.state.staff.services.find(row.first) &&
        !x.state.orders.orders.find(row.first) &&
        !x.state.orders.bindings.find(row.first) &&
        !x.state.order_settlements.find(row.first) &&
        !x.state.closings.find(row.first) && !seen_labor.count(row.first))
      return common_failure("MissingPrivateShopFact", row.first);
  for (const auto &row : x.common.state().wallets.rows())
    if (row.first.rfind("private_shop_wallet:", 0) == 0) {
      bool owned = false;
      for (const auto &shop : x.state.shops.rows())
        owned |= shop.second.wallet_id == row.first;
      if (!owned)
        return common_failure("UnboundPrivateShopWallet", row.first);
    }
  for (const auto &row : x.common.state().inventories.rows())
    if (row.first.rfind("private_shop_inventory:", 0) == 0) {
      bool owned = false;
      for (const auto &shop : x.state.shops.rows())
        owned |= shop.second.inventory_id == row.first;
      if (!owned)
        return common_failure("UnboundPrivateShopInventory", row.first);
    }
  for (const auto &row : x.economy.obligations.rows())
    if (row.second.authority_ref.rfind("private_shop:", 0) == 0 &&
        !rents.count(row.first))
      return common_failure("UnboundPrivateShopRent", row.first);
  if (!x.catalog.staff.policies.empty() || !x.state.staff.contracts.empty() ||
      !x.state.staff.decisions.empty() || !x.state.staff.services.empty()) {
    if (!x.estates || !x.estate_access)
      return common_failure("PrivateShopLaborSourceUnavailable");
    PrivateShopStaffContext staff{
        x,          x.catalog.staff,     x.state.staff,
        *x.estates, x.estate_employment, x.estate_access};
    auto valid = validate_private_shop_staff(staff);
    if (!valid.ok)
      return valid;
    for (const auto &row : x.state.staff.contracts.rows()) {
      auto id = organization_instance_sequence(row.first, "private_shop_staff"),
           wage = organization_instance_sequence(row.second.obligation_id,
                                                 "private_shop_wage");
      if (!id || !wage || !allocated.insert(*id).second ||
          !allocated.insert(*wage).second)
        return common_failure("PrivateShopStaffIdCollision", row.first);
    }
    for (const auto &row : x.state.staff.decisions.rows()) {
      auto id = organization_instance_sequence(row.first,
                                               "private_shop_staff_decision");
      if (!id || !allocated.insert(*id).second)
        return common_failure("PrivateShopStaffIdCollision", row.first);
    }
  }
  auto orders = validate_private_shop_orders(x);
  if (!orders.ok)
    return orders;
  for (const auto &row : x.state.orders.orders.rows()) {
    auto id = organization_instance_sequence(row.first, "private_shop_order");
    if (!id || !allocated.insert(*id).second)
      return common_failure("PrivateShopOrderIdCollision", row.first);
  }
  for (const auto &row : x.state.orders.bindings.rows()) {
    auto id =
        organization_instance_sequence(row.first, "private_shop_order_bound");
    if (!id || !allocated.insert(*id).second)
      return common_failure("PrivateShopOrderIdCollision", row.first);
  }
  auto operations = validate_private_shop_operations(x);
  if (!operations.ok)
    return operations;
  return validate_private_shop_closings(x);
}
} // namespace hanmo::resident
