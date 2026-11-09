#pragma once
#include "dynamic_session_model.hpp"
#include "resident/domains/military/battle_model.hpp"
namespace hanmo::resident {
// Completed result only, in the existing dynamic_military/2.objective_evidence
// slot. No deadline, holding timer, roster or other mutable balance is copied.
struct DynamicMilitaryObjectiveEvidence2 {
 StableId id,session_ref;Integer generation=0;WorldMoment effective_at;
 StableId objective_definition_ref;
 SourceRef session_initialization_source_ref,objective_transition_source_ref;
 DynamicObjectiveCause cause_kind=DynamicObjectiveCause::actual_entry;
 std::vector<SourceRef> cause_source_refs,actual_judgment_source_refs,actual_exit_source_refs;
 MilitaryBattleResult outcome;
};
}
