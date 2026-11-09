// 无第二经验账：严格映射S4永久Study与人物创建来源；军权和入口载荷由S2先核。
#include "military_commanded_damage_integration.hpp"
#include "world_bundle.hpp"
#include "resident/domains/growth/growth_military_entry_history.hpp"
#include "resident/domains/military/military_commanded_damage_source.hpp"
namespace hanmo::resident {
namespace {
GrowthMilitaryEntryContext context(const WorldBundle& w){return {*w.catalog->common,*w.catalog->family,w.family,w.family_birth_enabled()?&w.family_birth:nullptr};}
Outcome<MilitaryCommandedSkillObservation> mapped(Outcome<GrowthMilitaryEntryQualification> source){
 if(!source.ok)return {false,{},source.error};auto& s=source.value;MilitaryCommandedSkillObservation out;
 out.source_identity=s.source_identity;out.actor_id=std::move(s.actor_id);out.skill_id=std::move(s.skill_id);out.before_fact_id=std::move(s.before_fact_id);out.authored_actor_id=std::move(s.authored_actor_id);out.observed_tick=s.observed_tick;out.leadership=s.leadership;out.experience=s.experience;out.level=s.level;out.causal_fact_ids=std::move(s.causal_fact_ids);return {true,std::move(out),{}};
}
}
Outcome<MilitaryCommandedSkillObservation> MilitaryCommandedWorldRead::current_commanded_skill(const StableId& actor,const StableId& skill)const{
 if(!world_.military_commanded_damage_enabled()||!phase_||!world_.catalog->family||!world_.catalog->growth)return {false,{},common_failure("MilitaryCommandedCreationPhaseRequired",actor).error};
 return mapped(read_growth_military_entry_qualification(world_.common,context(world_),world_.growth,actor,skill,&*phase_));
}
Outcome<MilitaryCommandedSkillObservation> MilitaryCommandedWorldRead::commanded_skill_before_entry(const StableId& actor,const StableId& skill,const StableId& id)const{
 if(!world_.military_commanded_damage_enabled()||!world_.catalog->family||!world_.catalog->growth)return {false,{},common_failure("MilitaryCommandedDamageProfileRequired",id).error};
 const MilitaryRoutEntryFact* entry=nullptr;for(const auto& row:world_.military.rout.entries)if(row.id==id){if(entry)return {false,{},common_failure("MilitaryCommandedEntryAmbiguous",id).error};entry=&row;}
 if(!entry||!entry->initial.commanded_damage)return {false,{},common_failure("MilitaryCommandedEntryRequired",id).error};
 const BattleUnit* unit=nullptr;for(const auto& row:entry->initial.units)if(row.commander_actor_id==actor){if(unit)return {false,{},common_failure("MilitaryCommandedUnitMismatch",actor).error};unit=&row;}
 bool bound=false;if(unit)for(const auto& row:entry->initial.commanded_damage->units)if(row.unit_id==unit->unit_id&&row.commander_actor_id==actor&&row.skill_id==skill)bound=true;
 if(!bound)return {false,{},common_failure("MilitaryCommandedUnitMismatch",actor).error};
 return mapped(read_growth_military_entry_qualification_before_fact(world_.common,context(world_),world_.growth,actor,skill,entry->tick,id,phase_?&*phase_:nullptr));
}
Status validate_military_commanded_damage_world(const WorldBundle& w){
 if(!w.military_commanded_damage_enabled())return common_failure("MilitaryCommandedDamageProfileRequired");MilitaryCommandedWorldRead read(w);
 for(const auto& e:w.military.rout.entries){if(!e.initial.commanded_damage)return common_failure("MilitaryCommandedEntryRequired",e.id);auto checked=validate_military_commanded_damage_actor_sources(w.common,e,*e.initial.commanded_damage,read);if(!checked.ok)return checked;}
 return common_success();
}
}
