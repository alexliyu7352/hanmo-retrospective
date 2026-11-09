// 新实现按原05公式显式选择；整数范围先验，任何缺资格均不退回旧平面攻击值。
#include "military_commanded_damage.hpp"
#include "resident/model/common_model.hpp"
#include "military_combat_conditions.hpp"
#include <set>
namespace hanmo::resident {
Status validate_military_commanded_damage_rule(const MilitaryCommandedDamageRule& r){return r.profile=="entry_commanded_damage_05_v1"?common_success():common_failure("MilitaryCommandedDamageProfileMismatch");}
Outcome<StableId> military_commanded_skill(const StableId& category){
 if(category=="infantry")return {true,"infantry",{}};
 if(category=="archer")return {true,"archery",{}};
 if(category=="cavalry")return {true,"cavalry",{}};
 return {false,{},common_failure("MilitaryCommandedTroopUnsupported",category).error};
}
Outcome<Integer> military_commanded_strength(Integer leadership,Integer level,Integer training){
 if(leadership<1||leadership>110||level<0||level>4||training<0||training>100)return {false,{},common_failure("MilitaryCommandedQualificationOutOfRange").error};
 return {true,35+leadership/2+5*level+training/5,{}};
}
Outcome<MilitaryCommandedDamageQuote> military_commanded_damage_base(Integer attacking,Integer defending,Integer al,Integer as,Integer at,Integer dl,Integer ds,Integer dt){
 if(attacking<1||attacking>500||defending<1||defending>500)return {false,{},common_failure("MilitaryCommandedRosterOutOfRange").error};
 auto a=military_commanded_strength(al,as,at),d=military_commanded_strength(dl,ds,dt);
 if(!a.ok)return {false,{},a.error};if(!d.ok)return {false,{},d.error};
 return {true,{a.value,d.value,std::max<Integer>(1,attacking*a.value/(6*(100+d.value)))},{}};
}
namespace { Outcome<MilitaryCommandedDamageQuote> guarded_original_base(Integer attacking,Outcome<MilitaryCommandedDamageQuote> quote){if(!quote.ok)return quote;quote.value.defense_strength=quote.value.defense_strength*125/100;quote.value.base_casualties=std::max<Integer>(1,attacking*quote.value.attack_strength/(6*(100+quote.value.defense_strength)));return quote;} }
Outcome<MilitaryCommandedDamageQuote> military_commanded_guarded_damage_base(Integer attacking,Integer defending,Integer al,Integer as,Integer at,Integer dl,Integer ds,Integer dt){return guarded_original_base(attacking,military_commanded_damage_base(attacking,defending,al,as,at,dl,ds,dt));}
namespace {
bool fields(FieldOrder actual,FieldOrder expected){if(actual.empty())return true;std::sort(actual.begin(),actual.end());std::sort(expected.begin(),expected.end());return actual==expected;}
Integer skill_level(Integer xp){return xp>=1200?4:xp>=650?3:xp>=300?2:xp>=100?1:0;}
}
Status validate_military_commanded_damage_shape(const MilitaryBattleRecord& b,const MilitaryBattleCatalog& cat,const MilitaryCommandedDamageState& state){
 if(!b.rout_retreat_enabled||!b.combat_conditions||state.entry_tick<0||state.entry_tick!=b.created_tick||state.units.empty()||state.units.size()!=b.units.size()||state.units.size()>64||!fields(state.field_order,{"entry_tick","units"}))return common_failure("MilitaryCommandedShapeMismatch",b.id);
 auto condition=validate_military_combat_condition_shape(b,*b.combat_conditions);if(!condition.ok)return condition;
 std::vector<StableId> ids;for(const auto& u:b.units)ids.push_back(u.unit_id);std::sort(ids.begin(),ids.end());StableId previous;
 for(std::size_t i=0;i<state.units.size();++i){const auto& r=state.units[i];const BattleUnit* u=nullptr;for(const auto& row:b.units)if(row.unit_id==r.unit_id)u=&row;const TroopDefinition* t=nullptr;for(const auto& row:cat.troop_types)if(row.id==r.troop_type_id)t=&row;
  if(!u||!t||r.unit_id.empty()||r.unit_id!=ids[i]||(!previous.empty()&&r.unit_id<=previous)||r.commander_actor_id.empty()||r.troop_type_id.empty()||r.commander_actor_id!=u->commander_actor_id||r.troop_type_id!=u->troop_type_id)return common_failure("MilitaryCommandedUnitMismatch",r.unit_id);previous=r.unit_id;
  auto skill=military_commanded_skill(t->category);if(!skill.ok)return {false,{},skill.error};if(r.skill_id!=skill.value||r.experience<0||r.experience>1200||r.level!=skill_level(r.experience)||!military_commanded_strength(r.leadership,r.level,r.training).ok)return common_failure("MilitaryCommandedQualificationMismatch",r.unit_id);
  const MilitaryCombatConditionSnapshot* frozen=nullptr;for(const auto& row:b.combat_conditions->units)if(row.unit_id==r.unit_id)frozen=&row;if(!frozen||!frozen->training||*frozen->training!=r.training)return common_failure("MilitaryCommandedTrainingSourceRequired",r.unit_id);
  const auto kind=static_cast<int>(r.command_source.kind);if(kind<0||kind>3||r.command_source.id.empty()||(kind==0&&r.command_source.id!=r.unit_id)||(r.authored_actor_id.empty()&&r.causal_fact_ids.empty()))return common_failure("MilitaryCommandedSourceMismatch",r.unit_id);
  std::set<StableId> causes;for(const auto& id:r.causal_fact_ids)if(id.empty()||!causes.insert(id).second)return common_failure("MilitaryCommandedCausalSourceMismatch",r.unit_id);
  if(!fields(r.field_order,{"unit_id","commander_actor_id","troop_type_id","skill_id","authored_actor_id","command_source","leadership","experience","level","training","causal_fact_ids"})||!fields(r.source_field_order,{"kind","id"}))return common_failure("MilitaryCommandedShapeMismatch",r.unit_id);
 }
 return common_success();
}
Outcome<MilitaryCommandedDamageQuote> preview_military_commanded_attack_base(const MilitaryBattleRecord& b,const MilitaryBattleCatalog& cat,const StableId& attacking,const StableId& defending){
 if(!b.commanded_damage)return {false,{},common_failure("MilitaryCommandedEntryRequired",b.id).error};auto valid=validate_military_commanded_damage_shape(b,cat,*b.commanded_damage);if(!valid.ok)return {false,{},valid.error};
 const MilitaryCommandedDamageUnit *a=nullptr,*d=nullptr;const BattleUnit *au=nullptr,*du=nullptr;
 for(const auto& r:b.commanded_damage->units){if(r.unit_id==attacking)a=&r;if(r.unit_id==defending)d=&r;}for(const auto& u:b.units){if(u.unit_id==attacking)au=&u;if(u.unit_id==defending)du=&u;}
 if(!a||!d||!au||!du||attacking==defending)return {false,{},common_failure("MilitaryCommandedUnitMismatch").error};
 return military_commanded_damage_base(au->fit.size(),du->fit.size(),a->leadership,a->level,a->training,d->leadership,d->level,d->training);
}
Outcome<MilitaryCommandedDamageQuote> preview_military_commanded_guarded_attack_base(const MilitaryBattleRecord& b,const MilitaryBattleCatalog& cat,const StableId& attacking,const StableId& defending){
 if(!b.learned_tactics)return {false,{},common_failure("MilitaryTacticEntryRequired",b.id).error};auto quote=preview_military_commanded_attack_base(b,cat,attacking,defending);if(!quote.ok)return quote;const BattleUnit* attacker=nullptr;for(const auto& u:b.units)if(u.unit_id==attacking)attacker=&u;if(!attacker)return {false,{},common_failure("MilitaryCommandedUnitMismatch",attacking).error};return guarded_original_base(Integer(attacker->fit.size()),std::move(quote));
}

}
