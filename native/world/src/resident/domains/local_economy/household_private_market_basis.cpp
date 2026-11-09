#include "household_private_market_basis.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
template <class T> T take(Outcome<T> v) {
  if (!v.ok)
    throw Failure{std::move(v.error)};
  return std::move(v.value);
}
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
int cmp(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b));
}
void context(const PrivateHouseholdMarketBasisContext &x) {
  const auto &o = x.operation;
  const auto &t = x.inventory_titles;
  if (&t.common != &o.shops.common || &t.catalog != &o.shops.common_catalog ||
      t.foundation != &o.body.state ||
      t.foundation_catalog != &o.body.catalog ||
      t.family_catalog != &o.shops.family_catalog || !t.clock ||
      cmp(take(t.clock->read_current_moment()),
          take(o.clock.read_current_moment())))
    fail("PrivateHouseholdMarketSameWorldRequired");
}
PrivateHouseholdMarketBasis
basis(const PrivateHouseholdMarketBasisContext &x,
      const PrivateHouseholdRetailSeller &seller,
      const HouseholdPurchaseAcceptedTerms *purchase) {
  context(x);
  const auto &o = x.operation;
  const auto &c = o.shops.common;
  const auto *op = o.shops.state.operations.find(seller.operation_id);
  const auto *shop = o.shops.state.shops.find(seller.shop_id);
  if (!op || !shop || op->shop_id != shop->id ||
      op->actor_id != shop->terms.proprietor_actor_id ||
      !op->staff_contract_id.empty())
    fail("OriginalSelfOperatedPrivateMarketRequired", seller.operation_id);
  const MomentOwnerSource source{private_shop_operation_started_source(op->id),
                                 2};
  const auto *a =
      take(read_original_private_shop_operation_acceptance(o, source));
  if (take(read_original_private_shop_opening(
          o.shops, a->original_shop_epoch_ref)) != shop)
    fail("PrivateMarketOriginalEpochMismatch", shop->id);
  const MarketServiceDefinition *definition = nullptr;
  for (const auto &d : o.markets.market_service_definitions)
    if (d.definition_ref.id == a->market_definition_ref.id &&
        d.definition_ref.version == a->market_definition_ref.version &&
        d.definition_ref.catalog_digest ==
            a->market_definition_ref.catalog_digest) {
      if (definition)
        fail("DuplicatePrivateMarketDefinition", d.definition_ref.id);
      definition = &d;
    }
  if (!definition)
    fail("OriginalPrivateMarketDefinitionRequired", shop->id);
  const auto *actor = c.actor(shop->terms.proprietor_actor_id);
  if (!actor || actor->inventory_id.empty() ||
      actor->inventory_id == shop->inventory_id)
    fail("OriginalProprietorPersonalInventoryRequired", shop->id);
  NativePrivateInventoryTitleRead titles(x.inventory_titles);
  auto title = purchase
                   ? take(titles.read_private_inventory_title_before_source(
                         actor->inventory_id, purchase->source))
                   : take(titles.read_current_private_inventory_title(
                         actor->inventory_id));
  if (title.inventory_id != actor->inventory_id ||
      title.legal_owner.kind != Owner::Kind::actor ||
      title.legal_owner.id != actor->id || title.legal_owner.extra)
    fail("PrivateMarketPersonalInventoryTitleMismatch", actor->inventory_id);
  const auto at =
      purchase ? purchase->accepted_at : take(o.clock.read_current_moment());
  if (cmp(a->accepted_at, at) > 0 ||
      cmp(at, a->committed_service_window.to) >= 0)
    fail("PrivateMarketAcceptanceOutsideOriginalWindow", op->id);
  if (purchase) {
    if (purchase->plan.known_market_ref != shop->terms.market_shop_id ||
        purchase->item_id != op->item_id ||
        take(read_actual_common_fact_position(c, source.source_ref)) >=
            take(read_actual_common_fact_position(c, purchase->source)))
      fail("PrivateMarketOriginalPurchaseScopeMismatch", purchase->source.id);
  } else {
    const auto *action = c.action(op->id);
    if (shop->status != "open" || !op->result_fact_id.empty() || !action ||
        action->actor_id != actor->id || action->kind != "private_shop_operate")
      fail("ActualPrivateMarketOperationRequired", op->id);
  }
  return {shop,
          op,
          a,
          definition,
          std::move(title),
          source,
          a->original_shop_epoch_ref};
}
template <class F> Outcome<PrivateHouseholdMarketBasis> guard(F f) {
  try {
    return {true, f(), {}};
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
} // namespace
Outcome<PrivateHouseholdMarketBasis>
read_current_private_household_market_basis(
    const PrivateHouseholdMarketBasisContext &x,
    const PrivateHouseholdRetailSeller &seller) {
  return guard([&] { return basis(x, seller, nullptr); });
}
Outcome<PrivateHouseholdMarketBasis>
read_private_household_market_basis_before_purchase(
    const PrivateHouseholdMarketBasisContext &x,
    const HouseholdPurchaseState &state,
    const HouseholdPurchaseAcceptedTerms &a) {
  return guard([&] {
    if (&state != &x.purchases)
      fail("PrivateMarketPurchaseOwnedGraphMismatch");
    if (a.source.domain != "local_economy" ||
        a.source.kind != "household_purchase_accepted" ||
        state.accepted_terms.find(a.source.id) != &a)
      fail("OwnedPrivateMarketPurchaseAcceptanceRequired", a.source.id);
    const auto *p = state.purchases.find(a.source.id);
    const auto *seller = std::get_if<PrivateHouseholdRetailSeller>(&a.seller);
    if (!p || !seller || !same_source_ref(p->accepted_terms_ref, a.source) ||
        p->plan_ref != a.plan.plan_id)
      fail("PrivateMarketPurchaseRootMismatch", a.source.id);
    return basis(x, *seller, &a);
  });
}
} // namespace hanmo::resident
