#include "household_world_scope.hpp"
#include "market_history/economic_fifo_read.hpp"
#include "resident/domains/commerce/private_shop_absence.hpp"
#include "resident/domains/growth/growth_fixed_spatial_inputs.hpp"
#include "resident/kernel/native_character_family_world.hpp"
#include "resident/kernel/native_field_passage.hpp"
#include "resident/kernel/native_private_inventory_world.hpp"
#include "resident/kernel/native_private_shop_household_custody.hpp"
#include "resident/kernel/native_private_shop_operations.hpp"
#include "resident/kernel/native_transport_catalog.hpp"
#include "resident/kernel/private_shop_integration.hpp"
#include "resident/kernel/world_bundle.hpp"
#include <algorithm>
#include <limits>

namespace hanmo::resident {
namespace {
Status roots(const NativeCompleteWorldCoreSources &core,
             const NativeHouseholdWorldLateSources &late,
             const DynamicStepInput *phase) {
  const auto &w = core.characters.early.world;
  if (!w.catalog)
    return common_failure("NativeHouseholdWorldCatalogRequired", {},
                          ErrorKind::internal);
  const auto &p = *w.catalog;
  auto private_custody =
      validate_native_private_shop_household_custody_layout(w);
  if (!private_custody.ok)
    return private_custody;
  if (!p.common || !p.local_economy || !p.family || !p.estate || !p.growth ||
      !p.travel || !p.actor_foundation || !p.exact_time ||
      !p.complete_exact_time_runtime || !p.household_purchases ||
      !p.household_retail_settlements || !w.actor_foundation ||
      !w.action_elapsed || !w.facility_claims || !w.movement_exact ||
      !w.spatial_carriers || !w.transport_registrations ||
      !w.group_movement_executions || !w.economic_transport_expenses ||
      !w.population_transport || !w.household_purchases ||
      !w.household_retail_settlements)
    return common_failure("NativeHouseholdWorldOriginalRootsRequired", {},
                          ErrorKind::unsupported_capability);
  if ((p.selected_household_version != 2 &&
       p.selected_household_version != 3) ||
      (p.selected_household_version == 3 && !p.naval))
    return common_failure("NativeHouseholdWorldSelectionMismatch", {},
                          ErrorKind::unsupported_capability);
  const auto shops =
      selected_native_private_shop_operation_versions(p.identity);
  if (!shops.ok)
    return {false, {}, shops.error};
  if (bool(shops.value.shops) != bool(p.private_shops))
    return common_failure("NativeHouseholdWorldShopSelectionMismatch");
  if (shops.value.shops) {
    if (!shops.value.exact_operations() || !w.private_shop_operation_sources ||
        !w.private_shop_operation_capabilities)
      return common_failure("NativeHouseholdWorldOriginalShopSourcesRequired",
                            {}, ErrorKind::unsupported_capability);
  } else {
    auto absent = validate_unselected_private_shop_sources(w);
    if (!absent.ok)
      return absent;
  }
  if (p.growth_market_history && !w.services.market_history())
    return common_failure("NativeHouseholdWorldOriginalMarketReadRequired");
  auto selected = selected_native_transport_versions(p.identity);
  if (!selected.ok)
    return {false, {}, selected.error};
  // 未选私铺的明选 Field/Army 仍沿原静置家户与库存历史；商业登记由
  // 原军队根排除，不把已选 cargo/普通编组或外借商业 owner 混入此支。
  const bool field_army_only =
      selected.value.group && p.field_group_passage && !p.private_shops &&
      std::none_of(p.identity.module_versions.begin(),
                   p.identity.module_versions.end(),
                   [](const auto &m) { return m.first == "cargo_handling"; });
  if (bool(selected.value.group && !field_army_only) !=
      bool(late.transport_business))
    return common_failure("NativeHouseholdWorldTransportSourceRequired", {},
                          ErrorKind::unsupported_capability);
  auto layout = validate_native_transport_layout(w);
  if (!layout.ok)
    return layout;
  if (field_army_only) {
    layout = validate_native_field_army_transport_roots(w);
    if (!layout.ok)
      return layout;
  }
  if (bool(late.public_staffing) != bool(late.public_authority) ||
      bool(late.public_staffing) != bool(late.public_market_capture))
    return common_failure("NativeHouseholdWorldPublicSourcesRequired", {},
                          ErrorKind::unsupported_capability);
  if (!late.public_staffing) {
    auto absent = validate_unselected_public_household_market(
        w.common, *p.household_purchases, *w.household_purchases,
        *w.population_transport, *p.household_retail_settlements);
    if (!absent.ok)
      return absent;
  }
  if (w.population_military_enabled() != bool(late.population_military))
    return common_failure("NativeHouseholdWorldPopulationSourceRequired", {},
                          ErrorKind::unsupported_capability);
  if (const auto *m = late.population_military) {
    if (!p.military || !p.organization || !w.military_exact)
      return common_failure("NativeHouseholdWorldPopulationSourceRequired", {},
                            ErrorKind::unsupported_capability);
    auto version = selected_population_military_version(
        *p.population_military, w.siege_integration_enabled());
    if (!version.ok)
      return {false, {}, version.error};
    Integer markers = 0;
    for (const auto &row : p.identity.module_versions)
      if (row.first == "population_military") {
        if (++markers != 1 || row.second != "3")
          return common_failure(
              "NativeHouseholdWorldPopulationSelectionMismatch");
      }
    if (version.value != 3 || markers != 1)
      return common_failure("NativeHouseholdWorldPopulationSelectionMismatch");
    if (&m->common != &w.common || &m->economy != &w.local_economy ||
        &m->economy_catalog != p.local_economy.get() ||
        &m->organization != &w.organization ||
        &m->organization_catalog != p.organization.get() ||
        &m->military != &w.military ||
        &m->military_catalog != p.military.get() ||
        &m->state != &w.population_military ||
        &m->catalog != p.population_military.get() ||
        m->selected_version != 3 ||
        m->population_transport != &*w.population_transport ||
        m->military_exact != &*w.military_exact || !m->dynamic_land_sources ||
        m->actual_integration != w.siege_integration_enabled() ||
        !m->actual_clock)
      return common_failure("NativeHouseholdWorldPopulationSnapshotMismatch",
                            {}, ErrorKind::internal);
    const auto now = w.actual_phase_moment();
    if (!now.ok)
      return {false, {}, now.error};
    const auto observed = m->actual_clock->read_current_moment();
    if (!observed.ok)
      return {false, {}, observed.error};
    const auto same = compare_world_moments(now.value, observed.value);
    if (!same.ok)
      return {false, {}, same.error};
    if (same.value)
      return common_failure("NativeHouseholdWorldPopulationMomentMismatch", {},
                            ErrorKind::internal);
  }
  if (late.actual_market) {
    const auto &x = late.actual_market->context();
    const auto &material = x.materials.sources();
    if (!p.growth_market_history ||
        p.growth_market_history->selected_version != 2 ||
        &x.common != &w.common || &x.shared != p.common.get() ||
        &x.identity != &p.identity || &x.catalog != p.local_economy.get() ||
        &x.economy != &w.local_economy || x.phase != phase ||
        &material.common != &w.common || &material.shared != p.common.get() ||
        &material.identity != &p.identity ||
        &material.economy != p.local_economy.get() ||
        material.instances !=
            (w.item_instances ? &*w.item_instances : nullptr) ||
        material.properties !=
            (w.lot_properties ? &*w.lot_properties : nullptr) ||
        material.item_catalog != p.item_services.get())
      return common_failure("NativeHouseholdWorldMarketSnapshotMismatch", {},
                            ErrorKind::internal);
    auto checked = late.actual_market->validate_bindings();
    if (!checked.ok)
      return checked;
    const auto now = w.actual_phase_moment();
    if (!now.ok)
      return {false, {}, now.error};
    const auto observed = material.clock.read_current_moment();
    if (!observed.ok)
      return {false, {}, observed.error};
    const auto same = compare_world_moments(now.value, observed.value);
    if (!same.ok)
      return {false, {}, same.error};
    if (same.value)
      return common_failure("NativeHouseholdWorldMarketMomentMismatch", {},
                            ErrorKind::internal);
  }
  const auto &s = core.characters.spatial;
  if (&s.movement != &*w.movement_exact ||
      &s.carriers != &*w.spatial_carriers ||
      &s.registrations != &*w.transport_registrations)
    return common_failure("NativeHouseholdWorldSpatialSnapshotMismatch", {},
                          ErrorKind::internal);
  return core.characters.family.validate_bindings();
}
} // namespace

struct NativeHouseholdWorldScope::Impl final : CheckpointAllocatedObject {
  const NativeCompleteWorldCoreSources &core;
  NativeHouseholdWorldLateSources late;
  const DynamicStepInput *phase;
  // 原私铺先拥有真实许可、任务、市场读；后续 context 才能借它。
  std::unique_ptr<PrivateShopWorldAccess> shops;
  std::unique_ptr<NativePrivateInventoryWorldRead> private_inventory;
  std::optional<GrowthMarketHistoryWorldRead> public_market;
  CharacterConditionSources body;
  FamilyHouseholdControlAccess controls;
  std::optional<PublicMarketStaffingContext> staffing;
  PopulationChangeTimeContext population;
  MobileInventoryAdoptionContext mobile;
  std::optional<HouseholdMobileCustodyContext> custody;
  std::unique_ptr<NativeHouseholdFoundationLease> household;

