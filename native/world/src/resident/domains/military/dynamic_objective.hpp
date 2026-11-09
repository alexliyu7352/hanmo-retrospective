// Fixed S2 §2.3 F / §2.5 objective semantics. No new clock or round conversion.
#pragma once
#include "dynamic_session.hpp"
#include "dynamic_land_postures.hpp"
namespace hanmo::resident {
struct DynamicObjectiveUnitRead {
 DynamicLandPostureUnit actual_unit;
 // Present only for the original, currently lawful retreat responsibility.
 // The original reader verifies its start/progress/exit and pin chain.
 std::optional<SourceRef> actual_routing_source;
};
struct DynamicObjectiveOriginalInputs {
 StableId session_ref;Integer generation=0;WorldMoment effective_at;
 BattleObjectiveDefinition original_definition;
 DynamicMilitarySessionDefinitionRead original_timing;
 DynamicMilitaryObjectiveCurrent current;
 std::vector<StableId> actual_protected_unit_refs;
 std::vector<DynamicObjectiveUnitRead> actual_units;
 DynamicObjectiveCause actual_cause=DynamicObjectiveCause::actual_entry;
 std::vector<SourceRef> actual_cause_sources;
};
struct DynamicObjectiveEvaluation {
 std::vector<StableId> actual_qualified_unit_refs,actual_contesting_unit_refs;
 std::optional<DynamicObjectiveQualificationTransition> qualification_transition;
 std::optional<MilitaryBattleResult> outcome;
};
Outcome<DynamicObjectiveEvaluation> evaluate_dynamic_military_objective(
 const DynamicObjectiveOriginalInputs&,ExactArithmeticBudget={});
class DynamicMilitaryObjectiveOriginalRead {
public:
 virtual ~DynamicMilitaryObjectiveOriginalRead()=default;
 // The original consumer must prove the complete after-S4/S5 inputs at this
 // actual phase and all changed geometry/participation sources. It must not
 // supply pre-casualty or today's values for a historical result.
 virtual Outcome<DynamicObjectiveOriginalInputs> read_actual_objective_inputs(
  const DynamicMilitarySessionCurrent2&,const WorldMoment&,
  DynamicObjectiveCause,const std::vector<SourceRef>& actual_causes)const=0;
 // Reconstructs only the immutable original heads before this result's real
 // Common append cut. It must prove complete same-M roster/geometry/S4/S5
 // coverage; today's session current cannot stand in for that historical cut.
 virtual Outcome<DynamicObjectiveOriginalInputs> read_original_objective_inputs_before_source(
  const DynamicMilitaryObjectiveEvidence2&)const=0;
 // Actual exit or retained-in-place responsibility, after the objective and
 // original CLC/custody/participation consumers. No automatic return to camp.
 virtual Outcome<std::vector<SourceRef>> read_actual_terminal_disposition_sources(
  const DynamicMilitaryObjectiveEvidence2&)const=0;
};
struct DynamicObjectiveQualificationApplication {
 DynamicObjectiveEvaluation evaluation;
 std::optional<MomentOwnerSource> actual_transition;
};
// Only true false/true condition changes create a transition. The returned
// outcome is a pure result until the original complete terminal writer adopts
// it in this same candidate, after all original consequences are applied.
Outcome<DynamicObjectiveQualificationApplication> apply_dynamic_military_objective_qualification_in_candidate(
 CommonStore&,const MilitaryState&,MilitaryExactState&,
 const DynamicMilitarySessionOriginalRead&,const DynamicMilitaryObjectiveOriginalRead&,
 const CurrentMomentRead&,const StableId& session,Integer generation,
 DynamicObjectiveCause,const std::vector<SourceRef>& actual_causes,ExactArithmeticBudget={});
// Appends the actual military/military_objective_result Common source only if
// the original finite judgment has completed. A qualification change is adopted
// first. The original terminal consumer must close the same candidate before
// publication; this function does not fabricate exits, CLC or battle effects.
Outcome<std::optional<MomentOwnerSource>> apply_dynamic_military_objective_result_in_candidate(
 CommonStore&,const MilitaryState&,MilitaryExactState&,
 const DynamicMilitarySessionOriginalRead&,const DynamicMilitaryObjectiveOriginalRead&,
 const CurrentMomentRead&,const StableId& session,Integer generation,
 DynamicObjectiveCause,const std::vector<SourceRef>& actual_causes,ExactArithmeticBudget={});
Outcome<const DynamicMilitaryObjectiveEvidence2*> read_verified_dynamic_military_objective_result(
 const CommonStore&,const MilitaryExactState&,const DynamicMilitarySessionOriginalRead&,
 const DynamicMilitaryObjectiveOriginalRead&,const CurrentMomentRead&,
 const MomentOwnerSource&,ExactArithmeticBudget={});
Status validate_dynamic_military_objective_results(const CommonStore&,const MilitaryExactState&,
 const DynamicMilitarySessionOriginalRead&,const DynamicMilitaryObjectiveOriginalRead&,
 const CurrentMomentRead&,ExactArithmeticBudget={});
struct DynamicMilitaryActionContext;
Outcome<MomentOwnerSource> terminate_dynamic_military_session_from_objective_in_candidate(
 CommonStore&,const MilitaryState&,MilitaryExactState&,
 const DynamicMilitaryActionContext&,const DynamicMilitaryObjectiveOriginalRead&,
 const MomentOwnerSource& actual_objective_result);
struct DynamicObjectiveGateRead {StableId session_ref;Integer generation=0;WorldMoment at;SourceRef actual_basis;};
// Earliest exact deadline/continuous-hold gate. It names the retained real
// initialization or qualification source, never a synthetic due fact.
Outcome<std::optional<DynamicObjectiveGateRead>> predict_dynamic_military_objective_gate(
 const CommonStore&,const MilitaryState&,const MilitaryExactState&,
 const DynamicMilitarySessionOriginalRead&,const StableId& session,Integer generation,
 const WorldMoment& actual_current,ExactArithmeticBudget={});
}
