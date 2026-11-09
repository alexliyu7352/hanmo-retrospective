// Original S1 placement adopted by the existing Repair Fixed input vector.
#pragma once
#include "repair_contract.hpp"
#include "resident/domains/travel/actor_spatial_source_membership.hpp"
namespace hanmo::resident {
struct RepairSpatialSourceContext {
 const RepairReadContext& repair;
 const TravelCatalog& travel_catalog;
 const ActorSpatialHeadSourceReadContext& original;
 const MilitarySpatialHandoffState* military_handoffs=nullptr;
};
// Call at actual Start/resume/input-change M, after the real adopted claim.
// No future service interval is treated as already supplied.
Outcome<FixedAppliedInput> capture_repair_worker_spatial_input(
 const RepairSpatialSourceContext&,const RepairJob&);
// Original input is kept even if the actor has moved since this interval.
Status validate_repair_worker_spatial_span(const RepairSpatialSourceContext&,
 const RepairJob&,const std::vector<FixedServiceInputRef>& actual_inputs,
 const MomentSpan&,const std::vector<StableId>& actual_claim_refs);
}
