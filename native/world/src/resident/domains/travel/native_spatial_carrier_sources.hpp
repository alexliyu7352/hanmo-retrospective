// 同候选原S5/Common/ActorStart/S1来源的具体读适配；不保存人口、库存或位置副本。
#pragma once
#include "group_transport_registration.hpp"
#include "resident/domains/actor_foundation/authored_origin.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/storage/carried_location_read.hpp"
namespace hanmo::resident {
struct FamilyCatalog;
Outcome<std::uint64_t>
read_spatial_carrier_domain_revision(const SpatialCarrierState &,
                                     const TransportRegistrationState &,
                                     const DomainReadKey &);
class NativeCivilianSpatialCarrierSources final
    : public SpatialCarrierSources,
      public CarriedLocationValidationRead {
public:
  NativeCivilianSpatialCarrierSources(
      const CommonStore &, const CommonCatalog &, const TravelCatalog &,
      const MovementExactState &, const SpatialCarrierState &,
      const TransportRegistrationState &, const LocalEconomyState &,
      const PopulationTransportState &, const AuthoredActorFoundationState &,
      const ActorFoundationCatalog &, const TransportRegistrationBusinessRead &,
      const CurrentReadStampRead &,
      const TransportVehicleTakeover *vehicles = nullptr,
      const FamilyCatalog *family_catalog = nullptr);
  // 未选 group 可借空可选根；选型由同 World 工厂先验，真实消费再强制拒缺源。
  NativeCivilianSpatialCarrierSources(
      const CommonStore &, const CommonCatalog &, const TravelCatalog &,
      const MovementExactState &, const SpatialCarrierState &,
      const TransportRegistrationState &, const LocalEconomyState &,
      const PopulationTransportState *, const AuthoredActorFoundationState &,
      const ActorFoundationCatalog &, const TransportRegistrationBusinessRead *,
      const CurrentReadStampRead &,
      const TransportVehicleTakeover *vehicles = nullptr,
      const FamilyCatalog *family_catalog = nullptr);
  Outcome<TransportPopulationSource>
  population(const SourceRef &) const override;
  Outcome<TransportManifestRead>
  military_manifest(const SpatialCarrierRef &) const override;
  Outcome<TransportCapacitySource>
  carrier_capacity(const SpatialCarrierRef &) const override;
  Outcome<Integer> vehicle_capacity(const StableId &) const override;
  Outcome<MovementAnchor>
  original_actor_anchor(const StableId &) const override;
  Outcome<MovementAnchor> original_site_anchor(const StableId &) const override;
  Outcome<Integer> selected_lot_weight(const CargoSelection &) const override;
  Status verify_registration_source(const TransportUnitState &) const override;
  Status verify_initiator(const GroupInitiator &, const TransportUnitRef &,
                          const SubjectRef &,
                          const SourceRef &accepted_business) const override;
  Status
  validate_actor_carried_location(const StableId &,
                                  const CarriedLocation &) const override;
  Outcome<WorldMoment> current_moment() const override;
  Outcome<std::shared_ptr<const ReadStamp>>
  capture_manifest_reads(const TransportManifestRead &) const override;

private:
  // 仅真实人口/业务分支调用，不以默认对象或全拒接口替代缺失拥有根。
  const PopulationTransportState &required_population() const;
  const TransportRegistrationBusinessRead &required_business() const;
  const CommonStore &common_;
  const CommonCatalog &common_catalog_;
  const TravelCatalog &travel_catalog_;
  const MovementExactState &movement_;
  const SpatialCarrierState &carriers_;
  const TransportRegistrationState &registrations_;
  const LocalEconomyState &economy_;
  const PopulationTransportState *population_;
  const AuthoredActorFoundationState &foundation_;
  const ActorFoundationCatalog &actor_catalog_;
  const TransportRegistrationBusinessRead *business_;
  const CurrentReadStampRead &stamps_;
  const TransportVehicleTakeover *vehicles_;
  const FamilyCatalog *family_catalog_;
};
} // namespace hanmo::resident
