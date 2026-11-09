#include "economic_fifo_world_scope.hpp"
#include "resident/domains/growth/growth_fixed_spatial_inputs.hpp"
#include "resident/domains/politics/takeover/politics_takeover_absence.hpp"
#include "resident/kernel/household_provision_integration.hpp"
#include "resident/kernel/native_character_family_world.hpp"
#include "resident/kernel/native_item_craft_state.hpp"
#include "resident/kernel/native_production_cycles.hpp"
#include "resident/kernel/world_bundle.hpp"
#include <limits>
namespace hanmo::resident {
namespace {
Status fifo_roots(const NativeCompleteWorldCoreSources &core,
                  const NativeEconomicFifoWorldLateSources &late,
                  const DynamicStepInput *phase,
                  const EconomicWorkSourcesState *mutable_production) {
  const auto &w = core.characters.early.world;
  if (!w.catalog || !w.catalog->common || !w.catalog->local_economy ||
      !w.catalog->exact_time || !w.catalog->growth_market_history ||
      w.catalog->growth_market_history->selected_version != 2 ||
      w.growth_market_history.version != 2)
    return common_failure("EconomicFifoOriginalWorldRootsRequired");
  auto valid = validate_native_item_craft_layout(w);
  if (!valid.ok)
    return valid;
  // 原初始化尚未写 Calendar origin；这里只核选型/真实根，不提前读取待写业务。
  valid = validate_native_production_cycles_catalog(*w.catalog);
  if (!valid.ok)
    return valid;
  const auto &p = *w.catalog;
  if ((mutable_production &&
       (!w.production_cycles || mutable_production != &*w.production_cycles)) ||
      (late.original_payments && !mutable_production))
    return common_failure("EconomicFifoProductionWriteRootMismatch");
  if (bool(p.production_cycles) != bool(w.production_cycles))
    return common_failure("EconomicFifoSelectedProductionRootMismatch");
  if (bool(w.item_instances) != bool(late.item_outputs) ||
      bool(w.production_cycles) != bool(late.political))
    return common_failure("EconomicFifoSelectedOriginalOwnerRequired");
  if (phase && (phase->from_tick != w.common.tick() ||
                phase->from_tick == std::numeric_limits<Integer>::max() ||
                phase->to_tick != phase->from_tick + 1))
    return common_failure("EconomicFifoActualPhaseMismatch");
  auto now = w.actual_phase_moment();
  if (!now.ok)
    return {false, {}, std::move(now.error)};
  auto clock = core.characters.early.clock.read_current_moment();
  if (!clock.ok)
    return {false, {}, std::move(clock.error)};
  auto same = compare_world_moments(now.value, clock.value);
  if (!same.ok)
    return {false, {}, std::move(same.error)};
  if (same.value)
    return common_failure("EconomicFifoActualClockMismatch");
  // 未选原子根只沿原缺席门；不能因为当前数据为空而跳过已选 provider。
  if (p.household_provision_management) {
    if (!household_provision_management_enabled(w) ||
        !w.household_provision_management_seed_complete)
      return common_failure("EconomicFifoHouseholdManagementRootRequired");
  } else {
    valid = validate_household_provision_world(w);
    if (!valid.ok)
      return valid;
  }
  if (p.production_disruptions) {
    if (!w.production_disruptions_enabled() ||
        !w.production_disruptions_seed_complete)
      return common_failure("EconomicFifoDisruptionRootRequired");
  } else if (w.production_disruptions_seed_complete ||
             w.production_disruptions.version != 1 ||
             !w.production_disruptions.registrations.empty() ||
             !w.production_disruptions.settlements.empty()) {
    return common_failure("EconomicFifoUnselectedDisruptionRoot");
  }
  if (w.production_cycles) {
    if (!late.installed || !p.organization || !p.politics ||
        !w.population_transport || w.population_transport->version != 3 ||
        !w.transport_registrations || !w.spatial_carriers ||
        !w.movement_exact || !w.actor_foundation || !p.actor_foundation ||
        !w.economic_facility_capacity_sources ||
        !w.production_facility_conditions || !p.economic_facilities ||
        !p.production_facility_conditions ||
        !p.local_economy->production_management)
      return common_failure(
          "EconomicFifoProductionOriginalPredecessorsRequired");
    valid = late.installed->validate_bindings();
    if (!valid.ok)
      return valid;
    const auto political = late.political->bindings();
    const auto &opening = political.opening;
    const auto &installation = late.installed->installation();
    const bool takeover_selected = w.political_takeover_enabled();
    if (takeover_selected != bool(late.takeover) ||
        takeover_selected != bool(political.takeover))
      return common_failure("EconomicFifoTakeoverSelectionMismatch");
    if (!takeover_selected) {
      valid = validate_unselected_political_takeover_sources(w);
      if (!valid.ok)
        return valid;
    }
    if (late.takeover) {
      const auto &takeover = *late.takeover;
      if (!p.political_takeover || !p.military ||
          p.political_takeover->version != 2 ||
          w.political_takeover.version != 2 || &takeover.common != &w.common ||
          &takeover.common_catalog != p.common.get() ||
          &takeover.organizations != &w.organization ||
          &takeover.organization_catalog != p.organization.get() ||
          &takeover.politics_catalog != p.politics.get() ||
          &takeover.catalog != p.political_takeover.get() ||
          &takeover.state != &w.political_takeover ||
          &takeover.military != &w.military ||
          &takeover.military_catalog != p.military.get() ||
          takeover.actual_clock != &political.organization.clock ||
          takeover.organization_runtime != &political.organization.runtime ||
          takeover.politics_state != &w.politics || takeover.phase != phase)
        return common_failure("EconomicFifoProductionInputWorldMismatch");
    }
    if (&political.politics != &w.politics ||
        &political.politics_catalog != p.politics.get() ||
        &political.common_catalog != p.common.get() ||
        &political.economy_catalog != p.local_economy.get() ||
        &opening.common != &w.common || &opening.economy != &w.local_economy ||
        &opening.sources != &*w.production_cycles ||
        &opening.organization != &w.organization ||
        &opening.foundation != &*w.actor_foundation ||
        &opening.foundation_catalog != p.actor_foundation.get() ||
        &opening.action_sources != &core.action_time ||
        &installation.common != &w.common ||
        &installation.catalog != p.economic_facilities.get() ||
        &installation.state != &*w.economic_facility_capacity_sources ||
        &installation.production_assets !=
            &*p.local_economy->production_management ||
        &installation.estate_catalog != p.estate.get() ||
        installation.shops_catalog != p.private_shops.get() ||
        &core.characters.spatial.movement != &*w.movement_exact ||
        &core.characters.spatial.carriers != &*w.spatial_carriers ||
        &core.characters.spatial.registrations != &*w.transport_registrations ||
        w.population_military_enabled() != bool(late.population_military))
      return common_failure("EconomicFifoProductionInputWorldMismatch");
    auto production_now = opening.clock.read_current_moment();
    if (!production_now.ok)
      return {false, {}, std::move(production_now.error)};
    auto production_same =
        compare_world_moments(now.value, production_now.value);
    if (!production_same.ok)
      return {false, {}, std::move(production_same.error)};
    if (production_same.value)
      return common_failure("EconomicFifoProductionClockMismatch");
  } else if (late.installed || late.condition_effects || late.takeover ||
             late.transport || late.population_military) {
    return common_failure("EconomicFifoUnselectedProductionPredecessor");
  }
  return common_success();
}
} // namespace
struct NativeEconomicFifoWorldScope::Impl final : CheckpointAllocatedObject {
  const NativeCompleteWorldCoreSources &core;
  const WorldBundle &world;
  NativeEconomicFifoWorldLateSources late;
  const DynamicStepInput *phase;
  WorldMoment actual_at;
  EconomicWorkSourcesState *mutable_production;
  NativeEconomicMaterialRead materials;
  std::optional<NativeEconomicProductionInputLease> inputs;
  std::optional<EconomicWorkValidationContext> work;
  std::optional<EconomicProductionContext> production;
  std::optional<HouseholdProvisionManagementContext> management;
  std::optional<NativeEconomicFifoRead> fifo;
  Impl(const NativeCompleteWorldCoreSources &c,
       NativeEconomicFifoWorldLateSources l, const DynamicStepInput *p,
       WorldMoment at, EconomicWorkSourcesState *write)
      : core(c), world(c.characters.early.world), late(std::move(l)), phase(p),
        actual_at(std::move(at)), mutable_production(write),
        materials(
            {world.common, *world.catalog->common, world.catalog->identity,
             *world.catalog->local_economy, c.characters.early.clock,
             world.item_instances ? &*world.item_instances : nullptr,
             world.lot_properties ? &*world.lot_properties : nullptr,
             world.catalog->item_services.get(), late.item_outputs.get()}) {
    const auto &catalog = *world.catalog;
    if (world.production_cycles) {
      const auto political = late.political->bindings();
      const auto &clock = political.opening.clock;
      inputs.emplace(NativeEconomicProductionInputDependencies{
          {world.common, world.local_economy, *world.population_transport,
           world.organization, *world.transport_registrations, clock,
           late.population_military},
          {world.common, world.organization, *catalog.organization,
           world.local_economy, political.organization.runtime,
           political.organization.clock, political.organization.calendar},
          political.opening,
          {late.installed->installation(),
           *catalog.production_facility_conditions,
           *world.production_facility_conditions, late.condition_effects.get(),
           clock, *late.installed},
          late.takeover
              ? std::optional<PoliticalTakeoverContext>{*late.takeover}
              : std::nullopt,
          *catalog.local_economy,
          *catalog.production_cycles,
          PopulationLaborTransportSources{late.transport.get(), world},
          *world.spatial_carriers,
          *world.movement_exact,
          core.characters.spatial.original,
          world.tasks,
          world.household_purchases ? &*world.household_purchases : nullptr,
          political.organization.staffing,
          late.takeover ? nullptr : &world});
      work.emplace(EconomicWorkValidationContext{
          *catalog.production_cycles, *world.production_cycles,
          inputs->inputs(), clock,
          catalog.exact_time->scale_ref.units_per_half_day});
      if (mutable_production)
        production.emplace(EconomicProductionContext{
            *catalog.production_cycles, *mutable_production, inputs->inputs(),
            clock, catalog.exact_time->scale_ref.units_per_half_day,
            late.original_payments.get()});
    }
    if (catalog.household_provision_management)
      management.emplace(HouseholdProvisionManagementContext{
          *catalog.household_provision_management,
          world.household_provision_management});
    fifo.emplace(EconomicFifoReadContext{
        world.common, *catalog.common, catalog.identity, world.local_economy,
        *catalog.local_economy, materials, work ? &*work : nullptr,
        world.political_takeover_enabled() ? &world.economic_takeover_execution
                                           : nullptr,
        phase, catalog.production_disruptions.get(),
        catalog.production_disruptions ? &world.production_disruptions
                                       : nullptr,
        management ? &*management : nullptr,
        catalog.family_life && catalog.family_life->selected_version == 4
            ? &core.characters.family.life()
            : nullptr});
  }
};
NativeEconomicFifoWorldScope::NativeEconomicFifoWorldScope(
    std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}
NativeEconomicFifoWorldScope::~NativeEconomicFifoWorldScope() = default;
Outcome<std::unique_ptr<NativeEconomicFifoWorldScope>>
NativeEconomicFifoWorldScope::create(
    const NativeCompleteWorldCoreSources &core,
    NativeEconomicFifoWorldLateSources late, const DynamicStepInput *phase,
    EconomicWorkSourcesState *mutable_production) {
  auto valid = fifo_roots(core, late, phase, mutable_production);
  if (!valid.ok)
    return {false, {}, std::move(valid.error)};
  auto at = core.characters.early.world.actual_phase_moment();
  if (!at.ok)
    return {false, {}, std::move(at.error)};
  auto result = std::unique_ptr<NativeEconomicFifoWorldScope>(
      new NativeEconomicFifoWorldScope(
          std::make_unique<Impl>(core, std::move(late), phase,
                                 std::move(at.value), mutable_production)));
  valid = result->validate_bindings();
  if (!valid.ok)
    return {false, {}, std::move(valid.error)};
  return {true, std::move(result), {}};
}
bool NativeEconomicFifoWorldScope::bound_to(
    const WorldBundle &world) const noexcept {
  return &impl_->world == &world;
}
Status NativeEconomicFifoWorldScope::validate_bindings() const {
  const auto &x = *impl_;
  auto valid = fifo_roots(x.core, x.late, x.phase, x.mutable_production);
  if (!valid.ok)
    return valid;
  auto actual = x.world.actual_phase_moment();
  if (!actual.ok)
    return {false, {}, std::move(actual.error)};
  auto same = compare_world_moments(x.actual_at, actual.value);
  if (!same.ok)
    return {false, {}, std::move(same.error)};
  if (same.value)
    return common_failure("EconomicFifoScopeMomentExpired");
  if (x.inputs) {
    valid = x.inputs->validate_bindings();
    if (!valid.ok)
      return valid;
  }
  return x.fifo->validate_bindings();
}
const NativeEconomicMaterialRead &
NativeEconomicFifoWorldScope::materials() const noexcept {
  return impl_->materials;
}
const NativeEconomicFifoRead &
NativeEconomicFifoWorldScope::fifo() const noexcept {
  return *impl_->fifo;
}
const NativeEconomicProductionInputLease *
NativeEconomicFifoWorldScope::production_inputs() const noexcept {
  return impl_->inputs ? &*impl_->inputs : nullptr;
}
EconomicProductionContext *
NativeEconomicFifoWorldScope::production() const noexcept {
  return impl_->production ? &*impl_->production : nullptr;
}
} // namespace hanmo::resident
