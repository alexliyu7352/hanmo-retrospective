// 委任知情只认原当面送达记录；猜中他人使命ID不能看到远方权源或期限状态。
#include "diplomacy_delegation.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <set>
namespace hanmo::resident {
namespace {
Status valid_knowledge(const DiplomaticHistoryRead& r,const DiplomacyV2Catalog& catalog,const DiplomaticMissionKnowledge& k) {
    const auto* policy=catalog.delegation_policies.find(k.delegation_policy_id);
    const auto* mission=r.state.missions.find(k.mission_authority_fact_id);
    const auto* offer=mission?r.state.offers.find(mission->proposal_id):nullptr;
    const auto* header=r.common.state().facts.find(k.id);
    const auto n=organization_instance_sequence(k.id,"diplomatic_mission_known");
    const auto m=organization_instance_sequence(k.mission_authority_fact_id,"diplomatic_mission");
    const auto o=offer?organization_instance_sequence(offer->id,"diplomatic_offer"):std::optional<Integer>{};
    if(!policy||!mission||!offer||!header||header->id!=k.id||header->kind!=diplomatic_mission_known_fact_kind||header->domain!=diplomatic_fact_domain||
        !n||!m||!o||*o>=*m||*m>=*n||*n>=r.common.state().next_instance_id||k.tick!=mission->issued_tick||k.tick!=offer->issued_tick||k.tick>r.common.tick()||
        k.envoy_actor_id!=policy->envoy_actor_id||mission->envoy_actor_id!=k.envoy_actor_id||offer->envoy_actor_id!=k.envoy_actor_id||
        k.issuer_actor_id!=mission->issuer_actor_id||k.issuer_actor_id!=offer->issuer.actor_id||k.issuer_actor_id==k.envoy_actor_id||
        mission->policy_id!=policy->mission_policy_id||offer->terms_id!=policy->terms_id||mission->issuer_office_id!=policy->issuer_office_id||
        k.site_id!=mission->report_site_id||k.session_id.empty()||!mission->return_report_required||
        (k.session_actor_id!=k.issuer_actor_id&&k.session_actor_id!=k.envoy_actor_id)||
        k.provider_actor_id!=(k.session_actor_id==k.issuer_actor_id?k.envoy_actor_id:k.issuer_actor_id))
        return common_failure("InvalidDiplomaticMissionKnowledge",k.id);
    InteractionContext context{k.site_id,k.provider_actor_id,k.role_id};context.kind=k.context_kind;
    if(!face_to_face_contact_context_valid(context))return common_failure("InvalidDiplomaticMissionKnowledge",k.id);
    DiplomaticMissionAccess original(r.common,r.organizations,r.organization_catalog,r.catalog,r.state);
    auto source=original.diplomatic_mission_source(mission->mission_authority_fact_id);if(!source.ok)return {false,{},source.error};
    return validate_historical_office_signer(r.common,r.organizations,r.organization_catalog,offer->issuer,k.tick,*n);
}
}
Status validate_diplomatic_mission_knowledge(const DiplomaticHistoryRead& read,const DiplomacyV2Catalog& catalog,const DiplomacyV2State& state) {
    if(read.catalog.version!=2||read.state.version!=2||catalog.version!=2||state.version!=2)return common_failure("DiplomaticDelegationVersionRequired");
    std::set<StableId> missions,policies;
    for(const auto& row:state.mission_knowledge.rows()) {
        const auto& k=row.second;
        if(row.first!=k.id||!missions.insert(k.mission_authority_fact_id).second||!policies.insert(k.delegation_policy_id).second)
            return common_failure("DuplicateDiplomaticMissionKnowledge",row.first);
        auto checked=valid_knowledge(read,catalog,k);if(!checked.ok)return checked;
    }
    for(const auto& row:read.state.missions.rows()) if(row.second.envoy_actor_id!=row.second.issuer_actor_id&&!missions.count(row.first))
        return common_failure("MissingDiplomaticMissionKnowledge",row.first);
    for(const auto& row:read.common.state().facts.rows()) if(row.second.domain==diplomatic_fact_domain&&row.second.kind==diplomatic_mission_known_fact_kind&&!state.mission_knowledge.find(row.first))
        return common_failure("MissingDiplomaticMissionKnowledge",row.first);
    return common_success();
}
Outcome<DiplomaticMissionApproval> DiplomaticDelegatedMissionAccess::diplomatic_mission_for_acceptance(const StableId& id,const StableId& actor) const {
    const auto fail=[](const StableId& code)->Outcome<DiplomaticMissionApproval>{return {false,{},common_failure(code).error};};
    const auto* mission=read_.state.missions.find(id);
    if(!mission||mission->envoy_actor_id!=actor)return fail("DiplomaticMissionNotKnown");
    if(mission->issuer_actor_id==actor)return original_.diplomatic_mission_for_acceptance(id,actor);
    const DiplomaticMissionKnowledge* known=nullptr;
    for(const auto& row:state_.mission_knowledge.rows()) if(row.second.mission_authority_fact_id==id&&row.second.envoy_actor_id==actor){known=&row.second;break;}
    if(!known)return fail("DiplomaticMissionNotKnown");
    if(read_.catalog.version!=2||read_.state.version!=2||catalog_.version!=2||state_.version!=2)return fail("DiplomaticDelegationVersionRequired");
    auto proof=valid_knowledge(DiplomaticHistoryRead{read_.common,read_.organizations,read_.organization_catalog,read_.catalog,read_.state},catalog_,*known);if(!proof.ok)return {false,{},proof.error};
    auto source=original_.diplomatic_mission_source(id);if(!source.ok)return source;
    if(read_.common.tick()>=mission->acceptance_expires_tick)return fail("DiplomaticMissionAcceptanceExpired");
    const auto* offer=read_.state.offers.find(mission->proposal_id);
    if(!offer||offer->status!=DiplomaticOfferStatus::awaiting_delivery)return fail("DiplomaticMissionUnavailable");
    auto issuer=revalidate_diplomatic_issuer(read_.common,read_.organizations,read_.organization_catalog,read_.catalog,*offer);
    if(!issuer.ok)return {false,{},issuer.error};
    ActingAuthority authority{mission->issuer_organization_id,mission->issuer_office_id,mission->issuer_authority_epoch,mission->issuer_actor_id};
    auto authorized=check_authority(read_.common,read_.organizations,read_.organization_catalog,authority,"diplomatic_commission_envoy",{ScopeRef::Kind::site,mission->report_site_id},0);
    if(!authorized.ok)return {false,{},authorized.error};
    auto location=read_.common.resolved_location(actor);const auto* at=location.ok?std::get_if<SettlementLocation>(&location.value):nullptr;
    if(!at||at->place_id!=mission->report_site_id)return fail("DiplomaticMissionAtWrongSite");
    if(mission->delivery_expires_tick-read_.common.tick()<=mission->delivery_work_ticks)return fail("DiplomaticDeliveryWindowTooShort");
    return source;
}
}
