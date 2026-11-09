// Facility addendum §5: immutable original S9 placement, no second WIP inventory.
#pragma once
#include "resident/facility_claims/model.hpp"
#include "resident/kernel/world_timeline.hpp"
#include <variant>
#include <vector>
namespace hanmo::resident {
struct CraftOccupancyJobRef {StableId job_ref;};
struct CraftOccupancyRepairRef {StableId repair_ref;};
using CraftOccupancyOwnerRef=std::variant<CraftOccupancyJobRef,CraftOccupancyRepairRef>;
struct CraftWipContent {StableId job_ref;Integer current_stage_ordinal=0;SourceRef last_material_consumption_ref;};
struct RepairItemContent {StableId repair_ref,item_instance_id;MomentOwnerSource original_custody_ref;Integer instance_version=0;};
using CraftFacilityContentRef=std::variant<CraftWipContent,RepairItemContent>;
enum class CraftFacilityOccupancyKind {occupied,continues,cleared};
struct CraftFacilityOccupancyEvent {
 StableId event_id;CraftOccupancyOwnerRef job_or_repair_ref;FacilitySlotRef slot_ref;
 StableId placement_claim_id;std::optional<StableId> previous_event_ref;WorldMoment occurred_at;
 CraftFacilityOccupancyKind occupancy_kind=CraftFacilityOccupancyKind::occupied;
 MomentOwnerSource original_terms_ref;std::optional<MomentOwnerSource> original_stage_result_ref;
 std::vector<CraftFacilityContentRef> actual_content_refs;std::optional<MomentOwnerSource> clearance_result_ref;
};
SourceRef craft_facility_occupancy_source(const StableId&);
}
