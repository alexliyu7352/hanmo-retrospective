// S1真实编组登记；接受范围、人口与载具分别沿原业务/S5/S2写口核验。
#pragma once
#include "../local_economy/population_transport.hpp"
#include "spatial_carrier.hpp"
namespace hanmo::resident {
struct BoundSpatialLocationWrite;
struct ProductionPopulationChangeContext;
struct TransportSelectionRegistration {
  StableId selection_id;
  SourceRef departure_anchor_ref;
  std::vector<StableId> release_allocation_ids;
};
struct TransportRegistrationAuthority {
  SourceRef business_source;
  SubjectRef subject;
  GroupInitiator initiator;
  Integer accepted_scope_revision = 0, labor_until_tick = 0;
  std::vector<StableId> accepted_selection_ids, named_participants,
      vehicle_refs, inventory_ids;
  StableId custody_ref;
};
struct TransportRegistrationSource {
  StableId id, transport_unit_id;
  TransportRegistrationAuthority accepted_scope;
  WorldMoment occurred_at;
  MovementAnchor departure_anchor;
  std::vector<TransportSelectionRegistration> selections;
  std::vector<SourceRef> actual_vehicle_takeover_refs;
};
// Accepted scope retains its original selection/source subject. For the finite
// household cohort branch the actual unit subject is the one S5 materialized
// result, read through its original registration and binding, never copied back
// into the accepted business terms.
Outcome<SubjectRef> read_registered_transport_subject(
    const CommonStore &, const LocalEconomyState &,
    const PopulationTransportState &, const TransportRegistrationSource &);
struct TransportUnitReleaseSource {
  StableId id, transport_unit_id, handover_site_id, receiver_custody_ref;
  Integer composition_revision = 0;
  WorldMoment occurred_at;
  MovementAnchor actual_anchor;
  SourceRef accepted_handover_source;
  TransportUnitState released_composition;
  std::vector<SourceRef> cargo_handover_refs, participant_release_refs,
      vehicle_release_refs, obligation_disposition_refs;
  std::vector<StableId> population_binding_ids;
};
struct TransportRegistrationState {
  OrderedTable<TransportRegistrationSource> registrations;
  OrderedTable<TransportUnitReleaseSource> releases;
};
// 此口由已接受业务的唯一owner实现；公开请求不携带自由位置、人数或授权布尔值。
class TransportRegistrationBusinessRead {
public:
  virtual ~TransportRegistrationBusinessRead() = default;
  virtual Outcome<TransportRegistrationAuthority>
  read_registration_authority(const SourceRef &) const = 0;
  virtual Status validate_registration_authority(
      const TransportRegistrationAuthority &) const = 0;
  // Registration acceptance is immutable; subsequent dispatch is checked by
  // the original business owner against its finite current continuation.
  virtual Status
  validate_registered_initiator(const TransportRegistrationSource &,
                                const TransportUnitState &,
                                const GroupInitiator &, const SubjectRef &,
                                const WorldMoment &) const = 0;
  virtual Outcome<std::vector<StableId>> read_releasable_labor_allocations(
      const StableId &accepted_selection,
      const SourceRef &accepted_business) const = 0;
  virtual Status
  validate_transport_handover(const TransportUnitReleaseSource &,
                              const TransportUnitState &) const = 0;
};
class TransportVehicleTakeover {
public:
  virtual ~TransportVehicleTakeover() = default;
  virtual Outcome<MovementAnchor>
  read_vehicle_anchor(const StableId &) const = 0;
  virtual Outcome<SourceRef>
  take_over_vehicle_in_candidate(CommonStore &, const StableId &vehicle_id,
                                 const TransportUnitRef &,
                                 const SourceRef &accepted_business) = 0;
  virtual Status validate_vehicle_takeover(const SourceRef &,
                                           const StableId &vehicle_id,
                                           const TransportUnitRef &) const = 0;
  virtual Status
  release_vehicle_in_candidate(CommonStore &, const StableId &vehicle_id,
                               const TransportUnitRef &,
                               const SourceRef &accepted_handover) = 0;
};
// 真实S1登记源适配S5：selection权限仍沿实际业务provider，登记/释放只认本表真实结果。
class NativePopulationTransportSources final
    : public PopulationTransportSourceRead {
public:
  NativePopulationTransportSources(
      const CommonStore &, const LocalEconomyState &,
      const PopulationTransportState &, const SpatialCarrierState &,
      const TransportRegistrationState &,
      const PopulationTransportSourceRead &business_selection_sources,
      const TransportRegistrationBusinessRead &);
  Outcome<PopulationSelectionAuthority> read_population_selection_authority(
      const PopulationSelectionRequest &) const override;
  Outcome<PopulationMaterializationSource>
  read_population_transport_registration(
      const RegisterPopulationTransportFromSelection &) const override;
  Status validate_population_transport_binding(
      const PopulationTransportBinding &) const override;
  Status
  validate_population_selection_release(const PopulationSelection &,
                                        const SourceRef &) const override;
  Outcome<PopulationTransportReleaseSource>
  read_population_transport_release(const PopulationTransportBinding &,
                                    const SourceRef &) const override;
  Status
  validate_population_transport_release(const PopulationTransportBinding &,
                                        const SourceRef &) const override;

private:
  const CommonStore &common_;
  const LocalEconomyState &economy_;
  const PopulationTransportState &population_;
  const SpatialCarrierState &carriers_;
  const TransportRegistrationState &registrations_;
  const PopulationTransportSourceRead &selection_sources_;
  const TransportRegistrationBusinessRead &business_;
};
struct RegisterTransportUnit {
  SourceRef accepted_business_source;
};
Outcome<TransportUnitRef> register_transport_unit_in_candidate(
    CommonStore &, LocalEconomyState &, PopulationTransportState &,
    SpatialCarrierState &, MovementExactState &, TransportRegistrationState &,
    const SpatialCarrierSources &,
    const PopulationTransportSourceRead &business_selection_sources,
    PopulationProvisionHandoffWriter &,
    const TransportRegistrationBusinessRead &, TransportVehicleTakeover &,
    const RegisterTransportUnit &, const WorldMoment &,
    std::vector<BoundSpatialLocationWrite> &, ExactArithmeticBudget budget = {},
    ProductionPopulationChangeContext *production_inputs = nullptr);
struct DissolveTransportUnit {
  TransportUnitRef transport_unit_ref;
  Integer expected_composition_revision = 0;
  StableId handover_site_id, receiver_custody_ref;
  SourceRef accepted_handover_source;
  std::vector<SourceRef> cargo_handover_refs, participant_release_refs,
      vehicle_release_refs, obligation_disposition_refs;
};
Status dissolve_transport_unit_in_candidate(
    CommonStore &, LocalEconomyState &, PopulationTransportState &,
    SpatialCarrierState &, MovementExactState &, TransportRegistrationState &,
    const SpatialCarrierSources &,
    const PopulationTransportSourceRead &business_selection_sources,
    const TransportRegistrationBusinessRead &, TransportVehicleTakeover &,
    const DissolveTransportUnit &, const WorldMoment &,
    std::vector<BoundSpatialLocationWrite> &, ExactArithmeticBudget budget = {},
    ProductionPopulationChangeContext *production_inputs = nullptr);
Outcome<std::optional<TransportUnitRef>>
read_actor_transport_participation(const SpatialCarrierState &,
                                   const StableId &);
Outcome<MovementAnchor> read_population_transport_anchor(
    const LocalEconomyState &, const PopulationTransportState &,
    const SpatialCarrierState &, const MovementExactState &, const StableId &,
    ExactArithmeticBudget budget = {});
Status validate_transport_registration_sources(
    const CommonStore &, const LocalEconomyState &,
    const PopulationTransportState &, const SpatialCarrierState &,
    const MovementExactState &, const TransportRegistrationState &,
    const PopulationTransportSourceRead &,
    const TransportRegistrationBusinessRead &, const TransportVehicleTakeover &,
    ExactArithmeticBudget budget = {});
Status visit_transport_registration_checkpoint(CodecVisitor &,
                                               TransportRegistrationState &);
} // namespace hanmo::resident
