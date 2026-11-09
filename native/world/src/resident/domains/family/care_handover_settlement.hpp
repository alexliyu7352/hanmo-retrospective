// Original same-candidate settlement handover. S1 owns the actual dependency
// operation; Care owns the completion, binding, satisfaction and Need change.
#pragma once
#include "care_handover_completion.hpp"
namespace hanmo::resident {
struct TravelDependentState;
struct CareDependentPlacementContext;
// Only already applied service/fee results enter this port. There is no caller
// placement result or desired position. Each original owner still verifies
// these inputs before the candidate can be published.
struct CareHandoverServiceCompletionInputs {
 MomentOwnerSource care_activity_ref;
 std::vector<MomentOwnerSource> actual_participation_refs,fee_result_refs;
 ActorFoundationRuleRef transition_rule_ref;
};
Outcome<BirthCareNeedApplication> complete_care_settlement_handover_and_need_in_candidate(
 CommonStore&,CareGoalWriteTables,CareCoverageWriteTables,
 OrderedTable<CareHandoverExecution>&,ActionElapsedStore&,TravelDependentState&,
 const CareCoverageSources&,const CareHandoverBusinessRead&,
 const CareDependentPlacementContext&,const StableId& actual_handover,
 CareHandoverServiceCompletionInputs&&,
 AffairsState&,const StateNeedPolicyRead&,const StateGoalRead&,StateTaskConsumer&,
 CareFamilyActivityWriteBinding* activities=nullptr);
}
