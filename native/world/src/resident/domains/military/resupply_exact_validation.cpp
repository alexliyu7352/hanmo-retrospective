#include "resupply_exact_detail.hpp"
#include "resupply_original_rules.hpp"
namespace hanmo::resident {using namespace resupply_exact_detail;namespace resupply_exact_detail {
Status validate_current_escrow(const CommonStore& c,const MilitaryState& m,const ExactResupplyReadContext& x,const DynamicMilitaryLogisticsFact2& f){return status([&]{
 const ArmyRecord* army=nullptr;for(const auto& a:m.armies)if(a.id==f.original_terms.army_id)army=&a;
 if(!army||army->commander_actor_id!=f.actor_id||army->organization_id!=f.original_terms.organization_id||army->garrison_id!=f.original_terms.source_garrison_id||army->member_actor_ids!=participants(f,x))fail("ArmyActionAuthorityMismatch",f.id);
 take(validate_original_army_resupply_escrow(c,m,x.catalog,x.access,f.id,f.original_terms));
});}
Status validate_fact_history(const CommonStore& c,const MilitaryState& military,const MilitaryActionTimeState& state,const ExactResupplyReadContext& x,const DynamicMilitaryLogisticsFact2& f,const ActionElapsedState& entry,bool pending){return status([&]{
 bound(c,military,state,x);header(c,f.id);const auto& t=f.original_terms;
 if(f.command!="army_resupply"||f.id.empty()||f.actor_id!=t.actor_id||f.rule_version!=2||f.elapsed_source_ref.action_ref!=f.id||f.original_context||t.authority||!t.goods||!t.money||*t.money<0||*t.money>1000000||!t.source_inventory_id||!t.source_wallet_id)fail("ExactResupplyAcceptanceIdentityMismatch",f.id);
 const GarrisonDefinition* camp=nullptr;const GarrisonDefinition* origin=nullptr;for(const auto& row:x.catalog.garrisons){if(row.id==t.garrison_id)camp=&row;if(row.id==t.source_garrison_id)origin=&row;}
 if(!camp||!origin||!c.actor(f.actor_id)||t.army_id.empty()||t.organization_id.empty()||t.place_id!=camp->place_id||*t.source_inventory_id!=camp->inventory_id||*t.source_wallet_id!=camp->wallet_id||t.goods->size()>1)fail("ExactResupplyOriginalTermsMismatch",f.id);
 for(const auto& g:*t.goods)if(g.first!=origin->food_item_id||g.second<1||g.second>1000000||!x.access.common_catalog||!x.access.common_catalog->items.find(g.first))fail("MilitarySupplyTermsMismatch",f.id);
 for(const auto& legacy:military.logistics_facts)if(legacy.id==f.id)fail("DuplicateLegacyExactMilitaryAction",f.id);
 for(const auto& legacy:military.action_facts)if(legacy.id==f.id)fail("DuplicateLegacyExactMilitaryAction",f.id);
 for(const auto& legacy:military.logistics_actions)if(legacy.action_id==f.id)fail("DuplicateLegacyExactMilitaryExecution",f.id);
 if(state.dynamic_action_facts.find(f.id))fail("DuplicateExactMilitaryAction",f.id);
 take(x.original.validate_resupply_acceptance(f));participants(f,x);const auto now=take(x.clock.read_current_moment());
 if(entry.action_ref!=f.id||entry.execution_source_ref!=f.id||entry.duration_definition_ref!="army_resupply_original_1_half_day_v2"||entry.owner_cost_state_ref!=f.id||entry.owner_work_remainder||!same_moment(entry.started_at,f.started_at,x.budget)||compare(entry.required_units.value,take(fixed_duration_work_units(1,x.scale.units_per_half_day,x.budget)).value,x.budget)||take(compare_world_moments(entry.last_settled_at,f.started_at,x.budget))<0||take(compare_world_moments(entry.last_settled_at,now,x.budget))>0)fail("ExactResupplyElapsedBindingMismatch",f.id);
 const auto credited=take(duration_to_scale_units(take(elapsed_world_duration(f.started_at,entry.last_settled_at,x.budget)),x.scale.units_per_half_day,x.budget));
 if(compare(credited.value,entry.credited_units.value,x.budget)||compare(entry.credited_units.value,entry.required_units.value,x.budget)>0)fail("ExactResupplyActualServiceMismatch",f.id);
 if(take(compare_world_moments(f.started_at,entry.last_settled_at,x.budget))<0)take(x.original.validate_resupply_actual_service(f,{f.started_at,entry.last_settled_at}));
 if(f.phase=="started"){
  if(f.completed_at||f.result||f.success||f.terminal_source_ref)fail("ExactResupplyPrematureResult",f.id);
  const auto order=take(compare_world_moments(entry.last_settled_at,due(f,x.budget),x.budget));if(order>0||(!pending&&order==0))fail("ExactResupplyUnconsumedCompletion",f.id);
 }else if(f.phase=="completed"){
  // Full-gate resupply has no independent trigger and changes no position.
  // Its result address is an owned field; it is not a Common causal header.
  if(!f.completed_at||!f.result||f.terminal_source_ref||!same_moment(*f.completed_at,due(f,x.budget),x.budget)||!same_moment(*f.completed_at,entry.last_settled_at,x.budget)||compare(entry.credited_units.value,entry.required_units.value,x.budget)||f.result->ok||!f.result->order_id.empty()||!f.result->receipt_id.empty()||f.result->actor_changes||f.result->army_id!=(f.success?t.army_id:StableId{}))fail("ExactResupplyCompletedResultMismatch",f.id);
  take(x.original.validate_resupply_completed_result(f));
 }else fail("InvalidExactResupplyPhase",f.id);
});}
}
Status validate_exact_resupply_sources(const CommonStore& c,const MilitaryState& military,const MilitaryActionTimeState& state,const ActionElapsedStore& elapsed,const ExactResupplyReadContext& x,bool pending){return status([&]{
 NativeResupplyActionTimeRead reader(c,military,state,elapsed,x);const auto reg=exact_resupply_action_registration();
 for(const auto& row:state.dynamic_logistics_facts.rows())if(row.second.command=="army_resupply"){
  const auto& f=row.second;if(row.first!=f.id)fail("ExactResupplySourceKeyMismatch",row.first);const auto* entry=find_fixed_elapsed(elapsed,f.id);if(!entry)fail("ExactResupplyElapsedRequired",f.id);take(validate_fact_history(c,military,state,x,f,*entry,pending));
  if(pending&&f.phase=="started")take(read_fixed_service_intervals_at_completion_due(elapsed,f.id,f.started_at,entry->last_settled_at,take(x.clock.read_current_moment()),x.scale,x.budget));else take(read_fixed_service_intervals(elapsed,f.id,f.started_at,entry->last_settled_at,take(x.clock.read_current_moment()),x.scale,x.budget));
  const auto proof=take(reader.read_action_time_owner(reg,f.id));std::vector<FixedServiceTransition> transitions;for(const auto& item:elapsed.fixed_service_transitions)if(item.action_ref==f.id)transitions.push_back(item);take(reader.validate_fixed_action_sources(reg,proof,*entry,transitions));
  if(f.phase=="started"){const auto* a=c.action(f.id);if(!a||!std::holds_alternative<ElapsedRunning>(entry->timing)||a->remaining_ticks!=1)fail("ExactResupplyActiveEnvelopeMismatch",f.id);for(const auto& id:participants(f,x)){const auto* actor=c.actor(id);if(!actor||actor->action_id!=f.id)fail("ExactResupplyOccupationMissing",id);}take(validate_current_escrow(c,military,x,f));}
  else if(c.action(f.id)||!std::holds_alternative<ElapsedTerminal>(entry->timing))fail("ExactResupplyTerminalNotReleased",f.id);
 }
 for(const auto& row:c.state().actions.rows())if(row.second.kind=="army_resupply"){
  bool legacy=false;for(const auto& old:military.logistics_facts)if(old.id==row.first)legacy=true;
  if(!legacy&&!state.dynamic_logistics_facts.find(row.first))fail("MissingExactResupplySource",row.first);
 }
});}
Outcome<const DynamicMilitaryLogisticsFact2*> read_verified_exact_resupply_result(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s,const ActionElapsedStore& elapsed,const ExactResupplyReadContext& x,const SourceRef& ref,Integer schema){return guard<const DynamicMilitaryLogisticsFact2*>([&]{
 if(schema!=2||ref.domain!="military"||ref.kind!="military_logistics_completed")fail("ExactResupplyCompletedSourceRequired",ref.id);const auto& f=fact(s,ref.id);const auto* e=find_fixed_elapsed(elapsed,f.id);if(!e||f.phase!="completed")fail("ExactResupplyCompletedSourceRequired",ref.id);
 take(validate_fact_history(c,m,s,x,f,*e,false));take(read_fixed_service_intervals(elapsed,f.id,f.started_at,*f.completed_at,take(x.clock.read_current_moment()),x.scale,x.budget));NativeResupplyActionTimeRead reader(c,m,s,elapsed,x);auto reg=exact_resupply_action_registration();auto proof=take(reader.read_action_time_owner(reg,f.id));std::vector<FixedServiceTransition> transitions;for(const auto& t:elapsed.fixed_service_transitions)if(t.action_ref==f.id)transitions.push_back(t);take(reader.validate_fixed_action_sources(reg,proof,*e,transitions));if(c.action(f.id)||!std::holds_alternative<ElapsedTerminal>(e->timing))fail("ExactResupplyTerminalNotReleased",f.id);return &f;
});}
}