  Impl(const NativeCompleteWorldCoreSources &c,
       NativeHouseholdWorldLateSources l, const DynamicStepInput *p,
       std::unique_ptr<PrivateShopWorldAccess> s, CharacterConditionSources b,
       std::unique_ptr<NativePrivateInventoryWorldRead> inventory)
      : core(c), late(l), phase(p), shops(std::move(s)),
        private_inventory(std::move(inventory)), body(std::move(b)),
        controls(world().common, world().family, *world().catalog->family,
                 *world().catalog->common, world().local_economy,
                 &core.characters.family.ages()),
        staffing(
            late.public_staffing
                ? std::optional<
                      PublicMarketStaffingContext>{{world().common,
                                                    world().local_economy,
                                                    *world()
                                                         .population_transport,
                                                    *world()
                                                         .household_purchases,
                                                    controls,
                                                    *late.public_staffing,
                                                    body.actual_now}}
                : std::nullopt),
        population{world().common,
                   world().local_economy,
                   *world().population_transport,
                   world().organization,
                   *world().transport_registrations,
                   core.characters.early.clock,
                   late.population_military},
        mobile{world().common,
               *world().catalog->common,
               *world().catalog->travel,
               *world().spatial_carriers,
               *world().movement_exact,
               *world().transport_registrations,
               core.characters.spatial.original,
               late.transport_business,
               core.characters.early.clock} {
    // 公库市场真实读取不以是否经营私铺为前提，仍沿原唯一历史reader。
    if (!shops && world().catalog->growth_market_history) {
      if (late.actual_market)
        public_market.emplace(world().services.market_history()->read(
            world().local_economy, phase, *late.actual_market));
      else
        public_market.emplace(world().services.market_history()->read(
            world().local_economy, phase));
    }
    if (world().catalog->selected_household_version == 3)
      custody.emplace(HouseholdMobileCustodyContext{
          mobile, world().family, world().military, world().naval,
          *world().catalog->naval, *world().catalog->estate});
  }

