#include "resident/domains/growth/growth_model.hpp"
#include "economic_inventory_placement.hpp"
#include "resident/domains/commerce/private_shop.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> v) {
  if (!v.ok)
    throw Failure{v.error};
  return std::move(v.value);
}
template <class T, class F> Outcome<T> read(F &&f) {
  try {
    return {true, f(), {}};
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
const PrivateShopRecord *
private_shop_inventory(const EconomicFixedInventoryPlacementContext &x,
                       const StableId &id) {
  if (!x.private_shops)
    return nullptr;
  const auto &shops = *x.private_shops;
  if (&shops.common != &x.common || &shops.common_catalog != &x.shared ||
      &shops.economy_catalog != &x.economy || shops.markets != &x.growth ||
      shops.estate_catalog != &x.estates)
    fail("PrivateShopInventorySnapshotMismatch", id);
  const PrivateShopRecord *found = nullptr;
  for (const auto &row : shops.state.shops.rows())
    if (row.second.inventory_id == id) {
      if (found || row.first != row.second.id)
        fail("AmbiguousPrivateShopInventorySource", id);
      const auto *original = take(read_original_private_shop_opening(
          shops, {private_shop_module, private_shop_open_kind, row.first}));
      const auto sequence = organization_instance_sequence(
          original->id, private_shop_open_prefix);
      const auto *inventory = x.common.inventory(id);
      if (!sequence ||
          original->inventory_id !=
              "private_shop_inventory:" + std::to_string(*sequence) ||
          !inventory ||
          !owner_equal(
              inventory->owner,
              {Owner::Kind::actor, original->terms.proprietor_actor_id, {}}) ||
          inventory->capacity != original->terms.inventory_capacity ||
          x.shared.required_inventories.find(id))
        fail("PrivateShopOriginalInventoryCreationMismatch", id);
      found = original;
    }
  return found;
}
SourceRef site(const EconomicFixedInventoryPlacementContext &x,
               const StableId &id) {
  const auto *place = x.shared.places.find(id);
  const auto *town = x.travel.settlements.find(id);
  if (place && town)
    fail("AmbiguousFixedInventorySite", id);
  if (place) {
    if (place->id != id)
      fail("InvalidFixedInventoryPlace", id);
    return {"common_catalog", "place", id};
  }
  if (town) {
    if (town->id != id || town->entry_place.empty() ||
        !x.shared.places.find(town->entry_place))
      fail("InvalidFixedInventorySettlement", id);
    return {"travel_catalog", "settlement", id};
  }
  fail("OriginalFixedInventorySiteRequired", id);
}
FixedInventoryPlacement
placement(const EconomicFixedInventoryPlacementContext &x, const StableId &id) {
  const auto *shop = private_shop_inventory(x, id);
  if (id.empty() || !x.common.inventory(id) ||
      (!x.shared.required_inventories.find(id) && !shop))
    fail("OriginalFixedInventoryRequired", id);
  for (const auto &actor : x.common.state().actors.rows())
    if (actor.second.inventory_id == id)
      fail("FixedInventoryAliasesActorContainer", id);
  for (const auto &mobile : x.travel.mobile_inventory_definitions.rows())
    if (mobile.second.inventory_id == id)
      fail("FixedInventoryAliasesPortableContainer", id);
  FixedInventoryPlacement result;
  result.inventory_id = id;
  std::optional<MovementAnchor> actual;
  auto add = [&](const StableId &original_site, const SourceRef &source) {
    auto ref = site(x, original_site);
    auto anchor =
        take(read_original_spatial_site_anchor(x.shared, x.travel, ref));
    if (actual && !take(same_spatial_anchor(*actual, anchor)))
      fail("ConflictingOriginalFixedInventorySites", id);
    if (!actual) {
      actual = std::move(anchor);
      result.original_site_ref = std::move(ref);
    }
    for (const auto &old : result.original_sources)
      if (same_source_ref(old.source_ref, source))
        fail("DuplicateFixedInventoryCatalogSource", source.id);
    result.original_sources.push_back({source, 1});
  };
  if (x.economy.enabled)
    for (const auto &row : x.economy.cohorts.rows())
      if (row.second.private_inventory_id == id) {
        const auto &c = row.second;
        if (c.id != row.first || c.household_group_id.empty() ||
            !x.economy.households.find(c.household_group_id))
          fail("OriginalHouseholdInventorySourceRequired", row.first);
        add(c.site_id,
            {"local_economy_catalog", "cohort_home_inventory", c.id});
      }
  for (const auto &row : x.estates.estates.rows())
    if (row.second.inventory_id == id) {
      if (row.first != row.second.id)
        fail("OriginalEstateInventorySourceMismatch", row.first);
      add(row.second.place_id,
          {"estate_catalog", "estate_inventory", row.first});
    }
  if (x.growth.economy)
    for (const auto &row : x.growth.shops.rows())
      if (row.second.inventory_id == id) {
        if (row.first != row.second.id)
          fail("OriginalMarketInventorySourceMismatch", row.first);
        add(row.second.place_id,
            {"growth_catalog", "shop_inventory", row.first});
      }
  // 动态铺库恰一真实开店源，不能与作者静态库或另一店铺别名混用。
  if (shop) {
    if (actual)
      fail("PrivateShopInventoryAliasesInitialContainer", id);
    add(shop->terms.place_id,
        {private_shop_module, private_shop_open_kind, shop->id});
  }
  // A production input_inventory is merely an input source. No declaration says
  // every remote input store is situated at unit.site_id; it is not a locator.
  if (!actual || result.original_sources.empty())
    fail("OriginalFixedInventoryPlacementRequired", id);
  return result;
}
} // namespace
Outcome<FixedInventoryPlacement> NativeEconomicFixedInventoryPlacementRead::
    read_original_fixed_inventory_placement(const StableId &id) const {
  return read<FixedInventoryPlacement>([&] { return placement(x_, id); });
}
Outcome<std::vector<StableId>> NativeEconomicFixedInventoryPlacementRead::
    enumerate_original_fixed_inventory_ids() const {
  return read<std::vector<StableId>>([&] {
    std::vector<StableId> ids;
    auto add = [&](const StableId &id) {
      if (id.empty())
        fail("OriginalFixedInventoryRequired");
      if (std::find(ids.begin(), ids.end(), id) == ids.end())
        ids.push_back(id);
    };
    if (x_.economy.enabled)
      for (const auto &row : x_.economy.cohorts.rows())
        add(row.second.private_inventory_id);
    for (const auto &row : x_.estates.estates.rows())
      add(row.second.inventory_id);
    if (x_.growth.economy)
      for (const auto &row : x_.growth.shops.rows())
        add(row.second.inventory_id);
    if (x_.private_shops)
      for (const auto &row : x_.private_shops->state.shops.rows())
        add(row.second.inventory_id);
    for (const auto &id : ids)
      placement(x_, id);
    return ids;
  });
}
Status NativeEconomicFixedInventoryPlacementRead::
    validate_original_fixed_inventory_creation(const CommonStore &common,
                                               const CommonCatalog &shared,
                                               const StableId &id) const {
  try {
    if (&common != &x_.common || &shared != &x_.shared)
      fail("FixedInventoryCreationSnapshotMismatch", id);
    placement(x_, id);
    return common_success();
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
Outcome<FixedInventoryPlacement> read_economic_fixed_inventory_placement_source(
    const EconomicFixedInventoryPlacementContext &x,
    const MomentOwnerSource &source) {
  return read<FixedInventoryPlacement>([&] {
    if (source.source_schema != 1)
      fail("UnsupportedFixedInventorySourceSchema", source.source_ref.id);
    const auto &r = source.source_ref;
    StableId inventory;
    if (r.domain == "local_economy_catalog" &&
        r.kind == "cohort_home_inventory" && x.economy.enabled) {
      const auto *row = x.economy.cohorts.find(r.id);
      if (row)
        inventory = row->private_inventory_id;
    } else if (r.domain == "estate_catalog" && r.kind == "estate_inventory") {
      const auto *row = x.estates.estates.find(r.id);
      if (row)
        inventory = row->inventory_id;
    } else if (r.domain == "growth_catalog" && r.kind == "shop_inventory" &&
               x.growth.economy) {
      const auto *row = x.growth.shops.find(r.id);
      if (row)
        inventory = row->inventory_id;
    } else if (x.private_shops && r.domain == private_shop_module &&
               r.kind == private_shop_open_kind) {
      inventory = take(read_original_private_shop_opening(*x.private_shops, r))
                      ->inventory_id;
    }
    if (inventory.empty())
      fail("OriginalFixedInventorySourceMissing", r.id);
    auto actual = placement(x, inventory);
    if (std::count_if(actual.original_sources.begin(),
                      actual.original_sources.end(), [&](const auto &v) {
                        return v.source_schema == source.source_schema &&
                               same_source_ref(v.source_ref, r);
                      }) != 1)
      fail("OriginalFixedInventorySourceMismatch", r.id);
    return actual;
  });
}
Status bind_opened_private_shop_inventory_in_candidate(
    SpatialCarrierState &carriers,
    const EconomicFixedInventoryPlacementContext &x,
    const PrivateShopOpeningReceipt &opened, const CurrentMomentRead &clock) {
  try {
    if (!x.private_shops || x.private_shops->phase)
      fail("PrivateShopInventoryOpeningCandidateRequired", opened.shop_id);


    auto source_memory = CheckpointMemoryCharge(0);
    const SourceRef opening{private_shop_module, private_shop_open_kind,
                            opened.shop_id};
    const auto *shop =
        take(read_original_private_shop_opening(*x.private_shops, opening));
    const auto now = take(clock.read_current_moment());
    if (now.half_day_tick != x.common.tick() ||
        shop->opened_tick != x.common.tick() ||
        opened.inventory_id != shop->inventory_id ||
        opened.wallet_id != shop->wallet_id ||
        opened.rent_obligation_id != shop->rent_obligation_id ||
        carriers.inventory_bindings.find(shop->inventory_id))
      fail("PrivateShopInventoryOpeningCandidateRequired", opening.id);
    auto original = placement(x, shop->inventory_id);
    if (original.original_sources.size() != 1 ||
        !same_source_ref(original.original_sources.front().source_ref, opening))
      fail("PrivateShopInventoryOpeningSourceMismatch", opening.id);
    // 原开店 CreateInventory 已完成才绑定，同 World
    // 外层候选负责连资源和开店回滚。


    auto binding_memory = CheckpointMemoryCharge(0);
    InventorySpatialBinding binding{shop->inventory_id,
                                    original.original_site_ref.id, opening, 0};
    if (!carriers.inventory_bindings.insert(binding.inventory_id, binding))
      fail("DuplicatePrivateShopInventoryBinding", shop->inventory_id);
    return common_success();
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
} // namespace hanmo::resident
