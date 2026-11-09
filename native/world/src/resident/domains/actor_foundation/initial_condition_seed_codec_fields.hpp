// Explicit actor_start_context/3 source union. JSON/binary/shape share this table.
#pragma once
#include "character_origin_codec_detail.hpp"
namespace hanmo::resident::initial_condition_seed_codec {
template<class C>Status source(C& c,InitialConditionSeedSource& value){
 Integer tag=static_cast<Integer>(value.index());c.field("tag",tag);
 if(tag<0||tag>3)return c.reject("InvalidInitialConditionSeedSource");
 if(c.reading())switch(tag){case 0:value=ScenarioOriginRef{};break;case 1:value=CreatedConditionSeedSource{};break;case 2:value=BirthConditionSeedSource{};break;case 3:value=NamedConditionSeedSource{};break;}
 if(tag==0)return character_origin_codec::scenario(c,std::get<ScenarioOriginRef>(value));
 if(tag==1){auto& created=std::get<CreatedConditionSeedSource>(value);c.field("creation_result_ref",created.creation_result_ref);return created.creation_result_ref.empty()?c.reject("InitialCreationSourceRequired"):c.status();}
 if(tag==2){auto& born=std::get<BirthConditionSeedSource>(value);c.object("birth_record_ref",[&](C& v){return character_origin_codec::birth(v,born.birth_record_ref);});c.field("allocation_source_ref",born.allocation_source_ref);return born.allocation_source_ref.empty()?c.reject("InitialBirthAllocationRequired"):c.status();}
 auto& named=std::get<NamedConditionSeedSource>(value);c.object("population_origin_ref",[&](C& v){return character_origin_codec::population(v,named.population_origin_ref);});c.field("registration_result_ref",named.registration_result_ref);c.field("allocation_source_ref",named.allocation_source_ref);return named.registration_result_ref.empty()||named.allocation_source_ref.empty()?c.reject("InitialNamingSourceRequired"):c.status();
}
template<class C>Status seed3(C& c,InitialConditionSeed& value){
 c.field("seed_key",value.seed_key);c.field("actor_id",value.actor_id);
 c.object("source",[&](C& v){return source(v,value.source);});
 c.field("hp",value.hp);c.field("fatigue",value.fatigue);c.field("injury",value.injury);
 c.sequence("episodes",value.episodes,[](C& v,auto&){return v.reject("UnsupportedInitialConditionEpisode");});
 c.object("activity_limit_profile",[&](C& v){return character_origin_codec::rule(v,value.activity_limit_profile);});
 return value.seed_key.empty()||value.actor_id.empty()||value.hp<0||value.hp>100||value.fatigue<0||value.fatigue>100||value.injury<0||value.injury>3?c.reject("InvalidInitialConditionSeed"):c.status();
}
}
