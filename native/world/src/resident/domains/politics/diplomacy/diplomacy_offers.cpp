// 真实Office专项Grant签发提议；本叶不消费资源、不创建Task、不移动使者或准许通行。
#include "diplomacy_offers.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
constexpr const char* propose_kind="diplomatic_propose_passage";
// 与原组织授权同一已授Grant集合，进一步保存本条具名来源供历史校验。
const Grant* issuing_grant(const OrganizationState& state,const OrganizationCatalog& catalog,const ActingAuthority& authority,const StableId& site,Integer tick){
    const auto* office=state.offices.find(authority.office_id);if(!office)return nullptr;
    for(const auto& id:office->grant_ids){const auto* grant=catalog.grants.find(id);if(!grant||grant->office_id!=office->id||grant->amount_limit<0||tick<grant->valid_from||(grant->valid_until&&tick>*grant->valid_until)||std::find(grant->allowed_command_kinds.begin(),grant->allowed_command_kinds.end(),propose_kind)==grant->allowed_command_kinds.end())continue;for(const auto& scope:grant->scopes)if(scope.kind==ScopeRef::Kind::site&&scope.id==site)return grant;}
    return nullptr;
}
// 真实地点来自CommonStore；在途或缺失人物不能被界面坐标冒作当场。
bool at_site(const CommonStore& common,const StableId& actor,const StableId& site){auto place=common.resolved_location(actor);const auto* at=place.ok?std::get_if<SettlementLocation>(&place.value):nullptr;return at&&at->place_id==site;}
}
// 所有字段从作者目录与当前权源取得；预览没有ID、事实或行动写入。
Outcome<DiplomaticProposeQuote> preview_diplomatic_proposal(const CommonStore& common,const CommonCatalog& cc,const OrganizationState& organizations,const OrganizationCatalog& oc,const DiplomacyCatalog& catalog,const ActivityRead& activity,const StableId& actor,const DiplomaticProposeCommand& command){
    const auto denied=[&](const char* code){return Outcome<DiplomaticProposeQuote>{false,{},common_failure(code).error};};
    if(!catalog.enabled)return denied("DiplomacyNotEnabled");
    if(catalog.version!=1&&catalog.version!=2)return denied("DiplomacyVersionMismatch");
    if(command.authority.actor_id!=actor)return denied("DiplomaticActorMismatch");
    const auto* policy=catalog.mission_policies.find(command.policy_id);const auto* terms=catalog.passage_terms.find(command.terms_id);
    if(!policy||!terms||std::find(policy->passage_terms_ids.begin(),policy->passage_terms_ids.end(),command.terms_id)==policy->passage_terms_ids.end()||command.authority.organization_id!=policy->issuer_organization_id||terms->beneficiary_organization_id!=policy->issuer_organization_id||terms->grantor_organization_id!=policy->target_organization_id||!cc.places.find(policy->issuer_site_id))return denied("DiplomaticTermsOutsideMandate");
    const auto* person=common.actor(actor);if(!person||!person->alive||!person->available)return denied("DiplomaticActorUnavailable");
    if(!person->action_id.empty()||activity.in_open_battle(actor))return denied("ActorBusy");
    if(!at_site(common,actor,policy->issuer_site_id))return denied("DiplomaticIssuerNotAtSite");
    const auto* target=organizations.organizations.find(policy->target_organization_id);if(!target||target->status!=OrganizationStatus::active)return denied("DiplomaticCounterpartyUnavailable");
    auto proof=check_authority(common,organizations,oc,command.authority,propose_kind,{ScopeRef::Kind::site,policy->issuer_site_id},0);if(!proof.ok)return {false,{},proof.error};
    const auto* grant=issuing_grant(organizations,oc,command.authority,policy->issuer_site_id,common.tick());if(!grant)return denied("DiplomaticAuthorityRequired");
    if(common.tick()<0||policy->offer_valid_ticks<=0||common.tick()>INT64_MAX-policy->offer_valid_ticks)return denied("DiplomaticDeadlineOverflow");
    DiplomaticOffer proposal;proposal.policy_id=policy->id;proposal.terms_id=terms->id;proposal.issuer_grant_id=grant->id;proposal.envoy_actor_id=actor;proposal.issuer=command.authority;proposal.issued_tick=common.tick();proposal.expires_tick=common.tick()+policy->offer_valid_ticks;
    if(grant->valid_until&&*grant->valid_until<INT64_MAX)proposal.expires_tick=std::min(proposal.expires_tick,*grant->valid_until+1);
    return {true,{common.state().identity,std::move(proposal)},{}};
}
// 真实提议与Common fact在同一外层候选追加；明确ID局部变量避免move后空键。
Outcome<StableId> propose_diplomatic_offer_in_transaction(CommonStore& common,const CommonCatalog& cc,const OrganizationState& organizations,const OrganizationCatalog& oc,const DiplomacyCatalog& catalog,DiplomacyState& state,const ActivityRead& activity,const StableId& actor,const DiplomaticProposeCommand& command){
    if(state.version!=catalog.version||(state.version!=1&&state.version!=2))return {false,{},common_failure("DiplomacyVersionMismatch").error};auto quote=preview_diplomatic_proposal(common,cc,organizations,oc,catalog,activity,actor,command);if(!quote.ok)return {false,{},quote.error};
    auto id=common.next_id("diplomatic_offer");if(!id.ok)return id;auto offer=std::move(quote.value.proposal);offer.id=id.value;offer.last_fact_id=id.value;
    DiplomaticOfferFact fact;fact.id=id.value;fact.offer_id=id.value;fact.kind="diplomatic_offer_proposed";fact.actor_id=actor;fact.terms_id=offer.terms_id;fact.tick=common.tick();fact.witness_actor_ids={actor};
    auto recorded=common.append_fact({id.value,fact.kind,"politics"});if(!recorded.ok)return {false,{},recorded.error};if(!state.offers.insert(id.value,std::move(offer))||!state.facts.insert(id.value,std::move(fact)))return {false,{},common_failure("DuplicateDiplomaticOffer",id.value).error};return id;
}
// 过期、换任和撤Grant只阻止新的对外交涉；不删除已发生的提议历史。
Status revalidate_diplomatic_issuer(const CommonStore& common,const OrganizationState& organizations,const OrganizationCatalog& oc,const DiplomacyCatalog& catalog,const DiplomaticOffer& offer){
    const auto* policy=catalog.mission_policies.find(offer.policy_id);const auto* terms=catalog.passage_terms.find(offer.terms_id);
    if(!catalog.enabled||!policy||!terms||(catalog.version==1&&offer.envoy_actor_id!=offer.issuer.actor_id)||(!common.actor(offer.envoy_actor_id))||offer.issuer.organization_id!=policy->issuer_organization_id||terms->beneficiary_organization_id!=policy->issuer_organization_id||terms->grantor_organization_id!=policy->target_organization_id)return common_failure("InvalidDiplomaticOffer",offer.id);
    if(common.tick()>=offer.expires_tick)return common_failure("DiplomaticOfferExpired",offer.id);
    auto proof=check_authority(common,organizations,oc,offer.issuer,propose_kind,{ScopeRef::Kind::site,policy->issuer_site_id},0);if(!proof.ok)return common_failure("DiplomaticAuthorityChanged",offer.id);
    const auto* grant=issuing_grant(organizations,oc,offer.issuer,policy->issuer_site_id,common.tick());if(!grant||grant->id!=offer.issuer_grant_id)return common_failure("DiplomaticAuthorityChanged",offer.id);
    return common_success();
}
}
