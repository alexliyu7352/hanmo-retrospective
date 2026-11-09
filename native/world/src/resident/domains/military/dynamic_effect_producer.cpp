#include "dynamic_effect_producer.hpp"
#include "resident/domains/actor_foundation/condition_source_read.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include <algorithm>
#include <iterator>
#include <set>
#include <tuple>
namespace hanmo::resident {using namespace movement_detail;namespace {
bool same_refs(const std::vector<SourceRef>& a,const std::vector<SourceRef>& z){if(a.size()!=z.size())return false;for(std::size_t n=0;n<a.size();++n)if(!same_source(a[n],z[n]))return false;return true;}
void add(std::vector<SourceRef>& out,const SourceRef& ref){if(!source_valid(ref))fail("DynamicMilitaryAppliedConsumerSourceRequired",ref.id);if(std::none_of(out.begin(),out.end(),[&](const auto& r){return same_source(r,ref);}))out.push_back(ref);}
SourceRef actual_header(const CommonStore& common,const StableId& id){const auto* h=common.state().facts.find(id);if(!h||h->id!=id)fail("DynamicMilitaryAppliedConsumerHeadRequired",id);return {h->domain,h->kind,h->id};}
const DynamicMilitaryBattleFact2& battle_for(const MilitaryExactState& exact,const StableId& group){const DynamicMilitaryBattleFact2* found=nullptr;for(const auto& row:exact.dynamic_battle_facts.rows())if(row.second.effect_group_ref==group){if(found)fail("DynamicMilitaryDuplicateEffectResult",group);found=&row.second;}if(!found)fail("DynamicMilitaryBattleSourceRequired",group);return *found;}
const DynamicMilitaryCasualtyFact2& casualty_for(const MilitaryExactState& exact,const StableId& group){const DynamicMilitaryCasualtyFact2* found=nullptr;for(const auto& row:exact.dynamic_casualty_facts.rows())if(row.second.effect_group_ref==group){if(found)fail("DynamicMilitaryDuplicateEffectResult",group);found=&row.second;}if(!found)fail("DynamicMilitaryCasualtySourceRequired",group);return *found;}
void same_candidate(const CommonStore& common,const MilitaryState& military,const MilitaryExactState& exact,const DynamicMilitaryGroupWriteContext& c){
 if(&c.actions.clock!=&c.land.clock||!c.participation.bound_to(common,military,exact))fail("DynamicMilitaryGroupSameCandidateRequired");
 if (bool(c.population) != bool(c.land.population) ||
     (c.population && (&c.population->common != &common ||
                       &c.population->military != &military ||
                       c.population->military_exact != &exact ||
                       c.population->actual_clock != &c.actions.clock ||
                       &c.population->state != c.land.population ||
                       &c.population->catalog != c.land.population_catalog ||
                       c.population->dynamic_land_sources != &c.land.effects ||
                       c.population->selected_version != 4)))
   fail("DynamicMilitaryPopulationSameCandidateRequired");
 if(&c.named.originals.common!=&common||&c.named.originals.state!=&c.named.foundation||&c.named.originals.catalog!=&c.named.catalog||c.named.originals.military!=&military||c.named.originals.military_exact!=&exact||c.named.originals.military_formations!=c.land.formations||c.named.originals.military_returns!=c.land.returns)fail("DynamicMilitaryNamedSameCandidateRequired");
}
}
Outcome<DynamicMilitaryGroupApplication> apply_dynamic_land_effect_group_in_candidate(CommonStore& common,MilitaryState& military,MilitaryExactState& exact,const DynamicMilitaryGroupWriteContext& c,const StableId& session,Integer generation){return guard<DynamicMilitaryGroupApplication>([&]{
 same_candidate(common,military,exact,c);const auto at=take(c.actions.clock.read_current_moment());const auto b=c.actions.budget;if(session.empty()||generation<1)fail("DynamicMilitarySessionIdentityRequired",session);
 const auto* current=exact.session_current_states.find(dynamic_military_session_key(session,generation));if(!current)fail("DynamicMilitarySessionCurrentRequired",session);
 // Replaying an actually completed same-M group performs no ID, draw, service,
 // body, population, expense or participation write.
 const DynamicBattleEffectGroup2* prior=nullptr;
 for(const auto& row:exact.effect_groups.rows())if(row.second.session_ref==session&&same_moment(row.second.effective_at,at,b)){const auto input=take(c.originals.read_original_land_effect_inputs(row.second));if(input.generation!=generation)continue;if(prior)fail("DuplicateDynamicMilitarySameMomentGroup",session);prior=&row.second;}
 if(prior){take(validate_dynamic_military_effect_groups(common,military,exact,c.battle_catalog,c.actions.catalog,c.originals,c.actions.clock,b));const auto& casualty=casualty_for(exact,prior->id);take(validate_dynamic_military_land_consequence(common,military,exact,c.land,{"military","military_casualties",casualty.id}));DynamicMilitaryGroupApplication out;out.effect_group=MomentOwnerSource{{"military","dynamic_effect_group",prior->id},2};out.battle=MomentOwnerSource{{"military","military_battle",battle_for(exact,prior->id).id},2};out.casualty=MomentOwnerSource{{"military","military_casualties",casualty.id},2};if(prior->objective_result_ref)out.objective=MomentOwnerSource{*prior->objective_result_ref,2};out.already_applied=true;return out;}
 const auto due=take(collect_dynamic_military_actions_at(common,exact,c.actions,session,generation,at));DynamicMilitaryGroupApplication out;
 std::vector<DynamicMilitaryActionDue> effects;
 for(const auto& item:due){if(!item.committed_projectile&&item.stage==MilitaryTacticalWorkStage::recovery){take(complete_dynamic_military_action_recovery_in_candidate(common,exact,c.actions,item.action_ref));const auto* action=exact.actions.find(item.action_ref);const auto* done=action?std::get_if<DynamicTacticalCompleted>(&action->phase):nullptr;if(!done||!same_moment(done->completed_at,at,b))fail("DynamicMilitaryActualRecoveryRequired",item.action_ref);out.actual_recovery_sources.push_back({done->result_source_ref,2});}else effects.push_back(item);}
 if(effects.empty())return out;
 const bool already_terminal=std::holds_alternative<DynamicMilitaryTerminal>(current->lifecycle);
 if(already_terminal){const auto& terminal=std::get<DynamicMilitaryTerminal>(current->lifecycle);if(terminal.unresolved_committed_effect_refs.empty()||take(compare_world_moments(terminal.closed_at,at,b))>=0||std::any_of(effects.begin(),effects.end(),[](const auto& item){return !item.committed_projectile;}))fail("DynamicMilitaryRetainedImpactRequired",session);}
 DynamicBattleEffectGroup2 group;group.id=take(common.next_id("dynamic_effect_group"));group.session_ref=session;group.effective_at=at;const SourceRef group_ref{"military","dynamic_effect_group",group.id};
 // This real append is the immutable before-effects cut. Due source headers
 // below represent the actual selected positive gates, not predicted events.
 take(common.append_fact({group.id,group_ref.kind,group_ref.domain}));
 for(const auto& item:effects){const auto* action=exact.actions.find(item.action_ref);if(!action||action->session_ref!=session||action->generation!=generation||!same_moment(item.at,at,b))fail("DynamicMilitaryActualDueActionRequired",item.action_ref);const SourceRef effect{"military","dynamic_effect",take(common.next_id("dynamic_effect"))};take(common.append_fact({effect.id,effect.kind,effect.domain}));group.due_effect_refs.push_back(effect);group.once_keys.push_back({session,action->id,item.committed_projectile?1:0});}
 const auto input=take(c.originals.read_original_land_effect_inputs(group));
 if(input.session_ref!=session||input.generation!=generation||!same_source(input.actual_effect_group_source,group_ref)||!same_moment(input.effective_at,at,b)||!same_refs(input.complete_due_effect_refs,group.due_effect_refs)||input.once_keys.size()!=group.once_keys.size())fail("DynamicMilitaryActualDueSetMismatch",group.id);
 for(std::size_t n=0;n<group.once_keys.size();++n){const auto& a=group.once_keys[n];const auto& z=input.once_keys[n];if(a.session_ref!=z.session_ref||a.action_ref!=z.action_ref||a.effect_ordinal!=z.effect_ordinal)fail("DynamicMilitaryActualDueSetMismatch",group.id);}
 const auto calculated=take(calculate_dynamic_land_effect_group(c.battle_catalog,c.actions.catalog,input,b));take(adopt_dynamic_land_calculated_fields(group,input,calculated));
 const auto group_id=group.id;if(!exact.effect_groups.insert(group_id,std::move(group)))fail("DuplicateDynamicMilitaryEffectGroup",group_id);out.effect_group=MomentOwnerSource{group_ref,2};
 // Once the immutable full group exists, original action sources can resolve
 // its real activation/release/impact; no caller supplied elapsed credit.
 for(const auto& source:input.complete_due_effect_refs)take(adopt_dynamic_military_action_effect_in_candidate(common,exact,c.actions,{source,2}));
 take(adopt_dynamic_land_effect_current_in_candidate(common,military,exact,c.battle_catalog,c.actions.catalog,c.originals,c.actions.sessions,c.actions.clock,group_ref,b));
 const auto roster=take(apply_dynamic_military_land_consequences_in_candidate(common,military,exact,c.land,group_ref));if(roster.already_applied)fail("DynamicMilitaryFreshGroupReusedCasualty",group_id);out.battle=roster.battle_source;out.casualty=roster.casualty_source;
 auto& named=c.named;out.named=take(apply_dynamic_military_named_conditions_in_candidate(common,named.foundation,named.deaths,named.catalog,military,exact,named.life,roster.casualty_source.source_ref,named.reads,named.stamps,named.versions,named.originals,named.capability_changes,named.care_life));
 auto* saved=exact.effect_groups.find(group_id);for(const auto& receipt:out.named.condition_receipts)add(saved->condition_source_refs,character_condition_receipt_source(receipt));for(const auto& death:out.named.death_refs)add(saved->condition_source_refs,{death.domain,death.kind,death.id});for(const auto& transition:out.named.availability_transition_ids)add(saved->condition_source_refs,military_availability_transition_source(transition));
 if(c.population){const auto applied=take(consume_exact_population_military_casualty_in_candidate(*c.population,roster.casualty_source.source_ref));for(const auto& id:applied.fact_ids)add(out.population_sources,actual_header(common,id));for(const auto& id:applied.receipt_ids)add(out.population_sources,actual_header(common,id));saved=exact.effect_groups.find(group_id);saved->population_source_refs=out.population_sources;}
 auto participation=take(c.participation.consume_actual_military_casualty_in_candidate(roster.casualty_source));saved=exact.effect_groups.find(group_id);for(const auto& ref:participation.actual_terminal_refs)add(saved->terminal_refs,ref);out.actual_spatial_writes=std::move(participation.actual_spatial_writes);
 // Only the original after-all-consequences reader can judge the objective.
 // CLC/actual stays or exits are then required by the terminal source writer.
 if(!already_terminal){
 std::optional<DynamicObjectiveCause> objective_cause;std::vector<SourceRef> objective_causes;
 const bool qualification_changed=!calculated.strikes.allocation.member_allocations.empty()||!calculated.commander_losses.consequences.empty()||!calculated.dispositions.member_dispositions.empty()||std::any_of(calculated.postures.morale_changes.begin(),calculated.postures.morale_changes.end(),[](const auto& x){return x.before_morale!=x.after_morale;})||std::any_of(calculated.strikes.allocation.morale_changes.begin(),calculated.strikes.allocation.morale_changes.end(),[](const auto& x){return x.before_morale!=x.after_morale;})||std::any_of(calculated.command_shock.morale_changes.begin(),calculated.command_shock.morale_changes.end(),[](const auto& x){return x.before_morale!=x.after_morale;});
 if(!calculated.dispositions.actual_exits.empty()){objective_cause=DynamicObjectiveCause::actual_exit;objective_causes={group_ref};}
 else if(qualification_changed){objective_cause=DynamicObjectiveCause::unit_qualification_changed;objective_causes={roster.casualty_source.source_ref};}
 else{const auto gate=take(predict_dynamic_military_objective_gate(common,military,exact,c.actions.sessions,session,generation,at,b));if(gate&&same_moment(gate->at,at,b)){objective_cause=DynamicObjectiveCause::objective_gate;objective_causes={gate->actual_basis};}}
 if(objective_cause)out.objective=take(apply_dynamic_military_objective_result_in_candidate(common,military,exact,c.actions.sessions,c.objectives,c.actions.clock,session,generation,*objective_cause,objective_causes,b));
 if(out.objective){saved=exact.effect_groups.find(group_id);saved->objective_result_ref=out.objective->source_ref;out.terminal=take(terminate_dynamic_military_session_from_objective_in_candidate(common,military,exact,c.actions,c.objectives,*out.objective));saved=exact.effect_groups.find(group_id);add(saved->terminal_refs,out.terminal->source_ref);}
 }
 take(c.originals.validate_original_effect_group_consumers(*exact.effect_groups.find(group_id)));
 take(validate_dynamic_military_effect_groups(common,military,exact,c.battle_catalog,c.actions.catalog,c.originals,c.actions.clock,b));
 take(validate_dynamic_military_land_consequence(common,military,exact,c.land,roster.casualty_source.source_ref));
 take(validate_dynamic_military_session_actions(common,military,exact,c.actions,session,generation,at));take(read_dynamic_military_session_current(common,military,exact,c.actions.sessions,session,generation,at,b));
 if(out.objective||already_terminal)take(validate_dynamic_military_objective_results(common,exact,c.actions.sessions,c.objectives,c.actions.clock,b));
 return out;
});}
}
