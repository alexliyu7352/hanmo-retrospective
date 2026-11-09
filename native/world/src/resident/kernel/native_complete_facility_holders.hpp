// Stable-address composition of the original selected specialist readers.
// Root membership routes a call; only the original reader proves its authority.
#pragma once
#include "resident/domains/craft/craft_facility_sources.hpp"
#include "resident/domains/local_economy/household_facility_sources.hpp"
#include <optional>
namespace hanmo::resident {
class NativeCompleteFacilityHolderRead final:public FacilityHolderSourceRead {
    const FacilityClaimsState& claims_;
    const WorldMoment& current_;
    const CraftJobsState* craft_jobs_;
    const ItemInstancesState* repair_items_;
    const HouseholdPurchaseState* household_purchases_;
    ExactArithmeticBudget arithmetic_;
    std::optional<NativeCraftFacilityHolderRead> craft_;
    std::optional<HouseholdFacilitySourceRead> household_;
    bool bound_=false;
    void require_bound()const;
    const FacilityHolderSourceRead& specialist(const ReservationRef&)const;
public:
    // Null means an explicitly unselected root, never an invented empty root.
    // Roots and current must outlive this immovable same-snapshot router.
    NativeCompleteFacilityHolderRead(const FacilityClaimsState&,const WorldMoment&,
        const CraftJobsState*,const ItemInstancesState* selected_repair_items,
        const HouseholdPurchaseState*,ExactArithmeticBudget={});
    NativeCompleteFacilityHolderRead(const NativeCompleteFacilityHolderRead&)=delete;
    NativeCompleteFacilityHolderRead&operator=(const NativeCompleteFacilityHolderRead&)=delete;
    NativeCompleteFacilityHolderRead(NativeCompleteFacilityHolderRead&&)=delete;
    NativeCompleteFacilityHolderRead&operator=(NativeCompleteFacilityHolderRead&&)=delete;
    // Construct the genuine readers once, after the surrounding household /
    // Craft contexts exist and before any business callback. Contexts outlive us.
    Status bind_sources(const CraftFacilityOccupancyContext*,const HouseholdPurchaseContext*);
    Outcome<FacilityHolderKind> read_facility_holder_kind(const ReservationRef&)const override;
    Status validate_facility_holder_selection(const FacilityHolderProfileSelection&)const override;
    Outcome<std::vector<FacilityClaimDependency>> enumerate_holder_claim_dependencies(const FacilitySlotRef&)const override;
    Status validate_holder_claim_admission(const FacilityClaim2&,const FacilityClaimMutationReceipt&)const override;
    Status validate_holder_claim_close(const FacilityClaim2&,FacilityClaimCloseReason,const FacilityClaimMutationReceipt&)const override;
    Status validate_holder_mutation_source(const FacilitySlotRef&,const FacilityClaimMutationReceipt&)const override;
    Outcome<bool> has_actual_facility_service_started(const FacilityClaim2&,const WorldMoment&)const override;
};
}