  const WorldBundle &world() const { return core.characters.early.world; }
  NativeHouseholdFoundationDependencies dependencies() const {
    const auto &w = world();
    const auto &p = *w.catalog;
    const auto &spatial = core.characters.spatial;
    const auto &clock = core.characters.early.clock;
    return {
        p.identity,
        {w.common, *p.common, *p.household_purchases, *w.household_purchases,
         *w.action_elapsed, body, *w.facility_claims, *w.spatial_carriers,
         *w.movement_exact, *w.transport_registrations, spatial.original, clock,
         spatial.military_handoffs},
        {w.common, w.local_economy, *p.local_economy, *w.population_transport,
         *w.household_purchases, *p.household_purchases, *w.spatial_carriers,
         *w.movement_exact, *w.transport_registrations, spatial.original, clock,
         &population},
        shops ? std::optional<PrivateShopContext>{shops->context()}
              : std::nullopt,
        w.private_shops,
        w.private_shop_operation_sources ? &*w.private_shop_operation_sources
                                         : nullptr,
        w.private_shop_operation_capabilities
            ? &*w.private_shop_operation_capabilities
            : nullptr,
        {w.common,
         *p.common,
         *w.movement_exact,
         *w.spatial_carriers,
         *w.transport_registrations,
         *w.actor_foundation,
         *p.actor_foundation,
         spatial.original,
         p.family.get(),
         spatial.births,
         {},
         spatial.military_actions},
        p.family_life && p.family_life->selected_version == 4
            ? &core.characters.family.life()
            : nullptr,
        {w.common, *p.common, *p.local_economy, *p.estate, *p.growth,
         *p.travel},
        mobile,
        *w.group_movement_executions,
        controls,
        staffing ? &*staffing : nullptr,
        late.public_authority,
        core.facility_holders,
        p.local_economy->production_management
            ? &*p.local_economy->production_management
            : nullptr,
        w.economic_facility_capacity_sources
            ? &*w.economic_facility_capacity_sources
            : nullptr,
        p.production_facility_conditions.get(),
        w.production_facility_conditions ? &*w.production_facility_conditions
                                         : nullptr,
        *p.household_retail_settlements,
        *w.household_retail_settlements,
        shops ? shops->context().market_history
              : (public_market ? &*public_market : nullptr),
        late.public_market_capture,
        p.complete_exact_time_runtime->facility_claims,
        p.exact_time->scale_ref,
        *w.economic_transport_expenses,
        w.movement_exact->cargo_handling,
        body.actual_now,
        p.selected_household_version,
        custody,
        shops ? nullptr : &w,
        private_inventory ? &private_inventory->context() : nullptr,
        p.private_shop_household_custody.get(),
        w.private_shop_household_custody ? &*w.private_shop_household_custody
                                         : nullptr,
        p.private_shop_household_custody ? p.travel.get() : nullptr};
  }
};

NativeHouseholdWorldScope::NativeHouseholdWorldScope(std::unique_ptr<Impl> p)
    : impl_(std::move(p)) {}
NativeHouseholdWorldScope::~NativeHouseholdWorldScope() = default;
Outcome<std::unique_ptr<NativeHouseholdWorldScope>>
NativeHouseholdWorldScope::create(const NativeCompleteWorldCoreSources &core,
                                  NativeHouseholdWorldLateSources late,
                                  const DynamicStepInput *phase) {
  auto checked = roots(core, late, phase);
  if (!checked.ok)
    return {false, {}, checked.error};
  auto body = core.characters.family.body().sources();
  if (!body.ok)
    return {false, {}, body.error};
  std::unique_ptr<PrivateShopWorldAccess> shops;
  if (core.characters.early.world.catalog->private_shops) {
    auto selected = PrivateShopWorldAccess::create_for_complete(
        core, phase, late.actual_market.get());
    if (!selected.ok)
      return {false, {}, selected.error};
    shops = std::move(selected.value);
  }
  std::unique_ptr<NativePrivateInventoryWorldRead> private_inventory;
  const auto &world = core.characters.early.world;
  if (world.catalog->private_shop_household_custody) {
    auto original = NativePrivateInventoryWorldRead::create(world);
    if (!original.ok)
      return {false, {}, original.error};
    private_inventory = std::move(original.value);
  }
  auto impl = std::make_unique<Impl>(core, late, phase, std::move(shops),
                                     std::move(body.value),
                                     std::move(private_inventory));
  auto household = make_native_household_foundation_lease(impl->dependencies());
  if (!household.ok)
    return {false, {}, household.error};
  impl->household = std::move(household.value);
  auto result = std::unique_ptr<NativeHouseholdWorldScope>(
      new NativeHouseholdWorldScope(std::move(impl)));
  checked = result->validate_bindings();
  if (!checked.ok)
    return {false, {}, checked.error};
  return {true, std::move(result), {}};
}
bool NativeHouseholdWorldScope::bound_to(const WorldBundle &w) const noexcept {
  return &impl_->world() == &w;
}
Status NativeHouseholdWorldScope::validate_bindings() const {
  auto checked = roots(impl_->core, impl_->late, impl_->phase);
  if (!checked.ok)
    return checked;
  const auto &w = impl_->world();
  if (impl_->phase && (w.common.tick() < 0 ||
                       w.common.tick() == std::numeric_limits<Integer>::max() ||
                       impl_->phase->from_tick != w.common.tick() ||
                       impl_->phase->to_tick != w.common.tick() + 1))
    return common_failure("NativeHouseholdWorldPhaseMismatch", {},
                          ErrorKind::internal);
  const auto now = w.actual_phase_moment();
  if (!now.ok)
    return {false, {}, now.error};
  const auto same = compare_world_moments(now.value, impl_->body.actual_now);
  if (!same.ok)
    return {false, {}, same.error};
  if (same.value)
    return common_failure("NativeHouseholdWorldMomentMismatch", {},
                          ErrorKind::internal);
  if (&impl_->body.common != &w.common ||
      &impl_->body.state != &*w.actor_foundation ||
      &impl_->body.catalog != w.catalog->actor_foundation.get() ||
      impl_->body.shared != w.catalog->common.get() ||
      impl_->body.elapsed != &*w.action_elapsed ||
      (impl_->shops && impl_->shops->context().phase != impl_->phase) ||
      !foundation().bound_to(w.common, w.local_economy, *w.household_purchases,
                             w.private_shops, *w.household_retail_settlements,
                             *w.action_elapsed))
    return common_failure("NativeHouseholdWorldSnapshotMismatch", {},
                          ErrorKind::internal);
  if (impl_->private_inventory) {
    auto original = impl_->private_inventory->validate_bindings();
    if (!original.ok)
      return original;
    if (!impl_->private_inventory->bound_to(w))
      return common_failure("PrivateShopCustodyTitleOwnerMismatch", {},
                            ErrorKind::internal);
  }
  return foundation().validate_bindings();
}
Status NativeHouseholdWorldScope::validate_sources() const {
  auto checked = validate_bindings();
  return checked.ok ? foundation().validate_sources() : checked;
}
const NativeHouseholdFoundationLease &
NativeHouseholdWorldScope::foundation() const noexcept {
  return *impl_->household;
}
const NativeHouseholdServiceLease &
NativeHouseholdWorldScope::service() const noexcept {
  return foundation().service();
}
} // namespace hanmo::resident
