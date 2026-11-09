// 未送达的提议不因全局政治账存在而出现在对方页面；每人只读本人的已知ID。
#include "diplomacy_views.hpp"
#include <algorithm>
#include <tuple>
namespace hanmo::resident {
namespace {
bool same_identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
bool witnessed(const DiplomaticOfferFact& f,const StableId& actor){return std::find(f.witness_actor_ids.begin(),f.witness_actor_ids.end(),actor)!=f.witness_actor_ids.end();}
StableId known_status(const DiplomacyState& state,const DiplomaticOfferFact& fact){
    if(fact.kind==diplomatic_proposal_fact_kind)return "awaiting_delivery";
    if(fact.kind==diplomatic_presentation_fact_kind)return "presented";
    if(fact.kind=="diplomatic_offer_replied"){const auto* reply=state.replies.find(fact.id);if(reply)return reply->decision==DiplomaticReplyKind::accept?"accepted_pending_ratification":"refused";}
    return {};
}
}
// 汇总时保每位见证人的最后已知fact，而非提议今天的全知最后状态。
void rebuild_diplomacy_view_index(const DiplomacyPageRead& read,DiplomacyViewIndex& out){
    const auto& common=read.common;const auto& state=read.state;
    out={};out.identity=common.state().identity;out.fact_count=state.facts.size();out.offer_count=state.offers.size();out.mission_count=state.missions.size();
    std::map<StableId,std::map<StableId,const DiplomaticOfferFact*>> known;
    for(const auto& row:state.facts.rows()){const auto& f=row.second;if(f.tick>common.tick())continue;for(const auto& observer:f.witness_actor_ids){auto& last=known[observer][f.offer_id];if(!last||std::tie(last->offer_revision,last->tick,last->id)<std::tie(f.offer_revision,f.tick,f.id))last=&f;}}
    for(const auto& person:known){auto& entries=out.by_observer[person.first];for(const auto& offer:person.second)entries.push_back({offer.first,offer.second->id});
        std::sort(entries.begin(),entries.end(),[&](const auto& a,const auto& b){const auto* fa=state.facts.find(a.fact_id);const auto* fb=state.facts.find(b.fact_id);const auto* oa=state.offers.find(a.offer_id);const auto* ob=state.offers.find(b.offer_id);const bool live_a=oa&&oa->expires_tick>common.tick()&&known_status(state,*fa)!="refused";const bool live_b=ob&&ob->expires_tick>common.tick()&&known_status(state,*fb)!="refused";if(live_a!=live_b)return live_a>live_b;return std::tie(fa->tick,fa->id)>std::tie(fb->tick,fb->id);});
    }
    // 仅实际本人持有且获提议专项授权的Office产生可选作者条款；不枚举远方人物。
    std::map<StableId,std::vector<const DiplomaticMissionPolicy*>> by_issuer;for(const auto& row:read.catalog.mission_policies.rows())by_issuer[row.second.issuer_organization_id].push_back(&row.second);
    for(const auto& row:read.organizations.offices.rows()){const auto& office=row.second;if(!office.holder_actor_id)continue;auto candidates=by_issuer.find(office.organization_id);if(candidates==by_issuer.end())continue;ActingAuthority authority{office.organization_id,office.id,office.epoch,*office.holder_actor_id};for(const auto* policy_ptr:candidates->second){const auto& policy=*policy_ptr;auto permitted=check_authority(common,read.organizations,read.organization_catalog,authority,"diplomatic_propose_passage",{ScopeRef::Kind::site,policy.issuer_site_id},0);if(!permitted.ok)continue;for(const auto& terms:policy.passage_terms_ids)out.proposal_choices[*office.holder_actor_id].push_back({authority,policy.id,terms});}}
    out.initialized=true;
}
// 当前本人场所是读屏障；无关事实再多也不会占用此人的256项上限。
Outcome<DiplomacyPageView> build_diplomacy_page_view(const DiplomacyPageRead& read,const DiplomacyViewIndex& index,const StableId& observer,const StableId& place){
    const auto& common=read.common;const auto& catalog=read.catalog;const auto& state=read.state;
    const auto fail=[](const StableId& code)->Outcome<DiplomacyPageView>{return {false,{},common_failure(code).error};};
    const auto* actor=common.actor(observer);const auto* location=actor?std::get_if<SettlementLocation>(&actor->location):nullptr;if(!location||location->place_id!=place)return fail("DiplomacyPageAtWrongPlace");
    if(!index.initialized||!same_identity(index.identity,common.state().identity)||index.fact_count!=state.facts.size()||index.offer_count!=state.offers.size()||index.mission_count!=state.missions.size())return fail("StaleDiplomacyViewIndex");
    DiplomacyPageView result;result.source_identity=common.state().identity;result.observer_actor_id=observer;result.place_id=place;auto choices=index.proposal_choices.find(observer);if(choices!=index.proposal_choices.end()){result.truncated=choices->second.size()>256;for(std::size_t i=0;i<std::min<std::size_t>(256,choices->second.size());++i){const auto& choice=choices->second[i];const auto* policy=catalog.mission_policies.find(choice.policy_id);const auto* terms=catalog.passage_terms.find(choice.terms_id);if(!policy||!terms||choice.authority.actor_id!=observer)return fail("InvalidDiplomaticViewSource");auto quote=preview_diplomatic_proposal(common,read.common_catalog,read.organizations,read.organization_catalog,catalog,read.activity,observer,{choice.authority,choice.policy_id,choice.terms_id});result.proposal_options.push_back({choice,policy->name_key,terms->name_key,quote.ok?StableId{}:quote.error.original.code,quote.ok});}}
    auto entries=index.by_observer.find(observer);if(entries==index.by_observer.end())return {true,std::move(result),{}};result.truncated=result.truncated||entries->second.size()>256;
    for(std::size_t i=0;i<std::min<std::size_t>(256,entries->second.size());++i){const auto& known=entries->second[i];const auto* fact=state.facts.find(known.fact_id);const auto* offer=state.offers.find(known.offer_id);const auto* policy=offer?catalog.mission_policies.find(offer->policy_id):nullptr;const auto* terms=fact?catalog.passage_terms.find(fact->terms_id):nullptr;if(!fact||!offer||!policy||!terms||fact->offer_id!=offer->id||!witnessed(*fact,observer)||fact->tick>common.tick())return fail("InvalidDiplomaticViewSource");
        DiplomaticOfferView row;row.offer_id=offer->id;row.source_fact_id=fact->id;row.policy_id=policy->id;row.policy_name_key=policy->name_key;row.terms_id=terms->id;row.terms_name_key=terms->name_key;row.issuer_organization_id=policy->issuer_organization_id;row.target_organization_id=policy->target_organization_id;row.envoy_actor_id=offer->envoy_actor_id;row.known_status=known_status(state,*fact);if(row.known_status.empty())return fail("InvalidDiplomaticViewSource");if(common.tick()>=offer->expires_tick&&row.known_status!="refused")row.known_status="expired";row.terms_revision=fact->terms_revision;row.offer_revision=fact->offer_revision;row.issued_tick=offer->issued_tick;row.expires_tick=offer->expires_tick;row.known_tick=fact->tick;if(offer->envoy_actor_id==observer)row.mission_authority_fact_id=offer->mission_authority_fact_id;result.offers.push_back(std::move(row));
    }return {true,std::move(result),{}};
}
}
