#include "resident/kernel/catalog_section_observation.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "character_creation_source.hpp"
#include "resident/domains/actor_foundation/created_origin.hpp"
#include "family_model.hpp"
#include "resident/domains/growth/growth_skill_api.hpp"
#include "resident/domains/interaction/interaction_model.hpp"
#include "resident/domains/social/social_model.hpp"
#include <charconv>
#include <set>

namespace hanmo::resident {
namespace {
bool instance_id(const StableId& id,const StableId& prefix,Integer next){
    if(id.rfind(prefix+":",0)!=0)return false;
    const auto suffix=id.substr(prefix.size()+1);Integer n=0;const auto parsed=std::from_chars(suffix.data(),suffix.data()+suffix.size(),n);
    return parsed.ec==std::errc{}&&parsed.ptr==suffix.data()+suffix.size()&&n>0&&n<next&&std::to_string(n)==suffix;
}
bool token(const StableId& value){
    if(value.empty()||value.size()>64||value.front()<'a'||value.front()>'z')return false;
    return std::all_of(value.begin(),value.end(),[](char c){return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.';});
}
const std::vector<Integer ActorAttributes::*> attribute_members={&ActorAttributes::leadership,&ActorAttributes::martial,&ActorAttributes::intellect,&ActorAttributes::administration,&ActorAttributes::charisma};
const std::vector<StableId AppearanceFeatures::*> feature_members={&AppearanceFeatures::face_shape,&AppearanceFeatures::eyes,&AppearanceFeatures::brows,&AppearanceFeatures::nose,&AppearanceFeatures::mouth,&AppearanceFeatures::hair,&AppearanceFeatures::beard,&AppearanceFeatures::skin_palette,&AppearanceFeatures::build};
bool appearance_equal(const AppearanceProfile& actual,const CharacterCreationAppearance& choice){
    if(actual.format_version!=1||actual.generator_version!="appearance-v1"||actual.generation_seed!="not-sampled"||actual.source!="custom"||actual.attire_style!=choice.attire_style||!actual.base_features.present||!actual.parent_sources.empty())return false;
    for(const auto member:feature_members)if(actual.base_features.*member!=choice.features.*member)return false;
    return true;
}
}
bool same_character_creation_draft(const CharacterCreationDraft& a,const CharacterCreationDraft& b,bool extended){
    if(a.mode!=b.mode||a.display_name!=b.display_name||a.age_years!=b.age_years||a.skills.extended.has_value()!=extended||b.skills.extended.has_value()!=extended)return false;
    for(const auto member:attribute_members)if(a.attributes.*member!=b.attributes.*member)return false;
    for(const auto& id:growth_skill_ids(extended))if(growth_skill_xp(a.skills,id)!=growth_skill_xp(b.skills,id))return false;
    return true;
}
Status validate_character_creation_catalog(const CommonStore& common,const CommonCatalog& shared,const FamilyCatalog& family,const CharacterCreationCatalog& catalog){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 27, "CharacterCreationCatalog", "validate_character_creation_catalog", &catalog);

    const auto* prototype=family.prototypes.find(catalog.prototype_actor_id);
    if(!prototype||prototype->historical||common.actor(catalog.prototype_actor_id)||!shared.places.find(catalog.starting_place_id)||catalog.initial_money<0||catalog.initial_money>2147483647||catalog.initial_goods.size()>20)return common_failure("InvalidCharacterCreationCatalog");
    if(catalog.allowed_modes.empty()||catalog.allowed_modes.size()>2||std::count(catalog.allowed_modes.begin(),catalog.allowed_modes.end(),CharacterCreationMode::standard)!=1)return common_failure("InvalidCharacterCreationModes");
    std::set<CharacterCreationMode> modes;
    for(const auto mode:catalog.allowed_modes)if(character_creation_mode_name(mode).empty()||!modes.insert(mode).second)return common_failure("InvalidCharacterCreationModes");
    Integer weight=0;std::set<StableId> items;
    for(const auto& grant:catalog.initial_goods){const auto* item=shared.items.find(grant.item_id);if(!item||grant.quantity<1||grant.quantity>100||item->weight<1||item->weight>10000||!items.insert(grant.item_id).second)return common_failure("InvalidCharacterCreationGoods",grant.item_id);weight+=item->weight*grant.quantity;}
    if(weight>20)return common_failure("CharacterCreationGoodsTooHeavy");
    if(catalog.appearance_choices.empty()||catalog.appearance_choices.size()>4096)return common_failure("CharacterCreationAppearanceRequired");
    std::set<StableId> shapes;
    for(const auto& row:catalog.appearance_choices.rows()){
        if(row.first!=row.second.id||!token(row.first)||!token(row.second.attire_style)||!row.second.features.present)return common_failure("InvalidCharacterCreationAppearance",row.first);
        StableId shape=row.second.attire_style;for(const auto member:feature_members){if(!token(row.second.features.*member))return common_failure("InvalidCharacterCreationAppearance",row.first);shape+="|"+row.second.features.*member;}
        if(!shapes.insert(shape).second)return common_failure("DuplicateCharacterCreationAppearance",row.first);
    }
    return common_success();
}
static Status validate_creation_source(const CommonStore& common,const FamilyState& family,const FamilyCatalog& catalog,const CommonCatalog& shared,bool check_catalog){
    if(check_catalog&&catalog.character_creation){auto checked=validate_character_creation_catalog(common,shared,catalog,*catalog.character_creation);if(!checked.ok)return checked;}
    Integer count=0;
    for(const auto& row:family.facts.rows()){
        const auto* source=std::get_if<CharacterCreationFact>(&row.second);if(!source)continue;
        if(!catalog.character_creation)return common_failure("CharacterCreationCapabilityRequired",row.first);
        const auto& definition=*catalog.character_creation;const auto* header=common.state().facts.find(row.first);const auto* actor=common.state().actors.find(source->actor_id);
        const auto* identity_value=family.facts.find(source->actor_id);const auto* identity=identity_value?std::get_if<IdentityCreationFact>(identity_value):nullptr;
        const auto* identity_header=common.state().facts.find(source->actor_id);const auto* appearance=definition.appearance_choices.find(source->appearance_choice_id);
        if(++count!=1||!instance_id(row.first,"family",common.state().next_instance_id)||!instance_id(source->actor_id,"actor",common.state().next_instance_id)||!header||header->domain!="family"||header->kind!="family_character_creation"||source->kind!=header->kind||!actor||!identity||!identity_header||identity_header->domain!="family"||identity_header->kind!="actor_creation")return common_failure("InvalidCharacterCreationSource",row.first);
        if(source->created_tick!=0||common.tick()<0||source->scenario_id!=common.state().metadata.scenario_id||source->prototype_actor_id!=definition.prototype_actor_id||family.founder_actor_id!=source->actor_id||!appearance||std::find(definition.allowed_modes.begin(),definition.allowed_modes.end(),source->draft.mode)==definition.allowed_modes.end())return common_failure("CharacterCreationSourceMismatch",row.first);
        auto valid=validate_character_creation_draft(source->draft,shared.extended_skills);if(!valid.ok)return valid;
        const auto birth=-source->draft.age_years*720;
        if(identity->source!=row.first||identity->actor_id!=source->actor_id||identity->prototype_id!=definition.prototype_actor_id||identity->created_tick!=0||identity->birth_tick!=birth||identity->display_name!=source->draft.display_name||actor->prototype_id!=definition.prototype_actor_id||actor->created_tick!=0||actor->birth_tick!=birth||actor->display_name!=source->draft.display_name||!actor->appearance||!appearance_equal(*actor->appearance,*appearance)||actor->wallet_id!="personal:"+actor->id||actor->inventory_id!="bag:"+actor->id)return common_failure("CharacterCreationIdentityMismatch",row.first);
        for(const auto member:attribute_members)if(actor->attributes.*member<source->draft.attributes.*member||actor->attributes.*member>source->draft.attributes.*member+5)return common_failure("CharacterCreationAttributeHistoryMismatch",row.first);
        if(actor->skills.extended.has_value()!=shared.extended_skills)return common_failure("CharacterCreationSkillProfileMismatch",row.first);
        for(const auto& id:growth_skill_ids(shared.extended_skills))if(growth_skill_xp(actor->skills,id)<growth_skill_xp(source->draft.skills,id))return common_failure("CharacterCreationSkillHistoryMismatch",id);
    }
    for(const auto& row:common.state().facts.rows())if(row.second.domain=="family"&&row.second.kind=="family_character_creation"){
        const auto* source=family.facts.find(row.first);if(!source||!std::holds_alternative<CharacterCreationFact>(*source))return common_failure("MissingCharacterCreationSource",row.first);
    }
    if(catalog.character_creation&&count!=1)return common_failure("MissingCharacterCreationSource");
    return common_success();
}
Status validate_character_creation_source(const CommonStore& c,const FamilyState& s,const FamilyCatalog& f,const CommonCatalog& shared){return validate_creation_source(c,s,f,shared,true);}
Status validate_character_creation_source_body(const CommonStore& c,const FamilyState& s,const FamilyCatalog& f,const CommonCatalog& shared){return validate_creation_source(c,s,f,shared,false);}
Outcome<CharacterCreationMode> family_character_creation_mode(const CommonStore& common,const FamilyState& family,const FamilyCatalog& catalog,const CommonCatalog& shared){
    auto valid=validate_character_creation_source(common,family,catalog,shared);if(!valid.ok)return {false,{},valid.error};
    if(!catalog.character_creation)return {true,CharacterCreationMode::standard,{}};
    for(const auto& row:family.facts.rows())if(const auto* source=std::get_if<CharacterCreationFact>(&row.second))return {true,source->draft.mode,{}};
    return {false,{},common_failure("MissingCharacterCreationSource").error};
}
Outcome<CharacterCreationMode> family_character_creation_mode_body(const CommonStore& common,const FamilyState& family,const FamilyCatalog& catalog,const CommonCatalog& shared){
    auto valid=validate_character_creation_source_body(common,family,catalog,shared);if(!valid.ok)return {false,{},valid.error};
    if(!catalog.character_creation)return {true,CharacterCreationMode::standard,{}};
    for(const auto& row:family.facts.rows())if(const auto* source=std::get_if<CharacterCreationFact>(&row.second))return {true,source->draft.mode,{}};
    return {false,{},common_failure("MissingCharacterCreationSource").error};
}
static Outcome<StableId> initialize_character_creation_impl(CommonStore& common,FamilyState& family,InteractionState& interaction,SocialState& social,const FamilyCatalog& catalog,const CommonCatalog& shared,const CharacterCreationDraft& draft,const StableId& choice_id,CharacterCreationV2Write* exact){
    const auto fail=[](Status error)->Outcome<StableId>{return {false,{},error.error};};
    if(!catalog.character_creation||catalog.founder_selection!=FamilyFounderSelection::created_character)return fail(common_failure("CharacterCreationCapabilityRequired"));
    if(common.tick()!=0||common.state().metadata.scenario_id.empty())return fail(common_failure("CharacterCreationInitialBoundaryRequired"));
    auto valid=validate_character_creation_catalog(common,shared,catalog,*catalog.character_creation);if(!valid.ok)return fail(valid);
    valid=validate_character_creation_draft(draft,shared.extended_skills);if(!valid.ok)return fail(valid);
    const auto& definition=*catalog.character_creation;const auto* choice=definition.appearance_choices.find(choice_id);
    if(!choice||std::find(definition.allowed_modes.begin(),definition.allowed_modes.end(),draft.mode)==definition.allowed_modes.end())return fail(common_failure("CharacterCreationSelectionDenied"));
    if(exact){
        auto now=exact->clock.read_current_moment();if(!now.ok)return fail({false,{},now.error});
        for(const auto& row:exact->state.character_origin_records.registration_sources.rows())if(row.second.registration_kind==RegistrationSourceV2::Kind::created){
            auto existing=read_created_character_origin_v2(common,exact->state,exact->catalog,catalog,shared,row.second.actor_id,now.value);if(!existing.ok)return fail({false,{},existing.error});
            if(!same_character_creation_draft(existing.value.creation->draft,draft,shared.extended_skills)||existing.value.appearance->id!=choice_id||family.founder_actor_id!=row.second.actor_id)return fail(common_failure("CharacterCreationSourceConflict"));
            return {true,row.second.actor_id,{}};
        }
    }
    for(const auto& row:family.facts.rows())if(const auto* existing=std::get_if<CharacterCreationFact>(&row.second)){
        if(!same_character_creation_draft(existing->draft,draft,shared.extended_skills)||existing->appearance_choice_id!=choice_id)return fail(common_failure("CharacterCreationSourceConflict"));
        valid=validate_character_creation_source(common,family,catalog,shared);if(!valid.ok)return fail(valid);return {true,existing->actor_id,{}};
    }
    auto point=common.begin_savepoint();if(!point.ok)return {false,{},point.error};
    auto candidate=family;auto next_interaction=interaction;auto next_social=social;
    StableId registered_id,registered_actor;
    auto abort=[&](Status error)->Outcome<StableId>{
        if(exact&&!registered_id.empty()){
            auto& records=exact->state.character_origin_records;
            if(const auto* applied=records.registration_sources.find(registered_id))exact->state.actor_start_context.initial_condition_seeds.erase(applied->initial_condition_seed_ref.seed_key);
            records.registration_sources.erase(registered_id);records.origins.erase(registered_actor);
            exact->state.character_condition_extensions.actor_condition_heads.erase(registered_actor);
        }
        auto rolled=common.rollback_savepoint(point.value);return fail(rolled.ok?error:rolled);
    };
    auto source_id=common.next_id("family");if(!source_id.ok)return abort({false,{},source_id.error});
    auto actor_id=common.next_id("actor");if(!actor_id.ok)return abort({false,{},actor_id.error});
    const auto& id=actor_id.value;const auto wallet="personal:"+id,bag="bag:"+id;const Owner owner{Owner::Kind::actor,id,{}};
    if(common.actor(id)||common.wallet(wallet)||common.inventory(bag)||common.state().facts.find(id)||common.state().facts.find(source_id.value)||next_social.memories.find(id)||std::any_of(next_interaction.knowledge.begin(),next_interaction.knowledge.end(),[&](const auto& value){return value.actor_id==id;}))return abort(common_failure("ActorIdentityCollision",id));
    auto resources=common.apply_resources({{CreateWallet{{wallet,owner,definition.initial_money,{},{}}},CreateInventory{{bag,owner,20,0,{},{}}}}, {}});if(!resources.ok)return abort(resources);
    auto appearance=checkpoint_make_shared<AppearanceProfile>();appearance->generator_version="appearance-v1";appearance->generation_seed="not-sampled";appearance->source="custom";appearance->base_features=choice->features;appearance->attire_style=choice->attire_style;
    const auto* place=shared.places.find(definition.starting_place_id);
    ActorRecord actor;actor.id=id;actor.prototype_id=definition.prototype_actor_id;actor.display_name=draft.display_name;actor.birth_tick=exact?std::optional<Integer>{}:std::optional<Integer>{-draft.age_years*720};actor.created_tick=0;actor.location=SettlementLocation{place->settlement_id,place->id};actor.alive=true;actor.available=true;actor.hp=100;actor.wallet_id=wallet;actor.inventory_id=bag;actor.attributes=draft.attributes;actor.skills=draft.skills;actor.appearance=std::move(appearance);
    actor.field_order={"id","prototype_id","display_name","birth_tick","created_tick","location","alive","available","hp","fatigue","merit","attributes","skills","wallet_id","inventory_id","action_id","injury","combat","appearance"};
    valid=common.create_actor(actor);if(!valid.ok)return abort(valid);
    ResourceRecipe goods;for(const auto& grant:definition.initial_goods)goods.operations.push_back(AddLot{bag,grant.item_id,grant.quantity,owner,{}});valid=common.apply_resources(goods);if(!valid.ok)return abort(valid);
    if(!exact){valid=common.append_fact({source_id.value,"family_character_creation","family"});if(!valid.ok)return abort(valid);}
    valid=common.append_fact({id,"actor_creation","family"});if(!valid.ok)return abort(valid);
    CharacterCreationFact source;source.actor_id=id;source.prototype_actor_id=definition.prototype_actor_id;source.scenario_id=common.state().metadata.scenario_id;source.appearance_choice_id=choice_id;source.draft=draft;
    if(exact){registered_id=source_id.value;registered_actor=id;valid=record_created_character_origin_in_candidate(common,*exact,catalog,shared,source_id.value,id,draft);if(!valid.ok)return abort(valid);}
    else if(!candidate.facts.insert(source_id.value,std::move(source))||!candidate.facts.insert(id,IdentityCreationFact{definition.prototype_actor_id,id,0,*actor.birth_tick,draft.display_name,source_id.value,{}}))return abort(common_failure("DuplicateCharacterCreationSource"));
    candidate.founder_actor_id=id;next_interaction.knowledge.push_back({id,{}});next_social.memories.insert(id,{});
    valid=common.set_player_actor(id);if(!valid.ok)return abort(valid);
    if(!exact){valid=validate_character_creation_source(common,candidate,catalog,shared);if(!valid.ok)return abort(valid);}
    valid=common.commit_savepoint(point.value);if(!valid.ok)return fail(valid);
    family=std::move(candidate);interaction=std::move(next_interaction);social=std::move(next_social);return {true,id,{}};
}
}

namespace hanmo::resident {
Outcome<StableId> initialize_character_creation(CommonStore& common,FamilyState& family,InteractionState& interaction,SocialState& social,const FamilyCatalog& catalog,const CommonCatalog& shared,const CharacterCreationDraft& draft,const StableId& choice){return initialize_character_creation_impl(common,family,interaction,social,catalog,shared,draft,choice,nullptr);}
Outcome<StableId> initialize_character_creation_v2(CommonStore& common,FamilyState& family,InteractionState& interaction,SocialState& social,const FamilyCatalog& catalog,const CommonCatalog& shared,const CharacterCreationDraft& draft,const StableId& choice,CharacterCreationV2Write& exact){return initialize_character_creation_impl(common,family,interaction,social,catalog,shared,draft,choice,&exact);}
}
