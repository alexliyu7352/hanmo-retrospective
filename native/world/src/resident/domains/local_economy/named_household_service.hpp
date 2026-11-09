#pragma once
#include "household_purchase_contract.hpp"
namespace hanmo::resident {
struct NamedHouseholdServiceCoverage {StableId purchase_ref,original_action_ref;SourceRef admission_result_ref;StableId active_claim_ref;MomentSpan covered_span;std::vector<FacilitySourceRef> current_capacity_source_refs;SourceRef usage_right_ref;std::vector<SourceRef> actor_participation_source_refs;};
struct NamedHouseholdResumeQuote {WorldIdentity identity;StableId purchase_id;SourceRef suspension_source;WorldMoment resumed_at;HouseholdPurchasePresence presence;FacilityClaimQuote claim;std::vector<FacilitySourceRef> capacity_sources;};
Outcome<const HouseholdServiceEvent2*> read_named_household_service_event(const HouseholdPurchaseContext&,const SourceRef&);
Outcome<NamedHouseholdServiceCoverage> read_named_household_service_coverage(const HouseholdPurchaseContext&,const StableId&,const MomentSpan&);
Status record_named_household_admission_in_candidate(CommonStore&,HouseholdPurchaseState&,const HouseholdPurchaseContext&,const StableId&,FacilityClaim2&,FacilityClaimMutationReceipt&);
Status provide_named_household_service_interval_in_candidate(HouseholdPurchaseState&,ActionElapsedStore&,const HouseholdPurchaseContext&,const StableId&,const MomentSpan&);
Outcome<FacilityClaimMutationReceipt> close_named_household_service_owner_in_candidate(CommonStore&,HouseholdPurchaseState&,ActionElapsedStore&,const HouseholdPurchaseContext&,const StableId&);
Outcome<FacilityClaimMutationReceipt> suspend_named_household_service_owner_in_candidate(CommonStore&,HouseholdPurchaseState&,ActionElapsedStore&,const HouseholdPurchaseContext&,const StableId&,const SourceRef& actual_interruption,bool terminate=false);
Outcome<NamedHouseholdResumeQuote> preview_resume_household_purchase_service(const HouseholdPurchaseContext&,const StableId&);
Outcome<StableId> resume_household_purchase_service_in_candidate(CommonStore&,HouseholdPurchaseState&,FacilityClaimsState&,ActionElapsedStore&,const HouseholdPurchaseContext&,const NamedHouseholdResumeQuote&);
Outcome<ActionTimeOwnerProof> read_named_household_action_time_owner(const HouseholdPurchaseContext&,const ActionTimeRegistration&,const StableId&);
Status validate_named_household_fixed_sources(const HouseholdPurchaseContext&,const ActionTimeOwnerProof&,const ActionElapsedState&,const std::vector<FixedServiceTransition>&);
Status validate_named_household_service_claim(const HouseholdPurchaseContext&,const FacilityClaim2&,const FacilityClaimMutationReceipt&,bool admission);
Status validate_named_household_service(const HouseholdPurchaseContext&,const HouseholdPurchase&);
}
