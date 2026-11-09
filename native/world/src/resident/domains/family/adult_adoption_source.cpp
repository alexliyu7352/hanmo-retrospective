#include "resident/kernel/catalog_section_observation.hpp"
#include "adult_adoption_source.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/domains/interaction/face_to_face_contact.hpp"
#include <algorithm>
#include <set>
#include <tuple>

namespace hanmo::resident {
namespace {
constexpr Integer maximum_tick=2147483647,adult_ticks=18*720;
bool valid_id(const StableId& value){return !value.empty()&&value.size()<=256;}
const DialogueDefinition* find_dialogue(const InteractionCatalog& catalog,const StableId& name){
    for(const auto& value:catalog.dialogues)if(value.id==name)return &value;
    return nullptr;
}
const DialogueNode* find_node(const DialogueDefinition& dialogue,const StableId& name){
    for(const auto& value:dialogue.nodes)if(value.id==name)return &value;
    return nullptr;
}
bool context_equal(const InteractionContext& a,const InteractionContext& b){
    return std::tie(a.place_id,a.provider_actor_id,a.role_id,a.kind,a.encounter_id,a.input_error)==
           std::tie(b.place_id,b.provider_actor_id,b.role_id,b.kind,b.encounter_id,b.input_error);
}
Status adult_at(const CommonState& common,const StableId& id,Integer tick){
    const auto* actor=common.actors.find(id);
    if(!actor)return common_failure("UnknownActor",id);
    if(tick<0||tick>common.identity.tick||tick>maximum_tick||!actor->birth_tick||
       *actor->birth_tick< -maximum_tick||*actor->birth_tick>tick)return common_failure("ActorAgeUnknown",id);
    return tick-*actor->birth_tick>=adult_ticks?common_success():common_failure("AdultAdoptionRequiresAdult",id);
}
}
bool adult_adoption_agreement_equal(const AdultAdoptionAgreement& a,const AdultAdoptionAgreement& b){
    return std::tie(a.request_id,a.adopter_actor_id,a.adoptee_actor_id,a.dialogue_id,a.node_id,a.node_kind,
                    a.session_id,a.scenario_id,a.world_id,a.terms_version,a.accepted_tick,a.accepted_revision,
                    a.adopter_birth_tick,a.adoptee_birth_tick)==
           std::tie(b.request_id,b.adopter_actor_id,b.adoptee_actor_id,b.dialogue_id,b.node_id,b.node_kind,
                    b.session_id,b.scenario_id,b.world_id,b.terms_version,b.accepted_tick,b.accepted_revision,
                    b.adopter_birth_tick,b.adoptee_birth_tick)&&context_equal(a.context,b.context);
}
Status validate_adult_adoption_request_catalog(const AdultAdoptionRequestCatalog& requests,const CommonCatalog& shared,const InteractionCatalog& interaction){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 22, "AdultAdoptionRequestCatalog", "validate_adult_adoption_request_catalog", &requests);

