#include "family_origin_v2.hpp"
#include "family_model.hpp"
namespace hanmo::resident {namespace {
Status selected(const CommonStore& common,const FamilyOriginV2Sources& sources){
 std::size_t count=0;for(const auto& module:sources.catalog.identity.module_versions)if(module.first=="actor_start_context"){++count;if(module.second!="3")return common_failure("FamilyOriginV2ProfileRequired");}
 if(count!=1||sources.catalog.identity.content_digest!=common.state().metadata.content_digest||sources.catalog.scenario_id!=common.state().metadata.scenario_id)return common_failure("FamilyOriginV2ProfileRequired");return common_success();
}
}
Status validate_family_identity_v2(const CommonStore& common,const FamilyState& family,const FamilyCatalog& catalog,const CommonCatalog& shared,const ActorRecord& actor,const FamilyOriginV2Sources& sources){
 auto valid=selected(common,sources);if(!valid.ok)return valid;
 const auto* original=sources.foundation.character_origin_records.origins.find(actor.id);
 if(!original||original->origin_kind!=CharacterOriginRecordV2::Kind::created)return validate_family_identity(common,family,catalog,actor);
 auto now=sources.clock.read_current_moment();if(!now.ok)return {false,{},now.error};auto source=read_created_character_origin_v2(common,sources.foundation,sources.catalog,catalog,shared,actor.id,now.value);if(!source.ok)return {false,{},source.error};
 if(family.facts.find(actor.id))return common_failure("CreatedOriginLegacyIdentityDuplicate",actor.id);
 const auto* wallet=common.wallet(actor.wallet_id);const auto* bag=common.inventory(actor.inventory_id);const Owner owner{Owner::Kind::actor,actor.id,{},{}};
 if(!wallet||!bag||!owner_equal(wallet->owner,owner)||!owner_equal(bag->owner,owner))return common_failure("DynamicActorAssetsMismatch",actor.id);
 return common_success();
}
// 核对真实人物、创建来源与家户创始人覆盖；缺失或重复来源时拒绝。
Status validate_family_creation_sources_v2(
    const CommonStore &common, const FamilyState &family,
    const FamilyCatalog &catalog, const CommonCatalog &shared,
    const FamilyOriginV2Sources &sources) {
  auto valid = selected(common, sources);
  if (!valid.ok)
    return valid;
  if (catalog.character_creation) {
    valid = validate_character_creation_catalog(common, shared, catalog,
                                                *catalog.character_creation);
    if (!valid.ok)
      return valid;
  }
  std::size_t origins = 0, registrations = 0;
  for (const auto &row :
       sources.foundation.character_origin_records.origins.rows())
    if (row.second.origin_kind == CharacterOriginRecordV2::Kind::created) {
      ++origins;
      const auto *actor = common.state().actors.find(row.first);
      if (!actor || row.first != family.founder_actor_id)
        return common_failure("CreatedFounderSourceMismatch", row.first);
      valid = validate_family_identity_v2(common, family, catalog, shared,
                                          *actor, sources);
      if (!valid.ok)
        return valid;
    }
  for (const auto &row :
       sources.foundation.character_origin_records.registration_sources.rows())
    if (row.second.registration_kind == RegistrationSourceV2::Kind::created) {
      ++registrations;
      const auto *origin =
          sources.foundation.character_origin_records.origins.find(
              row.second.actor_id);
      const auto *source =
          origin ? std::get_if<CreatedCharacterSource>(&origin->source)
                 : nullptr;
      if (!source || row.first != row.second.registration_result_id ||
          source->creation_result_ref != row.first)
        return common_failure("CreatedRegistrationCoverageMismatch", row.first);
    }
  const std::size_t required =
      catalog.founder_selection == FamilyFounderSelection::created_character
          ? 1
          : 0;
  if (bool(catalog.character_creation) != (required == 1) ||
      origins != required || registrations != required)
    return common_failure("CreatedFounderSourceCoverageMismatch");
  for (const auto &row : family.facts.rows())
    if (std::holds_alternative<CharacterCreationFact>(row.second))
      return common_failure("CreatedOriginLegacySourceDuplicate", row.first);
  for (const auto &row : common.state().facts.rows())
    if (row.second.domain == "family") {
      if (row.second.kind == "family_character_creation")
        return common_failure("CreatedOriginLegacySourceDuplicate", row.first);
      if (row.second.kind == "actor_creation" &&
          !family.facts.find(row.first)) {
        const auto *origin =
            sources.foundation.character_origin_records.origins.find(row.first);
        if (!origin ||
            origin->origin_kind != CharacterOriginRecordV2::Kind::created ||
            origin->origin_fact_ref != std::optional<StableId>{row.first})
          return common_failure("MissingActorCreationSource", row.first);
      }
    }
  return common_success();
}
// 按已选创建模式核对真实创始人记录；身份或配置不符时拒绝。
Status validate_family_founder_v2(const CommonStore &common,
                                  const FamilyState &family,
                                  const FamilyCatalog &catalog,
                                  const CommonCatalog &shared,
                                  const FamilyOriginV2Sources &sources) {
  auto valid = selected(common, sources);
  if (!valid.ok)
    return valid;
  if (catalog.founder_selection != FamilyFounderSelection::created_character)
    return validate_family_founder(common.state(), family, catalog);
  if (!catalog.character_creation || !catalog.allowed_founder_actor_ids.empty())
    return common_failure("InvalidFamilyFounder");
  const auto *actor = common.state().actors.find(family.founder_actor_id);
  if (!actor)
    return common_failure("InvalidFamilyFounder");
  return validate_family_identity_v2(common, family, catalog, shared, *actor,
                                     sources);
}
}
