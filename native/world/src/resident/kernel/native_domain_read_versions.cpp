#include "native_domain_read_versions.hpp"
#include "native_field_application_read.hpp"
#include "native_military_training_period.hpp"
#include "native_production_cycles.hpp"
#include "native_transport_funding.hpp"
#include "original_world_read_boundary.hpp"
#include "resident/domains/local_economy/population_manifest_history.hpp"
#include "resident/domains/military/military_return_source.hpp"
#include "resident/domains/military/military_spatial_manifest.hpp"
#include "resident/domains/travel/cargo_handling.hpp"
#include "resident/domains/travel/native_spatial_carrier_sources.hpp"
#include "world_bundle.hpp"
#include "world_read_stamp.hpp"
namespace hanmo::resident {
namespace {
Outcome<std::uint64_t> unavailable(const StableId &code, const StableId &id) {
  return {false,
          {},
          common_failure(code, id, ErrorKind::unsupported_capability).error};
}
Outcome<StableId> selected_version(const WorldBundle &world,
                                   const StableId &module) {
  if (!world.catalog)
    return {
        false, {}, common_failure("DomainReadCatalogRequired", module).error};
  const StableId *selected = nullptr;
  for (const auto &row : world.catalog->identity.module_versions)
    if (row.first == module) {
      if (selected)
        return {
            false,
            {},
            common_failure("DuplicateDomainReadModuleSelection", module).error};
      selected = &row.second;
    }
  if (!selected)
    return {false,
            {},
            common_failure("DomainReadModuleNotSelected", module,
                           ErrorKind::unsupported_capability)
                .error};
  return {true, *selected, {}};
}
class OwnedCurrentRead final : public CurrentReadStampRead,
                               public CheckpointAllocatedObject {
  NativeDomainReadVersionSource versions_;
  std::unique_ptr<CurrentReadStampRead> reader_;

public:
  OwnedCurrentRead(const WorldBundle &world, ReadStampLimits limits,
                   const MilitaryFormationSourceRead *formations,
                   const MilitaryReturnSourceRead *returns,
                   const NativeMilitaryTrainingPeriodRead *periods)
      : versions_(world, formations, returns, periods),
        reader_(world.make_current_read_stamp_reader(versions_, limits)) {}
  Outcome<WorldMoment> read_current_moment() const override {
    return reader_->read_current_moment();
  }
  Outcome<std::shared_ptr<const ReadStamp>>
  capture(const std::vector<ResourceKey> &resources,
          const std::vector<DomainReadKey> &domains,
          const std::vector<StableId> &sources) const override {
    return reader_->capture(resources, domains, sources);
  }
};
} // namespace
Outcome<std::uint64_t> NativeDomainReadVersionSource::current_version(
    const StableId &domain, const StableId &kind, const StableId &id) const {
  if (id.empty())
    return unavailable("DomainReadIdRequired", kind);
  if (!world_.catalog)
    return {
        false,
        {},
        common_failure("DomainReadCatalogRequired", kind, ErrorKind::internal)
            .error};
  if (formations_ &&
      !formations_->bound_to(world_.common.state(), world_.military))
    return {false,
            {},
            common_failure("FormationDomainReadCandidateMismatch", id,
                           ErrorKind::internal)
                .error};
  if (returns_ && !returns_->bound_to(world_.common, world_.military))
    return {false,
            {},
            common_failure("ReturnDomainReadCandidateMismatch", id,
                           ErrorKind::internal)
                .error};
  if (periods_ &&
      (!world_.production_cycles ||
       !periods_->bound_to(world_.common, *world_.production_cycles)))
    return {false,
            {},
            common_failure("MilitaryPeriodDomainReadCandidateMismatch", id,
                           ErrorKind::internal)
                .error};
  const DomainReadKey key{domain, kind, id};
  if (domain == "actor_foundation" && kind == "condition_head") {
    auto selected = selected_version(world_, "actor_start_context");
    if (!selected.ok)
      return {false, {}, selected.error};
    if ((selected.value != "2" && selected.value != "3") ||
        !world_.actor_foundation || !world_.catalog->actor_foundation)
      return unavailable("ConditionDomainReadSourceRequired", id);
    const auto &state = *world_.actor_foundation;
    const auto *head =
        state.character_condition_extensions.actor_condition_heads.find(id);
    const auto *origin = state.character_origin_records.origins.find(id);
    if (!head || head->actor_id != id || head->condition_revision < 0 ||
        !origin || origin->actor_id != id ||
        origin->initial_condition_seed_ref.seed_key !=
            head->initial_condition_seed_ref.seed_key ||
        !world_.common.actor(id))
      return unavailable("ConditionDomainReadIdentityMismatch", id);
    // Zero is returned only for this real, initialized original head.
    return {true, static_cast<std::uint64_t>(head->condition_revision), {}};
  } else if (domain == "commerce" && kind == "self_transport_funding") {
    auto selected = selected_native_private_transport_funding_version(
        world_.catalog->identity);
    if (!selected.ok)
      return {false, {}, std::move(selected.error)};
    if (selected.value != 1 || !world_.private_transport_funding)
      return unavailable("PrivateTransportFundingDomainSourceRequired", id);
    return read_self_transport_funding_domain_version(
        *world_.private_transport_funding, key);
  } else if (domain == "travel") {
    auto selected = selected_version(world_, "travel");
    if (!selected.ok)
      return {false, {}, selected.error};
    if (selected.value != "4" && selected.value != "5")
      return unavailable("ExactTravelDomainReadSelectionRequired", id);
    if (kind == "cargo_handling_terms" || kind == "cargo_handling_execution") {
      auto cargo = selected_version(world_, "cargo_handling");
      if (!cargo.ok)
        return {false, {}, cargo.error};
      if (cargo.value != "1" || !world_.movement_exact)
        return unavailable("CargoHandlingDomainReadSourceRequired", id);
      return read_cargo_handling_domain_revision(
          world_.common, world_.movement_exact->cargo_handling, key);
    }
    if (kind == "transport_unit" || kind == "carrier_position" ||
        kind == "transport_registration" || kind == "transport_release") {
      if (!world_.spatial_carriers || !world_.transport_registrations)
        return unavailable("SpatialDomainReadSourceRequired", id);
      return read_spatial_carrier_domain_revision(
          *world_.spatial_carriers, *world_.transport_registrations, key);
    }
  } else if (domain == "local_economy" &&
             (kind == "population_source_revision" ||
              kind == "population_transport_binding" ||
              kind == "labor_allocation")) {
    auto selected =
        selected_native_population_transport_version(world_.catalog->identity);
    if (!selected.ok)
      return {false, {}, selected.error};
    if (!selected.value || !world_.population_transport ||
        world_.population_transport->version != selected.value)
      return unavailable("PopulationDomainReadSourceRequired", id);
    return read_population_transport_domain_revision(
        world_.local_economy, *world_.population_transport, key);
  } else if (domain == "military" && kind == "army_composition") {
    if (world_.field_battle && !world_.military_exact) {
      auto selected = selected_version(world_, "field_battle");
      if (!selected.ok)
        return {false, {}, selected.error};
      if (selected.value != "1" || !world_.catalog->field_battle ||
          !world_.catalog->military || !formations_ || !returns_ || !periods_)
        return unavailable("FieldMilitaryDomainReadSourceRequired", id);
      // 版本来自同 World 真 Field/原成军名册折叠；不构造空 dynamic 根或零版本。
      WorldCurrentMomentRead clock(world_);
      OriginalWorldReadBoundary boundary(world_, clock);
      NativeFieldApplicationRead field_clock(world_, clock, boundary);
      MilitaryFieldHistoryRead field_history{*world_.field_battle, field_clock};
      MilitarySelectedHistoryRead history{world_.catalog->identity, nullptr,
                                          &field_history};
      return read_selected_military_army_composition_domain_revision(
          world_.common, world_.military, *world_.catalog->military, history,
          clock, key,
          world_.population_military_enabled() ? &world_.population_military
                                               : nullptr,
          world_.population_military_enabled()
              ? world_.catalog->population_military.get()
              : nullptr,
          {}, formations_, returns_, periods_);
    }
    auto selected = selected_version(world_, "dynamic_military");
    if (!selected.ok)
      return {false, {}, selected.error};
    if ((selected.value != "2" && selected.value != "3" &&
         selected.value != "4") ||
        !world_.military_exact || !world_.catalog->military)
      return unavailable("MilitaryDomainReadSourceRequired", id);
    WorldCurrentMomentRead clock(world_);
    return read_military_army_composition_domain_revision(
        world_.common, world_.military, *world_.catalog->military,
        *world_.military_exact, clock, key,
        world_.population_military_enabled() ? &world_.population_military
                                             : nullptr,
        world_.population_military_enabled()
            ? world_.catalog->population_military.get()
            : nullptr,
        {}, formations_, returns_, periods_);
  }
  return unavailable("UnsupportedOriginalDomainRead", domain + "/" + kind);
}
std::unique_ptr<CurrentReadStampRead> make_native_current_read_stamp_reader(
    const WorldBundle &world, ReadStampLimits limits,
    const MilitaryFormationSourceRead *formations,
    const MilitaryReturnSourceRead *returns,
    const NativeMilitaryTrainingPeriodRead *periods) {
  return std::unique_ptr<CurrentReadStampRead>(
      new OwnedCurrentRead(world, limits, formations, returns, periods));
}
std::unique_ptr<CurrentReadStampRead>
WorldBundle::make_current_read_stamp_reader(ReadStampLimits limits) const {
  return make_native_current_read_stamp_reader(*this, limits);
}
} // namespace hanmo::resident
