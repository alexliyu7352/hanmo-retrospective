#include "dynamic_objective.hpp"
#include "dynamic_action.hpp"
#include "dynamic_land_postures.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include <algorithm>
namespace hanmo::resident {using namespace movement_detail;
Outcome<MomentOwnerSource> terminate_dynamic_military_session_from_objective_in_candidate(
 CommonStore& common,const MilitaryState& military,MilitaryExactState& exact,
 const DynamicMilitaryActionContext& c,const DynamicMilitaryObjectiveOriginalRead& original,
 const MomentOwnerSource& source){return guard<MomentOwnerSource>([&]{
 const auto* outcome=take(read_verified_dynamic_military_objective_result(common,exact,c.sessions,original,c.clock,source,c.budget));
 const auto session=outcome->session_ref;const auto generation=outcome->generation;
 auto* current=exact.session_current_states.find(dynamic_military_session_key(session,generation));
 if(!current)fail("DynamicMilitarySessionCurrentRequired",session);
 if(const auto* ended=std::get_if<DynamicMilitaryTerminal>(&current->lifecycle)){
  if(!same_source(ended->outcome_source_ref,source.source_ref))fail("DynamicMilitaryObjectiveTerminalReplayMismatch",session);
  take(read_dynamic_military_session_current(common,military,exact,c.sessions,session,generation,take(c.clock.read_current_moment()),c.budget));
  const auto* original_close=take(read_dynamic_military_terminal_source(common,exact,session,generation,c.budget));return MomentOwnerSource{{"military","dynamic_session_state",original_close->id},2};
 }
 if(!same_moment(outcome->effective_at,take(c.clock.read_current_moment()),c.budget))
  fail("DynamicMilitaryObjectiveTerminalPhaseMismatch",session);
 DynamicSessionTerminated terminal;terminal.objective_outcome_source_ref=source.source_ref;
 terminal.actual_exit_or_stay_source_refs=take(original.read_actual_terminal_disposition_sources(*outcome));
 if(terminal.actual_exit_or_stay_source_refs.empty())fail("DynamicMilitaryActualTerminalDispositionRequired",session);
 // Preparation/recovery cease only through the original interruption writer.
 // A released projectile retains the same release source and future impact.
 for(auto& row:exact.actions.rows()){
  auto& action=row.second;if(action.session_ref!=session||action.generation!=generation)continue;
  if(const auto* released=std::get_if<DynamicTacticalReleased>(&action.effect_state);released&&!released->impact_source_ref){
   if(take(compare_world_moments(released->impact_due_at,outcome->effective_at,c.budget))<=0)
    fail("DynamicMilitarySameMomentCommittedEffectUnconsumed",action.id);
   if(std::find_if(terminal.retained_committed_effect_refs.begin(),terminal.retained_committed_effect_refs.end(),
      [&](const auto& r){return same_source(r,released->release_source_ref);})!=terminal.retained_committed_effect_refs.end())
    fail("DynamicMilitaryDuplicateRetainedCommitment",action.id);
   terminal.retained_committed_effect_refs.push_back(released->release_source_ref);
  }
  if(std::holds_alternative<DynamicTacticalPreparing>(action.phase)||std::holds_alternative<DynamicTacticalRecovering>(action.phase))
   take(interrupt_dynamic_military_action_in_candidate(common,exact,c,action.id,source));
 }
 for(auto& unit:current->unit_tactical_states)if(const auto* original_activation=dynamic_military_posture_activation(unit.posture)){
  const DynamicPostureClosed close{unit.unit_ref,*original_activation,DynamicPostureCloseReason::session_terminal,source.source_ref,{}};
  terminal.closed_posture_refs.push_back(take(apply_dynamic_military_session_transition_in_candidate(common,military,exact,
   c.sessions,c.clock,session,generation,close,c.budget)));
 }
 const auto ref=take(apply_dynamic_military_session_transition_in_candidate(common,military,exact,c.sessions,c.clock,
  session,generation,terminal,c.budget));
 take(read_dynamic_military_session_current(common,military,exact,c.sessions,session,generation,outcome->effective_at,c.budget));
 return MomentOwnerSource{ref,2};
});}
}
