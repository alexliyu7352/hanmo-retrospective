#include "resident/domains/military/military_model.hpp"
// 同候选真实contact才冻结人物资格；旧入战通过原事实序查历史，不接受今日XP回填。
#include "military_commanded_damage_source.hpp"
#include "military_officer_source.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
namespace hanmo::resident {
namespace {
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& r:rows)if(r.id==id)return &r;return nullptr;}
template<class T>Outcome<T> failure(const StableId& code,const StableId& id={}){return {false,{},common_failure(code,id).error};}
bool identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
bool same(const MilitaryUnitCommandSource& a,const MilitaryUnitCommandSource& b){return a.kind==b.kind&&a.id==b.id;}
Integer sequence(const StableId& id,const StableId& prefix){if(id.rfind(prefix,0)!=0)return -1;Integer n=0;const auto s=id.substr(prefix.size());auto parsed=std::from_chars(s.data(),s.data()+s.size(),n);return parsed.ec==std::errc{}&&parsed.ptr==s.data()+s.size()&&n>0&&std::to_string(n)==s?n:-1;}
Outcome<Integer> observed(const CommonStore& c,const DynamicStepInput* phase){
 if(!phase)return {true,c.tick(),{}};if(phase->from_tick<0||phase->from_tick!=c.tick()||phase->from_tick==std::numeric_limits<Integer>::max()||phase->to_tick!=phase->from_tick+1)return failure<Integer>("MilitaryCommandedPhaseMismatch");return {true,phase->to_tick,{}};
}
// 原因集合按实际Common头严格递增；数字后缀不能代替真实头序。
Status causes(const CommonStore& c,const std::vector<StableId>& ids,const StableId& boundary){
 const auto& order=c.state().facts.order();auto end=boundary.empty()?order.end():std::find(order.begin(),order.end(),boundary);
 if(!boundary.empty()&&end==order.end())return common_failure("MilitaryCommandedBoundaryMissing",boundary);
 auto last=order.end();for(const auto& id:ids){auto at=std::find(order.begin(),order.end(),id);const auto* head=c.state().facts.find(id);if(id.empty()||!head||head->id!=id||at==order.end()||at>=end||(last!=order.end()&&at<=last))return common_failure("MilitaryCommandedCausalSourceMismatch",id);last=at;}
 return common_success();
}
Status command_before(const CommonStore& c,const MilitaryUnitCommandSource& source,const StableId& boundary){
 if(source.kind==MilitaryUnitCommandSourceKind::authored)return common_success();
 const auto& order=c.state().facts.order();auto at=std::find(order.begin(),order.end(),source.id),end=std::find(order.begin(),order.end(),boundary);const auto* head=c.state().facts.find(source.id);
 const auto kind=source.kind==MilitaryUnitCommandSourceKind::army_formation?"military_action":source.kind==MilitaryUnitCommandSourceKind::officer_assignment?"military_officer_assignment":"military_field_command_assigned";
 return head&&head->id==source.id&&head->kind==kind&&head->domain=="military"&&at!=order.end()&&end!=order.end()&&at<end?common_success():common_failure("MilitaryCommandedCommandSourceOrder",source.id);
}
Status entry(const CommonStore& c,const MilitaryRoutEntryFact& e,Integer upper){
 const auto n=sequence(e.id,"military_rout_entry:");const auto* head=c.state().facts.find(e.id);
 if(n<1||n>=c.state().next_instance_id||!head||head->id!=e.id||head->kind!="military_rout_entry"||head->domain!="military"||e.tick<0||e.tick>upper||e.initial.created_tick!=e.tick||e.initial.round!=1||e.initial.phase!="planning"||!e.initial.round_receipt_ids.empty())return common_failure("MilitaryCommandedEntryRequired",e.id);
 return common_success();
}
}
Outcome<MilitaryCommandedDamageState> prepare_military_commanded_damage_entry(const CommonStore& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryBattleRecord& b,const MilitaryCommandedSkillRead& reader,const DynamicStepInput& phase){
 using R=MilitaryCommandedDamageState;auto upper=observed(c,&phase);if(!upper.ok)return {false,{},upper.error};
 if(!cat.commanded_damage_policy||!cat.combat_condition_policy||!b.rout_retreat_enabled||!b.combat_conditions||b.created_tick!=upper.value||b.round!=1||b.phase!="planning"||!b.round_receipt_ids.empty()||!b.ai_plans.empty()||b.plan_submitted||!b.pending_orders.empty())return failure<R>("MilitaryCommandedEntryRequired",b.id);
 auto rule=validate_military_commanded_damage_rule(*cat.commanded_damage_policy);if(!rule.ok)return {false,{},rule.error};
 const auto* contact=find(s.contact_facts,b.source_id);const auto* head=c.state().facts.find(b.source_id);
 if(!contact||contact->siege_id||contact->node_id||contact->tick!=b.created_tick||!head||head->id!=contact->id||head->kind!="military_contact"||head->domain!="military")return failure<R>("MilitaryCommandedContactRequired",b.source_id);
 R out;out.entry_tick=b.created_tick;std::vector<const BattleUnit*> units;for(const auto& u:b.units)units.push_back(&u);std::sort(units.begin(),units.end(),[](const auto* a,const auto* b){return a->unit_id<b->unit_id;});
 for(const auto* u:units){
  const auto* troop=find(cat.battle_catalog.troop_types,u->troop_type_id);if(!troop)return failure<R>("MilitaryCommandedTroopUnsupported",u->unit_id);auto skill=military_commanded_skill(troop->category);if(!skill.ok)return {false,{},skill.error};
  auto command=query_military_unit_command_source(c,s,cat,u->unit_id,phase);if(!command.ok)return {false,{},command.error};if(command.value.commander_actor_id!=u->commander_actor_id)return failure<R>("MilitaryCommandedUnitMismatch",u->unit_id);
  auto checked=command_before(c,command.value.command_source,b.source_id);if(!checked.ok)return {false,{},checked.error};
  auto read=reader.current_commanded_skill(u->commander_actor_id,skill.value);if(!read.ok)return {false,{},read.error};const auto& q=read.value;
  if(!identity(q.source_identity,c.state().identity)||q.actor_id!=u->commander_actor_id||q.skill_id!=skill.value||q.observed_tick!=upper.value||!q.before_fact_id.empty())return failure<R>("MilitaryCommandedSkillSourceMismatch",u->unit_id);
  checked=causes(c,q.causal_fact_ids,b.source_id);if(!checked.ok)return {false,{},checked.error};
  const MilitaryCombatConditionSnapshot* condition=nullptr;for(const auto& row:b.combat_conditions->units)if(row.unit_id==u->unit_id)condition=&row;if(!condition||!condition->training)return failure<R>("MilitaryCommandedTrainingSourceRequired",u->unit_id);
  MilitaryCommandedDamageUnit r;r.unit_id=u->unit_id;r.commander_actor_id=u->commander_actor_id;r.troop_type_id=u->troop_type_id;r.skill_id=skill.value;r.authored_actor_id=q.authored_actor_id;r.command_source=command.value.command_source;r.leadership=q.leadership;r.experience=q.experience;r.level=q.level;r.training=*condition->training;r.causal_fact_ids=q.causal_fact_ids;out.units.push_back(std::move(r));
 }
 auto valid=validate_military_commanded_damage_shape(b,cat.battle_catalog,out);return valid.ok?Outcome<R>{true,std::move(out),{}}:Outcome<R>{false,{},valid.error};
}
Status validate_military_commanded_damage_bindings(const CommonStore& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryRoutEntryFact& e,const MilitaryCommandedDamageState& state,const DynamicStepInput* phase){
 auto upper=observed(c,phase);if(!upper.ok)return {false,{},upper.error};auto valid=entry(c,e,upper.value);if(!valid.ok)return valid;
 if(!cat.commanded_damage_policy||!cat.combat_condition_policy)return common_failure("MilitaryCommandedDamagePrerequisiteRequired",e.id);
 valid=validate_military_commanded_damage_shape(e.initial,cat.battle_catalog,state);if(!valid.ok)return valid;
 for(const auto& r:state.units){auto source=phase?query_military_unit_command_source_before(c,s,cat,r.unit_id,e.id,*phase):query_military_unit_command_source_before(c,s,cat,r.unit_id,e.id);if(!source.ok)return {false,{},source.error};if(source.value.commander_actor_id!=r.commander_actor_id||!same(source.value.command_source,r.command_source))return common_failure("MilitaryCommandedCommandSourceMismatch",r.unit_id);valid=command_before(c,r.command_source,e.id);if(!valid.ok)return valid;valid=causes(c,r.causal_fact_ids,e.id);if(!valid.ok)return valid;}
 return common_success();
}
Status validate_military_commanded_damage_actor_sources(const CommonStore& c,const MilitaryRoutEntryFact& e,const MilitaryCommandedDamageState& state,const MilitaryCommandedSkillRead& reader){
 auto valid=entry(c,e,c.tick());if(!valid.ok)return valid;if(state.entry_tick!=e.tick)return common_failure("MilitaryCommandedEntryRequired",e.id);
 for(const auto& r:state.units){auto read=reader.commanded_skill_before_entry(r.commander_actor_id,r.skill_id,e.id);if(!read.ok)return {false,{},read.error};const auto& q=read.value;
  if(!identity(q.source_identity,c.state().identity)||q.actor_id!=r.commander_actor_id||q.skill_id!=r.skill_id||q.before_fact_id!=e.id||q.observed_tick!=e.tick||q.leadership!=r.leadership||q.experience!=r.experience||q.level!=r.level||q.authored_actor_id!=r.authored_actor_id||q.causal_fact_ids!=r.causal_fact_ids)return common_failure("MilitaryCommandedSkillHistoryMismatch",r.unit_id);
  valid=causes(c,q.causal_fact_ids,e.id);if(!valid.ok)return valid;
 }
 return common_success();
}
}
