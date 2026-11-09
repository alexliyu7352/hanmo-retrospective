// Actual S9 holder evidence. No second claim table and no synthesized due fact.
#pragma once
#include "craft_facility_occupancy.hpp"
#include "resident/facility_claims/facility_claim_runtime.hpp"
namespace hanmo::resident {
struct CraftJob;struct RepairJob;
enum class CraftFacilityBusinessKind {started,resumed,paused,inputs_rebound,completed,cancelled,interrupted,occupancy,boundary};
struct CraftFacilityBusinessSourceView {
 MomentOwnerSource actual_source;CraftOccupancyOwnerRef original_owner;
 FacilitySlotRef slot_ref;ReservationRef holder;WorldMoment occurred_at;
 CraftFacilityBusinessKind kind=CraftFacilityBusinessKind::started;
 const CraftJob* craft_job=nullptr;const RepairJob* repair_job=nullptr;
 const std::vector<StableId>* original_claim_refs=nullptr;
 const CraftFacilityOccupancyEvent* occupancy=nullptr;
};
// Original acceptance/control/terminal payload validation, independent of the
// shared claim sweep. Original professional adapters must not recurse into S0.
Outcome<CraftFacilityBusinessSourceView> read_craft_facility_business_source(
 const CraftFacilityOccupancyContext&,const MomentOwnerSource&);
class NativeCraftFacilityHolderRead final:public FacilityHolderSourceRead {
 const CraftFacilityOccupancyContext& x_;
public:
 explicit NativeCraftFacilityHolderRead(const CraftFacilityOccupancyContext&x):x_(x){}
 Outcome<FacilityHolderKind> read_facility_holder_kind(const ReservationRef&)const override;
 Status validate_facility_holder_selection(const FacilityHolderProfileSelection&)const override;
 Outcome<std::vector<FacilityClaimDependency>> enumerate_holder_claim_dependencies(const FacilitySlotRef&)const override;
 Status validate_holder_claim_admission(const FacilityClaim2&,const FacilityClaimMutationReceipt&)const override;
 Status validate_holder_claim_close(const FacilityClaim2&,FacilityClaimCloseReason,const FacilityClaimMutationReceipt&)const override;
 Status validate_holder_mutation_source(const FacilitySlotRef&,const FacilityClaimMutationReceipt&)const override;
 Outcome<bool> has_actual_facility_service_started(const FacilityClaim2&,const WorldMoment&)const override;
};
}
