#include "naval_repair_legacy_battle.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "naval_repair_exact_detail.hpp"
namespace hanmo::resident {using namespace naval_repair_exact_detail;namespace {
ExactNavalAssetHistoryContext assets(const ExactNavalRepairHistoryContext& x){return {x.actions,x.elapsed,{x.military_catalog,x.access,x.naval_catalog,x.scale,x.clock,x.budget}};}
void source(const ExactNavalRepairHistoryContext& x,const NewNavalRepairTimeSource2& f){if(x.actions.naval_repair_time_sources.find(f.timing.action_ref)!=&f||!x.access.travel_state)fail("CrossCandidateExactNavalRepairSources",f.timing.action_ref);validate_source_shape(x.common,x.naval,x.actions,assets(x).repair_sources,f);}
std::size_t position(const CommonStore& c,const SourceRef& ref){return take(read_actual_common_fact_position(c,ref));}
void not_engaged(const NavalBattleState& before,const StableId& fleet){for(const auto& battle:before.battles)if(battle.frame.phase!="closed")for(const auto& binding:battle.frame.bindings)if(binding.fleet_id==fleet)fail("NavalRepairFleetEngaged",battle.frame.id);}
}
Status validate_exact_naval_repair_legacy_battle_acceptance(const ExactNavalRepairHistoryContext& x,const NavalHistoryAccess& legacy,const NewNavalRepairTimeSource2& f){return status([&]{
 source(x,f);take(visit_naval_battles_before_exact_repair_source(x.common,x.military,x.military_catalog,x.access,*x.access.travel_state,x.naval,x.naval_catalog,legacy,assets(x),f.timing.start_fact_ref,2,[&](const NavalBattleState& before){return status([&]{not_engaged(before,f.original_terms.fleet_id);});}));
});}
Outcome<std::optional<ExactNavalRepairInterruption>> read_first_exact_naval_repair_legacy_contact(const ExactNavalRepairHistoryContext& x,const NavalHistoryAccess& legacy,const NewNavalRepairTimeSource2& f){return guard<std::optional<ExactNavalRepairInterruption>>([&]{
 source(x,f);const auto begin=position(x.common,f.timing.start_fact_ref);const auto now=take(x.clock.read_current_moment());std::optional<ExactNavalRepairInterruption> out;std::optional<std::size_t> first;
 auto inspect=[&](const NavalBattleState& before)->Status{return status([&]{for(const auto& contact:before.contacts){
  if(contact.first_fleet_id!=f.original_terms.fleet_id&&contact.second_fleet_id!=f.original_terms.fleet_id)continue;
  const SourceRef ref{"military_naval","naval_contact",contact.id};const auto index=position(x.common,ref);if(index<=begin)continue;
  // Legacy contact.tick is the original applied Calendar occurrence. Its
  // geometry fraction is not reinterpreted as a new exact source schema.
  const auto at=take(normalize_world_moment(contact.tick,{},x.budget));if(take(compare_world_moments(at,f.timing.started_at,x.budget))<0||take(compare_world_moments(at,now,x.budget))>0)fail("NavalRepairContactClockMismatch",contact.id);
  const bool is_first=contact.first_fleet_id==f.original_terms.fleet_id;const auto& approach=is_first?contact.first_approach:contact.second_approach;const auto& anchor=is_first?contact.first_anchor:contact.second_anchor;
  if(!approach.stationary||approach.actor_id!=f.original_terms.actor_id||!take(same_actual_movement_position(OriginalMovementAnchor{Location{anchor},ref},OriginalMovementAnchor{f.original_terms.anchor,f.timing.start_fact_ref},x.budget)))fail("NavalRepairContactAnchorMismatch",contact.id);
  if(!first||index<*first){first=index;out=ExactNavalRepairInterruption{ExactNavalRepairInterruptionKind::contact,{ref,1},at};}
 }});};
 if(f.timing.result_ref)take(visit_naval_battles_before_exact_repair_source(x.common,x.military,x.military_catalog,x.access,*x.access.travel_state,x.naval,x.naval_catalog,legacy,assets(x),*f.timing.result_ref,2,inspect));
 else take(visit_naval_battle_source_history_with_exact_repairs(x.common,x.military,x.military_catalog,x.access,*x.access.travel_state,x.naval,x.naval_catalog,legacy,assets(x),inspect));
 return out;
});}
Status validate_exact_naval_repair_legacy_battle_span(const ExactNavalRepairHistoryContext& x,const NavalHistoryAccess& legacy,const NewNavalRepairTimeSource2& f,const MomentSpan& span){return status([&]{
 source(x,f);const auto now=take(x.clock.read_current_moment());if(take(compare_world_moments(span.from,span.to,x.budget))>=0||take(compare_world_moments(span.from,f.timing.started_at,x.budget))<0||take(compare_world_moments(span.to,now,x.budget))>0||(f.timing.completion_or_interruption_at&&take(compare_world_moments(span.to,*f.timing.completion_or_interruption_at,x.budget))>0))fail("ExactNavalRepairActualSpanRequired",f.timing.action_ref);
 take(validate_exact_naval_repair_legacy_battle_acceptance(x,legacy,f));const auto first=take(read_first_exact_naval_repair_legacy_contact(x,legacy,f));if(first&&take(compare_world_moments(first->occurred_at,span.to,x.budget))<0)fail("NavalRepairContactNotConsumed",first->actual_cause.source_ref.id);
});}
Status validate_exact_naval_repair_legacy_battle_result(const ExactNavalRepairHistoryContext& x,const NavalHistoryAccess& legacy,const NewNavalRepairTimeSource2& f){return status([&]{
 source(x,f);if(!f.original_result||!f.timing.result_ref||!f.timing.completion_or_interruption_at)fail("ExactNavalRepairResultRequired",f.timing.action_ref);const auto first=take(read_first_exact_naval_repair_legacy_contact(x,legacy,f));const auto& result=*f.original_result;
 if(result.success&&first)fail("NavalRepairContactNotConsumed",result.id);
 if(result.reason=="NavalRepairContact"&&result.actual_interruption_source&&result.actual_interruption_source->source_schema==1&&result.actual_interruption_source->source_ref.domain=="military_naval"&&result.actual_interruption_source->source_ref.kind=="naval_contact"){
  if(!first||!same_source(first->actual_cause.source_ref,result.actual_interruption_source->source_ref)||!same_moment(first->occurred_at,*f.timing.completion_or_interruption_at,x.budget))fail("NavalRepairContactSourceRequired",result.id);
 }
 // Other selected exact contact kinds must be consumed by their real owner.
});}
}
