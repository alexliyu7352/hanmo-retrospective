// 使者实际到对方合法代表面前才能登记所递条款；反报价、认可与生效是后续独立决定。
#include "diplomacy_present.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
// 对方Office必须确有收取本类提议的专项授权，目录指定名字不能代替真实权限。
const Grant* receiving_grant(const OrganizationCatalog& catalog,const OfficeState& office,const StableId& place,Integer tick){
    for(const auto& id:office.grant_ids){const auto* grant=catalog.grants.find(id);if(!grant||grant->office_id!=office.id||grant->amount_limit<0||tick<grant->valid_from||(grant->valid_until&&tick>*grant->valid_until)||std::find(grant->allowed_command_kinds.begin(),grant->allowed_command_kinds.end(),"diplomatic_receive_passage")==grant->allowed_command_kinds.end())continue;for(const auto& scope:grant->scopes)if(scope.kind==ScopeRef::Kind::site&&scope.id==place)return grant;}
    return nullptr;
}
}
// 本人合法提议先过滤，外人猜中/猜错ID获得同一拒绝，不查询对方当前官职状态。
Outcome<DiplomaticPresentQuote> preview_diplomatic_presentation(const DiplomaticPresentRead& c,const StableId& actor,const DiplomaticPresentCommand& command){
    const auto fail=[&](const char* code){return Outcome<DiplomaticPresentQuote>{false,{},common_failure(code).error};};
    const auto* offer=c.state.offers.find(command.offer_id);
    if(!offer||offer->envoy_actor_id!=actor)return fail("DiplomaticOfferNotKnown");
    if(command.source_fact_id!=offer->last_fact_id||command.expected_revision!=offer->revision||command.terms_revision!=offer->terms_revision)return fail("DiplomaticOfferChanged");
    if(!offer->mission_authority_fact_id.empty())return fail("DiplomaticMissionTaskRequired");
    if(offer->status!=DiplomaticOfferStatus::awaiting_delivery)return fail("DiplomaticOfferNotAwaitingDelivery");
    auto issuer=revalidate_diplomatic_issuer(c.common,c.organizations,c.organization_catalog,c.catalog,*offer);if(!issuer.ok)return {false,{},issuer.error};
    const auto& policy=*c.catalog.mission_policies.find(offer->policy_id);const auto* office=c.organizations.offices.find(policy.target_office_id);
    if(!office||office->organization_id!=policy.target_organization_id||!office->holder_actor_id)return fail("DiplomaticRepresentativeUnavailable");
    ActingAuthority authority{office->organization_id,office->id,office->epoch,*office->holder_actor_id};
    auto authorized=check_authority(c.common,c.organizations,c.organization_catalog,authority,"diplomatic_receive_passage",{ScopeRef::Kind::site,policy.target_site_id},0);if(!authorized.ok)return fail("DiplomaticRepresentativeUnauthorized");
    const auto* grant=receiving_grant(c.organization_catalog,*office,policy.target_site_id,c.common.tick());if(!grant)return fail("DiplomaticRepresentativeUnauthorized");
    auto contact=preview_face_to_face_contact(c.common,c.interaction,c.interaction_catalog,c.interaction_access,actor,*office->holder_actor_id);if(!contact.ok)return {false,{},contact.error};
    if(contact.value.context.place_id!=policy.target_site_id)return fail("DiplomaticContactAtWrongSite");
    return {true,{offer->id,grant->id,offer->revision,offer->terms_revision,authority,std::move(contact.value)},{}};
}
// 展示报价必须来自当前Read所借同一个候选；失败不消耗任何原会话或人物行动。
Outcome<StableId> present_diplomatic_offer_in_transaction(CommonStore& common,DiplomacyState& state,const DiplomaticPresentRead& c,const StableId& actor,const DiplomaticPresentCommand& command,const DiplomaticPresentQuote& quote){
    if(&common!=&c.common||&state!=&c.state)return {false,{},common_failure("DiplomaticCandidateMismatch",{},ErrorKind::internal).error};
    auto current=preview_diplomatic_presentation(c,actor,command);if(!current.ok)return {false,{},current.error};const auto& fresh=current.value;
    if(quote.offer_id!=fresh.offer_id||quote.offer_revision!=fresh.offer_revision||quote.terms_revision!=fresh.terms_revision||quote.contact.actor_id!=actor||quote.contact.target_actor_id!=fresh.contact.target_actor_id||quote.representative.office_id!=fresh.representative.office_id||quote.representative.authority_epoch!=fresh.representative.authority_epoch||quote.representative_grant_id!=fresh.representative_grant_id)return {false,{},common_failure("DiplomaticOfferChanged").error};
    auto contact=revalidate_face_to_face_contact(common,c.interaction,c.interaction_catalog,c.interaction_access,quote.contact);if(!contact.ok)return {false,{},contact.error};
    auto* offer=state.offers.find(command.offer_id);if(offer->revision==INT64_MAX)return {false,{},common_failure("DiplomaticRevisionOverflow",offer->id).error};
    auto id=common.next_id("diplomatic_presentation");if(!id.ok)return id;
    DiplomaticPresentation presentation;presentation.id=id.value;presentation.offer_id=offer->id;presentation.proposal_fact_id=offer->last_fact_id;presentation.terms_id=offer->terms_id;presentation.envoy_actor_id=actor;presentation.representative=fresh.representative;presentation.representative_grant_id=fresh.representative_grant_id;presentation.tick=common.tick();presentation.terms_revision=offer->terms_revision;presentation.offer_revision=offer->revision+1;presentation.session_actor_id=fresh.contact.session_actor_id;presentation.session_id=fresh.contact.session_id;presentation.place_id=fresh.contact.context.place_id;presentation.provider_actor_id=fresh.contact.context.provider_actor_id;presentation.role_id=fresh.contact.context.role_id;presentation.context_kind=fresh.contact.context.kind;
    DiplomaticOfferFact fact;fact.id=id.value;fact.offer_id=offer->id;fact.kind="diplomatic_offer_presented";fact.actor_id=actor;fact.terms_id=offer->terms_id;fact.tick=common.tick();fact.terms_revision=offer->terms_revision;fact.offer_revision=offer->revision+1;fact.cause_fact_ids={offer->last_fact_id};fact.witness_actor_ids={actor,fresh.representative.actor_id};
    auto recorded=common.append_fact({id.value,fact.kind,"politics"});if(!recorded.ok)return {false,{},recorded.error};if(!state.presentations.insert(id.value,std::move(presentation))||!state.facts.insert(id.value,std::move(fact)))return {false,{},common_failure("DuplicateDiplomaticPresentation",id.value).error};offer->last_fact_id=id.value;offer->presentation_fact_id=id.value;offer->status=DiplomaticOfferStatus::presented;++offer->revision;return id;
}
}
