#include "character_presence_v2.hpp"
#include "character_availability_provenance.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
namespace hanmo::resident {namespace {
struct DeathBoundaryRead {const FamilyDeathRecord* record=nullptr;bool before=false;};
Outcome<DeathBoundaryRead> death_at_boundary(const CommonStore& common,const FamilyDeathState& deaths,const FamilyLifeCauseSources4& life,const StableId& actor,const SourceRef& boundary,const WorldMoment& at,const WorldMoment& now,CharacterAvailabilityProvenanceSink* provenance=nullptr,MilitaryCasualtyHistoryOrder ordering=MilitaryCasualtyHistoryOrder::legacy_serial){
 const FamilyDeathRecord* found=nullptr;for(const auto& row:deaths.deaths.rows())if(row.second.actor_id==actor){if(found)return {false,{},common_failure("ConflictingDeathSource",actor,ErrorKind::internal).error};found=&row.second;}
 if(!found){const auto* current=common.actor(actor);if(!current||!current->alive)return {false,{},common_failure("CharacterPresenceActualDeathSourceRequired",actor,ErrorKind::unsupported_capability).error};return {true,{},{}};}
 if(!found->precise)return {false,{},common_failure("CharacterPresenceExactDeathRequired",actor,ErrorKind::unsupported_capability).error};
 auto valid=validate_family_death4(common,*found,life,now,ordering);if(!valid.ok)return {false,{},valid.error};auto cause=read_life_cause4(common,life,found->precise->cause,&now,ordering);if(!cause.ok)return {false,{},cause.error};
 const auto actual=read_actual_common_fact_position(common,cause.value.cause_ref),cut=read_actual_common_fact_position(common,boundary);
 if(!actual.ok||!cut.ok)return {false,{},common_failure("CharacterPresenceDeathOrderRequired",actor,ErrorKind::internal).error};
 auto compared=compare_world_moments(found->precise->effective_at,at);if(!compared.ok)return {false,{},compared.error};
 if((compared.value<0&&actual.value>=cut.value)||(compared.value>0&&actual.value<=cut.value))return {false,{},common_failure("CharacterPresenceDeathTimeOrderMismatch",actor,ErrorKind::internal).error};
 const bool before=compared.value<0||(compared.value==0&&actual.value<cut.value);
 if(provenance){
  auto valid=provenance->append(CharacterAvailabilityProvenanceSlot::death,{"family_life","family_death",found->precise->death_id},4,before);if(!valid.ok)return {false,{},std::move(valid.error)};
  // Age/sea providers choose their own actual source identity and schema. The
  // checked view is borrowed only synchronously, never mapped to schema one.
  const auto& ref=cause.value.cause_ref;valid=provenance->append(CharacterAvailabilityProvenanceSlot::death_cause,{ref.domain,ref.kind,ref.id},cause.value.source_schema,before);if(!valid.ok)return {false,{},std::move(valid.error)};
 }
 return {true,{found,before},{}};
}
}

Outcome<CharacterPresenceV2> read_character_presence_before_fact_v2(const CommonStore& common,const AuthoredActorFoundationState& state,const ActorFoundationCatalog& catalog,const FamilyDeathState& deaths,const FamilyLifeCauseSources4& life,const StableId& actor,const SourceRef& boundary,const WorldMoment& at,const CurrentMomentRead& clock,const FamilyCatalog* family_catalog,const CommonCatalog* shared,const BornCharacterSourceRead* born_sources,CharacterAvailabilityProvenanceSink* provenance,MilitaryCasualtyHistoryOrder ordering){
 auto now=clock.read_current_moment();if(!now.ok)return {false,{},now.error};
 auto initial=read_character_initial_values_before_fact_v2(common,state,catalog,family_catalog,shared,actor,boundary,at,now.value,born_sources,provenance);if(!initial.ok)return {false,{},initial.error};
 const auto&source=initial.value.initial;const bool present=initial.value.present;
 CharacterPresenceV2 result{actor,present,present&&source.alive,source.origin->actor_id,{},at};if(!present)return {true,std::move(result),{}};
 auto death=death_at_boundary(common,deaths,life,actor,boundary,at,now.value,provenance,ordering);if(!death.ok)return {false,{},death.error};if(death.value.before){result.alive=false;result.death_ref=DeathRef{"family_life","family_death",death.value.record->precise->death_id};}
 return {true,std::move(result),{}};
}
Outcome<CharacterAppliedPresenceV2> read_character_applied_presence_at_prefix_v2(
 const CommonStore&common,const AuthoredActorFoundationState&state,const ActorFoundationCatalog&catalog,
 const FamilyDeathState&deaths,const FamilyLifeCauseSources4&life,const StableId&actor,
 const CommonActualFactPrefix&nested,const CommonActualFactPrefix&outer,
 const WorldMoment&outer_at,const CurrentMomentRead&clock,const FamilyCatalog*family,
 const CommonCatalog*shared,const BornCharacterSourceRead*born){
 auto invalid=[&](const char*code,ErrorKind kind=ErrorKind::internal)->Outcome<CharacterAppliedPresenceV2>{return {false,{},common_failure(code,actor,kind).error};};
 auto now=clock.read_current_moment();if(!now.ok)return {false,{},now.error};
 auto initial=read_character_initial_values_at_applied_prefix_v2(common,state,catalog,family,shared,actor,nested,outer,outer_at,now.value,born);if(!initial.ok)return {false,{},initial.error};
 auto nested_cut=read_actual_common_fact_prefix(common,nested);if(!nested_cut.ok)return {false,{},nested_cut.error};
 auto outer_cut=read_actual_common_fact_prefix(common,outer);if(!outer_cut.ok)return {false,{},outer_cut.error};
 const FamilyDeathRecord*found=nullptr;
 for(const auto&row:deaths.deaths.rows())if(row.second.actor_id==actor){
  if(found)return invalid("ConflictingDeathSource");
  if(!row.second.precise)return invalid("CharacterPresenceExactDeathRequired",ErrorKind::unsupported_capability);
  if(row.first!=row.second.precise->death_id)return invalid("FamilyDeathKeyMismatch");
  found=&row.second;
 }
 CharacterAppliedPresenceV2 result{initial.value.present,initial.value.present&&initial.value.initial.alive,initial.value.initial.origin,nullptr};
 if(!found){
  const auto*current=common.actor(actor);
  if(!current||!current->alive)return invalid("CharacterPresenceActualDeathSourceRequired",ErrorKind::unsupported_capability);
  return {true,result,{}};
 }
 constexpr auto ordering=MilitaryCasualtyHistoryOrder::actual_common_prefix;
 auto valid=validate_family_death4(common,*found,life,now.value,ordering);if(!valid.ok)return {false,{},valid.error};
 auto cause=read_life_cause4(common,life,found->precise->cause,&now.value,ordering);if(!cause.ok)return {false,{},cause.error};
 const auto*application=common.state().facts.find(found->precise->death_id);
 if(!application||application->id!=found->precise->death_id||application->domain!="family_life"||application->kind!="family_death")return invalid("CharacterAppliedPresenceDeathApplicationRequired");
 auto cause_position=read_actual_common_fact_position(common,cause.value.cause_ref);if(!cause_position.ok)return {false,{},cause_position.error};
 auto application_position=read_actual_common_fact_position(common,*application);if(!application_position.ok)return {false,{},application_position.error};
 if(cause_position.value>=application_position.value)return invalid("FamilyDeathBeforeCause");
 // Source proof may read a retained future death, but each genuine occurrence
 // is checked against the outer endpoint. It is not applied to nested early.
 auto boundary=[&](std::size_t position,const WorldMoment&occurred)->Status{
  auto current=compare_world_moments(occurred,now.value);if(!current.ok)return {false,{},current.error};
  if(current.value>0)return common_failure("CharacterAppliedPresenceFutureDeath",actor,ErrorKind::internal);
  auto compared=compare_world_moments(occurred,outer_at);if(!compared.ok)return {false,{},compared.error};
  if((position<outer_cut.value&&compared.value>0)||(position>=outer_cut.value&&compared.value<0))return common_failure("CharacterAppliedPresenceDeathTimeOrderMismatch",actor,ErrorKind::internal);
  return common_success();
 };
 valid=boundary(cause_position.value,found->precise->effective_at);if(!valid.ok)return {false,{},valid.error};
 valid=boundary(application_position.value,found->precise->recorded_at);if(!valid.ok)return {false,{},valid.error};
 if(application_position.value<nested_cut.value){
  if(!result.present)return invalid("CharacterAppliedPresenceDeathBeforeOrigin");
  result.alive=false;result.applied_death=found;
 }
 return {true,result,{}};
}
Outcome<const FamilyDeathRecord*> read_character_death_after_fact_v2(const CommonStore& common,const AuthoredActorFoundationState& state,const ActorFoundationCatalog& catalog,const FamilyDeathState& deaths,const FamilyLifeCauseSources4& life,const StableId& actor,const SourceRef& entry,const WorldMoment& entered,const CurrentMomentRead& clock,const FamilyCatalog* family_catalog,const CommonCatalog* shared,const BornCharacterSourceRead* born_sources,MilitaryCasualtyHistoryOrder ordering){
 auto presence=read_character_presence_before_fact_v2(common,state,catalog,deaths,life,actor,entry,entered,clock,family_catalog,shared,born_sources,nullptr,ordering);if(!presence.ok)return {false,nullptr,presence.error};
 if(!presence.value.present||!presence.value.alive)return {false,nullptr,common_failure("CharacterDeathHistoryLivingEntryRequired",actor).error};
 auto now=clock.read_current_moment();if(!now.ok)return {false,nullptr,now.error};auto death=death_at_boundary(common,deaths,life,actor,entry,entered,now.value,nullptr,ordering);if(!death.ok)return {false,nullptr,death.error};if(!death.value.record)return {true,nullptr,{}};
 auto cause=read_life_cause4(common,life,death.value.record->precise->cause,&now.value,ordering);if(!cause.ok)return {false,nullptr,cause.error};
 if(cause.value.cause_ref.id==entry.id||death.value.before)return {false,nullptr,common_failure("CharacterDeathHistoryEntryCauseMismatch",actor,ErrorKind::internal).error};
 return {true,death.value.record,{}};
}
}
