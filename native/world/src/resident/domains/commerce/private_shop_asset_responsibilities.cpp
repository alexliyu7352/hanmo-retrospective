// 以实际开设/关闭及双方接受/终止的Common顺序切片；后来开铺或离职不改变较早交割的责任。
#include "private_shop_asset_responsibilities.hpp"
#include "private_shop_operation_sources.hpp"
#include "resident/domains/local_economy/takeover/public_account_dependencies.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include "resident/kernel/original_world_read_boundary.hpp"
#include <map>
#include <set>
namespace hanmo::resident {
namespace {
Outcome<PrivateShopPrivateAssetResponsibilities>
read(const CommonStore &c, const PrivateShopState *state,
     const PrivateShopPrivateAssetScope &scope, const StableId &boundary,
     Integer tick) {
  auto fail = [&](const StableId &code, const StableId &id = StableId{})
      -> Outcome<PrivateShopPrivateAssetResponsibilities> {
    return {false, {}, common_failure(code, id).error};
  };
  if (!c.actor(scope.actor_id) || tick < 0 || tick > c.tick() ||
      (scope.wallet_ids.empty() && scope.inventory_ids.empty()))
    return fail("PrivateShopPrivateScopeInvalid");
  std::set<StableId> wallets, inventories;
  for (const auto &id : scope.wallet_ids)
    if (!c.wallet(id) || !wallets.insert(id).second)
      return fail("PrivateShopPrivateScopeInvalid", id);
  for (const auto &id : scope.inventory_ids)
    if (!c.inventory(id) || !inventories.insert(id).second)
      return fail("PrivateShopPrivateScopeInvalid", id);
  std::map<StableId, std::size_t> order;
  for (std::size_t i = 0; i < c.state().facts.size(); ++i)
    order.emplace(c.state().facts.rows()[i].first, i);
  std::size_t limit = order.size();
  std::optional<Integer> end;
  if (!boundary.empty()) {
    end = organization_instance_sequence(boundary, "family_inheritance");
    const auto *h = c.state().facts.find(boundary);
    if (!end || *end >= c.state().next_instance_id || !h || h->id != boundary ||
        h->kind != "family_inheritance_settled" ||
        h->domain != "family_inheritance" || !order.count(boundary))
      return fail("PrivateShopInheritanceBoundaryInvalid", boundary);
    limit = order.at(boundary);
  }
  PrivateShopPrivateAssetResponsibilities out{
      c.state().identity, scope, tick, boundary, {}};
  if (!state) {
    for (const auto &h : c.state().facts.rows())
      if (h.second.domain == private_shop_module)
        return fail("PrivateShopPrivateContextRequired");
    return {true, std::move(out), {}};
  }
  if ((state->version != 1 && state->version != 2 && state->version != 3) ||
      state->last_step_tick != c.tick())
    return fail("PrivateShopPrivateContextMismatch");
  bool broken = false;
  // 数值区间、真实追加序和载荷时刻必须同时落在同一侧，不能靠改ID跳过旧责任。
  auto prior = [&](const StableId &id, Integer when, const StableId &prefix,
                   const StableId &kind) {
    const auto *h = c.state().facts.find(id);
    const auto serial = organization_instance_sequence(id, prefix);
    const auto it = order.find(id);
    if (!h || h->id != id || h->domain != private_shop_module ||
        h->kind != kind || !serial || *serial >= c.state().next_instance_id ||
        it == order.end() || when < 0 || when > c.tick()) {
      broken = true;
      return false;
    }
    const bool before = it->second < limit;
    if ((end && before != (*serial < *end)) || (before && when > tick) ||
        (!before && when < tick)) {
      broken = true;
      return false;
    }
    return before;
  };
  for (const auto &row : state->shops.rows()) {
    const auto &shop = row.second;
    bool touches = (shop.terms.proprietor_actor_id == scope.actor_id) ||
                   (wallets.count(shop.wallet_id) ||
                    inventories.count(shop.inventory_id) ||
                    wallets.count(shop.terms.payer_wallet_id) ||
                    wallets.count(shop.terms.lessor_wallet_id));
    if (!touches)
      continue;
    if (shop.id != row.first)
      return fail("PrivateShopPrivateSourceMismatch", row.first);
    const bool opened = prior(row.first, shop.opened_tick, "private_shop",
                              "private_shop_opened");
    bool closed = false;
    Integer count = 0;
    for (const auto &closing : state->closings.rows())
      if (closing.second.shop_id == row.first) {
        if (++count != 1 || closing.first != closing.second.id)
          return fail("PrivateShopPrivateSourceMismatch", closing.first);
        const bool finished =
            prior(closing.first, closing.second.tick, "private_shop_closed",
                  "private_shop_closed");
        if (finished && !opened)
          return fail("PrivateShopPrivateSourceMismatch", closing.first);
        closed = finished;
      }
    if (opened && !closed)
      out.blockers.push_back({"private_shop_lease", row.first});
  }
  // 非店主本人仍可能是受雇人；完整终止前的服务承诺也不能因当前工资余额为零而忽略。
  for (const auto &row : state->staff.contracts.rows()) {
    const auto &contract = row.second;
    if (!wallets.count(contract.terms.payer_wallet_id) &&
        !wallets.count(contract.terms.payee_wallet_id))
      continue;
    if (row.first != contract.id)
      return fail("PrivateShopPrivateSourceMismatch", row.first);
    const bool accepted =
        prior(row.first, contract.accepted_tick, "private_shop_staff",
              "private_shop_staff_accepted");
    bool ended = false;
    if (!contract.terminal_fact_id.empty()) {
      const auto *terminal =
          state->staff.decisions.find(contract.terminal_fact_id);
      if (!terminal || terminal->id != contract.terminal_fact_id ||
          terminal->contract_id != row.first)
        return fail("PrivateShopPrivateSourceMismatch", row.first);
      ended = prior(terminal->id, terminal->tick, "private_shop_staff_decision",
                    "private_shop_staff_ended");
      if (ended && !accepted)
        return fail("PrivateShopPrivateSourceMismatch", row.first);
    }
    if (accepted && !ended)
      out.blockers.push_back({"private_shop_staff", row.first});
  }
  if (broken)
    return fail("PrivateShopPrivateSourceBoundaryMismatch");
  return {true, std::move(out), {}};
}
} // namespace
Outcome<PrivateShopPrivateAssetResponsibilities>
read_private_shop_private_asset_responsibilities(
    const CommonStore &c, const PrivateShopState *state,
    const PrivateShopPrivateAssetScope &scope) {
  return read(c, state, scope, {}, c.tick());
}
Outcome<PrivateShopPrivateAssetResponsibilities>
read_private_shop_private_asset_responsibilities_before_inheritance(
    const CommonStore &c, const PrivateShopState *state,
    const PrivateShopPrivateAssetScope &scope, const StableId &fact,
    Integer tick) {
  if (fact.empty())
    return {false,
            {},
            common_failure("PrivateShopInheritanceBoundaryInvalid").error};
  return read(c, state, scope, fact, tick);
}
Outcome<Integer> read_private_shop_responsibility_observed_tick(
    const PrivateShopResponsibilitySourceContext &x) {
  if (x.catalog.version == 3 || x.state.version == 3) {
    if (x.catalog.version != 3 || x.state.version != 3 || !x.read_boundary3 ||
        !x.moment_clock3 ||
        !x.read_boundary3->bound_to(x.common, *x.moment_clock3))
      return {false, 0,
              common_failure("PrivateShopResponsibilityReadBoundaryRequired")
                  .error};
    auto actual = x.read_boundary3->read_observed_moment();
    if (!actual.ok)
      return {false, 0, actual.error};
    return {true, actual.value.half_day_tick, {}};
  }
  Integer limit = x.common.tick();
  if (x.phase) {
    if (x.common.tick() == INT64_MAX || x.phase->from_tick != x.common.tick() ||
        x.phase->to_tick != x.common.tick() + 1)
      return {false, 0, common_failure("PrivateShopSourcePhaseMismatch").error};
    limit = x.phase->to_tick;
  }
  return {true, limit, {}};
}
Status validate_private_shop_public_responsibility_sources(
    const CommonStore &c, const LocalEconomyState &e,
    const LocalEconomyCatalog &ec, const PrivateShopState &state,
    const PrivateShopCatalog &catalog, const DynamicStepInput *phase,
    const OriginalWorldReadBoundary *read_boundary3,
    const CurrentMomentRead *moment_clock3) {
  auto observed = read_private_shop_responsibility_observed_tick(
      PrivateShopResponsibilitySourceContext{c, e, ec, state, catalog, phase,
                                             nullptr, nullptr, read_boundary3,
                                             moment_clock3});
  if (!observed.ok)
    return {false, {}, observed.error};
  const Integer limit = observed.value;
  if (!ec.enabled ||
      (catalog.version != 1 && catalog.version != 2 && catalog.version != 3) ||
      state.version != catalog.version || state.initialized_tick < 0 ||
      state.initialized_tick > c.tick() ||
      state.last_step_tick < state.initialized_tick ||
      state.last_step_tick > limit || e.last_step_tick < 0 ||
      e.last_step_tick > limit)
    return common_failure("PrivateShopResponsibilitySourceContextMismatch");
  std::map<StableId, std::size_t> order;
  for (std::size_t i = 0; i < c.state().facts.size(); ++i)
    order.emplace(c.state().facts.rows()[i].first, i);
  std::set<Integer> allocated;
  std::set<StableId> rent_ids, wage_ids;
  auto head = [&](const StableId &id, const StableId &prefix,
                  const StableId &kind, Integer tick) -> bool {
    const auto *h = c.state().facts.find(id);
    const auto n = organization_instance_sequence(id, prefix);
    return h && h->id == id && h->domain == private_shop_module &&
           h->kind == kind && n && *n < c.state().next_instance_id &&
           allocated.insert(*n).second && order.count(id) &&
           tick >= state.initialized_tick && tick <= limit;
  };
  auto account = [&](const StableId &id, const StableId &actor) -> bool {
    const auto *w = c.wallet(id);
    return w && w->id == id &&
           owner_equal(w->owner, {Owner::Kind::actor, actor, {}});
  };
  for (const auto &row : state.shops.rows()) {
    const auto &s = row.second;
    const auto &t = s.terms;
    const auto *p = catalog.premises.find(t.premises_id);
    const auto *actor = c.actor(t.proprietor_actor_id);
    const auto *lessor = c.actor(t.lessor_actor_id);
    const auto *rate = p ? ec.rates.find(p->rent_rate_profile) : nullptr;
    const auto n = organization_instance_sequence(s.id, "private_shop"),
               rent_n = organization_instance_sequence(s.rent_obligation_id,
                                                       "private_shop_rent");
    const auto *rent = e.obligations.find(s.rent_obligation_id);
    const auto *remainder = e.obligation_remainders.find(s.rent_obligation_id);
    const auto *stock = c.inventory(s.inventory_id);
    Integer due = 0;
    if (row.first != s.id ||
        !head(s.id, "private_shop", "private_shop_opened", s.opened_tick) ||
        !p || p->id != t.premises_id || !actor || !lessor ||
        actor->id == lessor->id || !n || !rent_n || *n == INT64_MAX ||
        *rent_n != *n + 1 || *rent_n >= c.state().next_instance_id ||
        !allocated.insert(*rent_n).second ||
        !rent_ids.insert(s.rent_obligation_id).second ||
        c.state().facts.find(s.rent_obligation_id))
      return common_failure("PrivateShopRentSourceMismatch", row.first);
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
        p->opening_fee != 900 || p->initial_capital != 300 ||
        p->inventory_capacity != 200 || p->daily_rent != 2 || !rate ||
        rate->per_capita || rate->numerator != 2 || rate->denominator != 1 ||
        s.wallet_id != "private_shop_wallet:" + std::to_string(*n) ||
        s.inventory_id != "private_shop_inventory:" + std::to_string(*n) ||
        !account(s.wallet_id, actor->id) ||
        !account(t.payer_wallet_id, actor->id) ||
        !account(t.lessor_wallet_id, lessor->id) || !stock ||
        stock->id != s.inventory_id ||
        !owner_equal(stock->owner, {Owner::Kind::actor, actor->id, {}}) ||
        stock->capacity != t.inventory_capacity ||
        __builtin_add_overflow(s.opened_tick, Integer{2}, &due) || !rent ||
        rent->id != s.rent_obligation_id ||
        rent->kind != ObligationKind::contract_payment ||
        rent->principal.kind != SubjectKind::actor ||
        rent->principal.id != actor->id ||
        rent->beneficiary.kind != SubjectKind::actor ||
        rent->beneficiary.id != lessor->id ||
        rent->source_wallet_or_inventory != s.wallet_id ||
        rent->destination != t.lessor_wallet_id ||
        rent->authority_ref != s.id ||
        rent->rate_profile != t.rent_rate_profile || rent->period_ticks != 2 ||
        rent->next_due_tick != due || rent->active || !rent->item_id.empty() ||
        !remainder || *remainder != 0)
      return common_failure("PrivateShopRentSourceMismatch", row.first);
  }
  for (const auto &row : state.staff.contracts.rows()) {
    const auto &record = row.second;
    const auto &t = record.terms;
    const auto *shop = state.shops.find(t.shop_id);
    const auto *policy = catalog.staff.policies.find(t.policy_id);
    const auto *worker = c.actor(t.worker_actor_id);
    const auto *rate =
        policy ? ec.rates.find(policy->wage_rate_profile) : nullptr;
    const auto n = organization_instance_sequence(record.id,
                                                  "private_shop_staff"),
               wage_n = organization_instance_sequence(record.obligation_id,
                                                       "private_shop_wage");
    const auto *wage = e.obligations.find(record.obligation_id);
    const auto *remainder = e.obligation_remainders.find(record.obligation_id);
    Integer through = 0, due = 0;
    if (row.first != record.id ||
        !head(record.id, "private_shop_staff", "private_shop_staff_accepted",
              record.accepted_tick) ||
        !shop || !policy || policy->id != t.policy_id ||
        policy->premises_id != shop->terms.premises_id || !worker || !n ||
        !wage_n || *n == INT64_MAX || *wage_n != *n + 1 ||
        *wage_n >= c.state().next_instance_id ||
        !allocated.insert(*wage_n).second ||
        !wage_ids.insert(record.obligation_id).second ||
        c.state().facts.find(record.obligation_id) ||
        order.at(shop->id) >= order.at(record.id) ||
        record.accepted_tick < shop->opened_tick ||
        t.employer_actor_id != shop->terms.proprietor_actor_id ||
        t.worker_actor_id != policy->worker_actor_id ||
        t.employer_actor_id == t.worker_actor_id ||
        t.payer_wallet_id != shop->wallet_id ||
        t.payee_wallet_id != worker->wallet_id ||
        !account(t.payee_wallet_id, worker->id) ||
        t.rate_profile != policy->wage_rate_profile || !rate ||
        rate->per_capita || rate->numerator != 3 || rate->denominator != 2 ||
        t.wage_numerator != 3 || t.wage_denominator != 2 ||
        t.service_ticks != policy->service_ticks || t.service_ticks <= 0 ||
        t.service_item_id != policy->service_item_id ||
        __builtin_add_overflow(record.accepted_tick, t.service_ticks,
                               &through) ||
        through != record.through_tick ||
        __builtin_add_overflow(record.accepted_tick, Integer{1}, &due))
      return common_failure("PrivateShopWageSourceMismatch", row.first);
    if (!wage || wage->id != record.obligation_id ||
        wage->kind != ObligationKind::wage ||
        wage->principal.kind != SubjectKind::actor ||
        wage->principal.id != t.employer_actor_id ||
        wage->beneficiary.kind != SubjectKind::actor ||
        wage->beneficiary.id != t.worker_actor_id ||
        wage->source_wallet_or_inventory != t.payer_wallet_id ||
        wage->destination != t.payee_wallet_id ||
        wage->rate_profile != t.rate_profile ||
        wage->authority_ref != record.id || wage->period_ticks != 1 ||
        wage->next_due_tick != due || wage->active || !wage->item_id.empty() ||
        !remainder || *remainder < 0 || *remainder > 1)
      return common_failure("PrivateShopWageSourceMismatch", row.first);
  }
  // 反向覆盖防删原来源后将具名私债伪装为普通未知责任。
  for (const auto &h : c.state().facts.rows())
    if (h.second.domain == private_shop_module) {
      if (h.second.kind == "private_shop_opened" && !state.shops.find(h.first))
        return common_failure("PrivateShopRentSourceMissing", h.first);
      if (h.second.kind == "private_shop_staff_accepted" &&
          !state.staff.contracts.find(h.first))
        return common_failure("PrivateShopWageSourceMissing", h.first);
    }
  for (const auto &row : e.obligations.rows()) {
    const auto &o = row.second;
    if ((o.id.rfind("private_shop_rent:", 0) == 0 ||
         o.authority_ref.rfind("private_shop:", 0) == 0) &&
        (!rent_ids.count(row.first) || row.first != o.id))
      return common_failure("PrivateShopRentSourceMissing", row.first);
    if ((o.id.rfind("private_shop_wage:", 0) == 0 ||
         o.authority_ref.rfind("private_shop_staff:", 0) == 0) &&
        (!wage_ids.count(row.first) || row.first != o.id))
      return common_failure("PrivateShopWageSourceMissing", row.first);
  }
  return common_success();
}
Outcome<bool> private_shop_obligation_outside_public_selection(
    const PrivateShopResponsibilitySourceContext &x,
    const RecurringObligation &obligation,
    const EconomicPublicAccountSelection &selection) {
  if (obligation.principal.kind != SubjectKind::actor ||
      obligation.beneficiary.kind != SubjectKind::actor)
    return {true, false, {}};
  const PrivateShopRecord *shop = nullptr;
  bool matched = false;
  for (const auto &row : x.state.shops.rows())
    if (row.second.rent_obligation_id == obligation.id) {
      if (matched)
        return {false,
                {},
                common_failure("PrivateShopDuplicateObligation", obligation.id)
                    .error};
      shop = &row.second;
      matched = obligation.kind == ObligationKind::contract_payment &&
                obligation.authority_ref == shop->id &&
                obligation.principal.id == shop->terms.proprietor_actor_id &&
                obligation.beneficiary.id == shop->terms.lessor_actor_id &&
                obligation.source_wallet_or_inventory == shop->wallet_id &&
                obligation.destination == shop->terms.lessor_wallet_id;
      if (!matched)
        return {false,
                {},
                common_failure("PrivateShopRentMismatch", obligation.id).error};
    }
  for (const auto &row : x.state.staff.contracts.rows())
    if (row.second.obligation_id == obligation.id) {
      if (matched)
        return {false,
                {},
                common_failure("PrivateShopDuplicateObligation", obligation.id)
                    .error};
      const auto &t = row.second.terms;
      shop = x.state.shops.find(t.shop_id);
      matched = shop && obligation.kind == ObligationKind::wage &&
                obligation.authority_ref == row.first &&
                obligation.principal.id == t.employer_actor_id &&
                obligation.beneficiary.id == t.worker_actor_id &&
                obligation.source_wallet_or_inventory == t.payer_wallet_id &&
                obligation.destination == t.payee_wallet_id &&
                t.payer_wallet_id == shop->wallet_id;
      if (!matched)
        return {false,
                {},
                common_failure("PrivateShopStaffWageMismatch", obligation.id)
                    .error};
    }
  if (!matched)
    return {true, false, {}};
  const auto *payer = x.common.wallet(obligation.source_wallet_or_inventory);
  const auto *payee = x.common.wallet(obligation.destination);
  const auto *stock = x.common.inventory(shop->inventory_id);
  const auto selected_wallet = [&](const StableId &id) {
    return std::find(selection.wallet_ids.begin(), selection.wallet_ids.end(),
                     id) != selection.wallet_ids.end();
  };
  const bool selected_inventory =
      std::find(selection.inventory_ids.begin(), selection.inventory_ids.end(),
                shop->inventory_id) != selection.inventory_ids.end();
  return {
      true,
      payer && payee && stock &&
          owner_equal(payer->owner,
                      {Owner::Kind::actor, obligation.principal.id, {}}) &&
          owner_equal(payee->owner,
                      {Owner::Kind::actor, obligation.beneficiary.id, {}}) &&
          owner_equal(
              stock->owner,
              {Owner::Kind::actor, shop->terms.proprietor_actor_id, {}}) &&
          !selected_wallet(payer->id) && !selected_wallet(payee->id) &&
          !selected_inventory,
      {}};
}
Outcome<bool> private_shop_obligation_outside_public_selection(
    const PrivateShopContext &x, const RecurringObligation &obligation,
    const EconomicPublicAccountSelection &selection) {
  return private_shop_obligation_outside_public_selection(
      PrivateShopResponsibilitySourceContext{
          x.common, x.economy, x.economy_catalog, x.state, x.catalog, x.phase,
          x.daily_rent_catalog, x.daily_rent, x.read_boundary3,
          x.exact_operations ? &x.exact_operations->clock : nullptr},
      obligation, selection);
}

} // namespace hanmo::resident
