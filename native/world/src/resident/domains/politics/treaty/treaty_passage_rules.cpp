// 文书转呈与两方签署均走真实会面；任一写入失败由调用者的同World候选完整撤回。
#include "treaty_passage_detail.hpp"
#include <limits>
namespace hanmo::resident {
namespace {
using namespace treaty_passage_detail;
bool same_contact(const TreatyPassageContact& a,const TreatyPassageContact& b){return a.actor_id==b.actor_id&&a.target_actor_id==b.target_actor_id&&a.session_actor_id==b.session_actor_id&&a.session_id==b.session_id&&a.place_id==b.place_id&&a.provider_actor_id==b.provider_actor_id&&a.role_id==b.role_id&&a.context_kind==b.context_kind;}
bool same_signature(const TreatyPassageSignature& a,const TreatyPassageSignature& b){if(a.id!=b.id||a.offer_id!=b.offer_id||a.accepted_reply_id!=b.accepted_reply_id||a.terms_id!=b.terms_id||a.knowledge_source_fact_id!=b.knowledge_source_fact_id||a.prior_signature_id!=b.prior_signature_id||a.side!=b.side||!same_authority(a.authority,b.authority)||!same_authority(a.witness_authority,b.witness_authority)||a.grant_id!=b.grant_id||a.witness_grant_id!=b.witness_grant_id||a.tick!=b.tick||a.terms_revision!=b.terms_revision||!same_contact(a.contact,b.contact)||a.controls.size()!=b.controls.size())return false;for(std::size_t i=0;i<a.controls.size();++i)if(!same_control(a.controls[i],b.controls[i]))return false;return true;}
// 双方当场代表分别核当前授权；对方在场仅能证明听见文本，不能替其作同意决定。
Outcome<std::pair<ActingAuthority,StableId>> current_representative(const TreatyPassageContext& c,const DiplomaticOffer& offer,const DiplomaticMissionPolicy& policy,const DiplomaticPassageTerms& terms,TreatyPassageSide side,const StableId& actor){
    const auto& d=c.diplomacy;const auto id=side==TreatyPassageSide::grantor?policy.target_office_id:offer.issuer.office_id;const auto org=side==TreatyPassageSide::grantor?terms.grantor_organization_id:terms.beneficiary_organization_id;
    const auto* office=d.organizations.offices.find(id);if(!office||office->organization_id!=org||office->holder_actor_id!=std::optional<StableId>{actor})return {false,{},common_failure("TreatyPassageRepresentativeUnauthorized").error};
    ActingAuthority authority{org,id,office->epoch,actor};const auto site=side==TreatyPassageSide::grantor?policy.target_site_id:policy.issuer_site_id;
    auto authorized=check_authority(d.common,d.organizations,d.organization_catalog,authority,treaty_passage_ratify_command,{ScopeRef::Kind::site,site},0);if(!authorized.ok)return {false,{},authorized.error};
    const auto* grant=ratification_grant(d.organization_catalog,*office,policy,terms,side,d.common.tick());if(!grant)return {false,{},common_failure("TreatyPassageSpecificGrantRequired").error};
    return {true,{std::move(authority),grant->id},{}};
}
}
Outcome<TreatyPassagePresentQuote> preview_treaty_passage_presentation(const TreatyPassageContext& c,const StableId& actor,const TreatyPassagePresentCommand& command){
    const auto fail=[](const char* code)->Outcome<TreatyPassagePresentQuote>{return {false,{},common_failure(code).error};};
    if(!c.catalog.enabled)return fail("TreatyPassageDisabled");auto history=audit(c);if(!history.ok)return {false,{},history.error};
    const auto source=history.value.documents.find(command.source_fact_id);if(source==history.value.documents.end()||!knows(source->second,actor)||source->second.offer_id!=command.offer_id||source->second.reply_id!=command.accepted_reply_id)return fail("TreatyPassageDocumentNotKnown");
    const auto* offer=c.diplomacy.state.offers.find(command.offer_id);if(!offer||command.terms_revision!=offer->terms_revision||offer->last_fact_id!=command.accepted_reply_id||!c.catalog.provisions.find(offer->terms_id))return fail("TreatyPassageTermsChanged");
    const auto* policy=c.diplomacy.catalog.mission_policies.find(offer->policy_id);const auto* terms=c.diplomacy.catalog.passage_terms.find(offer->terms_id);
    if(!policy||!terms)return fail("TreatyPassageTermsChanged");
    auto recipient=current_representative(c,*offer,*policy,*terms,TreatyPassageSide::grantor,command.target_actor_id);if(!recipient.ok)recipient=current_representative(c,*offer,*policy,*terms,TreatyPassageSide::beneficiary,command.target_actor_id);if(!recipient.ok)return {false,{},recipient.error};
    auto contact=preview_face_to_face_contact(c.diplomacy.common,c.diplomacy.interaction,c.diplomacy.interaction_catalog,c.diplomacy.interaction_access,actor,command.target_actor_id);if(!contact.ok)return {false,{},contact.error};
    TreatyPassageDelivery record;record.offer_id=command.offer_id;record.accepted_reply_id=command.accepted_reply_id;record.source_fact_id=command.source_fact_id;record.tick=c.diplomacy.common.tick();record.terms_revision=command.terms_revision;record.contact=freeze_contact(contact.value);record.recipient_authority=recipient.value.first;record.recipient_grant_id=recipient.value.second;
    return {true,{c.diplomacy.common.state().identity,std::move(record),std::move(contact.value)},{}};
}
Outcome<TreatyPassageSignQuote> preview_treaty_passage_signature(const TreatyPassageContext& c,const StableId& actor,const TreatyPassageSignCommand& command){
    const auto fail=[](const char* code)->Outcome<TreatyPassageSignQuote>{return {false,{},common_failure(code).error};};
    if(!c.catalog.enabled)return fail("TreatyPassageDisabled");auto history=audit(c);if(!history.ok)return {false,{},history.error};
    const auto knowledge=history.value.documents.find(command.knowledge_source_fact_id);if(knowledge==history.value.documents.end()||!knows(knowledge->second,actor)||knowledge->second.offer_id!=command.offer_id||knowledge->second.reply_id!=command.accepted_reply_id)return fail("TreatyPassageDocumentNotKnown");
    const auto& d=c.diplomacy;const auto* offer=d.state.offers.find(command.offer_id);const auto* policy=offer?d.catalog.mission_policies.find(offer->policy_id):nullptr;const auto* terms=offer?d.catalog.passage_terms.find(offer->terms_id):nullptr;
    if(!offer||!policy||!terms||offer->status!=DiplomaticOfferStatus::accepted_pending_ratification||offer->last_fact_id!=command.accepted_reply_id||offer->terms_revision!=command.terms_revision||!c.catalog.provisions.find(terms->id))return fail("TreatyPassageTermsChanged");
    if(d.common.tick()>=offer->expires_tick)return fail("TreatyPassageRatificationExpired");
    if(command.authority.actor_id!=actor)return fail("TreatyPassageRepresentativeUnauthorized");
    const auto side=command.authority.organization_id==terms->grantor_organization_id?TreatyPassageSide::grantor:TreatyPassageSide::beneficiary;
    auto signer=current_representative(c,*offer,*policy,*terms,side,actor);if(!signer.ok)return {false,{},signer.error};if(!same_authority(command.authority,signer.value.first))return fail("TreatyPassageRepresentativeUnauthorized");
    auto witness=current_representative(c,*offer,*policy,*terms,side==TreatyPassageSide::grantor?TreatyPassageSide::beneficiary:TreatyPassageSide::grantor,command.target_actor_id);if(!witness.ok)return {false,{},witness.error};
    auto prior=history.value.signatures.find(offer->id);StableId previous;
    if(prior!=history.value.signatures.end()&&!prior->second.empty()){if(prior->second.size()!=1)return fail("TreatyPassageAlreadyRatified");if(prior->second.front()->side==side)return fail("TreatyPassageAlreadySigned");previous=prior->second.front()->id;if(knowledge->second.signature_id!=previous)return fail("TreatyPassagePriorSignatureNotKnown");}
    if(previous!=command.expected_prior_signature_id)return fail("TreatyPassageSignatureChanged");
    if(terms->duration_ticks<=0||d.common.tick()>std::numeric_limits<Integer>::max()-terms->duration_ticks)return fail("TreatyPassageLifetimeOverflow");
    auto controls=current_controls(c,*terms);if(!controls.ok)return {false,{},controls.error};
    auto contact=preview_face_to_face_contact(d.common,d.interaction,d.interaction_catalog,d.interaction_access,actor,command.target_actor_id);if(!contact.ok)return {false,{},contact.error};
    TreatyPassageSignature record;record.offer_id=offer->id;record.accepted_reply_id=command.accepted_reply_id;record.terms_id=terms->id;record.knowledge_source_fact_id=command.knowledge_source_fact_id;record.prior_signature_id=previous;record.side=side;record.authority=signer.value.first;record.witness_authority=witness.value.first;record.grant_id=signer.value.second;record.witness_grant_id=witness.value.second;record.tick=d.common.tick();record.terms_revision=offer->terms_revision;record.contact=freeze_contact(contact.value);record.controls=std::move(controls.value);
    return {true,{d.common.state().identity,std::move(record),std::move(contact.value)},{}};
}
Outcome<StableId> present_treaty_passage_in_transaction(CommonStore& common,TreatyPassageState& state,const TreatyPassageContext& c,const StableId& actor,const TreatyPassagePresentCommand& command,const TreatyPassagePresentQuote& quote){
    if(&common!=&c.diplomacy.common||&state!=&c.state)return {false,{},common_failure("TreatyPassageCandidateMismatch",{},ErrorKind::internal).error};
    auto fresh=preview_treaty_passage_presentation(c,actor,command);if(!fresh.ok)return {false,{},fresh.error};const auto& a=quote.delivery;const auto& b=fresh.value.delivery;
    if(!same_identity(quote.identity,common.state().identity)||!a.id.empty()||a.offer_id!=b.offer_id||a.accepted_reply_id!=b.accepted_reply_id||a.source_fact_id!=b.source_fact_id||a.tick!=b.tick||a.terms_revision!=b.terms_revision||!same_contact(a.contact,b.contact)||!same_authority(a.recipient_authority,b.recipient_authority)||a.recipient_grant_id!=b.recipient_grant_id)return {false,{},common_failure("TreatyPassageQuoteChanged").error};
    auto contact=revalidate_face_to_face_contact(common,c.diplomacy.interaction,c.diplomacy.interaction_catalog,c.diplomacy.interaction_access,quote.contact);if(!contact.ok)return {false,{},contact.error};
    auto id=common.next_id("treaty_passage_document");if(!id.ok)return id;auto record=b;record.id=id.value;auto appended=common.append_fact({id.value,treaty_passage_delivery_kind,treaty_passage_domain});if(!appended.ok)return {false,{},appended.error};if(!state.deliveries.insert(id.value,std::move(record)))return {false,{},common_failure("DuplicateTreatyPassageDocument",id.value).error};return id;
}
Outcome<StableId> sign_treaty_passage_in_transaction(CommonStore& common,TreatyPassageState& state,const TreatyPassageContext& c,const StableId& actor,const TreatyPassageSignCommand& command,const TreatyPassageSignQuote& quote){
    if(&common!=&c.diplomacy.common||&state!=&c.state)return {false,{},common_failure("TreatyPassageCandidateMismatch",{},ErrorKind::internal).error};
    auto fresh=preview_treaty_passage_signature(c,actor,command);if(!fresh.ok)return {false,{},fresh.error};if(!same_identity(quote.identity,common.state().identity)||!same_signature(quote.signature,fresh.value.signature))return {false,{},common_failure("TreatyPassageQuoteChanged").error};
    auto contact=revalidate_face_to_face_contact(common,c.diplomacy.interaction,c.diplomacy.interaction_catalog,c.diplomacy.interaction_access,quote.contact);if(!contact.ok)return {false,{},contact.error};
    auto id=common.next_id("treaty_passage_signature");if(!id.ok)return id;auto record=fresh.value.signature;record.id=id.value;auto appended=common.append_fact({id.value,treaty_passage_signature_kind,treaty_passage_domain});if(!appended.ok)return {false,{},appended.error};if(!state.signatures.insert(id.value,std::move(record)))return {false,{},common_failure("DuplicateTreatyPassageSignature",id.value).error};return id;
}
}
