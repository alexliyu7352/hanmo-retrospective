#pragma once
#include "household_purchase_contract.hpp"
#include "resident/facility_claims/facility_claim_runtime.hpp"
namespace hanmo::resident {
// 实际家庭owner adapter。总registry由root组合，其它专业holder不由本类兜底。
class HouseholdFacilitySourceRead final:public FacilityHolderSourceRead {
 const HouseholdPurchaseContext& x_;
public:
 explicit HouseholdFacilitySourceRead(const HouseholdPurchaseContext& x):x_(x){}
 Outcome<FacilityHolderKind> read_facility_holder_kind(const ReservationRef&)const override;
 Outcome<bool> has_actual_facility_service_started(const FacilityClaim2&,const WorldMoment&)const override;
 Status validate_facility_holder_selection(const FacilityHolderProfileSelection&)const override;
 Outcome<std::vector<FacilityClaimDependency>> enumerate_holder_claim_dependencies(const FacilitySlotRef&)const override;
 Status validate_holder_claim_admission(const FacilityClaim2&,const FacilityClaimMutationReceipt&)const override;
 Status validate_holder_claim_close(const FacilityClaim2&,FacilityClaimCloseReason,const FacilityClaimMutationReceipt&)const override;
 Status validate_holder_mutation_source(const FacilitySlotRef&,const FacilityClaimMutationReceipt&)const override;
};
}
