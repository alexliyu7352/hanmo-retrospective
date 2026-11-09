// 本人到铺才能补本金或改价；原Common版本与报价身份重验，筹备中的空店不会因此获得营业收入。
#include "private_shop.hpp"
#include "resident/domains/local_economy/local_economy_internal.hpp"
#include "resident/storage/money_transfer_recipe.hpp"
namespace hanmo::resident {
namespace {
using local_economy_detail::CandidateStateGuard;
using local_economy_detail::PhaseSavepoint;
template <class T> Outcome<T> fail(const StableId &code) {
  return {false, {}, common_failure(code).error};
}
bool same_identity(const WorldIdentity &a, const WorldIdentity &b) {
  return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch &&
         a.revision == b.revision && a.tick == b.tick;
}
StableId shop_id(const PrivateShopManagementCommand &c) {
  return std::visit([](const auto &value) { return value.shop_id; }, c);
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
} // namespace
Outcome<PrivateShopManagementQuote>
preview_private_shop_management(const PrivateShopContext &x,
                                const StableId &actor,
                                const PrivateShopManagementCommand &command) {
  if (x.phase || !validate_private_shop_committed_read3(x).ok)
    return {false,
            {},
            common_failure("PrivateShopCommandRequiresCommittedClock").error};
  auto valid = validate_private_shops(x);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto *shop = x.state.shops.find(shop_id(command));
  if (!shop)
    return fail<PrivateShopManagementQuote>("UnknownPrivateShop");
  if (shop->status == "closed")
    return fail<PrivateShopManagementQuote>("PrivateShopAlreadyClosed");
  const auto *person = x.common.actor(actor);
  if (!person || actor != shop->terms.proprietor_actor_id)
    return fail<PrivateShopManagementQuote>("PrivateShopProprietorRequired");
  auto location = x.common.resolved_location(actor);
  if (!location.ok)
    return {false, {}, location.error};
  const auto *place = std::get_if<SettlementLocation>(&location.value);
  if (!person->alive || !person->available || !person->action_id.empty() ||
      x.activity.in_open_battle(actor) || !place ||
      place->place_id != shop->terms.place_id)
    return fail<PrivateShopManagementQuote>("PrivateShopPresenceRequired");
  PrivateShopManagementQuote out{
      x.common.state().identity,
      actor,
      command,
      {x.common.read_version({ResourceKey::Kind::actor, actor})}};
  if (const auto *funding = std::get_if<PrivateShopFund>(&command)) {
    if (funding->amount <= 0)
      return fail<PrivateShopManagementQuote>("InvalidPrivateShopCapital");
    const auto *payer = x.common.wallet(person->wallet_id);
    const auto *target = x.common.wallet(shop->wallet_id);
    if (!payer || !target || payer->id == target->id ||
        !owner_equal(payer->owner, {Owner::Kind::actor, actor, {}}) ||
        !owner_equal(target->owner, payer->owner))
      return fail<PrivateShopManagementQuote>(
          "PrivateShopPrivateWalletRequired");
    Integer balance = 0;
    if (__builtin_add_overflow(target->balance, funding->amount, &balance))
      return fail<PrivateShopManagementQuote>("PrivateShopCapitalOverflow");
    auto available = x.common.spendable(payer->id);
    if (!available.ok)
      return {false, {}, available.error};
    if (available.value < funding->amount)
      return fail<PrivateShopManagementQuote>("PrivateShopInsufficientCapital");
    out.versions.push_back(
        x.common.read_version({ResourceKey::Kind::wallet, payer->id}));
    out.versions.push_back(
        x.common.read_version({ResourceKey::Kind::wallet, target->id}));
  } else {
    const auto &price = std::get<PrivateShopSetMarkup>(command);
    if (price.markup_basis_points != 0 && price.markup_basis_points != 500 &&
        price.markup_basis_points != 1000)
      return fail<PrivateShopManagementQuote>("InvalidPrivateShopMarkup");
    if (price.markup_basis_points == shop->markup_basis_points)
      return fail<PrivateShopManagementQuote>("PrivateShopMarkupUnchanged");
  }
  return {true, std::move(out), {}};
}
Outcome<StableId>
manage_private_shop_in_candidate(CommonStore &c, PrivateShopState &s,
                                 const PrivateShopContext &x,
                                 const PrivateShopManagementQuote &q) {
  if (&c != &x.common || &s != &x.state)
    return fail<StableId>("PrivateShopCandidateMismatch");
  if (!same_identity(q.source_identity, c.state().identity))
    return fail<StableId>("StalePrivateShopManagement");
  auto current = preview_private_shop_management(x, q.actor_id, q.command);
  if (!current.ok)
    return {false, {}, current.error};
  if (!same_versions(q.versions, current.value.versions))
    return fail<StableId>("StalePrivateShopManagement");
  CandidateStateGuard<PrivateShopState> guard(s);
  const auto id = shop_id(q.command);
  const auto terms = s.shops.find(id)->terms;
  const auto target = s.shops.find(id)->wallet_id;
  const bool funding = std::holds_alternative<PrivateShopFund>(q.command);
  auto fact =
      c.next_id(funding ? "private_shop_capital" : "private_shop_markup");
  if (!fact.ok)
    return fact;
  auto appended = c.append_fact(
      {fact.value,
       funding ? "private_shop_capital_added" : "private_shop_markup_changed",
       private_shop_module});
  if (!appended.ok)
    return {false, {}, appended.error};
  if (funding) {
    const auto amount = std::get<PrivateShopFund>(q.command).amount;
    if (!s.capital.insert(fact.value,
                          {fact.value, id, q.actor_id, terms.payer_wallet_id,
                           target, c.tick(), amount}))
      return fail<StableId>("PrivateShopDuplicateCapital");
    ResourceRecipe recipe;
    recipe.expected_versions = q.versions;
    auto applied = resource_recipe::append_money_transfer(
        recipe, terms.payer_wallet_id, target, amount,
        resource_recipe::MoneyTransferPolicy::spendable_debit_original_credit);
    if (!applied.ok)
      return {false, {}, applied.error};
    applied = c.apply_resources(recipe);
    if (!applied.ok)
      return {false, {}, applied.error};
  } else {
    auto &shop = *s.shops.find(id);
    const auto markup =
        std::get<PrivateShopSetMarkup>(q.command).markup_basis_points;
    if (!s.markups.insert(fact.value, {fact.value, id, q.actor_id, c.tick(),
                                       shop.markup_basis_points, markup}))
      return fail<StableId>("PrivateShopDuplicateMarkup");
    shop.markup_basis_points = markup;
  }
  auto valid = validate_private_shops(x);
  if (!valid.ok)
    return {false, {}, valid.error};
  guard.commit();
  return fact;
}
Outcome<StableId> manage_private_shop(CommonStore &c, PrivateShopState &s,
                                      const PrivateShopContext &x,
                                      const PrivateShopManagementQuote &q) {
  if (&c != &x.common || &s != &x.state)
    return fail<StableId>("PrivateShopCandidateMismatch");
  PhaseSavepoint transaction(c);
  if (!transaction.token().ok)
    return {false, {}, transaction.token().error};
  CandidateStateGuard<PrivateShopState> guard(s);
  auto result = manage_private_shop_in_candidate(c, s, x, q);
  if (!result.ok)
    return result;
  auto committed = transaction.commit();
  if (!committed.ok)
    return {false, {}, committed.error};
  guard.commit();
  return result;
}
} // namespace hanmo::resident
