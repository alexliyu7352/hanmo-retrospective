#pragma once
#include "director_subject2.hpp"
#include "resident/domains/local_economy/household_purchase_demand.hpp"
namespace hanmo::resident {
// One finite route of the original Director. A cohort keeps its actual population
// selection and S5 owner-period execution; no synthetic Actor or command is made.
class CohortPurchaseDirectorRoute2 {
 const DirectorState& director_;const HouseholdPurchaseContext& purchase_;const HouseholdPurchaseKnownMarketRead& knowledge_;
public:
 CohortPurchaseDirectorRoute2(const DirectorState& director,const HouseholdPurchaseContext& purchase,const HouseholdPurchaseKnownMarketRead& knowledge):director_(director),purchase_(purchase),knowledge_(knowledge){}
 Outcome<std::optional<SubjectDecisionProposal2>> propose(const DirectorRegistration2&,const CohortKnownSubjectView&,Integer candidates)const;
 Status validate_binding(const SubjectPlanBinding&)const;
 Status validate_effect(const SubjectRef&,const SourceRef&)const;
};
// Called only within the complete WorldBundle candidate, with its same original
// S5/S7 objects. The returned adopted status follows an actual accepted purchase.
Outcome<SubjectDecisionApplied2> apply_director_cohort_purchase_in_candidate(
 CommonStore&,LocalEconomyState&,HouseholdPurchaseState&,FacilityClaimsState&,
 ActionElapsedStore&,HouseholdRetailState&,const HouseholdPurchaseContext&,
 const HouseholdPurchaseKnownMarketRead&,const SubjectDecisionProposal2&);
}
