#include "resident/kernel/catalog_section_observation.hpp"
// 原当面声明只提供招伴准入来源；拒绝或未声明不分配ID、不改成员或钱货。
#include "party_recruitment_consent.hpp"
#include "resident/domains/interaction/party_invitation.hpp"
#include "resident/storage/common_store.hpp"
#include <set>
#include <tuple>
namespace hanmo::resident {namespace {
// 报错不泄露未到达节点的声明内容或远方人物当前位置。
template<class T>Outcome<T> failed(const StableId& code){return {false,{},common_failure(code).error};}
}
Status validate_party_recruitment_consent_catalog(const PartyRecruitmentConsentCatalog& policy,const InteractionCatalog& catalog){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 55, "PartyRecruitmentConsentCatalog", "validate_party_recruitment_consent_catalog", &policy);

    if(policy.version!=1)return common_failure("UnsupportedPartyRecruitmentConsentVersion");if(!catalog.common)return common_failure("MissingInteractionCatalog");if(policy.declarations.size()>4096)return common_failure("PartyRecruitmentDeclarationLimit");
    std::set<StableId> ids;std::set<std::tuple<StableId,StableId,StableId,StableId>> sources;
    for(const auto& row:policy.declarations){
        if(row.id.empty()||!ids.insert(row.id).second||row.leader_actor_id.empty()||row.companion_actor_id.empty()||row.leader_actor_id==row.companion_actor_id||(row.decision!="accept"&&row.decision!="decline")||!sources.emplace(row.leader_actor_id,row.companion_actor_id,row.dialogue_id,row.node_id).second)return common_failure("InvalidPartyRecruitmentDeclaration");
        for(const auto& actor:{row.leader_actor_id,row.companion_actor_id})if(std::find(catalog.common->actor_ids.begin(),catalog.common->actor_ids.end(),actor)==catalog.common->actor_ids.end())return common_failure("UnknownPartyRecruitmentActor");
        const auto* role=catalog.common->roles.find(row.role_id);if(!role||role->initial_actor!=row.companion_actor_id||role->companion_cost_per_tick<0||std::find(role->services.begin(),role->services.end(),"join_party")==role->services.end())return common_failure("InvalidPartyRecruitmentRole");
        if(catalog.party_invitations)for(const auto& invitation:catalog.party_invitations->invitations)if(invitation.role_id==row.role_id)return common_failure("PartyRecruitmentDirectionConflict");
        const DialogueDefinition* dialogue=nullptr;for(const auto& value:catalog.dialogues)if(value.id==row.dialogue_id){if(dialogue)return common_failure("InvalidPartyRecruitmentDialogue");dialogue=&value;}
        if(!dialogue||dialogue->context_role!=row.role_id)return common_failure("InvalidPartyRecruitmentDialogue");const DialogueNode* node=nullptr;for(const auto& value:dialogue->nodes)if(value.id==row.node_id){if(node)return common_failure("InvalidPartyRecruitmentNode");node=&value;}
        if(!node||node->kind!="say"||(node->speaker!="$provider"&&node->speaker!=row.companion_actor_id))return common_failure("PartyRecruitmentProviderDeclarationRequired");
    }
    return common_success();
}
Outcome<std::optional<PartyRecruitmentConsentQuote>> preview_party_recruitment_consent(const CommonStore& common,const InteractionState& state,const InteractionCatalog& catalog,const PartyRecruitmentConsentCatalog* policy,const InteractionAccess& access,const StableId& leader,const InteractionContext& context){
    if(!policy)return {true,std::nullopt,{}};if(policy->version!=1)return failed<std::optional<PartyRecruitmentConsentQuote>>("UnsupportedPartyRecruitmentConsentVersion");bool protected_role=false;for(const auto& row:policy->declarations)if(row.role_id==context.role_id){protected_role=true;break;}if(!protected_role)return {true,std::nullopt,{}};
    auto valid=validate_party_recruitment_consent_catalog(*policy,catalog);if(!valid.ok)return {false,{},valid.error};
    auto source=preview_authored_provider_source(common,state,catalog,access,leader,context.provider_actor_id);if(!source.ok)return {false,{},source.error};const auto& actual=source.value.meeting.context;
    if(actual.place_id!=context.place_id||actual.provider_actor_id!=context.provider_actor_id||actual.role_id!=context.role_id||actual.kind!=context.kind||actual.encounter_id!=context.encounter_id||context.kind)return failed<std::optional<PartyRecruitmentConsentQuote>>("PartyRecruitmentSourceMismatch");
    const PartyRecruitmentDeclaration* declaration=nullptr;for(const auto& row:policy->declarations)if(row.leader_actor_id==leader&&row.companion_actor_id==source.value.speaker_actor_id&&row.role_id==actual.role_id&&row.dialogue_id==source.value.meeting.dialogue_id&&row.node_id==source.value.meeting.node_id){if(declaration)return failed<std::optional<PartyRecruitmentConsentQuote>>("InvalidPartyRecruitmentDeclaration");declaration=&row;}
    if(!declaration)return failed<std::optional<PartyRecruitmentConsentQuote>>("PartyRecruitmentDeclarationRequired");const auto* role=catalog.common->roles.find(declaration->role_id);
    return {true,PartyRecruitmentConsentQuote{std::move(source.value),declaration->id,declaration->decision,role->id,role->companion_cost_per_tick},{}};
}
Status revalidate_party_recruitment_consent(const CommonStore& common,const InteractionState& state,const InteractionCatalog& catalog,const PartyRecruitmentConsentCatalog* policy,const InteractionAccess& access,const PartyRecruitmentConsentQuote& quote){
    auto valid=revalidate_authored_provider_source(common,state,catalog,access,quote.source);if(!valid.ok)return valid;auto fresh=preview_party_recruitment_consent(common,state,catalog,policy,access,quote.source.meeting.actor_id,quote.source.meeting.context);if(!fresh.ok)return {false,{},fresh.error};
    if(!fresh.value||fresh.value->declaration_id!=quote.declaration_id||fresh.value->decision!=quote.decision||fresh.value->role_id!=quote.role_id||fresh.value->cost_per_tick!=quote.cost_per_tick)return common_failure("StalePartyRecruitmentConsent");return common_success();
}
}