    if(!requests.enabled)return requests.definitions.empty()?common_success():common_failure("AdultAdoptionCapabilityRequired");
    if(requests.definitions.empty()||requests.definitions.size()>4096)return common_failure("InvalidAdultAdoptionCatalogSize");
    std::set<std::tuple<StableId,StableId,StableId,StableId>> bindings;
    for(const auto& row:requests.definitions.rows()){
        const auto& d=row.second;
        for(const auto* id:{&d.id,&d.adopter_actor_id,&d.adoptee_actor_id,&d.dialogue_id,&d.node_id})
            if(!valid_id(*id))return common_failure("InvalidAdultAdoptionDefinition",row.first);
        if(row.first!=d.id||d.terms_version!=1||d.adopter_actor_id==d.adoptee_actor_id)return common_failure("InvalidAdultAdoptionParties",row.first);
        for(const auto* actor:{&d.adopter_actor_id,&d.adoptee_actor_id})
            if(std::find(shared.actor_ids.begin(),shared.actor_ids.end(),*actor)==shared.actor_ids.end())return common_failure("UnknownAdultAdoptionActor",*actor);
        const auto* dialogue=find_dialogue(interaction,d.dialogue_id);
        if(!dialogue||dialogue->context_role.empty()||!shared.roles.find(dialogue->context_role))return common_failure("AdultAdoptionOrdinaryDialogueRequired",row.first);
        const auto* node=find_node(*dialogue,d.node_id);
        if(!node||(node->kind!="say"&&node->kind!="choice"))return common_failure("InvalidAdultAdoptionRequestNode",row.first);
        // 原条款明确是被收养者本人请求；玩家选择或他人的台词不代表其意愿。
        if(node->kind!="say"||(node->speaker!="$provider"&&node->speaker!=d.adoptee_actor_id))return common_failure("AdultAdoptionProviderDeclarationRequired",row.first);
        if(!bindings.emplace(d.adopter_actor_id,d.adoptee_actor_id,d.dialogue_id,d.node_id).second)return common_failure("DuplicateAdultAdoptionRequestBinding",row.first);
    }
    return common_success();
}
Status validate_adult_adoption_agreement_source(const CommonState& common,const CommonCatalog& shared,const InteractionCatalog& interaction,const AdultAdoptionRequestCatalog& requests,const AdultAdoptionAgreement& a){
    if(!requests.enabled)return common_failure("AdultAdoptionUnavailable");
    auto valid=validate_adult_adoption_request_catalog(requests,shared,interaction);if(!valid.ok)return valid;
    const auto* d=requests.definitions.find(a.request_id);
    if(!d||std::tie(d->adopter_actor_id,d->adoptee_actor_id,d->dialogue_id,d->node_id,d->terms_version)!=
           std::tie(a.adopter_actor_id,a.adoptee_actor_id,a.dialogue_id,a.node_id,a.terms_version))return common_failure("AdultAdoptionAgreementBindingMismatch");
    if(a.world_id!=common.identity.world_id||a.scenario_id!=common.metadata.scenario_id||!valid_id(a.scenario_id)||
       std::all_of(a.world_id.begin(),a.world_id.end(),[](auto byte){return byte==0;}))return common_failure("AdultAdoptionWorldMismatch");
    if(a.accepted_revision<0||a.accepted_revision>common.identity.revision)return common_failure("AdultAdoptionSourceTimeMismatch");
    const auto* dialogue=find_dialogue(interaction,a.dialogue_id);const auto* node=find_node(*dialogue,a.node_id);
    const auto* role=shared.roles.find(dialogue->context_role);
    if(!valid_id(a.session_id)||a.node_kind!=node->kind||!face_to_face_contact_context_valid(a.context)||a.context.kind||
       a.context.provider_actor_id!=a.adoptee_actor_id||a.context.role_id!=dialogue->context_role||
       a.context.place_id!=role->place_id||!shared.places.find(a.context.place_id))return common_failure("InvalidAdultAdoptionAgreementSource");
    for(const auto* actor:{&a.adopter_actor_id,&a.adoptee_actor_id}){auto adult=adult_at(common,*actor,a.accepted_tick);if(!adult.ok)return adult;}
    if(a.adopter_birth_tick!=*common.actors.find(a.adopter_actor_id)->birth_tick||a.adoptee_birth_tick!=*common.actors.find(a.adoptee_actor_id)->birth_tick)return common_failure("AdultAdoptionIdentityChanged");
    return common_success();
}
Outcome<AdultAdoptionRequestQuote> preview_adult_adoption_request(const CommonStore& common,const InteractionState& state,const InteractionCatalog& interaction,const InteractionAccess& access,const AdultAdoptionRequestCatalog& requests,const StableId& adopter,const StableId& adoptee,const StableId& request_id){
    if(!requests.enabled)return {false,{},common_failure("AdultAdoptionUnavailable").error};
    if(adopter.empty()||adopter!=common.state().metadata.player_actor_id)return {false,{},common_failure("NotPlayerActor",adopter).error};
    if(!interaction.common)return {false,{},common_failure("MissingAdultAdoptionCommonCatalog").error};
    auto valid=validate_adult_adoption_request_catalog(requests,*interaction.common,interaction);if(!valid.ok)return {false,{},valid.error};
    const auto* d=requests.definitions.find(request_id);
    if(!d||d->adopter_actor_id!=adopter||d->adoptee_actor_id!=adoptee)return {false,{},common_failure("AdultAdoptionRequestMismatch",request_id).error};
    for(const auto* actor:{&adopter,&adoptee}){auto adult=adult_at(common.state(),*actor,common.tick());if(!adult.ok)return {false,{},adult.error};}
    auto proof=preview_authored_node_source(common,state,interaction,access,adopter,adoptee);if(!proof.ok)return {false,{},proof.error};
    if(proof.value.dialogue_id!=d->dialogue_id||proof.value.node_id!=d->node_id)return {false,{},common_failure("AdultAdoptionRequestNotCurrent",request_id).error};
    AdultAdoptionAgreement a;
    a.request_id=d->id;a.adopter_actor_id=adopter;a.adoptee_actor_id=adoptee;a.terms_version=d->terms_version;
    a.dialogue_id=proof.value.dialogue_id;a.node_id=proof.value.node_id;a.node_kind=proof.value.node_kind;
    a.session_id=proof.value.session_id;a.context=proof.value.context;a.world_id=common.state().identity.world_id;
    a.scenario_id=common.state().metadata.scenario_id;a.accepted_tick=common.tick();a.accepted_revision=common.state().identity.revision;
    a.adopter_birth_tick=*common.state().actors.find(adopter)->birth_tick;a.adoptee_birth_tick=*common.state().actors.find(adoptee)->birth_tick;
    valid=validate_adult_adoption_agreement_source(common.state(),*interaction.common,interaction,requests,a);if(!valid.ok)return {false,{},valid.error};
    return {true,{proof.value,std::move(a)},{}};
}
Status revalidate_adult_adoption_request(const CommonStore& common,const InteractionState& state,const InteractionCatalog& interaction,const InteractionAccess& access,const AdultAdoptionRequestCatalog& requests,const AdultAdoptionRequestQuote& quote){
    const auto& p=quote.source;const auto& a=quote.agreement;
    if(std::tie(p.actor_id,p.provider_actor_id,p.dialogue_id,p.node_id,p.node_kind,p.session_id,p.source_identity.world_id,p.source_identity.tick,p.source_identity.revision)!=
       std::tie(a.adopter_actor_id,a.adoptee_actor_id,a.dialogue_id,a.node_id,a.node_kind,a.session_id,a.world_id,a.accepted_tick,a.accepted_revision)||
       !context_equal(p.context,a.context))return common_failure("AdultAdoptionRequestSourceMismatch");
    auto valid=revalidate_authored_node_source(common,state,interaction,access,p);if(!valid.ok)return valid;
    auto fresh=preview_adult_adoption_request(common,state,interaction,access,requests,a.adopter_actor_id,a.adoptee_actor_id,a.request_id);
    if(!fresh.ok)return {false,{},fresh.error};
    return adult_adoption_agreement_equal(a,fresh.value.agreement)?common_success():common_failure("StaleAdultAdoptionRequest");
}
}
