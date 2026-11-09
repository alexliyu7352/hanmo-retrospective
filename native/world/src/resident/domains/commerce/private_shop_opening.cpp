#include "private_shop_age3.hpp"
#include "private_shop_opening_source3.hpp"
// 正常问租沿原provider声明；费用、私人本金和唯一原LE租金义务同候选建立，晚失败不得留下空铺或扣款。
#include "daily_rent/private_shop_daily_rent_business.hpp"
#include "private_shop.hpp"
#include "resident/domains/local_economy/local_economy_internal.hpp"
namespace hanmo::resident {
namespace {
using local_economy_detail::CandidateStateGuard;
using local_economy_detail::PhaseSavepoint;
template <class T> Outcome<T> fail(const StableId &code) {
  return {false, {}, common_failure(code).error};
}
bool identity(const WorldIdentity &a, const WorldIdentity &b) {
  return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch &&
         a.revision == b.revision && a.tick == b.tick;
}
Status same_candidate(CommonStore &c, LocalEconomyState &e, PrivateShopState &s,
                      const PrivateShopContext &x) {
  return &c == &x.common && &e == &x.economy && &s == &x.state
             ? common_success()
             : common_failure("PrivateShopCandidateMismatch");
}
} // namespace
Outcome<PrivateShopState> make_initial_private_shops(const CommonStore &common,
                                                     Integer version) {
  if (version != 1 && version != 2 && version != 3)
    return {false, {}, common_failure("UnsupportedPrivateShopVersion").error};
  PrivateShopState s;
  s.version = version;
  s.initialized_tick = s.last_step_tick = common.tick();
  return {true, std::move(s), {}};
}
Outcome<PrivateShopState>
make_initial_private_shops(const CommonStore &common) {
  return make_initial_private_shops(common, 1);
}
Outcome<PrivateShopOpeningQuote> preview_private_shop_opening(
    const PrivateShopContext &x, const PrivateShopMeetingContext &m,
    const StableId &actor, const PrivateShopOpen &command) {
  if (x.phase || !validate_private_shop_committed_read3(x).ok)
    return {false,
            {},
            common_failure("PrivateShopCommandRequiresCommittedClock").error};
  auto valid = validate_private_shops(x);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto *p = x.catalog.premises.find(command.premises_id);
  if (!p)
    return fail<PrivateShopOpeningQuote>("UnknownPrivateShopPremises");
  for (const auto &row : x.state.shops.rows())
    if (row.second.status != "closed" &&
        (row.second.terms.proprietor_actor_id == actor ||
         row.second.terms.premises_id == p->id))
      return fail<PrivateShopOpeningQuote>("PrivateShopAlreadyOccupied");
  for (const auto &id : {actor, p->lessor_actor_id}) {
    const auto *person = x.common.actor(id);
    if (!person || !person->alive || !person->available ||
        !person->action_id.empty() || x.activity.in_open_battle(id))
      return fail<PrivateShopOpeningQuote>("PrivateShopPartyUnavailable");
    auto age = read_private_shop_party_adult(x, id);
    if (!age.ok)
      return {false, {}, age.error};
    if (!age.value.adult)
      return fail<PrivateShopOpeningQuote>("PrivateShopAdultRequired");
  }
  const auto *role = x.common.state().roles.find(p->lessor_role_id);
  if (!role || !role->enabled || role->actor_id != p->lessor_actor_id)
    return fail<PrivateShopOpeningQuote>("PrivateShopLessorRoleUnavailable");
  auto agreement =
      preview_authored_provider_source(x.common, m.state, x.interaction_catalog,
                                       m.access, actor, p->lessor_actor_id);
  if (!agreement.ok)
    return {false, {}, agreement.error};
  const auto &a = agreement.value.meeting;
  if (a.dialogue_id != p->agreement_dialogue_id ||
      a.node_id != p->agreement_node_id || a.context.place_id != p->place_id ||
      a.context.role_id != p->lessor_role_id)
    return fail<PrivateShopOpeningQuote>("PrivateShopLeaseDeclarationRequired");
  auto eligibility = read_private_shop_opening_eligibility(
      x.common, x.common_catalog, x.family_catalog, x.clients, x.permissions,
      *p, actor, nullptr, x.study_history, x.skill_origins, &x);
  if (!eligibility.ok)
    return {false, {}, eligibility.error};
  auto assets = quote_private_shop_opening_assets(x.common, *p, actor);
  if (!assets.ok)
    return {false, {}, assets.error};
  PrivateShopOpeningQuote quote{x.common.state().identity,
                                actor,
                                p->id,
                                std::move(assets.value),
                                std::move(eligibility.value),
                                std::move(agreement.value)};
  if (x.daily_rent) {
    auto period = quote_private_shop_rent_period(x.common.tick(), 0);
    if (!period.ok)
      return {false, {}, period.error};
    quote.daily_rent = PrivateShopOpeningRentTerms{
        x.daily_rent_catalog->profile, x.daily_rent_catalog->arrears_policy,
        p->daily_rent, period.value.from_tick, period.value.through_tick};
  }
  return {true, std::move(quote), {}};
}
Outcome<PrivateShopOpeningReceipt> open_private_shop_in_candidate(
    CommonStore &c, LocalEconomyState &economy, PrivateShopState &state,
    const PrivateShopContext &x, const PrivateShopMeetingContext &meeting,
    const PrivateShopOpeningQuote &quote,
    PrivateShopDailyRentState *daily_rent) {
  auto same = same_candidate(c, economy, state, x);
  if (!same.ok)
    return {false, {}, same.error};
  if (daily_rent != x.daily_rent)
    return fail<PrivateShopOpeningReceipt>("PrivateShopRentCandidateMismatch");
  if (!identity(quote.source_identity, c.state().identity))
    return fail<PrivateShopOpeningReceipt>("StalePrivateShopOpening");
  auto proof = revalidate_authored_provider_source(
      c, meeting.state, x.interaction_catalog, meeting.access, quote.agreement);
  if (!proof.ok)
    return {false, {}, proof.error};
  auto current = preview_private_shop_opening(x, meeting, quote.actor_id,
                                              {quote.premises_id});
  if (!current.ok)
    return {false, {}, current.error};
  // 原报价财货读版本由asset planner再次逐项重验；不按新余额静默成交。
  if (!(current.value.daily_rent == quote.daily_rent) ||
      !private_shop_opening_terms_equal(current.value.assets.terms,
                                        quote.assets.terms) ||
      current.value.assets.total_private_cash !=
          quote.assets.total_private_cash ||
      current.value.agreement.meeting.session_id !=
          quote.agreement.meeting.session_id ||
      current.value.agreement.meeting.node_id !=
          quote.agreement.meeting.node_id ||
      quote.assets.terms.proprietor_actor_id != quote.actor_id ||
      quote.assets.terms.premises_id != quote.premises_id)
    return fail<PrivateShopOpeningReceipt>("StalePrivateShopOpening");
  CandidateStateGuard<LocalEconomyState> economy_guard(economy);
  CandidateStateGuard<PrivateShopState> shop_guard(state);
  std::unique_ptr<CandidateStateGuard<PrivateShopDailyRentState>> rent_guard;
  if (daily_rent)
    rent_guard =
        std::make_unique<CandidateStateGuard<PrivateShopDailyRentState>>(
            *daily_rent);
  auto id = c.next_id(private_shop_open_prefix);
  if (!id.ok)
    return {false, {}, id.error};
  auto appended =
      c.append_fact({id.value, private_shop_open_kind, private_shop_module});
  if (!appended.ok)
    return {false, {}, appended.error};
  auto assets = prepare_private_shop_opening_assets(c, quote.assets, id.value);
  if (!assets.ok)
    return {false, {}, assets.error};
  auto obligation = c.next_id("private_shop_rent");
  if (!obligation.ok)
    return {false, {}, obligation.error};
  Integer next = 0;
  if (__builtin_add_overflow(c.tick(), Integer{2}, &next))
    return fail<PrivateShopOpeningReceipt>("PrivateShopTickOverflow");
  const auto &t = current.value.assets.terms;
  RecurringObligation rent;
  rent.id = obligation.value;
  rent.principal = {SubjectKind::actor, t.proprietor_actor_id};
  rent.beneficiary = {SubjectKind::actor, t.lessor_actor_id};
  rent.kind = ObligationKind::contract_payment;
  rent.source_wallet_or_inventory = assets.value.wallet_id;
  rent.destination = t.lessor_wallet_id;
  rent.rate_profile = t.rent_rate_profile;
  rent.authority_ref = id.value;
  rent.period_ticks = 2;
  rent.next_due_tick = next;
  rent.active = false;
  if (!economy.obligations.insert(rent.id, rent) ||
      !economy.obligation_remainders.insert(rent.id, 0))
    return fail<PrivateShopOpeningReceipt>("PrivateShopRentAlreadyExists");
  const auto &a = current.value.agreement.meeting;
  const auto &q = current.value.eligibility;
  // 原许可两个持久引用复制到临时record前独立计账；表插入仍走原唯一owner。


  auto permission_record_memory = CheckpointMemoryCharge(0);
  PrivateShopRecord record;
  record.id = id.value;
  record.wallet_id = assets.value.wallet_id;
  record.inventory_id = assets.value.inventory_id;
  record.rent_obligation_id = rent.id;
  record.terms = t;
  record.opened_tick = c.tick();
  if (state.version == 3) {
    auto opened = read_private_shop_opening_write_moment3(x);
    if (!opened.ok)
      return {false, {}, opened.error};
    record.opened_at = std::move(opened.value);
  }
  record.meeting = {a.actor_id,       a.provider_actor_id, a.session_id,
                    a.dialogue_id,    a.node_id,           a.context.place_id,
                    a.context.role_id};
  record.qualification.minimum_commerce_xp = q.minimum_commerce_xp;
  record.qualification.minimum_commerce_level = q.minimum_commerce_level;
  record.qualification.commerce_study_source_ids = q.commerce_study_source_ids;
  record.qualification.permit_fact_id =
      checkpoint_fresh_string_copy(q.permission.source().permit_fact_id);
  record.qualification.clearance_fact_id =
      checkpoint_fresh_string_copy(q.permission.source().clearance_fact_id);
  for (const auto &client : q.original_clients) {
    const PrivateShopOpeningBoundary boundary{id.value, record.opened_tick};
    auto completion = validate_private_shop_client_completion_source(
        c, client, record.opened_tick, &boundary);
    if (!completion.ok)
      return {false, {}, completion.error};
    record.qualification.original_clients.push_back(
        {client.original_client, client.delivery_beneficiary, client.task_id,
         client.settlement_id, client.template_id, client.payment_fact_id,
         client.completed_tick, client.objective_fact_ids});
  }
  if (!state.shops.insert(record.id, record))
    return fail<PrivateShopOpeningReceipt>("PrivateShopAlreadyExists");
  auto applied = c.apply_resources(assets.value.resources);
  if (!applied.ok)
    return {false, {}, applied.error};
  if (daily_rent) {
    auto rent = initialize_private_shop_rent_in_candidate(
        c, economy, state, *daily_rent, id.value);
    if (!rent.ok)
      return {false, {}, rent.error};
  }
  auto valid = validate_private_shops(x);
  if (!valid.ok)
    return {false, {}, valid.error};
  if (rent_guard)
    rent_guard->commit();
  shop_guard.commit();
  economy_guard.commit();
  return {true,
          {id.value, record.wallet_id, record.inventory_id,
           record.rent_obligation_id},
          {}};
}
Outcome<PrivateShopOpeningReceipt> open_private_shop(
    CommonStore &c, LocalEconomyState &e, PrivateShopState &s,
    const PrivateShopContext &x, const PrivateShopMeetingContext &m,
    const PrivateShopOpeningQuote &q, PrivateShopDailyRentState *daily_rent) {
  auto same = same_candidate(c, e, s, x);
  if (!same.ok)
    return {false, {}, same.error};
  PhaseSavepoint transaction(c);
  if (!transaction.token().ok)
    return {false, {}, transaction.token().error};
  CandidateStateGuard<LocalEconomyState> economy_guard(e);
  CandidateStateGuard<PrivateShopState> shop_guard(s);
  std::unique_ptr<CandidateStateGuard<PrivateShopDailyRentState>> rent_guard;
  if (daily_rent)
    rent_guard =
        std::make_unique<CandidateStateGuard<PrivateShopDailyRentState>>(
            *daily_rent);
  auto result = open_private_shop_in_candidate(c, e, s, x, m, q, daily_rent);
  if (!result.ok)
    return result;
  auto committed = transaction.commit();
  if (!committed.ok)
    return {false, {}, committed.error};
  if (rent_guard)
    rent_guard->commit();
  shop_guard.commit();
  economy_guard.commit();
  return result;
}
} // namespace hanmo::resident
