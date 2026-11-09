// P24.1/FH5.3B actual handover outcome. Physical/activity owners must have
// applied their original results in this same unpublished candidate first.
#pragma once
#include "care_coverage.hpp"
#include "care_handover_time.hpp"
namespace hanmo::resident {
struct CareFamilyActivityWriteBinding;
struct CareHandoverCompletionInputs {
 MomentOwnerSource care_activity_ref,placement_result_ref;
 std::vector<MomentOwnerSource> actual_participation_refs,fee_result_refs;
 ActorFoundationRuleRef transition_rule_ref;
};
// All supplied references are independently re-read by the original owners;
// possession of these values is never permission or evidence by itself.
// This writes the one original binding/satisfaction/event, releases the one
// real Action, then adopts its Terminal. Any failure discards the whole world
// candidate, including earlier physical placement and original period effects.
Outcome<CareRequirementApplication> complete_care_handover_in_candidate(
 CommonStore&,CareGoalWriteTables,CareCoverageWriteTables,
 OrderedTable<CareHandoverExecution>&,ActionElapsedStore&,
 const CareCoverageSources&,const CareHandoverBusinessRead&,
 const StableId& actual_handover,const CareHandoverCompletionInputs&,
 CareFamilyActivityWriteBinding* activities=nullptr);
Outcome<BirthCareNeedApplication> complete_care_handover_and_need_in_candidate(
 CommonStore&,CareGoalWriteTables,CareCoverageWriteTables,
 OrderedTable<CareHandoverExecution>&,ActionElapsedStore&,
 const CareCoverageSources&,const CareHandoverBusinessRead&,
 const StableId& actual_handover,const CareHandoverCompletionInputs&,
 AffairsState&,const StateNeedPolicyRead&,const StateGoalRead&,StateTaskConsumer&,
 CareFamilyActivityWriteBinding* activities=nullptr);
}
