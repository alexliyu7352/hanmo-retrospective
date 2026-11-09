// 冷审计从真实接受回复逐条推导获知与签署；当年在场者不由今日职务反造。
#include "treaty_passage_detail.hpp"
#include <limits>
namespace hanmo::resident::treaty_passage_detail {
bool same_identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
bool same_authority(const ActingAuthority& a,const ActingAuthority& b){return a.organization_id==b.organization_id&&a.office_id==b.office_id&&a.authority_epoch==b.authority_epoch&&a.actor_id==b.actor_id;}
bool same_control(const TreatyPassageControlSnapshot& a,const TreatyPassageControlSnapshot& b){return a.jurisdiction_id==b.jurisdiction_id&&a.organization_id==b.organization_id&&a.occupation_fact_id==b.occupation_fact_id&&a.revision==b.revision;}
bool knows(const DocumentKnowledge& d,const StableId& actor){return std::find(d.witnesses.begin(),d.witnesses.end(),actor)!=d.witnesses.end();}
TreatyPassageContact freeze_contact(const FaceToFaceContactProof& p){return {p.actor_id,p.target_actor_id,p.session_actor_id,p.session_id,p.context.place_id,p.context.provider_actor_id,p.context.role_id,p.context.kind};}
bool contact_valid(const CommonStore& c,const TreatyPassageContact& p){InteractionContext context;context.place_id=p.place_id;context.provider_actor_id=p.provider_actor_id;context.role_id=p.role_id;context.kind=p.context_kind;return !p.session_id.empty()&&p.actor_id!=p.target_actor_id&&c.actor(p.actor_id)&&c.actor(p.target_actor_id)&&face_to_face_contact_context_valid(context)&&(p.session_actor_id==p.actor_id||p.session_actor_id==p.target_actor_id)&&p.provider_actor_id==(p.session_actor_id==p.actor_id?p.target_actor_id:p.actor_id);}
namespace {
// 本非财产命令只接受无钱货列表的专项Grant；不能以今日容器Owner冒历史授权。
bool grant_covers(const Grant& g,const DiplomaticMissionPolicy& policy,const DiplomaticPassageTerms& terms,TreatyPassageSide side,Integer tick){
    if(g.amount_limit<0||tick<g.valid_from||(g.valid_until&&tick>*g.valid_until)||!g.wallet_ids.empty()||!g.inventory_ids.empty()||std::find(g.allowed_command_kinds.begin(),g.allowed_command_kinds.end(),treaty_passage_ratify_command)==g.allowed_command_kinds.end())return false;
    const auto has=[&](const StableId& place){return std::any_of(g.scopes.begin(),g.scopes.end(),[&](const auto& scope){return scope.kind==ScopeRef::Kind::site&&scope.id==place;});};
    if(!has(side==TreatyPassageSide::grantor?policy.target_site_id:policy.issuer_site_id))return false;
    if(side==TreatyPassageSide::grantor)for(const auto& place:terms.place_ids)if(!has(place))return false;
    return true;
}
}
const Grant* ratification_grant(const OrganizationCatalog& catalog,const OfficeState& office,const DiplomaticMissionPolicy& policy,const DiplomaticPassageTerms& terms,TreatyPassageSide side,Integer tick){for(const auto& id:office.grant_ids){const auto* grant=catalog.grants.find(id);if(grant&&grant->office_id==office.id&&grant_covers(*grant,policy,terms,side,tick))return grant;}return nullptr;}
bool historical_grant(const OrganizationCatalog& catalog,const StableId& id,const ActingAuthority& authority,const DiplomaticMissionPolicy& policy,const DiplomaticPassageTerms& terms,TreatyPassageSide side,Integer tick){const auto* office=catalog.offices.find(authority.office_id);const auto* grant=catalog.grants.find(id);return office&&office->organization_id==authority.organization_id&&grant&&grant->office_id==office->id&&std::find(office->grant_ids.begin(),office->grant_ids.end(),id)!=office->grant_ids.end()&&grant_covers(*grant,policy,terms,side,tick);}
Outcome<std::vector<TreatyPassageControlSnapshot>> current_controls(const TreatyPassageContext& c,const DiplomaticPassageTerms& terms){
    const auto* provision=c.catalog.provisions.find(terms.id);if(!provision)return {false,{},common_failure("TreatyPassageProvisionRequired").error};std::vector<TreatyPassageControlSnapshot> out;std::set<StableId> seen;
    for(const auto& place:provision->places)if(seen.insert(place.jurisdiction_id).second){auto source=c.controls.treaty_control_now(place.jurisdiction_id);if(!source.ok)return {false,{},source.error};if(!same_identity(source.value.identity,c.diplomacy.common.state().identity)||source.value.control.jurisdiction_id!=place.jurisdiction_id||source.value.control.revision<0)return {false,{},common_failure("TreatyPassageControlSourceMismatch").error};if(source.value.control.organization_id!=terms.grantor_organization_id)return {false,{},common_failure("TreatyPassageGrantorControlLost",place.jurisdiction_id).error};out.push_back(std::move(source.value.control));}
    return {true,std::move(out),{}};
}
Outcome<Audit> audit(const TreatyPassageContext& c){
    const auto fail=[](const char* code,const StableId& id=StableId{})->Outcome<Audit>{return {false,{},common_failure(code,id).error};};
    const auto& d=c.diplomacy;const auto& common=d.common;if(c.state.version!=1||c.catalog.version!=1)return fail("UnsupportedTreatyPassageVersion");
    if(!c.catalog.enabled){if(!c.state.deliveries.empty()||!c.state.signatures.empty())return fail("UnexpectedTreatyPassageState");for(const auto& row:common.state().facts.rows())if(row.second.kind==treaty_passage_delivery_kind||row.second.kind==treaty_passage_signature_kind)return fail("UnexpectedTreatyPassageFact",row.first);return {true,{},{}};}
    auto diplomacy=validate_diplomacy_state(common,d.organizations,d.organization_catalog,d.catalog,d.state,c.envoy_tasks);if(!diplomacy.ok)return {false,{},diplomacy.error};
    Audit out;std::set<StableId> delivered,signed_ids;Integer last_sequence=0,last_tick=0;
    // 原回复和新文书按Common真实事实次序合并，不能靠表内排序把后闻变成先知。
    for(const auto& id:common.state().facts.order()){
        const auto* head=common.state().facts.find(id);if(!head||head->id!=id)return fail("TreatyPassageFactOrderMismatch",id);
        const auto* reply=d.state.replies.find(id);
        if(reply&&reply->decision==DiplomaticReplyKind::accept){const auto* fact=d.state.facts.find(id);const auto seq=organization_instance_sequence(id,"diplomatic_reply");if(!fact||!seq)return fail("TreatyPassageReplySourceMismatch",id);out.documents.emplace(id,DocumentKnowledge{reply->offer_id,id,{},reply->tick,*seq,fact->witness_actor_ids});continue;}
        const bool delivery=head->kind==treaty_passage_delivery_kind,signature=head->kind==treaty_passage_signature_kind;
        if(!delivery&&!signature)continue;
        const auto seq=organization_instance_sequence(id,delivery?"treaty_passage_document":"treaty_passage_signature");
        if(head->domain!=treaty_passage_domain||!seq||*seq<=last_sequence||*seq>=common.state().next_instance_id)return fail("TreatyPassageFactOrderMismatch",id);
        const auto* shown=c.state.deliveries.find(id);const auto* sign=c.state.signatures.find(id);if((delivery&&(!shown||sign))||(signature&&(!sign||shown)))return fail("MissingTreatyPassagePayload",id);
        const auto& offer_id=delivery?shown->offer_id:sign->offer_id;const auto& reply_id=delivery?shown->accepted_reply_id:sign->accepted_reply_id;
        const auto& source_id=delivery?shown->source_fact_id:sign->knowledge_source_fact_id;const auto& contact=delivery?shown->contact:sign->contact;
        const auto tick=delivery?shown->tick:sign->tick,terms_revision=delivery?shown->terms_revision:sign->terms_revision;
        const auto* offer=d.state.offers.find(offer_id);const auto* original=d.state.replies.find(reply_id);const auto* terms=offer?d.catalog.passage_terms.find(offer->terms_id):nullptr;const auto* policy=offer?d.catalog.mission_policies.find(offer->policy_id):nullptr;
        auto source=out.documents.find(source_id);
        if((delivery?shown->id:sign->id)!=id||!offer||!original||!terms||!policy||!c.catalog.provisions.find(terms->id)||offer->status!=DiplomaticOfferStatus::accepted_pending_ratification||offer->last_fact_id!=reply_id||original->offer_id!=offer_id||original->decision!=DiplomaticReplyKind::accept||tick<original->tick||tick<last_tick||tick>common.tick()||terms_revision!=offer->terms_revision||!contact_valid(common,contact)||source==out.documents.end()||source->second.offer_id!=offer_id||source->second.reply_id!=reply_id||source->second.tick>tick||source->second.sequence>=*seq||!knows(source->second,contact.actor_id))return fail("InvalidTreatyPassageDocumentHistory",id);
        auto knowledge=source->second;knowledge.tick=tick;knowledge.sequence=*seq;knowledge.witnesses={contact.actor_id,contact.target_actor_id};
        if(delivery){
            const bool grantor=shown->recipient_authority.organization_id==terms->grantor_organization_id;const auto side=grantor?TreatyPassageSide::grantor:TreatyPassageSide::beneficiary;
            if(shown->recipient_authority.actor_id!=contact.target_actor_id||shown->recipient_authority.organization_id!=(grantor?terms->grantor_organization_id:terms->beneficiary_organization_id)||shown->recipient_authority.office_id!=(grantor?policy->target_office_id:offer->issuer.office_id)||!historical_grant(d.organization_catalog,shown->recipient_grant_id,shown->recipient_authority,*policy,*terms,side,tick))return fail("InvalidTreatyPassageRecipient",id);
            auto recipient=validate_historical_office_signer(common,d.organizations,d.organization_catalog,shown->recipient_authority,tick,*seq);if(!recipient.ok)return {false,{},recipient.error};
            delivered.insert(id);out.documents.emplace(id,std::move(knowledge));last_tick=tick;last_sequence=*seq;continue;
        }
        if(tick>=offer->expires_tick||sign->terms_id!=terms->id||sign->authority.actor_id!=contact.actor_id||sign->witness_authority.actor_id!=contact.target_actor_id)return fail("InvalidTreatyPassageSignature",id);
        const bool grantor=sign->side==TreatyPassageSide::grantor;if(!grantor&&sign->side!=TreatyPassageSide::beneficiary)return fail("InvalidTreatyPassageSide",id);
        const auto other=grantor?TreatyPassageSide::beneficiary:TreatyPassageSide::grantor;
        if(sign->authority.organization_id!=(grantor?terms->grantor_organization_id:terms->beneficiary_organization_id)||sign->authority.office_id!=(grantor?policy->target_office_id:offer->issuer.office_id)||sign->witness_authority.organization_id!=(grantor?terms->beneficiary_organization_id:terms->grantor_organization_id)||sign->witness_authority.office_id!=(grantor?offer->issuer.office_id:policy->target_office_id)||!historical_grant(d.organization_catalog,sign->grant_id,sign->authority,*policy,*terms,sign->side,tick)||!historical_grant(d.organization_catalog,sign->witness_grant_id,sign->witness_authority,*policy,*terms,other,tick))return fail("InvalidTreatyPassageAuthority",id);
        for(const auto* authority:{&sign->authority,&sign->witness_authority}){auto valid=validate_historical_office_signer(common,d.organizations,d.organization_catalog,*authority,tick,*seq);if(!valid.ok)return {false,{},valid.error};}
        auto& prior=out.signatures[offer_id];if(prior.size()>1||(!prior.empty()&&(prior.front()->side==sign->side||sign->prior_signature_id!=prior.front()->id||knowledge.signature_id!=prior.front()->id))||(prior.empty()&&!sign->prior_signature_id.empty()))return fail("TreatyPassageDoubleSignatureMismatch",id);
        const auto* provision=c.catalog.provisions.find(terms->id);std::set<StableId> jurisdictions;std::size_t i=0;
        for(const auto& place:provision->places)if(jurisdictions.insert(place.jurisdiction_id).second){if(i>=sign->controls.size())return fail("MissingTreatyPassageControl",id);const auto& saved=sign->controls[i++];auto current=c.controls.treaty_control_before_signature(place.jurisdiction_id,id,tick);if(!current.ok)return {false,{},current.error};if(!same_identity(current.value.identity,common.state().identity)||!same_control(saved,current.value.control)||saved.jurisdiction_id!=place.jurisdiction_id||saved.organization_id!=terms->grantor_organization_id||saved.revision<0)return fail("TreatyPassageControlSourceMismatch",id);}
        if(i!=sign->controls.size()||terms->duration_ticks<=0||terms->expiry_notice_ticks<=0||terms->expiry_notice_ticks>=terms->duration_ticks||tick>std::numeric_limits<Integer>::max()-terms->duration_ticks)return fail("InvalidTreatyPassageLifetime",id);
        prior.push_back(sign);signed_ids.insert(id);knowledge.signature_id=id;out.documents.emplace(id,std::move(knowledge));last_tick=tick;last_sequence=*seq;
    }
    if(delivered.size()!=c.state.deliveries.size()||signed_ids.size()!=c.state.signatures.size())return fail("OrphanTreatyPassagePayload");
    for(const auto& row:c.state.deliveries.rows())if(row.first!=row.second.id||!delivered.count(row.first))return fail("OrphanTreatyPassagePayload",row.first);
    for(const auto& row:c.state.signatures.rows())if(row.first!=row.second.id||!signed_ids.count(row.first))return fail("OrphanTreatyPassagePayload",row.first);
    return {true,std::move(out),{}};
}
}
namespace hanmo::resident {
Status validate_treaty_passage_state(const TreatyPassageContext& c){auto result=treaty_passage_detail::audit(c);return result.ok?common_success():Status{false,{},result.error};}
Outcome<std::optional<TreatyPassageAgreement>> read_treaty_passage_agreement(const TreatyPassageContext& c,const StableId& offer){auto result=treaty_passage_detail::audit(c);if(!result.ok)return {false,{},result.error};auto found=result.value.signatures.find(offer);if(found==result.value.signatures.end()||found->second.size()!=2)return {true,std::nullopt,{}};const auto* first=found->second[0];const auto* second=found->second[1];const auto* terms=c.diplomacy.catalog.passage_terms.find(second->terms_id);const bool grantor=first->side==TreatyPassageSide::grantor;return {true,TreatyPassageAgreement{offer,terms->id,grantor?first->id:second->id,grantor?second->id:first->id,second->tick,second->tick+terms->duration_ticks,second->tick+terms->duration_ticks-terms->expiry_notice_ticks},{}};}
}
