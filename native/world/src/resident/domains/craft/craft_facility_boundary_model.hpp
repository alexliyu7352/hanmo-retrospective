// Original S9 until/capacity adoption. No second current occupancy or timer.
#pragma once
#include "craft_facility_occupancy_model.hpp"
#include "resident/domains/local_economy/economic_facility_sources.hpp"
namespace hanmo::resident {
// 整 slot 复核共用真实触发到期 episode；其余受影响 claim 不伪称自身到期。
struct CraftFacilityClaimUntil {StableId claim_id;FacilityClaimReceiptRef admission_receipt_ref;};
struct CraftFacilityCapacityChanged {FacilitySourceRef actual_change;};
using CraftFacilityBoundaryCause=std::variant<CraftFacilityClaimUntil,CraftFacilityCapacityChanged>;
struct CraftFacilityUnchangedService {StableId claim_id;};
struct CraftFacilityReadmittedService {StableId previous_claim_id,new_claim_id;MomentSpan accepted_window;std::vector<FacilityPhysicalUnitRef> selected_physical_units;};
struct CraftFacilitySuspendedWithoutPlacement {StableId previous_claim_id;MomentOwnerSource actual_control_ref;};
struct CraftFacilityRetainedPlacement {StableId previous_claim_id,retained_claim_id;MomentSpan accepted_review_window;MomentOwnerSource actual_occupancy_ref;std::optional<MomentOwnerSource> actual_control_ref;};
struct CraftFacilityClearedPlacement {StableId previous_claim_id;MomentOwnerSource actual_clearance_ref;};
using CraftFacilityBoundaryDisposition=std::variant<CraftFacilityUnchangedService,CraftFacilityReadmittedService,CraftFacilitySuspendedWithoutPlacement,CraftFacilityRetainedPlacement,CraftFacilityClearedPlacement>;
struct CraftFacilityBoundarySource {
 StableId id;CraftOccupancyOwnerRef job_or_repair_ref;std::optional<StableId> previous_boundary_ref;
 WorldMoment occurred_at;CraftFacilityBoundaryCause cause;MomentOwnerSource original_terms_ref;
 std::vector<MomentOwnerSource> adopted_input_refs;CraftFacilityBoundaryDisposition disposition;
};
// Eligibility is original body/place/contract/skill evidence. Technical errors
// are errors; a real temporary block has its own verified original source.
struct CraftFacilityEligible {
 std::vector<MomentOwnerSource> actual_inputs;
 // Calculated by the original accepted service/window rule. A maximum claim
 // length alone does not select a new window. Empty permits only an unchanged
 // still-valid claim; renewal requires this real original result.
 std::optional<MomentSpan> accepted_service_window;
};
struct CraftFacilityActuallyBlocked {MomentOwnerSource actual_blocking_source;std::vector<MomentOwnerSource> actual_inputs;};
using CraftFacilityEligibility=std::variant<CraftFacilityEligible,CraftFacilityActuallyBlocked>;
SourceRef craft_facility_boundary_source(const StableId&);
}
