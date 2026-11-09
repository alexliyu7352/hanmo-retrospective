// S4永久Study链是唯一兵法权源；军事只核原单位/统帅/entry，不复制XP账或嵌套World总验。
#include "military_command_resources_integration.hpp"
#include "world_bundle.hpp"
#include "resident/domains/growth/growth_tactics_history.hpp"
#include "resident/domains/military/military_command_resources_source.hpp"
namespace hanmo::resident {
namespace {
GrowthTacticsHistoryContext context(const WorldBundle& w){return {*w.catalog->common,*w.catalog->family,w.family,w.family_birth_enabled()?&w.family_birth:nullptr};}
Outcome<MilitaryTacticsEntryObservation> mapped(Outcome<GrowthTacticsSkillObservation> read){if(!read.ok)return {false,{},read.error};auto& s=read.value;MilitaryTacticsEntryObservation out;out.source_identity=s.source_identity;out.actor_id=std::move(s.actor_id);out.skill_id=std::move(s.skill_id);out.before_fact_id=std::move(s.before_fact_id);out.authored_actor_id=std::move(s.authored_actor_id);out.observed_tick=s.observed_tick;out.experience=s.experience;out.level=s.level;out.causal_fact_ids=std::move(s.causal_fact_ids);return {true,std::move(out),{}};}
}
Outcome<MilitaryTacticsEntryObservation> MilitaryTacticsWorldRead::current_tactics_entry(const StableId& actor)const{if(!world_.military_command_resources_enabled()||!phase_||!world_.catalog->common||!world_.catalog->family||!world_.catalog->growth)return {false,{},common_failure("MilitaryCommandResourcesPhaseRequired",actor).error};return mapped(read_growth_tactics_skill(world_.common,context(world_),world_.growth,actor,&*phase_));}
Outcome<MilitaryTacticsEntryObservation> MilitaryTacticsWorldRead::tactics_before_entry(const StableId& actor,const StableId& id)const{
 if(!world_.military_command_resources_enabled()||!world_.catalog->common||!world_.catalog->family||!world_.catalog->growth)return {false,{},common_failure("MilitaryCommandResourcesProfileRequired",id).error};const MilitaryRoutEntryFact* entry=nullptr;for(const auto& row:world_.military.rout.entries)if(row.id==id){if(entry)return {false,{},common_failure("MilitaryCommandResourcesEntryAmbiguous",id).error};entry=&row;}
 if(!entry||!entry->initial.command_resources)return {false,{},common_failure("MilitaryCommandResourcesEntryRequired",id).error};const BattleUnit* unit=nullptr;for(const auto& row:entry->initial.units)if(row.commander_actor_id==actor){if(unit)return {false,{},common_failure("MilitaryCommandResourcesUnitMismatch",actor).error};unit=&row;}
 bool bound=false;if(unit)for(const auto& row:entry->initial.command_resources->units)if(row.unit_id==unit->unit_id&&row.commander_actor_id==actor)bound=true;if(!bound)return {false,{},common_failure("MilitaryCommandResourcesUnitMismatch",actor).error};return mapped(read_growth_tactics_skill_before_entry(world_.common,context(world_),world_.growth,actor,entry->tick,id,phase_?&*phase_:nullptr));
}
Status validate_military_command_resources_world(const WorldBundle& w){if(!w.military_command_resources_enabled())return common_failure("MilitaryCommandResourcesProfileRequired");MilitaryTacticsWorldRead read(w);for(const auto& entry:w.military.rout.entries){if(!entry.initial.command_resources)return common_failure("MilitaryCommandResourcesEntryRequired",entry.id);auto checked=validate_military_command_resources_actor_sources(w.common,entry,*entry.initial.command_resources,read);if(!checked.ok)return checked;}return common_success();}
}
