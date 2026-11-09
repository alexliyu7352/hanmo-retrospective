#pragma once
#include "director_cohort_purchase2.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
// The central command owner performs the original prepare/apply and seals its
// actual Common result/history. No result identifier is allocated by Director.
class DirectorActorCommandWrite2 {
public:virtual ~DirectorActorCommandWrite2()=default;
 virtual bool bound_to(const CommonStore&,const DirectorState&)const noexcept=0;
 virtual Outcome<SubjectDecisionApplied2> apply_actor_command_in_candidate(const SubjectDecisionProposal2&)=0;
 virtual Status finalize_actor_adoption_in_candidate(const SubjectDecisionProposal2&,const DirectorAdoptionResult2&,const WorldMoment&)=0;
};
class DirectorHouseholdRoutineWrite2 {
public:virtual ~DirectorHouseholdRoutineWrite2()=default;
 virtual bool bound_to(const CommonStore&,const DirectorState&)const noexcept=0;
 virtual Outcome<SubjectDecisionApplied2> apply_household_routine_in_candidate(const SubjectDecisionProposal2&)=0;
};
class DirectorInstitutionalWrite2 {
public:virtual ~DirectorInstitutionalWrite2()=default;
 virtual bool bound_to(const CommonStore&,const DirectorState&)const noexcept=0;
 virtual Outcome<SubjectDecisionApplied2> apply_institutional_continuation_in_candidate(const SubjectDecisionProposal2&)=0;
 virtual Outcome<SubjectDecisionApplied2> apply_await_officer_in_candidate(const SubjectDecisionProposal2&)=0;
};
struct DirectorCohortPurchaseWrite2 {
 LocalEconomyState& economy;HouseholdPurchaseState& purchases;FacilityClaimsState& claims;
 ActionElapsedStore& elapsed;HouseholdRetailState& retail;
 const HouseholdPurchaseContext& original;const HouseholdPurchaseKnownMarketRead& knowledge;
};
struct NativeDirectorExecutorContext2 {
 CommonStore& common;DirectorState& director;const CurrentMomentRead& clock;
 DirectorActorCommandWrite2& actor_commands;
 DirectorCohortPurchaseWrite2* cohort_purchase=nullptr;
 DirectorHouseholdRoutineWrite2* household_routine=nullptr;
 DirectorInstitutionalWrite2* institutions=nullptr;
};
// Borrows one complete candidate lease. A failure, including the post-adoption
// finalizer, must propagate to the outer world's savepoint without publishing it.
class NativeDirectorSubjectExecutor final:public DirectorSubjectExecutor {
 NativeDirectorExecutorContext2& x_;
 Status check(const SubjectDecisionProposal2&,DirectorDecisionRoute2,SubjectKind)const;
public:
 explicit NativeDirectorSubjectExecutor(NativeDirectorExecutorContext2& x):x_(x){}
 bool bound_to(const CommonStore&,const DirectorState&)const noexcept;
 Outcome<SubjectDecisionApplied2> apply_actor(const SubjectDecisionProposal2&)override;
 Outcome<SubjectDecisionApplied2> apply_cohort_purchase(const SubjectDecisionProposal2&)override;
 Outcome<SubjectDecisionApplied2> apply_cohort_household_routine(const SubjectDecisionProposal2&)override;
 Outcome<SubjectDecisionApplied2> apply_institutional_continuation(const SubjectDecisionProposal2&)override;
 Outcome<SubjectDecisionApplied2> apply_await_officer(const SubjectDecisionProposal2&)override;
 Status finalize_actor_adoption_in_candidate(const SubjectDecisionProposal2&,const DirectorAdoptionResult2&,const WorldMoment&)override;
};
}
