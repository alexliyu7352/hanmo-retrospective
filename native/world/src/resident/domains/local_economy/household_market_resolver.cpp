#include "household_market_resolver.hpp"
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *c, const StableId &id = {}) {
  throw Failure{common_failure(c, id).error};
}
template <class T> T take(Outcome<T> v) {
  if (!v.ok)
    throw Failure{std::move(v.error)};
  return std::move(v.value);
}
void take(Status v) {
  if (!v.ok)
    throw Failure{std::move(v.error)};
}
template <class T, class F> Outcome<T> guard(F f) {
  try {
    return {true, f(), {}};
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
void context(const CommonStore &c, const HouseholdPurchaseCatalog &catalog,
             const HouseholdPurchaseState &s,
             const PrivateShopHouseholdCustodyContext *x) {
  if (x &&
      (&x->market.operation.shops.common != &c ||
       &x->market.operation.markets != &catalog || &x->market.purchases != &s))
    fail("HouseholdMarketResolverSameWorldRequired");
}
const HouseholdMarketServiceDefinition *
public_service(const HouseholdPurchaseCatalog &catalog,
               const StableId &market) {
  const HouseholdMarketServiceDefinition *r = nullptr;
  for (const auto &v : catalog.market_services.rows())
    if (v.second.market_ref == market) {
      if (r)
        fail("DuplicateHouseholdMarketService", market);
      r = &v.second;
    }
  if (!r)
    fail("HouseholdPublicMarketServiceRequired", market);
  return r;
}
} // namespace
Outcome<const HouseholdMarketServiceDefinition *>
read_household_market_service_preview(
    const CommonStore &c, const HouseholdPurchaseCatalog &catalog,
    const HouseholdPurchaseState &s,
    const PrivateShopHouseholdCustodyContext *x, const HouseholdPurchasePlan &p,
    const HouseholdRetailSeller &seller) {
  return guard<const HouseholdMarketServiceDefinition *>([&] {
    context(c, catalog, s, x);
    if (const auto *private_seller =
            std::get_if<PrivateHouseholdRetailSeller>(&seller)) {
      if (!x)
        fail("PrivateShopCustodySourcesRequired", p.plan_id);
      auto *r = take(
          read_private_shop_household_service_current(*x, *private_seller));
      if (r->market_ref != p.known_market_ref)
        fail("HouseholdPrivateMarketMismatch", p.plan_id);
      return r;
    }
    if (std::get<PublicHouseholdRetailSeller>(seller).market_shop_id !=
        p.known_market_ref)
      fail("HouseholdPublicMarketMismatch", p.plan_id);
    return public_service(catalog, p.known_market_ref);
  });
}
Outcome<const HouseholdMarketServiceDefinition *>
read_household_market_service_accepted(
    const CommonStore &c, const HouseholdPurchaseCatalog &catalog,
    const HouseholdPurchaseState &s,
    const PrivateShopHouseholdCustodyContext *x,
    const HouseholdPurchaseAcceptedTerms &a) {
  return guard<const HouseholdMarketServiceDefinition *>([&] {
    context(c, catalog, s, x);
    if (s.accepted_terms.find(a.source.id) != &a)
      fail("OwnedHouseholdMarketAcceptanceRequired", a.source.id);
    if (std::holds_alternative<PrivateHouseholdRetailSeller>(a.seller)) {
      if (!x)
        fail("PrivateShopCustodySourcesRequired", a.source.id);
      return take(read_private_shop_household_service_accepted(*x, a));
    }
    if (std::get<PublicHouseholdRetailSeller>(a.seller).market_shop_id !=
        a.plan.known_market_ref)
      fail("HouseholdPublicMarketMismatch", a.source.id);
    return public_service(catalog, a.plan.known_market_ref);
  });
}
Outcome<const HouseholdMarketServiceDefinition *>
read_household_market_service_for_plan(
    const CommonStore &c, const HouseholdPurchaseCatalog &catalog,
    const HouseholdPurchaseState &s,
    const PrivateShopHouseholdCustodyContext *x,
    const HouseholdPurchasePlan &p) {
  return guard<const HouseholdMarketServiceDefinition *>([&] {
    context(c, catalog, s, x);
    const HouseholdPurchaseAcceptedTerms *a = nullptr;
    for (const auto &v : s.accepted_terms.rows())
      if (v.second.plan.plan_id == p.plan_id) {
        if (a)
          fail("ExplicitHouseholdPurchaseServiceRequired", p.plan_id);
        a = &v.second;
      }
    if (a)
      return take(read_household_market_service_accepted(c, catalog, s, x, *a));
    return public_service(catalog, p.known_market_ref);
  });
}
Status validate_household_resolved_market_service(
    const CommonStore &c, const HouseholdPurchaseCatalog &catalog,
    const HouseholdPurchaseState &s,
    const PrivateShopHouseholdCustodyContext *x, const HouseholdPurchasePlan &p,
    const HouseholdMarketServiceDefinition &service) {
  try {
    context(c, catalog, s, x);
    if (service.market_ref != p.known_market_ref)
      fail("HouseholdResolvedMarketMismatch", p.plan_id);
    if (service.custody_terms_ref.domain == "local_economy" &&
        service.custody_terms_ref.kind ==
            "private_shop_household_custody_accepted") {
      if (!x)
        fail("PrivateShopCustodySourcesRequired", p.plan_id);
      auto *r = take(read_private_shop_household_custody(
          *x, {service.custody_terms_ref, 1}));
      if (&r->accepted_service != &service)
        fail("OriginalPrivateHouseholdServiceRequired", p.plan_id);
    } else if (public_service(catalog, p.known_market_ref) != &service)
      fail("OriginalPublicHouseholdServiceRequired", p.plan_id);
    return common_success();
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
Outcome<const StableId *> read_household_original_market_place(
    const CommonStore &c, const HouseholdPurchaseCatalog &catalog,
    const HouseholdPurchaseState &s,
    const PrivateShopHouseholdCustodyContext *x, const StableId &id) {
  return guard<const StableId *>([&] {
    context(c, catalog, s, x);
    const StableId *place = nullptr;
    auto accept = [&](const StableId &v) {
      if (v.empty() || (place && *place != v))
        fail("AmbiguousHouseholdOriginalMarketPlace", id);
      place = &v;
    };
    for (const auto &v : catalog.market_services.rows())
      if (v.second.market_ref == id)
        accept(v.second.place_id);
    if (x) {
      take(validate_private_shop_household_custody_catalog(*x));
      for (const auto &v : x->catalog.policies.rows()) {
        const auto *p = x->market.operation.shops.catalog.premises.find(
            v.second.premises_id);
        if (p && p->market_shop_id == id)
          accept(p->place_id);
      }
    }
    if (!place)
      fail("OriginalHouseholdMarketPlaceRequired", id);
    return place;
  });
}
Outcome<bool> household_market_catalog_contains(
    const CommonStore &c, const HouseholdPurchaseCatalog &catalog,
    const HouseholdPurchaseState &s,
    const PrivateShopHouseholdCustodyContext *x, const StableId &id) {
  return guard<bool>([&] {
    context(c, catalog, s, x);
    for (const auto &v : catalog.market_services.rows())
      if (v.second.market_ref == id)
        return true;
    if (!x)
      return false;
    take(validate_private_shop_household_custody_catalog(*x));
    for (const auto &v : x->catalog.policies.rows()) {
      const auto *p =
          x->market.operation.shops.catalog.premises.find(v.second.premises_id);
      if (p && p->market_shop_id == id)
        return true;
    }
    return false;
  });
}
} // namespace hanmo::resident
