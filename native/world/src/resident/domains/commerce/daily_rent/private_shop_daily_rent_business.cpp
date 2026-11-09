// 本人当场明确清债或复业；自愿资金按原报价重验，末端历史授权失败须撤回同一完整候选。
#include "private_shop_daily_rent_internal.hpp"
#include "resident/domains/local_economy/local_economy_internal.hpp"
namespace hanmo::resident {
namespace {
template <class T> Outcome<T> fail(const StableId &code) {
  return {false, {}, common_failure(code).error};
}
bool same_identity(const WorldIdentity &a, const WorldIdentity &b) {
  return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch &&
         a.revision == b.revision && a.tick == b.tick;
}
bool versions(const std::vector<ReadVersion> &a,
              const std::vector<ReadVersion> &b) {
  if (a.size() != b.size())
    return false;
  for (std::size_t i = 0; i < a.size(); ++i)
    if (a[i].resource.kind != b[i].resource.kind ||
        a[i].resource.id != b[i].resource.id || a[i].version != b[i].version)
      return false;
  return true;
}
bool request_equal(const PrivateShopRentVoluntaryRequest &a,
                   const PrivateShopRentVoluntaryRequest &b) {
  return a.shop_id == b.shop_id &&
         a.assessment_fact_id == b.assessment_fact_id &&
         a.arrear_id == b.arrear_id && a.payer_actor_id == b.payer_actor_id &&
         a.payer_wallet_id == b.payer_wallet_id &&
         a.payee_wallet_id == b.payee_wallet_id &&
         a.mandate_fact_id == b.mandate_fact_id &&
         a.death_notice_binding_fact_id == b.death_notice_binding_fact_id &&
         a.amount == b.amount;
}
PrivateShopRentVoluntaryRequest
request(const PrivateShopDailyRentQuote &q,
        const PrivateShopPayRentArrear &command) {
  return {q.shop_id,
          q.assessment_fact_id,
          q.arrear_id,
          q.actor_id,
          q.from_wallet_id,
          q.to_wallet_id,
          command.mandate_fact_id,
          command.death_notice_binding_fact_id,
          q.amount};
}
Status authority(const PrivateShopDailyRentContext &x,
                 const PrivateShopRentVoluntaryRequest &r,
                 const StableId &boundary = {}) {
  if (!x.executors)
    return common_failure("PrivateShopRentExecutorAuthorityUnavailable");
  auto a =
      boundary.empty()
          ? x.executors->private_shop_rent_voluntary_authority(r)
          : x.executors->private_shop_rent_voluntary_authority_before_payment(
                r, boundary, x.shops.common.tick());
  if (!a.ok)
    return {false, {}, a.error};
  const auto *shop = x.shops.state.shops.find(r.shop_id);
  if (!shop ||
      !same_identity(a.value.source_identity,
                     x.shops.common.state().identity) ||
      !request_equal(a.value.request, r) ||
      a.value.proprietor_actor_id != shop->terms.proprietor_actor_id ||
      a.value.observed_tick != x.shops.common.tick() ||
      a.value.acceptance_fact_id.empty() || a.value.death_fact_id.empty() ||
      a.value.death_cause_fact_id.empty() ||
      a.value.notice_handoff_fact_id.empty())
    return common_failure("PrivateShopRentExecutorAuthorityMismatch");
  return common_success();
}
} // namespace
Outcome<PrivateShopDailyRentQuote>
preview_private_shop_daily_rent(const PrivateShopDailyRentContext &x,
                                const StableId &actor,
                                const PrivateShopDailyRentCommand &command) {
  const auto &shop_context = x.shops;
  const auto &c = shop_context.common;
  if (shop_context.daily_rent != &x.state ||
      shop_context.daily_rent_catalog != &x.catalog ||
      shop_context.rent_executors != x.executors)
    return fail<PrivateShopDailyRentQuote>("PrivateShopRentCandidateMismatch");
  if (shop_context.phase ||
      !validate_private_shop_committed_read3(shop_context).ok)
    return fail<PrivateShopDailyRentQuote>(
        "PrivateShopRentRequiresCommittedClock");
  auto valid = validate_private_shops(shop_context);
  if (!valid.ok)
    return {false, {}, valid.error};
  const StableId id =
      std::visit([](const auto &q) { return q.shop_id; }, command);
  const auto *shop = shop_context.state.shops.find(id);
  if (!shop || shop->status == "closed")
    return fail<PrivateShopDailyRentQuote>("PrivateShopRentShopUnavailable");
  auto ledger = read_private_shop_rent_ledger(
      c, shop_context.economy, shop_context.state, x.catalog, x.state, id);
  if (!ledger.ok)
    return {false, {}, ledger.error};
  const auto *person = c.actor(actor);
  auto location = c.resolved_location(actor);
  if (!location.ok)
    return {false, {}, location.error};
  const auto *at = std::get_if<SettlementLocation>(&location.value);
  if (!person || !person->alive || !person->available ||
      !person->action_id.empty() ||
      shop_context.activity.in_open_battle(actor) ||
      shop_context.activity.in_interaction(actor) || !at ||
      at->place_id != shop->terms.place_id)
    return fail<PrivateShopDailyRentQuote>(
        "PrivateShopRentPayerPresenceRequired");
  PrivateShopDailyRentQuote out;
  out.source_identity = c.state().identity;
  out.actor_id = actor;
  out.command = command;
  out.shop_id = id;
  out.to_wallet_id = shop->terms.lessor_wallet_id;
  out.versions.push_back(c.read_version({ResourceKey::Kind::actor, actor}));
  if (const auto *pay = std::get_if<PrivateShopPayRentArrear>(&command)) {
    const auto *a = x.state.assessments.find(pay->assessment_fact_id);
    const auto *due =
        a ? shop_context.economy.arrears.find(a->arrear_id) : nullptr;
    if (!a || a->shop_id != shop->id ||
        a->id != ledger.value.assessment_fact_id || !due ||
        ledger.value.pending_arrear_id != due->id || due->paid_amount ||
        due->original_due != shop->terms.daily_rent)
      return fail<PrivateShopDailyRentQuote>(
          "PrivateShopRentExactUnpaidPeriodRequired");
    out.assessment_fact_id = a->id;
    out.arrear_id = due->id;
    out.from_wallet_id = pay->payer_wallet_id;
    out.amount = due->original_due;
    auto period =
        quote_private_shop_rent_period(shop->opened_tick, a->period_index);
    if (!period.ok)
      return {false, {}, period.error};
    out.period_index = period.value.index;
    out.from_tick = period.value.from_tick;
    out.through_tick = period.value.through_tick;
    if (actor == shop->terms.proprietor_actor_id) {
      if ((pay->payer_wallet_id != shop->wallet_id &&
           pay->payer_wallet_id != person->wallet_id) ||
          !pay->mandate_fact_id.empty() ||
          !pay->death_notice_binding_fact_id.empty())
        return fail<PrivateShopDailyRentQuote>(
            "PrivateShopRentVoluntaryPayerMismatch");
    } else {
      if (pay->payer_wallet_id != person->wallet_id ||
          pay->mandate_fact_id.empty() ||
          pay->death_notice_binding_fact_id.empty())
        return fail<PrivateShopDailyRentQuote>(
            "PrivateShopRentVoluntaryPayerMismatch");
      valid = authority(x, request(out, *pay));
      if (!valid.ok)
        return {false, {}, valid.error};
    }
  } else {
    if (actor != shop->terms.proprietor_actor_id)
      return fail<PrivateShopDailyRentQuote>(
          "PrivateShopRentProprietorResumeRequired");
    if (!ledger.value.suspended || !ledger.value.pending_arrear_id.empty())
      return fail<PrivateShopDailyRentQuote>(
          "PrivateShopRentResumeRequiresClearedDebt");
    for (const auto &operation : shop_context.state.operations.rows())
      if (operation.second.shop_id == id &&
          operation.second.result_fact_id.empty())
        return fail<PrivateShopDailyRentQuote>(
            "PrivateShopRentActiveOperation");
    auto period = locate_private_shop_rent_period(shop->opened_tick, c.tick());
    if (!period.ok)
      return {false, {}, period.error};
    out.period_index = period.value.index;
    out.from_tick = period.value.from_tick;
    out.through_tick = period.value.through_tick;
    out.from_wallet_id = shop->wallet_id;
    out.assessment_fact_id = period.value.index == ledger.value.period_index
                                 ? ledger.value.assessment_fact_id
                                 : StableId{};
    out.amount = out.assessment_fact_id.empty() ? shop->terms.daily_rent : 0;
  }
  const auto *payer = c.wallet(out.from_wallet_id);
  const auto *payee = c.wallet(out.to_wallet_id);
  if (!payer || !payee || payer->id == payee->id ||
      !owner_equal(payer->owner, {Owner::Kind::actor, actor, {}}) ||
      !owner_equal(payee->owner,
                   {Owner::Kind::actor, shop->terms.lessor_actor_id, {}}))
    return fail<PrivateShopDailyRentQuote>(
        "PrivateShopRentPaymentOwnerMismatch");
  auto spendable = c.spendable(payer->id);
  if (!spendable.ok)
    return {false, {}, spendable.error};
  if (spendable.value < out.amount)
    return fail<PrivateShopDailyRentQuote>(
        "PrivateShopRentFullPaymentRequired");
  Integer credited = 0;
  if (__builtin_add_overflow(payee->balance, out.amount, &credited))
    return fail<PrivateShopDailyRentQuote>("PrivateShopRentAmountOverflow");
  if (out.from_wallet_id != shop->wallet_id) {
    const auto *through = c.wallet(shop->wallet_id);
    if (!through ||
        __builtin_add_overflow(through->balance, out.amount, &credited))
      return fail<PrivateShopDailyRentQuote>("PrivateShopRentAmountOverflow");
  }
  for (const auto &wallet :
       {out.from_wallet_id, out.to_wallet_id, shop->wallet_id}) {
    bool exists = false;
    for (const auto &v : out.versions)
      exists |= v.resource.kind == ResourceKey::Kind::wallet &&
                v.resource.id == wallet;
    if (!exists)
      out.versions.push_back(
          c.read_version({ResourceKey::Kind::wallet, wallet}));
  }
  return {true, std::move(out), {}};
}
Outcome<StableId> apply_private_shop_daily_rent_in_candidate(
    CommonStore &c, LocalEconomyState &e, PrivateShopState &shops,
    PrivateShopDailyRentState &s, const PrivateShopDailyRentContext &x,
    const PrivateShopDailyRentQuote &q) {
  if (&c != &x.shops.common || &e != &x.shops.economy ||
      &shops != &x.shops.state || &s != &x.state)
    return fail<StableId>("PrivateShopRentCandidateMismatch");
  if (!same_identity(c.state().identity, q.source_identity))
    return fail<StableId>("StalePrivateShopDailyRent");
  auto fresh = preview_private_shop_daily_rent(x, q.actor_id, q.command);
  if (!fresh.ok)
    return {false, {}, fresh.error};
  const auto &v = fresh.value;
  if (q.shop_id != v.shop_id || q.assessment_fact_id != v.assessment_fact_id ||
      q.arrear_id != v.arrear_id || q.from_wallet_id != v.from_wallet_id ||
      q.to_wallet_id != v.to_wallet_id || q.amount != v.amount ||
      q.period_index != v.period_index || q.from_tick != v.from_tick ||
      q.through_tick != v.through_tick || !versions(q.versions, v.versions))
    return fail<StableId>("StalePrivateShopDailyRent");
  local_economy_detail::CandidateStateGuard<LocalEconomyState> eg(e);
  local_economy_detail::CandidateStateGuard<PrivateShopState> sg(shops);
  local_economy_detail::CandidateStateGuard<PrivateShopDailyRentState> rg(s);
  StableId source;
  const auto shop = *shops.shops.find(q.shop_id);
  if (const auto *pay = std::get_if<PrivateShopPayRentArrear>(&q.command)) {
    const auto original = *s.assessments.find(q.assessment_fact_id);
    const StableId funding =
        q.actor_id != shop.terms.proprietor_actor_id
            ? "executor_personal"
            : (q.from_wallet_id == shop.wallet_id ? "proprietor_shop"
                                                  : "proprietor_personal");
    auto paid = private_shop_rent_detail::pay(
        c, e, s, shop, original, funding, q.actor_id, q.from_wallet_id,
        pay->mandate_fact_id, pay->death_notice_binding_fact_id);
    if (!paid.ok)
      return paid;
    source = paid.value;
    if (funding == "executor_personal") {
      auto proven = authority(x, request(q, *pay), source);
      if (!proven.ok)
        return {false, {}, proven.error};
    }
  } else {
    auto id = private_shop_rent_detail::append(
        c, private_shop_rent_resumed_kind, private_shop_rent_resumed_kind,
        private_shop_daily_rent_module);
    if (!id.ok)
      return id;
    source = id.value;
    StableId assessment = q.assessment_fact_id;
    if (assessment.empty()) {
      auto made = private_shop_rent_detail::assess(
          c, e, shops, s, shop.id, q.period_index, "resume", source);
      if (!made.ok)
        return made;
      assessment = made.value;
    }
    if (!s.resumptions.insert(
            source, {source, shop.id, q.actor_id, assessment, c.tick()}))
      return fail<StableId>("PrivateShopRentDuplicateResumption");
    shops.shops.find(shop.id)->status = "open";
  }
  auto valid =
      validate_private_shop_daily_rent_sources(c, e, shops, x.catalog, s);
  if (!valid.ok)
    return {false, {}, valid.error};
  valid = validate_private_shops(x.shops);
  if (!valid.ok)
    return {false, {}, valid.error};
  eg.commit();
  sg.commit();
  rg.commit();
  return {true, source, {}};
}
} // namespace hanmo::resident
