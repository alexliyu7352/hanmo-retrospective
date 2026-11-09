// 先付具名日租再占真实Action；完成才把精确购入批次卖给民间，失败由原World候选完整回滚。
#include "private_shop_operation.hpp"
#include "daily_rent/private_shop_daily_rent_financial.hpp"
#include "household_retail.hpp"
#include "private_shop_action_time.hpp"
#include "private_shop_capability.hpp"
#include "private_shop_exact_service.hpp"
#include "private_shop_market_contract.hpp"
#include "private_shop_staff.hpp"
#include "resident/domains/actor_foundation/condition_source_read.hpp"
#include "resident/domains/family/actor_death_read.hpp"
#include "resident/domains/growth/growth_market_read.hpp"
#include "resident/domains/growth/growth_refs.hpp"
#include "resident/domains/local_economy/local_economy_internal.hpp"
#include "resident/kernel/command_types.hpp"
#include "resident/kernel/common_command_sources.hpp"
#include "resident/kernel/fixed_service_history.hpp"
#include "resident/storage/money_transfer_recipe.hpp"
#include <map>
namespace hanmo::resident {
namespace {
using local_economy_detail::CandidateStateGuard;
template <class T> Outcome<T> fail(const StableId &code) {
  return {false, {}, common_failure(code).error};
}
bool identity(const WorldIdentity &a, const WorldIdentity &b) {
  return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch &&
         a.revision == b.revision && a.tick == b.tick;
}
Outcome<StableId> fact(CommonStore &c, const StableId &prefix,
                       const StableId &kind,
                       const StableId &domain = private_shop_module) {
  auto id = c.next_id(prefix);
  if (!id.ok)
    return id;
  auto saved = c.append_fact({id.value, kind, domain});
  return saved.ok ? id : Outcome<StableId>{false, {}, saved.error};
}
bool versions_equal(const std::vector<ReadVersion> &a,
                    const std::vector<ReadVersion> &b) {
  if (a.size() != b.size())
    return false;
  for (std::size_t i = 0; i < a.size(); ++i)
    if (a[i].resource.kind != b[i].resource.kind ||
        a[i].resource.id != b[i].resource.id || a[i].version != b[i].version)
      return false;
  return true;
}
Status person_present(const PrivateShopContext &x,
                      const PrivateShopRecord &shop, const StableId &actor,
                      const StableId &action = {}) {
  const auto *p = x.common.state().actors.find(actor);
  if (!p || !p->alive || !p->available || p->action_id != action ||
      x.activity.in_open_battle(actor) ||
      (action.empty() && x.activity.in_interaction(actor)))
    return common_failure(
        "PrivateShopAttendanceUnavailable"); // 原店员伤势未愈时不新增到岗；本人经营规则仍交原公共准入，旧工时不按今日伤势倒删。
  bool purpose_selected = x.exact_operations && action.empty();
  if (x.exact_operations && !action.empty())
    for (const auto &a : x.exact_operations->sources.accepted_operations)
      if (a.operation_ref == action)
        purpose_selected = true;
  if (purpose_selected) {
    const auto &actual = *x.exact_operations;
    auto body = read_current_character_condition(actual.body, actor);
    if (!body.ok)
      return {false, {}, body.error};
    auto capability = read_character_capability_at_head(
        actual.body, body.value.head,
        {"private_shop_operation", 1,
         actual.body.catalog.identity.content_digest},
        std::nullopt, actual.body.actual_now);
    if (!capability.ok)
      return {false, {}, capability.error};
    if (!capability.value.allowed || capability.value.efficiency_bp <= 0)
      return common_failure("PrivateShopPurposeUnavailable");
  } else if (actor != shop.terms.proprietor_actor_id && p->injury > 0)
    return common_failure("PrivateShopStaffInjured");
  auto location = x.common.resolved_location(actor);
  if (!location.ok)
    return {false, {}, location.error};
  const auto *at = std::get_if<SettlementLocation>(&location.value);
  return at && at->place_id == shop.terms.place_id
             ? common_success()
             : common_failure("PrivateShopPresenceRequired");
}
// 原LE应付/付款是唯一租金账；此入口只允许已经重报价的本店本人租约，绝不伪造机构许可。
Status pay_rent(CommonStore &c, LocalEconomyState &e,
                const PrivateShopRecord &shop,
                PrivateShopOperation &operation) {
  const auto *obligation = e.obligations.find(shop.rent_obligation_id);
  const auto *payer = c.wallet(shop.wallet_id);
  const auto *payee = c.wallet(shop.terms.lessor_wallet_id);
  if (!obligation || !payer || !payee ||
      !owner_equal(payer->owner,
                   {Owner::Kind::actor, shop.terms.proprietor_actor_id, {}}) ||
      !owner_equal(payee->owner,
                   {Owner::Kind::actor, shop.terms.lessor_actor_id, {}}))
    return common_failure("PrivateShopRentSourceMismatch");
  auto due = fact(c, "local_due", "contract_payment_due", "local_economy");
  if (!due.ok)
    return {false, {}, due.error};
  if (!e.arrears.insert(due.value, {due.value, obligation->id, c.tick(),
                                    shop.terms.daily_rent, 0}))
    return common_failure("PrivateShopDuplicateRent");
  ResourceRecipe recipe;
  auto paid = resource_recipe::append_money_transfer(
      recipe, payer->id, payee->id, shop.terms.daily_rent,
      resource_recipe::MoneyTransferPolicy::spendable_debit_original_credit);
  if (!paid.ok)
    return paid;
  paid = c.apply_resources(recipe);
  if (!paid.ok)
    return paid;
  auto payment =
      fact(c, "local_payment", "obligation_payment", "local_economy");
  if (!payment.ok)
    return {false, {}, payment.error};
  if (!e.payments.insert(payment.value,
                         {payment.value, due.value, obligation->id, payer->id,
                          payee->id, c.tick(), c.tick(), shop.terms.daily_rent,
                          shop.id}))
    return common_failure("PrivateShopDuplicateRent");
  e.arrears.find(due.value)->paid_amount = shop.terms.daily_rent;
  e.obligations.find(shop.rent_obligation_id)->last_receipt_id = payment.value;
  operation.rent_arrear_id = due.value;
  operation.rent_payment_id = payment.value;
  return common_success();
}
// 已售成本只从原完成日推导；库存批次本身没有另一套可花数量或余额。
PrivateShopCostBasis basis_for(const PrivateShopContext &x,
                               const PrivateShopPurchaseFact &purchase) {
  PrivateShopCostBasis out{
      purchase.id, purchase.quantity, purchase.purchase_cash, 0, 0, 0};
  for (const auto &fact : x.common.state().facts.rows())
    if (const auto *rows = private_shop_actual_cost_lots(x, fact.first))
      for (const auto &lot : *rows)
        if (lot.purchase_fact_id == purchase.id) {
          out.sold_quantity += lot.quantity;
          out.recognized_cost += lot.cost;
        }
  return out;
}
Outcome<StableId> finish(CommonStore &c, LocalEconomyState &economy,
                         PrivateShopState &state, const PrivateShopContext &x,
                         const StableId &action_id, Integer tick,
                         bool completed, const StableId &reason) {
  auto *operation = state.operations.find(action_id);
  if (!operation || !operation->result_fact_id.empty())
    return fail<StableId>("PrivateShopOperationAlreadyFinished");
  const auto *shop = state.shops.find(operation->shop_id);
  if (!shop)
    return fail<StableId>("UnknownPrivateShop");
  PrivateShopDayResult result;
  result.operation_id = action_id;
  result.shop_id = shop->id;
  result.actor_id = operation->actor_id;
  result.item_id = operation->item_id;
  result.tick = tick;
  result.completed = completed;
  result.reason = reason;
  result.rent_paid = x.daily_rent ? 0 : shop->terms.daily_rent;
  auto wages =
      finish_private_shop_staff_day(c, economy, state, x, action_id, tick);
  if (!wages.ok)
    return {false, {}, wages.error};
  result.wage_paid = wages.value;
  ResourceRecipe assets;
  if (household_retail_operation_selected(x, action_id)) {
    auto summarized =
        summarize_household_retail_operation(x, action_id, result);
    if (!summarized.ok)
      return {false, {}, summarized.error};
  } else if (completed) {
    auto market = read_growth_market_midpoint(c, *x.markets, x.common_catalog,
                                              shop->terms.market_shop_id,
                                              operation->item_id);
    if (!market.ok)
      return {false, {}, market.error};
    auto rate = quote_private_shop_retail_rate(market.value.midpoint,
                                               operation->markup_basis_points);
    if (!rate.ok)
      return {false, {}, rate.error};
    result.market_observed_tick = market.value.observed_tick;
    result.market_revision = market.value.market_revision;
    result.market_stock = market.value.available_stock;
    result.base_price = market.value.base_price;
    result.midpoint = market.value.midpoint;
    const auto *inventory = c.inventory(shop->inventory_id);
    Integer remaining = rate.value.daily_quantity_limit;
    AdjustLots consumed;
    consumed.inventory_id = shop->inventory_id;
    for (const auto &row : state.purchases.rows()) {
      const auto &purchase = row.second;
      if (purchase.shop_id != shop->id ||
          purchase.item_id != operation->item_id)
        continue;
      auto basis = basis_for(x, purchase);
      for (const auto &original : purchase.stocked_lots) {
        if (!remaining)
          break;
        const auto found = std::find_if(
            inventory->lots.begin(), inventory->lots.end(),
            [&](const auto &lot) { return lot.id == original.lot_id; });
        if (found == inventory->lots.end())
          continue;
        const Integer amount = std::min(remaining, found->quantity);
        auto cost = quote_private_shop_cost_slice(basis, amount);
        if (!cost.ok)
          return {false, {}, cost.error};
        basis.sold_quantity = cost.value.sold_quantity_after;
        basis.recognized_cost = cost.value.recognized_cost_after;
        result.consumed_lots.push_back(
            {purchase.id, found->id, amount, cost.value.cost});
        consumed.lots.push_back({found->id, -amount});
        remaining -= amount;
        result.quantity += amount;
        if (__builtin_add_overflow(result.cost_of_goods, cost.value.cost,
                                   &result.cost_of_goods))
          return fail<StableId>("PrivateShopCostOverflow");
      }
    }
    if (__builtin_mul_overflow(result.quantity, rate.value.unit_price,
                               &result.income))
      return fail<StableId>("PrivateShopRetailOverflow");
    if (!consumed.lots.empty())
      assets.operations.push_back(consumed);
    if (result.income)
      assets.operations.push_back(
          WalletDelta{shop->wallet_id, result.income, false});
  }
  Integer expenses = 0;
  if (__builtin_add_overflow(result.cost_of_goods, result.rent_paid,
                             &expenses) ||
      __builtin_add_overflow(expenses, result.wage_paid, &expenses) ||
      __builtin_sub_overflow(result.income, expenses, &result.net_income))
    return fail<StableId>("PrivateShopOperatingOverflow");
  auto id = fact(c, "private_shop_day",
                 completed ? "private_shop_operating_completed"
                           : "private_shop_operating_interrupted");
  if (!id.ok)
    return id;
  result.id = id.value;
  auto changed = c.apply_resources(assets);
  if (!changed.ok)
    return {false, {}, changed.error};
  if (!state.operating_results.insert(id.value, result))
    return fail<StableId>("PrivateShopDuplicateOperatingResult");
  state.operations.find(action_id)->result_fact_id = id.value;
  ActionMutation erase;
  erase.kind = ActionMutation::Kind::erase;
  erase.action.id = action_id;
  changed = c.apply_action(erase);
  if (!changed.ok)
    return {false, {}, changed.error};
  ActorMutation free;
  free.actor_id = result.actor_id;
  free.action_id = StableId{};
  changed = c.apply_actor(free);
  if (!changed.ok)
    return {false, {}, changed.error};
  auto funds = c.spendable(shop->wallet_id);
  if (!funds.ok)
    return {false, {}, funds.error};
  state.shops.find(result.shop_id)->status =
      completed && (x.daily_rent || funds.value >= shop->terms.daily_rent)
          ? "open"
          : "suspended";
  return id;
}
} // namespace
Outcome<Integer> private_shop_observed_tick(const PrivateShopContext &x) {
  if (x.catalog.version == 3 || x.state.version == 3) {
    auto actual = read_private_shop_observed_moment3(x);
    if (!actual.ok)
      return {false, 0, actual.error};
    return {true, actual.value.half_day_tick, {}};
  }
  Integer tick = x.common.tick();
  if (x.phase) {
    if (tick == INT64_MAX || x.phase->from_tick != tick ||
        x.phase->to_tick != tick + 1)
      return {false, 0, common_failure("PrivateShopPhaseMismatch").error};
    tick = x.phase->to_tick;
  }
  return {true, tick, {}};
}
Outcome<PrivateShopOperationQuote>
preview_private_shop_operation(const PrivateShopContext &x,
                               const StableId &actor,
                               const PrivateShopOperate &command) {
  if (x.phase || !validate_private_shop_committed_read3(x).ok)
    return fail<PrivateShopOperationQuote>(
        "PrivateShopOperationRequiresCommittedClock");
  auto valid = validate_private_shops(x);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto *shop = x.state.shops.find(command.shop_id);
  if (!shop)
    return fail<PrivateShopOperationQuote>("UnknownPrivateShop");
  auto staff = private_shop_operation_staff(x, *shop, actor);
  if (!staff.ok)
    return {false, {}, staff.error};
  if (!staff.value.empty() &&
      x.state.staff.contracts.find(staff.value)->terms.service_item_id !=
          command.item_id)
    return fail<PrivateShopOperationQuote>(
        "PrivateShopStaffServiceItemMismatch");
  valid = person_present(x, *shop, actor);
  if (!valid.ok)
    return {false, {}, valid.error};
  if (shop->status == "closed")
    return fail<PrivateShopOperationQuote>("PrivateShopClosed");
  auto rent_use = private_shop_rent_use_current(x, shop->id);
  if (!rent_use.ok)
    return {false, {}, rent_use.error};
  if (!rent_use.value)
    return fail<PrivateShopOperationQuote>("PrivateShopRentResumeRequired");
  if (!x.common_catalog.items.find(command.item_id))
    return fail<PrivateShopOperationQuote>("UnknownItem");
  for (const auto &row : x.state.operations.rows())
    if (row.second.shop_id == shop->id) {
      Integer until = 0;
      if (__builtin_add_overflow(row.second.start_tick, Integer{2}, &until))
        return fail<PrivateShopOperationQuote>("PrivateShopTickOverflow");
      if (row.second.result_fact_id.empty() || x.common.tick() < until)
        return fail<PrivateShopOperationQuote>(
            "PrivateShopDayAlreadyCommitted");
    }
  auto market =
      read_growth_market_midpoint(x.common, *x.markets, x.common_catalog,
                                  shop->terms.market_shop_id, command.item_id);
  if (!market.ok)
    return {false, {}, market.error};
  if (!x.market_history)
    return fail<PrivateShopOperationQuote>(
        "PrivateShopMarketHistoryUnavailable");
  auto source = x.market_history->private_shop_market_history_current(
      shop->terms.market_shop_id, command.item_id);
  if (!source.ok)
    return {false, {}, source.error};
  if (!identity(source.value.source_identity, x.common.state().identity) ||
      source.value.shop_id != market.value.shop_id ||
      source.value.item_id != market.value.item_id ||
      source.value.inventory_id != market.value.inventory_id ||
      source.value.place_id != market.value.place_id ||
      source.value.observed_tick != market.value.observed_tick ||
      source.value.market_revision != market.value.market_revision ||
      source.value.available_stock != market.value.available_stock ||
      source.value.base_price != market.value.base_price ||
      source.value.midpoint != market.value.midpoint)
    return fail<PrivateShopOperationQuote>("PrivateShopMarketHistoryMismatch");
  auto funds = x.common.spendable(shop->wallet_id);
  if (!funds.ok)
    return {false, {}, funds.error};
  if (funds.value < (x.daily_rent ? 0 : shop->terms.daily_rent) +
                        (staff.value.empty() ? 0 : 3))
    return fail<PrivateShopOperationQuote>("PrivateShopDailyFundsRequired");
  if (x.common.tick() > INT64_MAX - 2)
    return fail<PrivateShopOperationQuote>("PrivateShopTickOverflow");
  return {true,
          {x.common.state().identity,
           actor,
           command,
           x.daily_rent ? 0 : shop->terms.daily_rent,
           2,
           staff.value,
           staff.value.empty() ? 0 : 3,
           {x.common.read_version({ResourceKey::Kind::actor, actor}),
            x.common.read_version({ResourceKey::Kind::wallet, shop->wallet_id}),
            x.common.read_version(
                {ResourceKey::Kind::wallet, shop->terms.lessor_wallet_id}),
            x.common.read_version(
                {ResourceKey::Kind::inventory, shop->inventory_id})}},
          {}};
}
namespace {
struct ExactShopStart {
  PrivateShopOperationSourcesState &sources;
  PrivateShopOperationCapabilitiesState &capabilities;
  ActionElapsedStore &elapsed;
  const PrivateShopOperationSourceContext &context;
  const ActorFoundationRuleRef &market;
};
Outcome<StableId> start_original_shop_operation(
    CommonStore &c, LocalEconomyState &economy, PrivateShopState &state,
    const PrivateShopContext &x, const PrivateShopOperationQuote &quote,
    ExactShopStart *exact) {
  if (&c != &x.common || &economy != &x.economy || &state != &x.state)
    return fail<StableId>("PrivateShopCandidateMismatch");
  if (!identity(quote.source_identity, c.state().identity))
    return fail<StableId>("StalePrivateShopOperation");
  auto current =
      preview_private_shop_operation(x, quote.actor_id, quote.command);
  if (!current.ok)
    return {false, {}, current.error};
  if (quote.rent_cash != current.value.rent_cash || quote.duration != 2 ||
      quote.staff_contract_id != current.value.staff_contract_id ||
      quote.wage_cap != current.value.wage_cap ||
      !versions_equal(quote.versions, current.value.versions))
    return fail<StableId>("StalePrivateShopOperation");
  if (bool(exact) != (x.exact_operations != nullptr))
    return fail<StableId>("PrivateShopExactStartOwnerRequired");
  PreparedPrivateShopOperationAcceptance prepared;
  if (exact) {
    if (&exact->context != x.exact_operations ||
        &exact->sources != &exact->context.sources ||
        &exact->capabilities != &exact->context.capabilities ||
        &exact->elapsed != &exact->context.elapsed)
      return fail<StableId>("PrivateShopExactStartCandidateMismatch");
    auto actual = prepare_private_shop_operation_acceptance(
        exact->context, current.value, exact->market);
    if (!actual.ok)
      return {false, {}, actual.error};
    prepared = std::move(actual.value);
  }
  CandidateStateGuard<LocalEconomyState> economy_guard(economy);
  CandidateStateGuard<PrivateShopState> guard(state);
  auto id = fact(c, "action", "private_shop_operating_started");
  if (!id.ok)
    return id;
  const auto shop = *state.shops.find(quote.command.shop_id);
  PrivateShopOperation operation;
  operation.id = id.value;
  operation.shop_id = shop.id;
  operation.actor_id = quote.actor_id;
  operation.item_id = quote.command.item_id;
  operation.staff_contract_id = current.value.staff_contract_id;
  operation.start_tick = c.tick();
  operation.markup_basis_points = shop.markup_basis_points;
  if (!x.daily_rent) {
    auto paid = pay_rent(c, economy, shop, operation);
    if (!paid.ok)
      return {false, {}, paid.error};
  }
  if (!state.operations.insert(id.value, operation))
    return fail<StableId>("PrivateShopDuplicateOperation");
  ActionMutation insert;
  insert.kind = ActionMutation::Kind::insert;
  insert.action = {id.value,       "private_shop_operate",
                   quote.actor_id, {quote.actor_id},
                   c.tick(),       2};
  auto changed = c.apply_action(insert);
  if (!changed.ok)
    return {false, {}, changed.error};
  ActorMutation busy;
  busy.actor_id = quote.actor_id;
  busy.action_id = id.value;
  changed = c.apply_actor(busy);
  if (!changed.ok)
    return {false, {}, changed.error};
  changed = reserve_private_shop_staff_day(c, x, operation);
  if (!changed.ok)
    return {false, {}, changed.error};
  state.shops.find(shop.id)->status = "open";
  if (exact) {
    auto adopted = adopt_private_shop_operation_acceptance_in_candidate(
        c, exact->sources, exact->capabilities, exact->context,
        std::move(prepared), id.value);
    if (!adopted.ok)
      return {false, {}, adopted.error};
    auto at = exact->context.clock.read_current_moment();
    if (!at.ok)
      return {false, {}, at.error};
    auto entry = begin_fixed_elapsed(
        id.value, id.value, private_shop_operation_duration_rule, id.value,
        at.value, 2, exact->context.scale, exact->context.spatial.budget);
    if (!entry.ok)
      return {false, {}, entry.error};
    FixedServiceTransition transition;
    transition.transition_id = id.value;
    transition.action_ref = id.value;
    transition.occurred_at = at.value;
    transition.execution_source_ref =
        private_shop_operation_started_source(id.value);
    transition.execution_source_schema = 2;
    transition.cause = FixedOwnerAdopted{transition.execution_source_ref, 2};
    auto input =
        read_private_shop_current_service_inputs(exact->context, id.value);
    if (!input.ok)
      return {false, {}, input.error};
    transition.input_basis_refs = std::move(input.value);
    NativePrivateShopActionTimeRead owner(exact->context);
    auto change = prepare_fixed_service_adoption(
        *c.action(id.value), entry.value, transition,
        private_shop_operation_action_time_registration(), exact->context.scale,
        owner, exact->context.spatial.budget);
    if (!change.ok)
      return {false, {}, change.error};
    adopted = apply_fixed_service_change_in_candidate(
        exact->elapsed, std::move(change.value),
        private_shop_operation_action_time_registration(), exact->context.scale,
        owner, exact->context.spatial.budget);
    if (!adopted.ok)
      return {false, {}, adopted.error};
  }
  auto valid = validate_private_shops(x);
  if (!valid.ok)
    return {false, {}, valid.error};
  economy_guard.commit();
  guard.commit();
  return id;
}
} // namespace
Outcome<StableId> start_private_shop_operation_in_candidate(
    CommonStore &c, LocalEconomyState &e, PrivateShopState &s,
    const PrivateShopContext &x, const PrivateShopOperationQuote &q) {
  return start_original_shop_operation(c, e, s, x, q, nullptr);
}
Outcome<StableId> start_private_shop_operation_in_candidate(
    CommonStore &c, LocalEconomyState &e, PrivateShopState &s,
    PrivateShopOperationSourcesState &sources,
    PrivateShopOperationCapabilitiesState &capabilities,
    ActionElapsedStore &elapsed, const PrivateShopOperationSourceContext &x,
    const PrivateShopOperationQuote &q, const ActorFoundationRuleRef &market) {
  ExactShopStart exact{sources, capabilities, elapsed, x, market};
  return start_original_shop_operation(c, e, s, x.shops, q, &exact);
}
Outcome<std::optional<StableId>> step_private_shop_operation_in_candidate(
    CommonStore &c, LocalEconomyState &economy, PrivateShopState &state,
    const PrivateShopContext &x, const StableId &id,
    const DynamicStepInput &input) {
  if (&c != &x.common || &economy != &x.economy || &state != &x.state ||
      x.phase != &input)
    return fail<std::optional<StableId>>("PrivateShopCandidateMismatch");
  auto observed = private_shop_observed_tick(x);
  if (!observed.ok)
    return {false, {}, observed.error};
  auto valid = validate_private_shops(x);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto *operation = state.operations.find(id);
  const auto *action = c.action(id);
  if (!operation || !action || !operation->result_fact_id.empty())
    return fail<std::optional<StableId>>("PrivateShopOperationMissing");
  const auto *shop = state.shops.find(operation->shop_id);
  Integer expected =
      operation->start_tick + Integer(operation->attendance_ticks.size());
  if (input.from_tick != expected)
    return fail<std::optional<StableId>>("PrivateShopAttendanceGap");
  if (x.exact_operations)
    for (const auto &a : x.exact_operations->sources.accepted_operations)
      if (a.operation_ref == id)
        return fail<std::optional<StableId>>(
            "PrivateShopExactServiceWriterRequired");
  const Integer remaining_ticks = action->remaining_ticks;
  CandidateStateGuard<LocalEconomyState> economic_guard(economy);
  CandidateStateGuard<PrivateShopState> guard(state);
  valid = person_present(x, *shop, operation->actor_id, id);
  auto rent_use = private_shop_rent_use_current(x, shop->id);
  if (!rent_use.ok)
    return {false, {}, rent_use.error};
  if (!rent_use.value)
    valid = common_failure("PrivateShopRentResumeRequired");
  Outcome<StableId> ended;
  if (!valid.ok)
    ended = finish(c, economy, state, x, id, input.to_tick, false,
                   valid.error.original.code);
  else {
    state.operations.find(id)->attendance_ticks.push_back(input.to_tick);
    valid = record_private_shop_staff_attendance(c, economy, state, x, id,
                                                 input.to_tick);
    if (!valid.ok)
      return {false, {}, valid.error};
    if (remaining_ticks == 1)
      ended = finish(c, economy, state, x, id, input.to_tick, true, {});
    else {
      ActionMutation progress;
      progress.kind = ActionMutation::Kind::patch;
      progress.action.id = id;
      progress.remaining_ticks = remaining_ticks - 1;
      valid = c.apply_action(progress);
      if (!valid.ok)
        return {false, {}, valid.error};
    }
  }
  if (!ended.ok && !ended.error.original.code.empty())
    return {false, {}, ended.error};
  state.last_step_tick = input.to_tick;
  valid = validate_private_shops(x);
  if (!valid.ok)
    return {false, {}, valid.error};
  economic_guard.commit();
  guard.commit();
  return {
      true, ended.ok ? std::optional<StableId>{ended.value} : std::nullopt, {}};
}
Status preview_private_shop_cancel_operation(const PrivateShopContext &x,
                                             const StableId &actor,
                                             const StableId &id) {
  if (x.phase || !validate_private_shop_committed_read3(x).ok)
    return common_failure("PrivateShopCommandRequiresCommittedClock");
  auto valid = validate_private_shops(x);
  if (!valid.ok)
    return valid;
  const auto *operation = x.state.operations.find(id);
  return operation && operation->actor_id == actor &&
                 operation->result_fact_id.empty() && x.common.action(id)
             ? common_success()
             : common_failure("PrivateShopOperationMissing");
}
Outcome<StableId> cancel_private_shop_operation_in_candidate(
    CommonStore &c, LocalEconomyState &economy, PrivateShopState &state,
    const PrivateShopContext &x, const StableId &actor, const StableId &id) {
  if (&c != &x.common || &economy != &x.economy || &state != &x.state ||
      x.phase)
    return fail<StableId>("PrivateShopCandidateMismatch");
  auto valid = preview_private_shop_cancel_operation(x, actor, id);
  if (!valid.ok)
    return {false, {}, valid.error};
  if (x.exact_operations)
    for (const auto &a : x.exact_operations->sources.accepted_operations)
      if (a.operation_ref == id)
        return fail<StableId>("PrivateShopExactCancellationWriterRequired");
  CandidateStateGuard<LocalEconomyState> economic_guard(economy);
  CandidateStateGuard<PrivateShopState> guard(state);
  auto result = finish(c, economy, state, x, id, c.tick(), false, "cancelled");
  if (!result.ok)
    return result;
  valid = validate_private_shops(x);
  if (!valid.ok)
    return {false, {}, valid.error};
  economic_guard.commit();
  guard.commit();
  return result;
}
Outcome<StableId> interrupt_private_shop_staff_operation_in_candidate(
    CommonStore &c, LocalEconomyState &economy, PrivateShopState &state,
    const PrivateShopContext &x, const StableId &action_id,
    const StableId &reason) {
  if (&c != &x.common || &economy != &x.economy || &state != &x.state ||
      x.phase)
    return fail<StableId>("PrivateShopStaffLifecycleCandidateMismatch");
  if (x.exact_operations)
    for (const auto &a : x.exact_operations->sources.accepted_operations)
      if (a.operation_ref == action_id)
        return fail<StableId>("PrivateShopExactStaffLifecycleWriterRequired");
  const auto *o = state.operations.find(action_id);
  const auto *contract =
      o ? state.staff.contracts.find(o->staff_contract_id) : nullptr;
  if (!o || !contract || contract->status != "active" ||
      !o->result_fact_id.empty() || !c.action(action_id))
    return fail<StableId>("PrivateShopStaffOperationMissing");
  if (reason == "expired") {
    if (c.tick() != contract->through_tick)
      return fail<StableId>("PrivateShopStaffExpiryMismatch");
  } else {
    if ((reason != "worker_dead" && reason != "employer_dead") ||
        !x.family_deaths)
      return fail<StableId>("PrivateShopDeathHistoryUnavailable");
    auto death = read_actor_death(c, *x.family_deaths,
                                  reason == "worker_dead"
                                      ? contract->terms.worker_actor_id
                                      : contract->terms.employer_actor_id);
    if (!death.ok)
      return {false, {}, death.error};
    if (!death.value || death.value->recorded_tick != c.tick())
      return fail<StableId>("PrivateShopStaffDeathMismatch");
  }
  CandidateStateGuard<LocalEconomyState> economy_guard(economy);
  CandidateStateGuard<PrivateShopState> guard(state);
  auto result = finish(c, economy, state, x, action_id, c.tick(), false,
                       "PrivateShopStaffTerminated");
  if (!result.ok)
    return result;
  economy_guard.commit();
  guard.commit();
  return result;
}

namespace {
struct ShopExactFailure {
  RuleError error;
};
template <class T> T exact_take(Outcome<T> v) {
  if (!v.ok)
    throw ShopExactFailure{std::move(v.error)};
  return std::move(v.value);
}
[[noreturn]] void exact_fail(const char *code, const StableId &id = {}) {
  throw ShopExactFailure{common_failure(code, id).error};
}
int exact_compare(const WorldMoment &a, const WorldMoment &b) {
  return exact_take(compare_world_moments(a, b));
}
void exact_candidate(CommonStore &c, const LocalEconomyState &e,
                     const PrivateShopState &s,
                     const PrivateShopOperationSourceContext &x) {
  if (&c != &x.shops.common || &e != &x.shops.economy || &s != &x.shops.state ||
      x.shops.exact_operations != &x)
    exact_fail("PrivateShopExactTerminalCandidateMismatch");
}
} // namespace
Outcome<StableId> interrupt_private_shop_exact_staff_operation_in_candidate(
    CommonStore &c, LocalEconomyState &economy, PrivateShopState &shops,
    PrivateShopOperationSourcesState &sources,
    PrivateShopOperationCapabilitiesState &capabilities,
    ActionElapsedStore &elapsed, const PrivateShopOperationSourceContext &x,
    const StableId &id, const SourceRef &staff_ended) {
  try {
    exact_candidate(c, economy, shops, x);
    if (&sources != &x.sources || &capabilities != &x.capabilities ||
        &elapsed != &x.elapsed)
      exact_fail("PrivateShopExactStaffLifecycleCandidateMismatch", id);
    const auto at = exact_take(x.clock.read_current_moment());
    const auto staff =
        exact_take(read_private_shop_exact_staff_terminal(x, staff_ended));
    exact_take(read_original_private_shop_operation_acceptance(
        x, {private_shop_operation_started_source(id), 2}));
    const auto *operation = shops.operations.find(id);
    const auto *entry = find_fixed_elapsed(elapsed, id);
    if (!operation || !entry || !c.action(id) ||
        !operation->result_fact_id.empty() ||
        operation->staff_contract_id != staff.contract->id ||
        exact_compare(at, staff.occurred_at))
      exact_fail("PrivateShopActualStaffTerminationBoundaryRequired", id);
    const bool stopped =
        std::holds_alternative<ElapsedSuspended>(entry->timing);
    for (const auto &old : sources.terminal_sources)
      if (old.operation_ref == id)
        exact_fail("PrivateShopTerminalAlreadyApplied", id);
    const auto result =
        exact_take(finish(c, economy, shops, x.shops, id, at.half_day_tick,
                          false, "PrivateShopStaffTerminated"));
    const MomentOwnerSource terminal{
        {private_shop_module, "private_shop_operating_interrupted", result}, 2};
    sources.terminal_sources.push_back(
        {id, terminal.source_ref, at, {staff_ended, 1}});
    exact_take(read_original_private_shop_operation_terminal(x, terminal));
    exact_take(
        adopt_private_shop_terminal_time_in_candidate(elapsed, x, terminal));
    if (stopped) {
      const FixedServiceTransition *stop = nullptr;
      for (const auto &t : elapsed.fixed_service_transitions)
        if (t.action_ref == id && t.kind == FixedTransitionKind::suspended &&
            (!stop || t.transition_ordinal > stop->transition_ordinal))
          stop = &t;
      if (!stop)
        exact_fail("PrivateShopActualSuspensionRequired", id);
      exact_take(adopt_private_shop_stopped_capabilities_in_candidate(
          capabilities, x, {stop->transition_id, stop->source_schema}));
    }
    return {true, result, {}};
  } catch (ShopExactFailure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Outcome<StableId> complete_private_shop_operation_in_candidate(
    CommonStore &c, LocalEconomyState &economy, PrivateShopState &shops,
    PrivateShopOperationSourcesState &sources, ActionElapsedStore &elapsed,
    const PrivateShopOperationSourceContext &x, const StableId &id,
    const MomentOwnerSource &final_labor) {
  try {
    exact_candidate(c, economy, shops, x);
    if (&sources != &x.sources || &elapsed != &x.elapsed)
      exact_fail("PrivateShopExactTerminalCandidateMismatch", id);
    const auto at = exact_take(x.clock.read_current_moment());
    const auto &labor =
        *exact_take(read_original_private_shop_labor(x, final_labor));
    if (labor.operation_ref != id ||
        labor.work_period_ref.period_ordinal != 1 ||
        exact_compare(labor.occurred_at, at))
      exact_fail("PrivateShopActualFinalLaborRequired", id);
    const auto completion = exact_take(read_fixed_service_completion(
        elapsed, {id}, at, x.scale, x.spatial.budget));
    if (exact_compare(completion.completed_at, at) || !c.action(id))
      exact_fail("PrivateShopActualCompletionBoundaryRequired", id);
    for (const auto &old : sources.terminal_sources)
      if (old.operation_ref == id)
        exact_fail("PrivateShopTerminalAlreadyApplied", id);
    const auto result = exact_take(
        finish(c, economy, shops, x.shops, id, at.half_day_tick, true, {}));
    const MomentOwnerSource terminal{
        {private_shop_module, "private_shop_operating_completed", result}, 2};
    sources.terminal_sources.push_back(
        {id, terminal.source_ref, at, final_labor});
    exact_take(read_original_private_shop_operation_terminal(x, terminal));
    exact_take(
        adopt_private_shop_terminal_time_in_candidate(elapsed, x, terminal));
    exact_take(validate_private_shops(x.shops));
    return {true, result, {}};
  } catch (ShopExactFailure &e) {
    return {false, {}, std::move(e.error)};
  }
}
namespace {
CheckpointCopyLease cancellation_token_memory(const StableId &actor,
                                              const StableId &operation) {


  return CheckpointCopyLease::reserve();
}
} // namespace
struct PendingPrivateShopCancellation::Impl : CheckpointAllocatedObject {
  CheckpointCopyLease copied_fields, result_fields;
  const CommonStore *common = nullptr;
  const PrivateShopState *shops = nullptr;
  const PrivateShopOperationSourceContext *context = nullptr;
  decltype(std::declval<const CommonStore &>()
               .actor_condition_capture_identity()) condition_capture;
  decltype(std::declval<const CommonStore &>()
               .resource_execution_capture_identity()) resource_capture;
  std::size_t resource_count = 0;
  WorldId world;
  StableId actor, operation, result;
  Integer epoch = 0, revision = 0;
  WorldMoment at;
  Impl(const CommonStore &c, const PrivateShopState &s,
       const PrivateShopOperationSourceContext &x, const StableId &who,
       const StableId &id, const WorldMoment &now)
      : copied_fields(cancellation_token_memory(who, id)), common(&c),
        shops(&s), context(&x),
        condition_capture(c.actor_condition_capture_identity()),
        resource_capture(c.resource_execution_capture_identity()),
        world(c.state().identity.world_id),
        actor(checkpoint_fresh_string_copy(who)),
        operation(checkpoint_fresh_string_copy(id)),
        epoch(c.state().identity.execution_epoch), revision(c.revision()),
        at(now) {}
  void adopt_result(const StableId &id) {


    result_fields = CheckpointCopyLease::reserve();
    result = checkpoint_fresh_string_copy(id);
  }
};
struct PrivateShopCancellationAccess {
  static Outcome<PendingPrivateShopCancellation>
  cancel(CommonStore &c, LocalEconomyState &e, PrivateShopState &s,
         const PrivateShopOperationSourceContext &x, const StableId &actor,
         const StableId &id) {
    try {
      exact_candidate(c, e, s, x);
      exact_take(preview_private_shop_cancel_operation(x.shops, actor, id));
      exact_take(read_original_private_shop_operation_acceptance(
          x, {private_shop_operation_started_source(id), 2}));
      if (!c.actor_condition_capture_identity() ||
          !c.resource_execution_capture_identity() || c.revision() == INT64_MAX)
        exact_fail("PrivateShopActualCommandCaptureRequired", id);
      const auto at = exact_take(x.clock.read_current_moment());
      auto pending = std::make_unique<PendingPrivateShopCancellation::Impl>(
          c, s, x, actor, id, at);
      pending->adopt_result(exact_take(
          finish(c, e, s, x.shops, id, at.half_day_tick, false, "cancelled")));
      pending->resource_count = c.resource_execution_drafts().size();
      // 未封口 token 不进入附段；原 Common
      // 必须随即封口、绑定和协调容量，否则丢弃候选。
      return {true, PendingPrivateShopCancellation(std::move(pending)), {}};
    } catch (ShopExactFailure &e) {
      return {false, {}, std::move(e.error)};
    }
  }
  static Outcome<StableId>
  result(const PendingPrivateShopCancellation &pending) {
    if (!pending.impl_)
      return {false,
              {},
              common_failure("PrivateShopPendingCancellationRequired").error};
    return {true, pending.impl_->result, {}};
  }
  static Status bind(CommonStore &c, PrivateShopOperationSourcesState &s,
                     PrivateShopOperationCapabilitiesState &capabilities,
                     ActionElapsedStore &elapsed,
                     const PrivateShopOperationSourceContext &x,
                     PendingPrivateShopCancellation &&pending,
                     const MomentOwnerSource &seal) {
    try {
      auto p = std::move(pending.impl_);
      if (!p || p->common != &c || p->shops != &x.shops.state ||
          p->context != &x || &c != &x.shops.common || &s != &x.sources ||
          &capabilities != &x.capabilities || &elapsed != &x.elapsed ||
          p->world != c.state().identity.world_id ||
          p->epoch != c.state().identity.execution_epoch ||
          p->revision + 1 != c.revision() ||
          exact_compare(p->at, exact_take(x.clock.read_current_moment())) ||
          p->condition_capture != c.actor_condition_capture_identity() ||
          p->resource_capture != c.resource_execution_capture_identity() ||
          p->resource_count != c.resource_execution_drafts().size())
        exact_fail("PrivateShopPendingCancellationCandidateMismatch");
      if (seal.source_schema != 1)
        exact_fail("PrivateShopActualCommandSealRequired", p->result);
      const auto command =
          exact_take(read_common_command_source(c, seal.source_ref));
      const auto *effect =
          command.result->details
              ? std::get_if<InteractionEffect>(&command.result->details->data)
              : nullptr;
      if (command.result->kind != "private_shop_cancel_operation" ||
          command.result->revision != c.revision() ||
          command.history->actor_id != p->actor || !effect ||
          effect->settlement_result != p->result)
        exact_fail("PrivateShopActualCancellationSealMismatch", p->result);
      const auto *operation = x.shops.state.operations.find(p->operation);
      if (!operation || operation->result_fact_id != p->result)
        exact_fail("PrivateShopPendingCancellationChanged", p->operation);
      for (const auto &old : s.terminal_sources)
        if (old.operation_ref == p->operation)
          exact_fail("PrivateShopTerminalAlreadyApplied", p->operation);
      const auto *entry = find_fixed_elapsed(elapsed, p->operation);
      if (!entry)
        exact_fail("PrivateShopOriginalElapsedRequired", p->operation);
      const bool stopped =
          std::holds_alternative<ElapsedSuspended>(entry->timing);
      const MomentOwnerSource terminal{{private_shop_module,
                                        "private_shop_operating_interrupted",
                                        p->result},
                                       2};
      s.terminal_sources.push_back(
          {p->operation, terminal.source_ref, p->at, seal});
      exact_take(read_original_private_shop_operation_terminal(x, terminal));
      exact_take(
          adopt_private_shop_terminal_time_in_candidate(elapsed, x, terminal));
      if (stopped) {
        const FixedServiceTransition *stop = nullptr;
        for (const auto &t : elapsed.fixed_service_transitions)
          if (t.action_ref == p->operation &&
              t.kind == FixedTransitionKind::suspended &&
              (!stop || t.transition_ordinal > stop->transition_ordinal))
            stop = &t;
        if (!stop)
          exact_fail("PrivateShopActualSuspensionRequired", p->operation);
        exact_take(adopt_private_shop_stopped_capabilities_in_candidate(
            capabilities, x, {stop->transition_id, stop->source_schema}));
      }
      return validate_private_shops(x.shops);
    } catch (ShopExactFailure &e) {
      return {false, {}, std::move(e.error)};
    }
  }
};
PendingPrivateShopCancellation::PendingPrivateShopCancellation() = default;
PendingPrivateShopCancellation::PendingPrivateShopCancellation(
    std::unique_ptr<Impl> p)
    : impl_(std::move(p)) {}
PendingPrivateShopCancellation::~PendingPrivateShopCancellation() = default;
PendingPrivateShopCancellation::PendingPrivateShopCancellation(
    PendingPrivateShopCancellation &&) noexcept = default;
PendingPrivateShopCancellation &PendingPrivateShopCancellation::operator=(
    PendingPrivateShopCancellation &&) noexcept = default;
Outcome<PendingPrivateShopCancellation>
cancel_private_shop_exact_operation_in_candidate(
    CommonStore &c, LocalEconomyState &e, PrivateShopState &s,
    const PrivateShopOperationSourceContext &x, const StableId &a,
    const StableId &id) {
  return PrivateShopCancellationAccess::cancel(c, e, s, x, a, id);
}
Outcome<StableId> read_pending_private_shop_cancellation_result(
    const PendingPrivateShopCancellation &p) {
  return PrivateShopCancellationAccess::result(p);
}
Status bind_private_shop_cancellation_in_candidate(
    CommonStore &c, PrivateShopOperationSourcesState &s,
    PrivateShopOperationCapabilitiesState &capabilities,
    ActionElapsedStore &elapsed, const PrivateShopOperationSourceContext &x,
    PendingPrivateShopCancellation &&p, const MomentOwnerSource &seal) {
  return PrivateShopCancellationAccess::bind(c, s, capabilities, elapsed, x,
                                             std::move(p), seal);
}
} // namespace hanmo::resident
