#include "care_handover_settlement.hpp"
#include "resident/domains/travel/care_dependent_placement.hpp"
namespace hanmo::resident {
Outcome<BirthCareNeedApplication> complete_care_settlement_handover_and_need_in_candidate(
 CommonStore& common,CareGoalWriteTables goals,CareCoverageWriteTables coverage,
 OrderedTable<CareHandoverExecution>& executions,ActionElapsedStore& elapsed,
 TravelDependentState& dependents,const CareCoverageSources& sources,
 const CareHandoverBusinessRead& business,const CareDependentPlacementContext& placement,
 const StableId& handover,CareHandoverServiceCompletionInputs&& service,
 AffairsState& affairs,const StateNeedPolicyRead& policies,const StateGoalRead& goal_read,
 StateTaskConsumer& tasks,CareFamilyActivityWriteBinding* activities){
 const auto& h=sources.handovers;const auto& g=h.acceptance.goals;
 if(&common!=&g.common||&placement.common!=&common||&placement.dependents!=&dependents||
  &placement.handovers!=&h||&executions!=&h.executions||&elapsed!=&h.elapsed||
  &goals.requirements!=&g.goals.requirements||&goals.transitions!=&g.goals.transitions||
  &goals.temporal_events!=&g.goals.temporal_events||&coverage.bindings!=&sources.tables.bindings||
  &coverage.satisfied!=&sources.tables.satisfied||&coverage.lost!=&sources.tables.lost||
  g.coverage!=&sources||!sources.placements||!sources.placements->bound_to(common,dependents))
  return {false,{},common_failure("CareSettlementSameCandidateRequired",handover,ErrorKind::internal).error};
 if((g.rules.selected_care_version()!=5&&g.rules.selected_care_version()!=6)||placement.selected_version!=2||!business.bound_to(common)||
  !sources.originals.bound_to(common)||!sources.periods.bound_to(common))
  return {false,{},common_failure("CareSettlementOriginalOwnersRequired",handover,ErrorKind::unsupported_capability).error};
 // Establish the original H/last actual period before asking S1 to perform
 // anything. This source-local read also handles an exact retained replay;
 // it neither reads nor manufactures a future Completed/Need result.
 const MomentOwnerSource execution{care_handover_execution_source(handover),3};
 auto original=read_care_handover_placement_input(h,execution.source_ref,execution.source_schema);
 if(!original.ok)return {false,{},std::move(original.error)};
 auto applied=adopt_care_settlement_handover_in_candidate(common,dependents,placement,execution);
 if(!applied.ok)return {false,{},std::move(applied.error)};
 CareHandoverCompletionInputs inputs;
 inputs.care_activity_ref=std::move(service.care_activity_ref);
 inputs.placement_result_ref=std::move(applied.value.placement_result);
 inputs.actual_participation_refs=std::move(service.actual_participation_refs);
 inputs.fee_result_refs=std::move(service.fee_result_refs);
 inputs.transition_rule_ref=std::move(service.transition_rule_ref);
 // The existing owner re-reads the actual S1 result, authority, activity,
 // service periods and effects, then releases the sole Action and adopts its
 // terminal. A late failure propagates so the whole private world is discarded.
 // Re-entry returns the original satisfaction and source-episode Need, without
 // repeating the S1 operation or applying today's requirement to the old run.
 return complete_care_handover_and_need_in_candidate(common,goals,coverage,executions,
  elapsed,sources,business,handover,inputs,affairs,policies,goal_read,tasks,activities);
}
}
