// Source-local S9 placement. Physical provider verifies immutable installation
// and claim bindings; this reader never invokes S0's whole history validator.
#pragma once
#include "craft_facility_occupancy_model.hpp"
#include "resident/domains/local_economy/economic_facility_sources.hpp"
namespace hanmo::resident {
struct CraftFacilityBoundaryReadContext;
struct CraftJobReadContext;struct RepairReadContext;struct CraftJobsState;struct ItemInstancesState;
struct CraftFacilityOccupancyContext {
 const CraftJobReadContext& craft;const RepairReadContext* repair=nullptr;
 const FacilityClaimsState& claims;const EconomicFacilitySourcesState& facility_sources;
 const EconomicFacilityOriginalSourceRead& physical;
 const CraftFacilityBoundaryReadContext* boundaries=nullptr;
};
struct CraftFacilityOccupancyView {
 SourceRef source_ref;Integer source_schema=1;FacilitySlotRef slot_ref;
 StableId placement_claim_id;ReservationRef holder;Integer occupied_units=0;
 const std::vector<CraftFacilityContentRef>* actual_content_refs=nullptr;
 WorldMoment observed_at;std::optional<MomentOwnerSource> clearance_result_ref;
 const CraftFacilityOccupancyEvent* original_event=nullptr;
};
Outcome<CraftFacilityOccupancyView> read_craft_facility_occupancy(const CraftFacilityOccupancyContext&,const SourceRef&,Integer source_schema);
Status validate_craft_facility_occupancies(const CraftFacilityOccupancyContext&);
// These are internal owner-writer tails, never command or caller-authored facts.
namespace craft_occupancy_detail {
Status adopt_actual_craft_stage_in_candidate(CommonStore&,CraftJobsState&,const CraftFacilityOccupancyContext&,const StableId& actual_stage);
Status clear_materialized_craft_in_candidate(CommonStore&,CraftJobsState&,const CraftFacilityOccupancyContext&,const StableId& actual_completion);
Status clear_cancelled_craft_in_candidate(CommonStore&,CraftJobsState&,const CraftFacilityOccupancyContext&,const StableId& actual_cancellation_result);
Status place_started_repair_in_candidate(CommonStore&,CraftJobsState&,ItemInstancesState&,const CraftFacilityOccupancyContext&,const StableId& actual_repair);
Status clear_completed_repair_in_candidate(CommonStore&,CraftJobsState&,ItemInstancesState&,const CraftFacilityOccupancyContext&,const StableId& actual_completion);
}
}
