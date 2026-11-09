#include "character_origin_validation.hpp"
namespace hanmo::resident {
Status validate_complete_character_origin_sources(const CommonStore& common,const AuthoredActorFoundationState& state,const ActorFoundationCatalog& catalog,const FamilyCatalog* family,const CommonCatalog* shared,const WorldMoment& now,const BornCharacterSourceRead* born){
 auto valid=validate_authored_start_context_sources(common.state(),state,catalog);if(!valid.ok)return valid;
 const auto& origins=state.character_origin_records;const auto& seeds=state.actor_start_context.initial_condition_seeds;
 if(origins.origins.size()!=common.state().actors.size()||seeds.size()!=origins.origins.size())return common_failure("CharacterOriginInitialSourceCoverageMismatch",{},ErrorKind::internal);
 for(const auto& actor:common.state().actors.rows()){
  auto original=read_character_initial_values_v2(common,state,catalog,family,shared,actor.first,now,born);if(!original.ok)return {false,{},original.error};
  if(!original.value.origin||original.value.origin->actor_id!=actor.first||!original.value.condition||original.value.condition->actor_id!=actor.first||original.value.condition->seed_key!=original.value.origin->initial_condition_seed_ref.seed_key)return common_failure("CharacterOriginInitialSourceIdentityMismatch",actor.first,ErrorKind::internal);
  for(const auto& other:origins.origins.rows())if(other.first!=actor.first&&other.second.initial_condition_seed_ref.seed_key==original.value.origin->initial_condition_seed_ref.seed_key)return common_failure("CharacterOriginSeedReused",actor.first,ErrorKind::internal);
 }
 // Registration/allocation records are original producers, never spare rows
 // accepted merely because their refs have plausible shapes. Every retained row
 // must serve exactly its own actor's original full source branch.
 for(const auto& row:origins.registration_sources.rows()){
  const auto* origin=origins.origins.find(row.second.actor_id);if(row.first!=row.second.registration_result_id||!origin)return common_failure("OrphanCharacterRegistrationSource",row.first,ErrorKind::internal);
  if(row.second.registration_kind==RegistrationSourceV2::Kind::created){const auto* source=std::get_if<CreatedCharacterSource>(&origin->source);if(origin->origin_kind!=CharacterOriginRecordV2::Kind::created||!source||source->creation_result_ref!=row.first)return common_failure("CharacterRegistrationOriginMismatch",row.first,ErrorKind::internal);}
  else return common_failure("NamedPopulationRegistrationSourceReadRequired",row.first,ErrorKind::unsupported_capability);
 }
 for(const auto& row:origins.allocation_sources.rows()){
  const auto* origin=origins.origins.find(row.second.actor_id);if(row.first!=row.second.allocation_result_id||!origin)return common_failure("OrphanInitialCapabilityAllocation",row.first,ErrorKind::internal);
  if(row.second.allocation_kind==InitialCapabilityAllocation::Kind::birth){const auto* basis=std::get_if<BirthInitialCapabilityBasis>(&origin->initial_capability_basis);if(origin->origin_kind!=CharacterOriginRecordV2::Kind::born||!basis||basis->allocation_source_ref!=row.first)return common_failure("InitialCapabilityAllocationOriginMismatch",row.first,ErrorKind::internal);}
  else return common_failure("NamedPopulationAllocationSourceReadRequired",row.first,ErrorKind::unsupported_capability);
 }
 return common_success();
}
}
