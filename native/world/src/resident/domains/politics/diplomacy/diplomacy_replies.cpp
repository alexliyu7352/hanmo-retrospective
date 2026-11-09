// 回复沿真实已收到提议、当前专项Office授权及当面接触复核；接收或接受都不自动缔约。
#include "diplomacy_replies.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
// 未亲历收到提议的身份不能通过猜ID得知对方今日公职、期限或已作何种回复。
const DiplomaticPresentation* known_presentation(const DiplomacyState& state,const DiplomaticOffer& offer,const StableId& actor){const auto* row=state.presentations.find(offer.presentation_fact_id);return row&&row->offer_id==offer.id&&row->representative.actor_id==actor?row:nullptr;}
// 必须是本Office实际列明的回复权限，目录政策名不授予任何额外代表资格。
const Grant* reply_grant(const OrganizationCatalog& catalog,const OfficeState& office,const StableId& place,Integer tick){for(const auto& id:office.grant_ids){const auto* g=catalog.grants.find(id);if(!g||g->office_id!=office.id||g->amount_limit<0||tick<g->valid_from||(g->valid_until&&tick>*g->valid_until)||std::find(g->allowed_command_kinds.begin(),g->allowed_command_kinds.end(),"diplomatic_reply_passage")==g->allowed_command_kinds.end())continue;for(const auto& s:g->scopes)if(s.kind==ScopeRef::Kind::site&&s.id==place)return g;}return nullptr;}
}
// 接受与拒绝由本人明确选择；NPC后继也必须提交同一决定命令，不从愿意当值推断。
Outcome<DiplomaticReplyQuote> preview_diplomatic_reply(const DiplomaticPresentRead& c,const StableId& actor,const DiplomaticReplyCommand& cmd){
    const auto fail=[](const StableId& code)->Outcome<DiplomaticReplyQuote>{return {false,{},common_failure(code).error};};
    const auto* offer=c.state.offers.find(cmd.offer_id);const auto* delivery=offer?known_presentation(c.state,*offer,actor):nullptr;if(!offer||!delivery)return fail("DiplomaticOfferNotKnown");
    if(cmd.source_fact_id!=offer->last_fact_id||cmd.expected_revision!=offer->revision||cmd.terms_revision!=offer->terms_revision)return fail("DiplomaticOfferChanged");
    if(offer->status!=DiplomaticOfferStatus::presented)return fail("DiplomaticReplyUnavailable");
    if(cmd.decision!=DiplomaticReplyKind::accept&&cmd.decision!=DiplomaticReplyKind::refuse)return fail("InvalidDiplomaticDecision");
    auto issuer=revalidate_diplomatic_issuer(c.common,c.organizations,c.organization_catalog,c.catalog,*offer);if(!issuer.ok)return {false,{},issuer.error};
    const auto* policy=c.catalog.mission_policies.find(offer->policy_id);const auto* office=policy?c.organizations.offices.find(policy->target_office_id):nullptr;if(!office||office->holder_actor_id!=actor)return fail("DiplomaticRepresentativeUnauthorized");
    ActingAuthority authority{policy->target_organization_id,office->id,office->epoch,actor};auto authorized=check_authority(c.common,c.organizations,c.organization_catalog,authority,"diplomatic_reply_passage",{ScopeRef::Kind::site,policy->target_site_id},0);if(!authorized.ok)return fail("DiplomaticRepresentativeUnauthorized");
    const auto* grant=reply_grant(c.organization_catalog,*office,policy->target_site_id,c.common.tick());if(!grant)return fail("DiplomaticRepresentativeUnauthorized");
    auto contact=preview_face_to_face_contact(c.common,c.interaction,c.interaction_catalog,c.interaction_access,actor,offer->envoy_actor_id);if(!contact.ok)return {false,{},contact.error};if(contact.value.context.place_id!=policy->target_site_id)return fail("DiplomaticContactAtWrongSite");
    return {true,{offer->id,grant->id,offer->revision,offer->terms_revision,authority,cmd.decision,std::move(contact.value)},{}};
}
// 当前报价与原会话来源同时重验；所有新事实属于调用方唯一候选事务。
Outcome<StableId> reply_diplomatic_offer_in_transaction(CommonStore& common,DiplomacyState& state,const DiplomaticPresentRead& c,const StableId& actor,const DiplomaticReplyCommand& cmd,const DiplomaticReplyQuote& quote){
    if(&common!=&c.common||&state!=&c.state)return {false,{},common_failure("DiplomaticCandidateMismatch",{},ErrorKind::internal).error};
    auto current=preview_diplomatic_reply(c,actor,cmd);if(!current.ok)return {false,{},current.error};const auto& fresh=current.value;
    if(quote.decision!=cmd.decision||quote.offer_id!=fresh.offer_id||quote.offer_revision!=fresh.offer_revision||quote.terms_revision!=fresh.terms_revision||quote.grant_id!=fresh.grant_id||quote.authority.actor_id!=actor||quote.authority.office_id!=fresh.authority.office_id||quote.authority.authority_epoch!=fresh.authority.authority_epoch||quote.contact.actor_id!=actor||quote.contact.target_actor_id!=fresh.contact.target_actor_id)return {false,{},common_failure("DiplomaticOfferChanged").error};
    auto contact=revalidate_face_to_face_contact(common,c.interaction,c.interaction_catalog,c.interaction_access,quote.contact);if(!contact.ok)return {false,{},contact.error};auto* offer=state.offers.find(cmd.offer_id);if(offer->revision==INT64_MAX)return {false,{},common_failure("DiplomaticRevisionOverflow").error};auto id=common.next_id("diplomatic_reply");if(!id.ok)return id;
    DiplomaticReply reply;reply.id=id.value;reply.offer_id=offer->id;reply.presentation_fact_id=offer->last_fact_id;reply.terms_id=offer->terms_id;reply.grant_id=fresh.grant_id;reply.authority=fresh.authority;reply.decision=cmd.decision;reply.tick=common.tick();reply.terms_revision=offer->terms_revision;reply.offer_revision=offer->revision+1;reply.session_actor_id=fresh.contact.session_actor_id;reply.session_id=fresh.contact.session_id;reply.place_id=fresh.contact.context.place_id;reply.provider_actor_id=fresh.contact.context.provider_actor_id;reply.role_id=fresh.contact.context.role_id;reply.context_kind=fresh.contact.context.kind;
    DiplomaticOfferFact fact;fact.id=id.value;fact.offer_id=offer->id;fact.kind="diplomatic_offer_replied";fact.actor_id=actor;fact.terms_id=offer->terms_id;fact.tick=common.tick();fact.terms_revision=offer->terms_revision;fact.offer_revision=reply.offer_revision;fact.cause_fact_ids={offer->last_fact_id};fact.witness_actor_ids={actor,offer->envoy_actor_id};auto recorded=common.append_fact({id.value,fact.kind,"politics"});if(!recorded.ok)return {false,{},recorded.error};if(!state.replies.insert(id.value,std::move(reply))||!state.facts.insert(id.value,std::move(fact)))return {false,{},common_failure("DuplicateDiplomaticReply",id.value).error};offer->last_fact_id=id.value;offer->status=cmd.decision==DiplomaticReplyKind::accept?DiplomaticOfferStatus::accepted_pending_ratification:DiplomaticOfferStatus::refused;++offer->revision;return id;
}
}
