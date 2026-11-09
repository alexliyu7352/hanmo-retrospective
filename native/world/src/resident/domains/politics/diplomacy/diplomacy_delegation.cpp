// 发令者当面说明后才登记指定本人知情；不替对方接受，也不支付未配置的旅费。
#include "diplomacy_delegation.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
template<class T> Outcome<T> denied(const StableId& code) { return {false,{},common_failure(code).error}; }
const Grant* commissioning_grant(const DiplomaticPresentRead& r,const OfficeState& office,const StableId& site) {
    for (const auto& id:office.grant_ids) {
        const auto* g=r.organization_catalog.grants.find(id);
        if (!g || g->office_id!=office.id || g->amount_limit<0 || g->valid_from>r.common.tick() || (g->valid_until && *g->valid_until<r.common.tick()) ||
            std::find(g->allowed_command_kinds.begin(),g->allowed_command_kinds.end(),"diplomatic_commission_envoy")==g->allowed_command_kinds.end() ||
            std::none_of(g->scopes.begin(),g->scopes.end(),[&](const auto& x){return x.kind==ScopeRef::Kind::site&&x.id==site;})) continue;
        bool valid=true;
        for(const auto& id:g->wallet_ids){const auto* x=r.common.wallet(id);if(!x||x->owner.kind!=Owner::Kind::organization||x->owner.id!=office.organization_id)valid=false;}
        for(const auto& id:g->inventory_ids){const auto* x=r.common.inventory(id);if(!x||x->owner.kind!=Owner::Kind::organization||x->owner.id!=office.organization_id)valid=false;}
        if(valid)return g;
    }
    return nullptr;
}
}
Outcome<DiplomaticDelegateQuote> preview_diplomatic_delegation(const DiplomaticPresentRead& r,const CommonCatalog& common_catalog,
    const ActivityRead& activity,const DiplomacyV2Catalog& catalog,const DiplomacyV2State& state,const StableId& issuer,
    const DiplomaticDelegateCommand& command) {
    if(r.catalog.version!=2||r.state.version!=2||catalog.version!=2||state.version!=2) return denied<DiplomaticDelegateQuote>("DiplomaticDelegationVersionRequired");
    if(command.expected_world_revision!=r.common.revision()) return denied<DiplomaticDelegateQuote>("DiplomaticDelegationRevisionMismatch");
    const auto* policy=catalog.delegation_policies.find(command.delegation_policy_id);
    const auto* office=policy?r.organizations.offices.find(policy->issuer_office_id):nullptr;
    if(!policy||!office||!office->holder_actor_id||*office->holder_actor_id!=issuer||policy->envoy_actor_id==issuer)
        return denied<DiplomaticDelegateQuote>("DiplomaticDelegationNotAvailable");
    for(const auto& row:state.mission_knowledge.rows()) if(row.second.delegation_policy_id==policy->id)
        return denied<DiplomaticDelegateQuote>("DiplomaticDelegationAlreadyIssued");
    const auto* mission=r.catalog.mission_policies.find(policy->mission_policy_id);
    if(!mission||!mission->task_terms||!mission->task_terms->return_report_required||mission->task_terms->report_work_ticks<=0)
        return denied<DiplomaticDelegateQuote>("DiplomaticMissionTermsMissing");
    const auto* envoy=r.common.actor(policy->envoy_actor_id);
    if(!envoy||!envoy->alive||!envoy->available||!envoy->action_id.empty()||activity.in_open_battle(envoy->id))
        return denied<DiplomaticDelegateQuote>("DiplomaticEnvoyUnavailable");
    const ActingAuthority authority{office->organization_id,office->id,office->epoch,issuer};
    auto proposal=preview_diplomatic_proposal(r.common,common_catalog,r.organizations,r.organization_catalog,r.catalog,activity,issuer,
        {authority,policy->mission_policy_id,policy->terms_id});
    if(!proposal.ok)return {false,{},proposal.error};
    auto authorized=check_authority(r.common,r.organizations,r.organization_catalog,authority,"diplomatic_commission_envoy",{ScopeRef::Kind::site,mission->issuer_site_id},0);
    if(!authorized.ok)return {false,{},authorized.error};
    const auto* grant=commissioning_grant(r,*office,mission->issuer_site_id);
    if(!grant)return denied<DiplomaticDelegateQuote>("DiplomaticMissionUnauthorized");
    auto contact=preview_face_to_face_contact(r.common,r.interaction,r.interaction_catalog,r.interaction_access,issuer,policy->envoy_actor_id);
    if(!contact.ok)return {false,{},contact.error};
    if(contact.value.context.place_id!=mission->issuer_site_id)return denied<DiplomaticDelegateQuote>("DiplomaticMissionAtWrongSite");
    const auto& t=*mission->task_terms;
    if(t.acceptance_valid_ticks<=0||t.delivery_work_ticks<=0||t.report_work_ticks<=0||t.continuity_policy_id!="continue_under_organization"||
        r.common.tick()>INT64_MAX-t.acceptance_valid_ticks||proposal.value.proposal.expires_tick-r.common.tick()<=t.delivery_work_ticks)
        return denied<DiplomaticDelegateQuote>("InvalidDiplomaticTaskTerms");
    DiplomaticDelegateQuote out;out.source_identity=r.common.state().identity;out.proposal=std::move(proposal.value.proposal);
    out.proposal.envoy_actor_id=policy->envoy_actor_id;out.contact=std::move(contact.value);
    auto& a=out.mission;a.policy_id=mission->id;a.rule_name_key=mission->name_key;a.terms_revision=out.proposal.terms_revision;
    a.issuer_organization_id=mission->issuer_organization_id;a.target_organization_id=mission->target_organization_id;
    a.envoy_actor_id=policy->envoy_actor_id;a.issuer_office_id=office->id;a.issuer_actor_id=issuer;a.issuer_authority_epoch=office->epoch;a.issuer_grant_id=grant->id;
    a.target_office_id=mission->target_office_id;a.delivery_site_id=mission->target_site_id;a.report_office_id=office->id;a.report_site_id=mission->issuer_site_id;
    a.continuity_policy_id=t.continuity_policy_id;a.issued_tick=r.common.tick();a.delivery_expires_tick=out.proposal.expires_tick;
    a.acceptance_expires_tick=std::min(a.issued_tick+t.acceptance_valid_ticks,a.delivery_expires_tick);
    if(grant->valid_until&&*grant->valid_until<INT64_MAX)a.acceptance_expires_tick=std::min(a.acceptance_expires_tick,*grant->valid_until+1);
    a.delivery_work_ticks=t.delivery_work_ticks;a.report_work_ticks=t.report_work_ticks;a.return_report_required=true;
    return {true,std::move(out),{}};
}
Outcome<StableId> delegate_diplomatic_mission_in_transaction(CommonStore& common,DiplomacyState& diplomacy,DiplomacyV2State& state,
    const DiplomaticPresentRead& read,const CommonCatalog& common_catalog,const ActivityRead& activity,const DiplomacyV2Catalog& catalog,
    const StableId& issuer,const DiplomaticDelegateCommand& command) {
    if(&common!=&read.common||&diplomacy!=&read.state)return denied<StableId>("DiplomaticDelegationCandidateMismatch");
    auto quote=preview_diplomatic_delegation(read,common_catalog,activity,catalog,state,issuer,command);
    if(!quote.ok)return {false,{},quote.error};
    auto offer_id=common.next_id("diplomatic_offer");if(!offer_id.ok)return offer_id;
    auto mission_id=common.next_id("diplomatic_mission");if(!mission_id.ok)return mission_id;
    auto known_id=common.next_id("diplomatic_mission_known");if(!known_id.ok)return known_id;
    auto offer=std::move(quote.value.proposal);offer.id=offer_id.value;offer.last_fact_id=offer.id;offer.mission_authority_fact_id=mission_id.value;
    auto mission=std::move(quote.value.mission);mission.mission_authority_fact_id=mission_id.value;mission.proposal_id=offer.id;mission.proposal_fact_id=offer.id;
    DiplomaticOfferFact fact;fact.id=offer.id;fact.offer_id=offer.id;fact.kind=diplomatic_proposal_fact_kind;fact.actor_id=issuer;fact.terms_id=offer.terms_id;
    fact.tick=common.tick();fact.witness_actor_ids={issuer,offer.envoy_actor_id};
    const auto& proof=quote.value.contact;DiplomaticMissionKnowledge knowledge;
    knowledge.id=known_id.value;knowledge.delegation_policy_id=command.delegation_policy_id;knowledge.mission_authority_fact_id=mission_id.value;
    knowledge.envoy_actor_id=offer.envoy_actor_id;knowledge.issuer_actor_id=issuer;knowledge.session_actor_id=proof.session_actor_id;
    knowledge.session_id=proof.session_id;knowledge.site_id=proof.context.place_id;knowledge.role_id=proof.context.role_id;
    knowledge.provider_actor_id=proof.context.provider_actor_id;knowledge.context_kind=proof.context.kind;knowledge.tick=common.tick();
    for(const auto& h:std::vector<FactDraft>{{offer_id.value,diplomatic_proposal_fact_kind,diplomatic_fact_domain},
        {mission_id.value,diplomatic_mission_authority_fact_kind,diplomatic_fact_domain},{known_id.value,diplomatic_mission_known_fact_kind,diplomatic_fact_domain}}) {
        auto added=common.append_fact(h);if(!added.ok)return {false,{},added.error};
    }
    if(!diplomacy.offers.insert(offer_id.value,std::move(offer))||!diplomacy.facts.insert(offer_id.value,std::move(fact))||
        !diplomacy.missions.insert(mission_id.value,std::move(mission))||!state.mission_knowledge.insert(known_id.value,std::move(knowledge)))
        return denied<StableId>("DuplicateDiplomaticDelegation");
    return mission_id;
}
}
