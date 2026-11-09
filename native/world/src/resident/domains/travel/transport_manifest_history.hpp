// 实际进入时的不可变manifest证据；不作为任何当前库存/人口数量写账。
#pragma once
#include "spatial_carrier.hpp"
#include "../../storage/inventory_entry_evidence.hpp"
namespace hanmo::resident {
struct TransportInventoryManifest {InventoryEntryEvidence entry_evidence;};
struct TransportPopulationManifest {SourceRef population_ref;Integer population_revision=0;TransportPopulationSource actual;};
struct TransportEntryManifestSource {StableId id;MovementExecutionRef execution;Integer entry_ordinal=0;PassageBoundaryRef boundary;WorldMoment occurred_at;TransportManifestRead manifest;std::vector<TransportInventoryManifest> inventory_sources;std::vector<TransportPopulationManifest> population_sources;};
struct TransportVesselDerivedTotals {StableId vessel_id;Integer persons=0,weight=0,maximum_persons=0,maximum_weight=0;};
// Restore-only derived values. None of these totals has a physical field in V.
struct TransportManifestDerivedTotals {Integer persons=0,working_crew=0,weight=0;std::vector<TransportVesselDerivedTotals> vessels;};
struct GroupMovementExecutionState;struct TransportRegistrationState;
class MilitaryTransportManifestHistoryRead {
public:
 virtual ~MilitaryTransportManifestHistoryRead()=default;
 virtual Status validate_military_manifest_history(const TransportEntryManifestSource&)const=0;
 virtual Outcome<TransportManifestDerivedTotals> read_military_manifest_derived(const TransportEntryManifestSource&)const=0;
 virtual Outcome<std::vector<StableId>> read_original_military_entry_inventory_ids(const TransportEntryManifestSource&)const=0;
};
class TransportManifestHistorySourceRead {
public:
 virtual ~TransportManifestHistorySourceRead()=default;
 virtual bool bound_to(const CommonStore&,const TransportRegistrationState&)const noexcept=0;
 virtual Outcome<Integer> population_manifest_revision(const SourceRef&)const=0;
 virtual Outcome<TransportPopulationSource> read_population_manifest_history(const SourceRef&,Integer population_revision,const WorldMoment& actual_entry_at)const=0;
 virtual Status validate_inventory_manifest_history(const TransportInventoryManifest&)const=0;
 virtual Status validate_population_manifest_history(const TransportPopulationManifest&,const WorldMoment& actual_entry_at)const=0;
 virtual Status validate_military_manifest_history(const TransportEntryManifestSource&)const=0;
 virtual Outcome<TransportManifestDerivedTotals> read_military_manifest_derived(const TransportEntryManifestSource&)const=0;
};
class NativeTransportInventoryEntryRead final:public InventoryEntryOriginalSourceRead {
 const CommonStore& common_;const SpatialCarrierState& carriers_;const MovementExactState& movement_;const GroupMovementExecutionState& executions_;const TransportRegistrationState& registrations_;const TravelCatalog& catalog_;const MilitaryTransportManifestHistoryRead* military_;
public:
 NativeTransportInventoryEntryRead(const CommonStore& c,const SpatialCarrierState& s,const MovementExactState& m,const GroupMovementExecutionState& e,const TransportRegistrationState& r,const TravelCatalog& t,const MilitaryTransportManifestHistoryRead* military=nullptr):common_(c),carriers_(s),movement_(m),executions_(e),registrations_(r),catalog_(t),military_(military){}
 bool bound_to(const CommonStore& c,const SpatialCarrierState& s,const MovementExactState& m,const GroupMovementExecutionState& e,const TransportRegistrationState& r)const noexcept{return &common_==&c&&&carriers_==&s&&&movement_==&m&&&executions_==&e&&&registrations_==&r;}
 Outcome<InventoryEntryAdoptionScope> read_original_inventory_entry_scope(const SourceRef&)const override;
};
Outcome<SourceRef> capture_transport_entry_manifest_in_candidate(CommonStore&,const SpatialCarrierState&,const MovementExactState&,GroupMovementExecutionState&,const CommonCatalog&,const InventoryEntryItemSources*,const SpatialCarrierSources&,const TransportManifestHistorySourceRead&,const MovementExecutionRef&,Integer entry_ordinal,const PassageBoundaryRef&,const WorldMoment&,ExactArithmeticBudget budget={});
// 借用原entry整行；仅在本候选/冻结读寿命内有效，不另拷库存与人口证明。
Outcome<const TransportEntryManifestSource*> read_historical_transport_entry_manifest(const CommonStore&,const SpatialCarrierState&,const MovementExactState&,const GroupMovementExecutionState&,const TransportManifestHistorySourceRead&,const MovementExecutionRef&,Integer entry_ordinal,const PassageBoundaryRef&,Integer manifest_revision,Integer composition_revision,ExactArithmeticBudget budget={});
Outcome<TransportManifestRead> read_historical_transport_manifest(const CommonStore&,const SpatialCarrierState&,const MovementExactState&,const GroupMovementExecutionState&,const TransportManifestHistorySourceRead&,const MovementExecutionRef&,Integer entry_ordinal,const PassageBoundaryRef&,Integer manifest_revision,Integer composition_revision,ExactArithmeticBudget budget={});
// Call only in the private decoded candidate, after original Common/S1/S2/P02
// source rows and immutable basis links have been restored. This has no IDs,
// facts, wallet writes, authority adoption or clock changes. Full source cold
// validation still follows before publication. A failed candidate is discarded.
Status hydrate_transport_entry_manifest_derived_values(const CommonStore&,const SpatialCarrierState&,const MovementExactState&,GroupMovementExecutionState&,const TransportRegistrationState&,const NativeTransportInventoryEntryRead&,const TransportManifestHistorySourceRead&,ExactArithmeticBudget budget={});
Status validate_transport_entry_manifest_sources(const CommonStore&,const SpatialCarrierState&,const MovementExactState&,const GroupMovementExecutionState&,const TransportManifestHistorySourceRead&,ExactArithmeticBudget budget={});
}
