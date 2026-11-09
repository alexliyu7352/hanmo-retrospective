#include "dynamic_effect_adoption.hpp"
#include "dynamic_session_random_history.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include <algorithm>
namespace hanmo::resident {using namespace movement_detail;namespace {
bool refs(const std::vector<SourceRef>& a,const std::vector<SourceRef>& b){if(a.size()!=b.size())return false;for(std::size_t n=0;n<a.size();++n)if(!same_source(a[n],b[n]))return false;return true;}
bool tactical_equal(const DynamicMilitaryUnitTacticalState& a,const DynamicMilitaryUnitTacticalState& z,ExactArithmeticBudget b){if(a.unit_ref!=z.unit_ref||military_local_body_key(a.body_ref)!=military_local_body_key(z.body_ref)||!same_source(a.entry_snapshot_ref,z.entry_snapshot_ref)||a.tactic_points!=z.tactic_points||!same_source(a.tactic_points_source_ref,z.tactic_points_source_ref)||a.statuses.size()!=z.statuses.size()||!dynamic_military_posture_values_equal(a.posture,z.posture,b)||!refs(a.qualification_source_refs,z.qualification_source_refs))return false;for(std::size_t n=0;n<a.statuses.size();++n)if(!dynamic_military_status_values_equal(a.statuses[n],z.statuses[n],b))return false;return true;}
DynamicMilitaryUnitTacticalState& unit(DynamicMilitarySessionCurrent2& s,const StableId& id){for(auto& u:s.unit_tactical_states)if(u.unit_ref==id)return u;fail("DynamicMilitarySessionUnitRequired",id);}
void install_status(DynamicMilitaryUnitTacticalState& u,const DynamicLandStatusChange& change,ExactArithmeticBudget b){auto old=std::find_if(u.statuses.begin(),u.statuses.end(),[&](const auto& s){return s.kind==change.kind;});if(bool(change.before)!=(old!=u.statuses.end())||(change.before&&!dynamic_military_status_values_equal(*change.before,*old,b)))fail("DynamicMilitaryStatusAdoptionBeforeMismatch",u.unit_ref);if(old==u.statuses.end())u.statuses.push_back(change.after);else *old=change.after;std::sort(u.statuses.begin(),u.statuses.end(),[](const auto& a,const auto& z){return a.kind<z.kind;});}
const DynamicBattleEffectGroup2& group(const CommonStore& common,const MilitaryExactState& exact,const SourceRef& ref){if(ref.domain!="military"||ref.kind!="dynamic_effect_group")fail("DynamicMilitaryEffectGroupRequired",ref.id);const auto* h=common.state().facts.find(ref.id);const auto* g=exact.effect_groups.find(ref.id);if(!h||h->domain!=ref.domain||h->kind!=ref.kind||!g||g->id!=ref.id)fail("DynamicMilitaryEffectGroupRequired",ref.id);return *g;}
}
Outcome<DynamicLandCurrentAdoption> adopt_dynamic_land_effect_current_in_candidate(CommonStore& common,const MilitaryState& military,MilitaryExactState& exact,const MilitaryBattleCatalog& battle_catalog,const DynamicMilitaryActionCatalog2& action_catalog,const DynamicMilitaryEffectOriginalRead& originals,const DynamicMilitarySessionOriginalRead& sessions,const CurrentMomentRead& clock,const SourceRef& ref,ExactArithmeticBudget b){return guard<DynamicLandCurrentAdoption>([&]{
 const auto& saved=group(common,exact,ref);const auto in=take(originals.read_original_land_effect_inputs(saved));take(validate_dynamic_land_calculated_fields(battle_catalog,action_catalog,in,saved,b));auto* current=exact.session_current_states.find(dynamic_military_session_key(in.session_ref,in.generation));if(!current||!same_source(current->initialization_source_ref,in.initialization_source))fail("DynamicMilitaryEffectSessionMismatch",ref.id);
 DynamicLandCurrentAdoption out{ref,{},{}};
 if(same_source(current->last_session_transition_ref,ref)){take(originals.validate_original_effect_group_consumers(saved));out.already_applied=true;return out;}
 if(!same_moment(take(clock.read_current_moment()),saved.effective_at,b))fail("DynamicMilitaryEffectCurrentPhaseMismatch",ref.id);
 auto* terminal=std::get_if<DynamicMilitaryTerminal>(&current->lifecycle);
 if(terminal){
  if(terminal->unresolved_committed_effect_refs.empty()||take(compare_world_moments(terminal->closed_at,saved.effective_at,b))>=0||saved.once_keys.size()!=saved.due_effect_refs.size()||!in.actual_activations.empty())fail("DynamicMilitaryRetainedImpactRequired",ref.id);
  for(std::size_t n=0;n<saved.once_keys.size();++n){const auto& key=saved.once_keys[n];const auto* action=exact.actions.find(key.action_ref);const auto* released=action?std::get_if<DynamicTacticalReleased>(&action->effect_state):nullptr;
   if(key.effect_ordinal!=1||!action||action->session_ref!=in.session_ref||action->generation!=in.generation||!released||!released->impact_source_ref||!same_source(*released->impact_source_ref,saved.due_effect_refs[n])||!same_moment(released->impact_due_at,saved.effective_at,b)||take(compare_world_moments(released->released_at,terminal->closed_at,b))>0||std::none_of(terminal->unresolved_committed_effect_refs.begin(),terminal->unresolved_committed_effect_refs.end(),[&](const auto& r){return same_source(r,released->release_source_ref);}))fail("DynamicMilitaryTerminalImpactBindingMismatch",key.action_ref);
  }
 }

 if(current->unit_tactical_states.size()!=in.before_effects.size())fail("DynamicMilitaryEffectCurrentCoverage",ref.id);
 for(const auto& original:in.before_effects)if(!tactical_equal(unit(*current,original.original_tactical.unit_ref),original.original_tactical,b))fail("DynamicMilitaryEffectCurrentBeforeMismatch",original.original_tactical.unit_ref);
 // No random call occurs here: the saved canonical slots must extend exactly
 // this one current stream. A stale quote cannot debit a later stream.
 const auto original_random=take(read_dynamic_military_random_before_source(common,exact,ref,saved.effective_at,b));
 take(validate_dynamic_military_random_chain(original_random,{},in.rng_before));
 auto after=current->session_rng;if(!saved.rng_slots.empty()){const auto& last=saved.rng_slots.back();after.current_state=last.after_state;after.draw_ordinal=last.after_draw_ordinal;after.last_commit_source_ref=last.effect_ref;}
 take(validate_dynamic_military_random_chain(in.rng_before,{},current->session_rng));take(validate_dynamic_military_random_chain(current->session_rng,saved.rng_slots,after));
 // Replacements close the actual previous posture at this same M, before the
 // unique current is replaced. All real closures are retained in its source table.
 for(const auto& effect:saved.posture_effects)if(const auto* applied=std::get_if<DynamicLandActivationResult>(&effect)){if(applied->failure!=DynamicLandActivationFailure::none||!applied->installed_posture)continue;auto& u=unit(*current,applied->unit_ref);if(const auto* old=dynamic_military_posture_activation(u.posture)){DynamicPostureClosed close{u.unit_ref,*old,DynamicPostureCloseReason::replaced,applied->effect_ref,applied->effect_ref};out.actual_posture_closures.push_back(take(apply_dynamic_military_session_transition_in_candidate(common,military,exact,sessions,clock,in.session_ref,in.generation,close,b)));}u.posture=*applied->installed_posture;}
 for(const auto& effect:saved.posture_effects)if(const auto* points=std::get_if<DynamicTacticPointChange2>(&effect)){auto& u=unit(*current,points->unit_ref);if(u.tactic_points!=points->before_points)fail("DynamicMilitaryPointAdoptionBeforeMismatch",u.unit_ref);u.tactic_points=points->after_points;u.tactic_points_source_ref=ref;}
 for(const auto& change:saved.status_changes)install_status(unit(*current,change.unit_ref),change,b);
 for(const auto& effect:saved.posture_effects)if(const auto* share=std::get_if<DynamicSupportConsumption2>(&effect)){auto& u=unit(*current,share->supporter_unit_ref);auto* support=std::get_if<DynamicSupportPosture>(&u.posture);if(!support||!same_source(support->token.activation_ref,share->activation_ref)||support->token.target_unit_ref!=share->target_unit_ref||support->token.consumed_by_effect_ref)fail("DynamicMilitarySupportAlreadyConsumed",share->activation_ref.id);support->token.consumed_by_effect_ref=share->consumed_by_effect_ref;}
 const auto consequences=take(calculate_dynamic_land_effect_group(battle_catalog,action_catalog,in,b));
 for(const auto& exit:consequences.dispositions.actual_exits){if(std::any_of(current->objective_current.escaped_units.begin(),current->objective_current.escaped_units.end(),[&](const auto& x){return x.unit_ref==exit.unit_ref;}))fail("DynamicMilitaryActualExitAlreadyAdopted",exit.unit_ref);DynamicMilitaryEscapedUnit actual{exit.unit_ref,exit.actual_effect_group};take(sessions.validate_actual_escape(in.session_ref,in.generation,actual));current->objective_current.escaped_units.push_back(std::move(actual));}
 std::sort(current->objective_current.escaped_units.begin(),current->objective_current.escaped_units.end(),[](const auto& a,const auto& z){return a.unit_ref<z.unit_ref;});
 current->session_rng=std::move(after);current->last_session_transition_ref=ref;
 if(terminal){for(const auto& key:saved.once_keys){const auto& released=std::get<DynamicTacticalReleased>(exact.actions.find(key.action_ref)->effect_state);auto found=std::find_if(terminal->unresolved_committed_effect_refs.begin(),terminal->unresolved_committed_effect_refs.end(),[&](const auto& r){return same_source(r,released.release_source_ref);});if(found==terminal->unresolved_committed_effect_refs.end())fail("DynamicMilitaryRetainedImpactAlreadySettled",key.action_ref);terminal->unresolved_committed_effect_refs.erase(found);}}

 // The caller now runs real F/S4/S5/objective/CLC and original action adoption;
 // only its enclosing complete candidate may run stable validation and publish.
 return out;
});}
}
