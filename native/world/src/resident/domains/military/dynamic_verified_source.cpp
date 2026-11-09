#include "dynamic_verified_source.hpp"
#include "dynamic_objective.hpp"
#include "military_tactical_ownership.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
namespace hanmo::resident {using namespace movement_detail;namespace {
void header(const CommonStore& common,const SourceRef& ref){
 const auto* row=common.state().facts.find(ref.id);
 if(!row||row->id!=ref.id||row->domain!=ref.domain||row->kind!=ref.kind)
  fail("DynamicMilitaryVerifiedSourceHeaderRequired",ref.id);
}
const DynamicMilitaryCasualtyFact2& casualty_for_group(const MilitaryExactState& exact,const StableId& group){
 const DynamicMilitaryCasualtyFact2* result=nullptr;
 for(const auto& row:exact.dynamic_casualty_facts.rows())if(row.second.effect_group_ref==group){
  if(result||row.first!=row.second.id)fail("DynamicMilitaryDuplicateEffectResult",group);
  result=&row.second;
 }
 if(!result)fail("DynamicMilitaryCasualtySourceRequired",group);return *result;
}
}
Outcome<Integer> read_dynamic_military_source_schema(const SourceRef& ref){return guard<Integer>([&]{
 if(ref.domain!="military"||ref.id.empty())fail("UnsupportedDynamicMilitarySource",ref.id);
 if(ref.kind=="dynamic_session_state"||ref.kind=="dynamic_tactical_action"||
    ref.kind=="dynamic_tactical_recovery"||ref.kind=="dynamic_effect_group"||
    ref.kind=="dynamic_effect"||ref.kind=="military_battle"||ref.kind=="military_casualties"||
    ref.kind=="military_objective_result")return Integer(2);
 if(ref.kind=="tactical_formation_created"||ref.kind=="tactical_mission_adopted"||
    ref.kind=="tactical_intent_accepted")return Integer(3);
 fail("UnsupportedDynamicMilitarySource",ref.id);
});}
Outcome<WorldMoment> read_verified_dynamic_military_source_moment(const CommonStore& common,
 const MilitaryState& military,const MilitaryExactState& exact,
 const DynamicMilitaryVerifiedSourceContext& c,const MomentOwnerSource& source){return guard<WorldMoment>([&]{
 const auto& ref=source.source_ref;if(source.source_schema!=take(read_dynamic_military_source_schema(ref)))
  fail("DynamicMilitaryVerifiedSourceSchemaMismatch",ref.id);
 if(&c.actions.clock!=&c.land.clock)fail("DynamicMilitaryVerifiedSourceClockMismatch",ref.id);
 header(common,ref);const auto now=take(c.actions.clock.read_current_moment());WorldMoment at;
 if(source.source_schema==3){
  if(!c.tactical_ownership)fail("MilitaryTacticalOriginalOwnersRequired",ref.id);
  if(&c.tactical_ownership->clock!=&c.actions.clock)fail("DynamicMilitaryVerifiedSourceClockMismatch",ref.id);
  at=take(read_military_tactical_ownership_source_moment3(common,military,exact,*c.tactical_ownership,source));
 }else if(ref.kind=="military_objective_result"){
  if(!c.objectives)fail("DynamicMilitaryObjectiveOriginalSourceRequired",ref.id);
  const auto* row=take(read_verified_dynamic_military_objective_result(common,exact,c.actions.sessions,
   *c.objectives,c.actions.clock,source,c.actions.budget));at=row->effective_at;
 }else if(ref.kind=="dynamic_session_state"){
  take(validate_dynamic_military_sessions(common,military,exact,c.actions.sessions,now,c.actions.budget));
  at=take(read_dynamic_session_source_moment(common,exact,ref,2,c.actions.budget));
 }else if(ref.kind=="dynamic_tactical_action"||ref.kind=="dynamic_tactical_recovery"){
  take(validate_dynamic_military_actions(common,military,exact,c.actions,now));
  at=take(read_dynamic_military_action_source_moment(common,exact,ref,2));
 }else{
  // A stable read requires the actual complete group and its real F/G consumers,
  // not just a Common header, a due prediction or the mutable current session.
  take(validate_dynamic_military_effect_groups(common,military,exact,c.battle_catalog,
   c.actions.catalog,c.effects,c.actions.clock,c.actions.budget));
  const DynamicBattleEffectGroup2* group=nullptr;
  if(ref.kind=="dynamic_effect_group")group=exact.effect_groups.find(ref.id);
  else if(ref.kind=="dynamic_effect"){
   for(const auto& row:exact.effect_groups.rows())for(const auto& effect:row.second.due_effect_refs)
    if(same_source(effect,ref)){if(group)fail("DynamicMilitaryEffectInTwoGroups",ref.id);group=&row.second;}
  }else if(ref.kind=="military_battle"){
   const auto* row=exact.dynamic_battle_facts.find(ref.id);
   if(!row||row->id!=ref.id)fail("DynamicMilitaryBattleSourceRequired",ref.id);
   group=exact.effect_groups.find(row->effect_group_ref);
  }else{
   const auto* row=exact.dynamic_casualty_facts.find(ref.id);
   if(!row||row->id!=ref.id)fail("DynamicMilitaryCasualtySourceRequired",ref.id);
   group=exact.effect_groups.find(row->effect_group_ref);
  }
  if(!group)fail("DynamicMilitaryEffectGroupRequired",ref.id);
  const auto& casualty=casualty_for_group(exact,group->id);
  take(validate_dynamic_military_land_consequence(common,military,exact,c.land,
   {"military","military_casualties",casualty.id}));
  if(ref.kind=="military_battle"||ref.kind=="military_casualties"){
   const MilitaryCasualtyHistoryContext history{common,now,c.land.formations,c.land.returns};
   at=take(read_military_source_moment(military,exact,ref,2,c.actions.budget,&history));
  }else at=group->effective_at;
  if(!same_moment(at,group->effective_at,c.actions.budget))fail("DynamicMilitaryVerifiedSourceMomentMismatch",ref.id);
 }
 if(take(compare_world_moments(at,now,c.actions.budget))>0)fail("DynamicMilitaryVerifiedSourceInFuture",ref.id);
 return at;
});}
}
