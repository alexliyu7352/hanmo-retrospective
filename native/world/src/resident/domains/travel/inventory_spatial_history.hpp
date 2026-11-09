// Original inventory placement, shared by current construction and cold reads.
#pragma once
#include "actor_spatial_history.hpp"
#include "mobile_inventory_adoption.hpp"
namespace hanmo::resident {
struct FixedInventoryPlacement {
 StableId inventory_id;SourceRef original_site_ref;
 std::vector<MomentOwnerSource> original_sources;
};
class FixedInventoryPlacementRead {
public:
 virtual ~FixedInventoryPlacementRead()=default;
 virtual Outcome<std::vector<StableId>> enumerate_original_fixed_inventory_ids()const=0;
 virtual Outcome<FixedInventoryPlacement> read_original_fixed_inventory_placement(const StableId&)const=0;
 // 默认只认原初态必备库；动态业务覆写必须核同 World 的真实 CreateInventory 来源。
 // 这不是任意库的定位许可，也不放宽初态安装入口。
 virtual Status validate_original_fixed_inventory_creation(
  const CommonStore&,const CommonCatalog&,const StableId&)const;
};
struct InventorySpatialHistoryContext {
 const MobileInventoryAdoptionContext& mobile;
 const GroupMovementExecutionState& group_executions;
 const AuthoredActorFoundationState& foundation;const ActorFoundationCatalog& actor_catalog;
 const FixedInventoryPlacementRead& fixed;
 const MilitarySpatialHandoffState* military_handoffs=nullptr;
 const FamilyCatalog* family_catalog=nullptr;
};
struct InventoryHistoricalSpatialPoint {
 StableId inventory_id;WorldMoment observed_at;SourceRef observation_source;
 MovementAnchor actual_anchor;std::vector<MomentOwnerSource> original_sources;
};
// Caller first validates its real service/handover payload and exact M. The
// observation must be an actual Common head; no arbitrary time override.
Outcome<InventoryHistoricalSpatialPoint> read_inventory_spatial_anchor_before_source(
 const InventorySpatialHistoryContext&,const StableId& inventory_id,
 const SourceRef& actual_observation,const WorldMoment& actual_at);
Outcome<StorageReachability> read_storage_reachability_before_source(
 const InventorySpatialHistoryContext&,const StableId& source_inventory,
 const StableId& destination_inventory,const SourceRef& actual_observation,
 const WorldMoment& actual_at,const SourceRef& original_handling_rule);
// Run once after Common and original domain initial rows, before registration.
// Restore never calls this. The actual bound catalog source stays the sole
// position binding; all additional catalog evidence is re-read, not copied.
Status validate_original_fixed_inventory_bindings(
 const CommonStore&,const CommonCatalog&,const TravelCatalog&,const SpatialCarrierState&,
 const TransportRegistrationState&,const FixedInventoryPlacementRead&);
Status install_original_fixed_inventory_bindings_in_candidate(
 const CommonStore&,const CommonCatalog&,const TravelCatalog&,SpatialCarrierState&,
 const TransportRegistrationState&,const FixedInventoryPlacementRead&);
}
