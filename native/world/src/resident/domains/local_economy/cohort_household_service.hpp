#pragma once
#include "household_purchase_contract.hpp"
namespace hanmo::resident {
// 本读面只借原S5接受/phase/period结果与S0真正claim；不持另一个工量/周期表。
struct CohortHouseholdServiceAttempt {Integer ordinal=0;SourceRef opening_source;FacilityClaimEpisode claim_episode;const HouseholdPurchaseServiceResult* result=nullptr;const WorkPeriodView* period=nullptr;};
struct CohortHouseholdResumeQuote {WorldIdentity identity;StableId purchase_id;Integer phase_revision=0;WorldMoment resumed_at;FacilityClaimQuote claim;std::vector<FacilitySourceRef> capacity_sources;SourceRef actual_continuation_source;};
struct CohortHouseholdPeriodClosed {SourceRef actual_result;std::optional<FacilityClaimMutationReceipt> claim_mutation;};
Outcome<std::vector<CohortHouseholdServiceAttempt>> read_cohort_household_service_attempts(const HouseholdPurchaseContext&,const StableId&);
Status provide_cohort_household_service_interval_in_candidate(HouseholdPurchaseState&,const HouseholdPurchaseContext&,const StableId&,const MomentSpan&);
Outcome<FacilityClaimMutationReceipt> interrupt_cohort_household_service_owner_in_candidate(CommonStore&,HouseholdPurchaseState&,const HouseholdPurchaseContext&,const StableId&,const SourceRef&);
Outcome<CohortHouseholdPeriodClosed> close_cohort_household_period_in_candidate(CommonStore&,HouseholdPurchaseState&,const HouseholdPurchaseContext&,const StableId&);
Outcome<CohortHouseholdResumeQuote> preview_resume_cohort_household_service(const HouseholdPurchaseContext&,const StableId&,const SourceRef& actual_continuation_source);
Outcome<StableId> resume_cohort_household_service_in_candidate(CommonStore&,HouseholdPurchaseState&,FacilityClaimsState&,const HouseholdPurchaseContext&,const CohortHouseholdResumeQuote&);
Status validate_cohort_household_service_claim(const HouseholdPurchaseContext&,const FacilityClaim2&,const FacilityClaimMutationReceipt&,bool admission);
Status validate_cohort_household_service(const HouseholdPurchaseContext&,const HouseholdPurchase&);
}
