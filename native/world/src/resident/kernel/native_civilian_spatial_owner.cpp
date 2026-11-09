#include "native_civilian_spatial_owner.hpp"
#include "native_field_passage.hpp"
#include "native_transport_catalog.hpp"
#include "world_bundle.hpp"
#include <new>
#include <stdexcept>
namespace hanmo::resident {
namespace {
// 原 layout 负责完整三根空性；这里补真实选择所需专业源及人口绑定边界。
Status roots(const WorldBundle &world,
             const TransportRegistrationBusinessRead *business,
             const TransportVehicleTakeover *vehicles, ReadStampLimits limits) {
  auto valid = validate_native_transport_layout(world);
  if (!valid.ok)
    return valid;
  if (!world.catalog || !world.catalog->common || !world.catalog->travel ||
      !world.catalog->actor_foundation || !world.actor_foundation ||
      !world.movement_exact || !world.spatial_carriers ||
      !world.transport_registrations)
    return common_failure("NativeCivilianSpatialRootsRequired", {},
                          ErrorKind::unsupported_capability);
  if (!limits.max_versions || !limits.max_sources || !limits.max_text_bytes)
    return common_failure("InvalidReadStampLimits", {}, ErrorKind::internal);
  auto selected = selected_native_transport_versions(world.catalog->identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (!selected.value.travel)
    return common_failure("NativeCivilianSpatialSelectionRequired", {},
                          ErrorKind::unsupported_capability);
  if (selected.value.group) {
    if (world.catalog->field_group_passage && !business) {
      if (vehicles)
        return common_failure("FieldArmyTransportHasCommercialVehicleOwner");
      auto army = validate_native_field_army_transport_roots(world);
      if (!army.ok)
        return army;
    } else if (!world.population_transport || !business)
      return common_failure("NativeGroupPopulationBusinessSourcesRequired", {},
                            ErrorKind::unsupported_capability);
  } else {
    if (business || vehicles)
      return common_failure("UnselectedGroupSpatialSources", {},
                            ErrorKind::internal);
    if (world.population_transport &&
        (!world.population_transport->transport_bindings.empty() ||
         !world.population_transport->releases.empty()))
      return common_failure("UnselectedGroupPopulationBindings", {},
                            ErrorKind::internal);
    // Carried 是实际编组地址，不能仅因三个辅助表为空而放行无来源的人物。
    for (const auto &entry : world.common.state().actors.rows())
      if (std::holds_alternative<CarriedLocation>(entry.second.location))
        return common_failure("UnselectedGroupCarriedActor", entry.first,
                              ErrorKind::internal);
  }
  return common_success();
}
} // namespace
// 所有引用在同一不可移动对象内建立；创建阶段不捕获尚未进入原 phase 的票据。
NativeCivilianSpatialOwner::NativeCivilianSpatialOwner(
    const WorldBundle &world, ReadStampLimits limits,
    const TransportRegistrationBusinessRead *business,
    const TransportVehicleTakeover *vehicles,
    const MilitaryFormationSourceRead *formations,
    const MilitaryReturnSourceRead *returns,
    const NativeMilitaryTrainingPeriodRead *periods)
    : world_(world), business_(business), vehicles_(vehicles), limits_(limits),
      stamps_(make_native_current_read_stamp_reader(world, limits, formations,
                                                    returns, periods)),
      original_(world.common, *world.catalog->common, *world.catalog->travel,
                *world.movement_exact, *world.spatial_carriers,
                *world.transport_registrations, world.local_economy,
                world.population_transport ? &*world.population_transport
                                           : nullptr,
                *world.actor_foundation, *world.catalog->actor_foundation,
                business, *stamps_, vehicles, world.catalog->family.get()) {}
Outcome<std::unique_ptr<NativeCivilianSpatialOwner>>
NativeCivilianSpatialOwner::create(
    const WorldBundle &world, ReadStampLimits limits,
    const TransportRegistrationBusinessRead *business,
    const TransportVehicleTakeover *vehicles,
    const MilitaryFormationSourceRead *formations,
    const MilitaryReturnSourceRead *returns,
    const NativeMilitaryTrainingPeriodRead *periods) {
  auto valid = roots(world, business, vehicles, limits);
  if (!valid.ok)
    return {false, {}, std::move(valid.error)};
  try {
    auto owner = std::unique_ptr<NativeCivilianSpatialOwner>(
        new NativeCivilianSpatialOwner(world, limits, business, vehicles,
                                       formations, returns, periods));
    valid = owner->validate_bindings();
    if (!valid.ok)
      return {false, {}, std::move(valid.error)};
    return {true, std::move(owner), {}};
  } catch (const std::bad_alloc &) {
    return {false,
            {},
            common_failure("NativeCivilianSpatialResourceExhausted", {},
                           ErrorKind::internal)
                .error};
  } catch (const std::length_error &) {
    return {false,
            {},
            common_failure("NativeCivilianSpatialResourceExhausted", {},
                           ErrorKind::internal)
                .error};
  }
}
bool NativeCivilianSpatialOwner::bound_to(
    const WorldBundle &world) const noexcept {
  return &world_ == &world;
}
Status NativeCivilianSpatialOwner::validate_bindings() const {
  if (!stamps_)
    return common_failure("NativeCivilianSpatialStampOwnerRequired", {},
                          ErrorKind::internal);
  return roots(world_, business_, vehicles_, limits_);
}
const NativeCivilianSpatialCarrierSources &
NativeCivilianSpatialOwner::original() const noexcept {
  return original_;
}
const CurrentReadStampRead &
NativeCivilianSpatialOwner::stamps() const noexcept {
  return *stamps_;
}
} // namespace hanmo::resident
