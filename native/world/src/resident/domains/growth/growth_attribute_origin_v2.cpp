#include "growth_attribute_origin_v2.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
namespace hanmo::resident {
Outcome<GrowthAttributeOriginReadV2> read_growth_attribute_origin_v2(const CharacterConditionSources&s,const StableId&actor){
 auto time=compare_world_moments(s.actual_now,s.actual_now);if(!time.ok)return {false,{},time.error};
 auto initial=read_character_initial_values_v2(s.common,s.state,s.catalog,s.family_catalog,s.shared,actor,s.actual_now,s.born_sources);if(!initial.ok)return {false,{},initial.error};
 return {true,{initial.value.origin,initial.value.base_attributes},{}};
}
Outcome<GrowthAttributeOriginReadV2> read_growth_attribute_origin_before_source_v2(
 const CharacterConditionSources&s,const StableId&actor,const SourceRef&boundary,const WorldMoment&at){
 auto initial=read_character_initial_values_before_fact_v2(s.common,s.state,s.catalog,s.family_catalog,s.shared,actor,boundary,at,s.actual_now,s.born_sources);if(!initial.ok)return {false,{},initial.error};
 if(!initial.value.present)return {false,{},common_failure("GrowthAttributeActorNotBeforeSource",actor,ErrorKind::internal).error};
 return {true,{initial.value.initial.origin,initial.value.initial.base_attributes},{}};
}
}
