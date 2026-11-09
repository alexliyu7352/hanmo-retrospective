#include "household_foundation_lease.hpp"
#include "resident/domains/commerce/private_shop_absence.hpp"
#include "resident/kernel/native_field_passage.hpp"
#include "resident/kernel/native_private_shop_operations.hpp"
#include "resident/kernel/native_transport_catalog.hpp"
#include "resident/kernel/world_bundle.hpp"
#include <algorithm>

namespace hanmo::resident {
namespace {
// 只认锁目录的显式家户/零售版本，不由空表推断是否启用。
Status selected_household(const CatalogIdentity &identity, Integer expected) {
  Integer purchases = 0, retail = 0;
  for (const auto &row : identity.module_versions) {
    if (row.first == "household_purchases") {
      if (++purchases != 1 || row.second != std::to_string(expected))
        return common_failure("NativeHouseholdFoundationSelectionMismatch",
                              row.first);
    } else if (row.first == "household_retail_settlements") {
      if (++retail != 1 || row.second != "1")
        return common_failure("NativeHouseholdFoundationSelectionMismatch",
                              row.first);
    }
  }
  if (purchases != 1 || retail != 1 || (expected != 2 && expected != 3))
    return common_failure("NativeHouseholdFoundationSelectionRequired", {},
                          ErrorKind::unsupported_capability);
  return common_success();
}
} // namespace

NativeHouseholdFoundationLease::NativeHouseholdFoundationLease(
    NativeHouseholdFoundationDependencies d)
    : dependencies_(std::move(d)),
      original_shop_links_unbound_(
          (!dependencies_.shops || (!dependencies_.shops->retail_catalog &&
                                    !dependencies_.shops->retail &&
                                    !dependencies_.shops->exact_operations)) &&
          !dependencies_.fixed_inventory.private_shops),
      fixed_inventory_(dependencies_.fixed_inventory),
      inventory_history_{dependencies_.mobile_inventory,
                         dependencies_.group_executions,
                         dependencies_.operation_spatial.foundation,
                         dependencies_.operation_spatial.actors,
                         fixed_inventory_,
                         dependencies_.named.military_handoffs,
                         dependencies_.operation_spatial.family_catalog},
      mobile_custody_(
          dependencies_.mobile_custody
              ? std::optional<
                    HouseholdMobileCustodyContext>{HouseholdMobileCustodyContext{
                    dependencies_.mobile_inventory,
                    dependencies_.mobile_custody->family,
                    dependencies_.mobile_custody->military,
                    dependencies_.mobile_custody->naval,
                    dependencies_.mobile_custody->naval_catalog,
                    dependencies_.mobile_custody->estate_catalog}}
              : std::nullopt),
      operations_(
          dependencies_.shops && dependencies_.operations &&
                  dependencies_.operation_capabilities
              ? std::optional<
                    PrivateShopOperationSourceContext>{PrivateShopOperationSourceContext{
                    *dependencies_.shops, *dependencies_.operations,
                    *dependencies_.operation_capabilities,
                    dependencies_.named.catalog,
                    dependencies_.operation_spatial, dependencies_.named.body,
                    dependencies_.named.clock, dependencies_.named.elapsed,
                    dependencies_.scale, dependencies_.named.military_handoffs,
                    dependencies_.life_history}}
              : std::nullopt),
      market_basis_(
          operations_ && dependencies_.private_inventory_titles &&
                  dependencies_.private_custody_catalog &&
                  dependencies_.private_custody_state &&
                  dependencies_.private_custody_travel
              ? std::optional<
                    PrivateHouseholdMarketBasisContext>{PrivateHouseholdMarketBasisContext{
                    *operations_, *dependencies_.private_inventory_titles,
                    dependencies_.named.state}}
              : std::nullopt),
      private_custody_(
          market_basis_
              ? std::optional<
                    PrivateShopHouseholdCustodyContext>{PrivateShopHouseholdCustodyContext{
                    *market_basis_, *dependencies_.private_custody_travel,
                    *dependencies_.private_custody_catalog,
                    *dependencies_.private_custody_state}}
              : std::nullopt),
      service_(service_dependencies()),
      transport_(service_.household(), dependencies_.named.carriers,
                 dependencies_.named.registrations, dependencies_.named.spatial,
                 dependencies_.named.movement, dependencies_.group_executions) {
  // 私有构造尾只改 context 指针，不改变原世界或补写历史。
  if (dependencies_.shops) {
    dependencies_.shops->retail_catalog = &dependencies_.retail_catalog;
    dependencies_.shops->retail = &dependencies_.retail_state;
    dependencies_.shops->exact_operations =
        operations_ ? &*operations_ : nullptr;
    dependencies_.fixed_inventory.private_shops = &*dependencies_.shops;
  }
}

NativeHouseholdServiceDependencies
NativeHouseholdFoundationLease::service_dependencies() {
  auto &d = dependencies_;
  d.cohort.private_market_custody =
      private_custody_ ? &*private_custody_ : nullptr;
  return {d.named,
          d.cohort,
          {d.identity, d.named.common, d.named.catalog, d.named.state, d.assets,
           d.shops ? &d.shops->catalog : nullptr, d.installed,
           d.condition_catalog, d.conditions, d.public_authority,
           operations_ ? &*operations_ : nullptr, d.now,
           d.mobile_inventory.budget, d.unselected_shops_world},
          d.controls,
          d.staffing,
          d.all_holders,
          {d.shops_state, d.shops ? &d.shops->catalog : nullptr,
           d.fixed_inventory.growth, d.retail_catalog, d.retail_state,
           d.market_history, d.market_capture},
          d.claim_catalog,
          d.scale,
          d.transport_expenses,
          d.handling,
          d.fixed_inventory.travel,
          d.group_executions,
          inventory_history_,
          d.now,
          d.selected_household_version,
          mobile_custody_ ? &*mobile_custody_ : nullptr,
          private_custody_ ? &*private_custody_ : nullptr};
}

bool NativeHouseholdFoundationLease::bound_to(
    const CommonStore &common, const LocalEconomyState &economy,
    const HouseholdPurchaseState &purchases, const PrivateShopState &shops,
    const HouseholdRetailState &retail,
    const ActionElapsedStore &elapsed) const noexcept {
  const auto &d = dependencies_;
  return &d.named.common == &common && &d.cohort.economy == &economy &&
         &d.named.state == &purchases && &d.shops_state == &shops &&
         &d.retail_state == &retail && &d.named.elapsed == &elapsed;
}

Status NativeHouseholdFoundationLease::validate_bindings() const {
  const auto &d = dependencies_;
  const auto &n = d.named;
  const auto &f = d.fixed_inventory;
  const auto &m = d.mobile_inventory;
  const auto &s = d.operation_spatial;
  const bool has_custody = d.private_inventory_titles ||
                           d.private_custody_catalog ||
                           d.private_custody_state || d.private_custody_travel;
  Integer custody_selection = 0;
  for (const auto &entry : d.identity.module_versions)
    if (entry.first == "private_shop_household_custody_sources") {
      if (++custody_selection != 1 || entry.second != "1")
        return common_failure(
            "UnsupportedPrivateShopHouseholdCustodySelection");
    }
  if (bool(custody_selection) != has_custody)
    return common_failure("PrivateShopHouseholdCustodySelectionGraphMismatch");
  if (has_custody &&
      (!d.private_inventory_titles || !d.private_custody_catalog ||
       !d.private_custody_state || !d.private_custody_travel || !operations_ ||
       !market_basis_ || !private_custody_ ||
       &d.private_inventory_titles->common != &n.common ||
       &d.private_inventory_titles->catalog != &n.shared ||
       d.private_inventory_titles->foundation != &n.body.state ||
       d.private_inventory_titles->foundation_catalog != &n.body.catalog ||
       d.private_inventory_titles->family_catalog !=
           d.operation_spatial.family_catalog ||
       !d.private_inventory_titles->clock ||
       d.private_custody_travel != &d.fixed_inventory.travel ||
       d.cohort.private_market_custody != &*private_custody_))
    return common_failure("NativeHouseholdCustodyOwnedGraphMismatch");
  if (has_custody) {
    auto title_now = d.private_inventory_titles->clock->read_current_moment();
    auto owner_now = n.clock.read_current_moment();
    if (!title_now.ok)
      return {false, {}, title_now.error};
    if (!owner_now.ok)
      return {false, {}, owner_now.error};
    auto same = compare_world_moments(title_now.value, owner_now.value);
    if (!same.ok)
      return {false, {}, same.error};
    if (same.value)
      return common_failure("NativeHouseholdCustodyClockMismatch");
  }
  if (!has_custody &&
      (market_basis_ || private_custody_ || d.cohort.private_market_custody))
    return common_failure("UnselectedNativeHouseholdCustodyGraph");
  if (!original_shop_links_unbound_)
    return common_failure("NativeHouseholdOriginalShopContextAlreadyBound", {},
                          ErrorKind::internal);
  auto checked = selected_household(d.identity, d.selected_household_version);
  if (!checked.ok)
    return checked;
  const auto shops =
      selected_native_private_shop_operation_versions(d.identity);
  if (!shops.ok)
    return {false, {}, shops.error};
  if (shops.value.shops) {
    if (!shops.value.exact_operations() || !d.shops || !d.operations ||
        !d.operation_capabilities || !operations_ ||
        (shops.value.shops != 2 && shops.value.shops != 3) ||
        shops.value.sources != 1 || shops.value.capabilities != 1 ||
        d.shops->catalog.version != shops.value.shops ||
        d.shops_state.version != shops.value.shops ||
        &d.shops->state != &d.shops_state)
      return common_failure("NativeHouseholdExactShopSourcesRequired", {},
                            ErrorKind::unsupported_capability);
  } else {
    const auto *world = d.unselected_shops_world;
    if (d.shops || d.operations || d.operation_capabilities || operations_ ||
        !world || !world->catalog || &world->catalog->identity != &d.identity ||
        &world->common != &n.common ||
        &world->local_economy != &d.cohort.economy ||
        &world->private_shops != &d.shops_state ||
        !world->household_purchases ||
        &*world->household_purchases != &n.state ||
        world->catalog->household_purchases.get() != &n.catalog ||
        !world->household_retail_settlements ||
        &*world->household_retail_settlements != &d.retail_state ||
        world->catalog->household_retail_settlements.get() !=
            &d.retail_catalog ||
        world->catalog->family.get() != s.family_catalog)
      return common_failure("NativeHouseholdUnselectedShopWorldRequired", {},
                            ErrorKind::internal);
    checked = validate_unselected_private_shop_sources(*world);
    if (!checked.ok)
      return checked;
  }
  const auto transport = selected_native_transport_versions(d.identity);
  if (!transport.ok)
    return {false, {}, transport.error};
  // 只复用上方已核同 World 的未选私铺证明；不新增借用或为已选私铺
  // 拼装空商业来源。原库存历史与实际位置验证在后续 validate_sources 保留。
  const auto *field_world = d.unselected_shops_world;
  const bool field_army_only =
      transport.value.group && !d.shops && field_world &&
      field_world->catalog && field_world->catalog->field_group_passage &&
      !field_world->catalog->private_shops &&
      std::none_of(d.identity.module_versions.begin(),
                   d.identity.module_versions.end(),
                   [](const auto &entry) {
                     return entry.first == "cargo_handling";
                   });
  if (field_army_only) {
    // 独立 lease 入口也必须证明消费根就是 Army 校验所读的 World；
    // 不允许多个 context 一致地借用另一组民用运输根来冒用此证明。
    if (!field_world->spatial_carriers || !field_world->movement_exact ||
        !field_world->transport_registrations ||
        !field_world->group_movement_executions ||
        !field_world->population_transport ||
        field_world->catalog->common.get() != &n.shared ||
        field_world->catalog->travel.get() != &m.travel_catalog ||
        field_world->catalog->local_economy.get() !=
            &d.cohort.economy_catalog ||
        &field_world->common != &d.cohort.common ||
        &*field_world->spatial_carriers != &n.carriers ||
        &*field_world->spatial_carriers != &d.cohort.carriers ||
        &*field_world->movement_exact != &n.movement ||
        &*field_world->movement_exact != &d.cohort.movement ||
        &*field_world->transport_registrations != &n.registrations ||
        &*field_world->transport_registrations != &d.cohort.registrations ||
        &*field_world->group_movement_executions != &d.group_executions ||
        &*field_world->population_transport != &d.cohort.population)
      return common_failure("NativeHouseholdFoundationSnapshotMismatch", {},
                            ErrorKind::internal);
    checked = validate_native_field_army_transport_roots(*field_world);
    if (!checked.ok)
      return checked;
  }
  if (bool(transport.value.group && !field_army_only) !=
      bool(m.original_business))
    return common_failure("NativeHouseholdTransportSelectionMismatch", {},
                          ErrorKind::unsupported_capability);
  if ((d.shops &&
       (&d.shops->common != &n.common ||
        &d.shops->common_catalog != &n.shared ||
        &d.shops->economy != &d.cohort.economy ||
        &d.shops->economy_catalog != &d.cohort.economy_catalog ||
        d.shops->markets != &f.growth ||
        d.shops->estate_catalog != &f.estates ||
        d.shops->market_history != d.market_history ||
        d.shops->retail_catalog != &d.retail_catalog ||
        d.shops->retail != &d.retail_state ||
        d.shops->exact_operations != (operations_ ? &*operations_ : nullptr) ||
        f.private_shops != &*d.shops)) ||
      (!d.shops && f.private_shops) || &f.common != &n.common ||
      &f.shared != &n.shared || &f.economy != &d.cohort.economy_catalog ||
      &m.common != &n.common || &m.common_catalog != &n.shared ||
      &m.travel_catalog != &f.travel || &m.carriers != &n.carriers ||
      &m.movement != &n.movement || &m.registrations != &n.registrations ||
      &m.spatial_sources != &n.spatial || &m.clock != &n.clock ||
      &s.common != &n.common || &s.common_catalog != &n.shared ||
      &s.carriers != &n.carriers || &s.movement != &n.movement ||
      &s.registrations != &n.registrations || &s.spatial != &n.spatial ||
      &s.foundation != &n.body.state || &s.actors != &n.body.catalog ||
      (d.shops && s.family_catalog != &d.shops->family_catalog) ||
      (d.mobile_custody && &d.mobile_custody->estate_catalog != &f.estates))
    return common_failure("NativeHouseholdFoundationSnapshotMismatch", {},
                          ErrorKind::internal);
  if (bool(mobile_custody_) != (d.selected_household_version == 3))
    return common_failure("NativeHouseholdMobileCustodySelectionMismatch", {},
                          ErrorKind::unsupported_capability);
  const auto same = compare_world_moments(n.body.actual_now, d.now, m.budget);
  if (!same.ok)
    return {false, {}, same.error};
  if (same.value)
    return common_failure("NativeHouseholdFoundationBodyClockMismatch", {},
                          ErrorKind::internal);
  return service_.validate_bindings();
}

Status NativeHouseholdFoundationLease::validate_sources() const {
  auto checked = validate_bindings();
  if (!checked.ok)
    return checked;
  const auto &d = dependencies_;
  checked = validate_mobile_inventory_bindings(d.mobile_inventory);
  if (!checked.ok)
    return checked;
  checked = validate_original_fixed_inventory_bindings(
      d.named.common, d.named.shared, d.fixed_inventory.travel,
      d.named.carriers, d.named.registrations, fixed_inventory_);
  if (!checked.ok)
    return checked;
  if (operations_) {
    checked = validate_private_shop_operation_sources(*operations_);
    if (!checked.ok)
      return checked;
  }
  if (private_custody_) {
    checked =
        validate_private_shop_household_custody_sources(*private_custody_);
    if (!checked.ok)
      return checked;
  }
  return service_.validate_sources();
}

Outcome<std::unique_ptr<NativeHouseholdFoundationLease>>
make_native_household_foundation_lease(
    NativeHouseholdFoundationDependencies d) {
  auto owner = std::make_unique<NativeHouseholdFoundationLease>(std::move(d));
  const auto checked = owner->validate_bindings();
  if (!checked.ok)
    return {false, {}, checked.error};
  return {true, std::move(owner), {}};
}
} // namespace hanmo::resident
