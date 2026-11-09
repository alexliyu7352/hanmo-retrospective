#pragma once
#include "craft_facility_boundary.hpp"
#include "craft_job.hpp"
#include "repair_contract.hpp"
namespace hanmo::resident {
struct CraftFacilityBoundaryWriteContext {
 CommonStore& common;CraftJobsState& jobs;ItemInstancesState& items;ActionElapsedStore& elapsed;
 const CraftFacilityBoundaryReadContext& read;
 const CraftJobWriteContext& craft;const RepairWriteContext* repair=nullptr;
};
// Original capacity producer calls the one holder algorithm in the shared
// ordered pass. It returns an owner receipt, not an adopted S0 mutation.
Outcome<FacilityClaimMutationReceipt> apply_craft_facility_capacity_boundary_in_candidate(
 CraftFacilityBoundaryWriteContext&,const FacilityCapacityReconciliationRequest&,
 const FacilityReconciliationProgress&);
// 真正到期复核沿同原决定算法，返回无 capacity_revision 的准备型，交物理 owner 最后绑定。
Outcome<PreparedFacilityClaimMutation> apply_craft_facility_until_boundary_in_candidate(
 CraftFacilityBoundaryWriteContext&,const FacilityClaimUntilRequest&,
 const FacilityUntilReconciliationProgress&);
}
