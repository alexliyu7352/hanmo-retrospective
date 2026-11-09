#pragma once
#include "organization_model.hpp"
#include "resident/model/common_model.hpp"
#include "resident/storage/common_store.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include <algorithm>
#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>

namespace hanmo::resident {
enum class OrganizationMemberPrefixState {
    unresolved, no_retained_episode, no_active_episode, active_episode
};
struct OrganizationMemberBeforePrefix {
    OrganizationMemberPrefixState state=OrganizationMemberPrefixState::unresolved;
    const OrgMemberState* episode_identity=nullptr;
    std::optional<MemberResponse> active_response;
};
namespace organization_detail {
inline Status validate_organization_member_identity(const StableId& key,
    const OrgMemberState& member,const OrganizationCatalog& catalog){
    const auto* initial=catalog.members.find(key);
    if(key!=member.id||(!initial&&(!catalog.named_membership_enabled||!member.episode_source))||
       (initial&&(initial->id!=key||initial->episode_source||member.episode_source))||
       (initial&&catalog.named_membership_enabled&&
        (member.policy_id!=initial->policy_id||member.duty_ids!=initial->duty_ids))||
       (initial&&(member.actor_id!=initial->actor_id||
                  member.organization_id!=initial->organization_id)))
        return common_failure("InvalidOrganizationMember",key);
    return common_success();
}

// The original named validator's source-fact match, shared without changing
// its current-only withdrawal actor check into historical body authority.
inline bool organization_membership_source_fact_matches(const CommonStore& c,
    const OrganizationState& org,const StableId& id,std::string_view kind,
    const MembershipOffer& offer,Integer tick){
    const auto* fact=org.facts.find(id);
    const auto* header=c.state().facts.find(id);
    return fact&&header&&fact->id==id&&header->id==id&&
        header->domain=="organization"&&std::string_view(header->kind)==kind&&
        std::string_view(fact->kind)==kind&&fact->organization_id==offer.organization_id&&
        (kind=="membership_withdrawn"?c.actor(fact->actor_id)!=nullptr:
         fact->actor_id==offer.target_actor_id)&&
        fact->office_id==offer.office_id&&fact->activity_id==offer.id&&fact->tick==tick;
}

struct OrganizationNamedMemberOrigin {
    const OrganizationFact* joined_parent=nullptr;
    const FactRecord* offered_head=nullptr;
    const FactRecord* accepted_head=nullptr;
    const FactRecord* joined_head=nullptr;
};
inline Outcome<OrganizationNamedMemberOrigin> read_organization_named_member_origin(
    const CommonStore& c,const OrganizationState& org,const OrganizationCatalog& catalog,
    const StableId& key,const OrgMemberState& member){
    auto identity=validate_organization_member_identity(key,member,catalog);
    if(!identity.ok)return {false,{},identity.error};
    if(!catalog.named_membership_enabled||catalog.members.find(key)||!member.episode_source)
        return {false,{},common_failure("InvalidMemberEpisodeSource",key).error};
    const auto& source=*member.episode_source;
    const auto* offer=org.membership_offers.find(source.offer_id);
    const auto* decision=org.membership_decisions.find(source.decision_id);
    const auto* terms=catalog.membership_terms.find(source.terms_id);
    const auto* joined=org.facts.find(source.joined_fact_id);
    if(!offer||!decision||!terms||!joined||offer->id!=source.offer_id||
       decision->id!=source.decision_id||terms->id!=source.terms_id||
       joined->id!=source.joined_fact_id||offer->status!=MembershipOfferStatus::accepted||
       offer->member_episode_id!=member.id||source.decision_id!=offer->decision_fact_id||
       source.terms_id!=offer->terms_id||source.joined_tick!=decision->tick||
       decision->kind!=MembershipDecisionKind::accept||decision->offer_id!=offer->id||
       decision->target_actor_id!=offer->target_actor_id||
       terms->organization_id!=offer->organization_id||
       member.actor_id!=offer->target_actor_id||member.organization_id!=offer->organization_id||
       member.policy_id!=terms->member_policy_id||member.duty_ids.size()!=1||
       source.wage_binding_id.empty()||joined->kind!="organization_member_response"||
       joined->actor_id!=member.actor_id||joined->organization_id!=member.organization_id||
       joined->response!=MemberResponse::serving||joined->tick!=source.joined_tick||
       (joined->cause_fact_ids.size()!=1||joined->cause_fact_ids.front()!=decision->id))
        return {false,{},common_failure("InvalidMemberEpisodeSource",key).error};
    if(!organization_membership_source_fact_matches(c,org,offer->source_fact_id,
           "membership_offered",*offer,offer->created_tick)||
       !organization_membership_source_fact_matches(c,org,decision->id,
           "membership_decided",*offer,decision->tick))
        return {false,{},common_failure("InvalidMemberEpisodeSource",key).error};
    const auto* acceptance=org.facts.find(decision->id);
    if(!acceptance||acceptance->code!="accept"||
       std::find(decision->source_fact_ids.begin(),decision->source_fact_ids.end(),offer->source_fact_id)==decision->source_fact_ids.end()||
       std::find(acceptance->cause_fact_ids.begin(),acceptance->cause_fact_ids.end(),offer->source_fact_id)==acceptance->cause_fact_ids.end())
        return {false,{},common_failure("InvalidMembershipDecision",decision->id).error};
    const auto* offered=c.state().facts.find(offer->source_fact_id);
    const auto* accepted=c.state().facts.find(decision->id);
    const auto* joined_head=c.state().facts.find(joined->id);
    if(!offered||offered->id!=offer->source_fact_id||!accepted||accepted->id!=decision->id||
       !joined_head||joined_head->id!=joined->id||joined_head->domain!="organization"||
       joined_head->kind!="organization_member_response")
        return {false,{},common_failure("MembershipJoinedCommonSourceRequired",key).error};
    if(catalog.service_effects_enabled&&
       (!joined->service_effects||!joined->service_effects->changes.empty()))
        return {false,{},common_failure("MembershipJoinServiceEffectsMismatch",key).error};
    return {true,{joined,offered,accepted,joined_head},{}};
}

inline bool valid_organization_member_prefix_response(MemberResponse response){
    switch(response){
    case MemberResponse::serving:
    case MemberResponse::payment_requested:
    case MemberResponse::refusing_new_duties:
    case MemberResponse::awaiting_release:
    case MemberResponse::departed:return true;
    }
    return false;
}
// Transient, one-subject projection. The caller feeds this with its one
// authenticated Common stream and exposes result() only after that whole read
// succeeds. Uninitialized means no subject was requested, never proven absence.
class OrganizationMemberPrefixFold {
    struct Episode {
        const OrgMemberState* member=nullptr;
        OrganizationNamedMemberOrigin origin;
        MemberResponse response=MemberResponse::serving;
        bool present=false,offered_seen=false,accepted_seen=false;
    };
    const StableId* actor_=nullptr;
    const StableId* organization_=nullptr;
    CheckpointVector<Episode> episodes_;
    std::size_t seed_count_=0,next_join_=0,retained_count_=0;
    std::optional<std::size_t> active_;
    bool initialized_=false;
public:
    OrganizationMemberPrefixFold()=default;
    OrganizationMemberPrefixFold(const OrganizationMemberPrefixFold&)=delete;
    OrganizationMemberPrefixFold& operator=(const OrganizationMemberPrefixFold&)=delete;
    OrganizationMemberPrefixFold(OrganizationMemberPrefixFold&&)=default;
    OrganizationMemberPrefixFold& operator=(OrganizationMemberPrefixFold&&)=default;

    Status initialize(const CommonStore& c,const OrganizationState& state,
        const OrganizationCatalog& catalog,const StableId& actor,const StableId& organization){
        if(initialized_||!episodes_.empty())
            return common_failure("OrganizationMemberPrefixAlreadyInitialized",actor);
        if(actor.empty()||organization.empty())
            return common_failure("OrganizationMemberPrefixSubjectRequired",actor);
        actor_=&actor;organization_=&organization;
        // Initial response comes only from the authored seed, never today's row.
        for(const auto& row:catalog.members.rows()){
            const auto& initial=row.second;
            if(initial.actor_id!=actor||initial.organization_id!=organization)continue;
            const auto* current=state.members.find(row.first);
            if(!current)return common_failure("MissingSeedOrganizationMember",row.first);
            auto identity=validate_organization_member_identity(row.first,*current,catalog);
            if(!identity.ok)return identity;
            if(!valid_organization_member_prefix_response(initial.response))
                return common_failure("InvalidOrganizationMember",row.first);
            Episode episode;episode.member=current;episode.response=initial.response;episode.present=true;
            episodes_.push_back(episode);++retained_count_;
            if(initial.response!=MemberResponse::departed){
                if(active_)return common_failure("DuplicateActiveMembership",row.first);
                active_=episodes_.size()-1;
            }
        }
        seed_count_=episodes_.size();next_join_=seed_count_;
        if(!catalog.named_membership_enabled&&seed_count_>1)
            return common_failure("InvalidOrganizationMember",actor);
        std::size_t seed_seen=0;
        bool dynamic_seen=false;
        for(const auto& row:state.members.rows()){
            const auto& member=row.second;
            if(member.actor_id!=actor||member.organization_id!=organization)continue;
            auto identity=validate_organization_member_identity(row.first,member,catalog);
            if(!identity.ok)return identity;
            if(catalog.members.find(row.first)){
                if(dynamic_seen||seed_seen>=seed_count_||episodes_[seed_seen].member!=&member)
                    return common_failure("OrganizationMemberOriginOrderMismatch",row.first);
                ++seed_seen;
            }else{
                dynamic_seen=true;
                auto origin=read_organization_named_member_origin(c,state,catalog,row.first,member);
                if(!origin.ok)return {false,{},origin.error};
                Episode episode;episode.member=&member;episode.origin=origin.value;
                episodes_.push_back(episode);
            }
        }
        if(seed_seen!=seed_count_)
            return common_failure("OrganizationMemberOriginOrderMismatch",actor);
        // Reverse accepted-offer coverage is needed for an absence result.
        // The complete original validator still owns its decision/grant chain.
        for(const auto& row:state.membership_offers.rows()){
            const auto& offer=row.second;
            if(offer.target_actor_id!=actor||offer.organization_id!=organization||
               offer.status!=MembershipOfferStatus::accepted)continue;
            const auto* member=state.members.find(offer.member_episode_id);
            if(row.first!=offer.id||!member||member->actor_id!=actor||
               member->organization_id!=organization||!member->episode_source||
               member->episode_source->offer_id!=row.first)
                return common_failure("MissingAcceptedMemberEpisode",offer.id);
        }
        initialized_=true;
        return common_success();
    }

    // Pointer matches are against real retained Common heads validated above.
    // This is called for each head in the existing stream, not by a new scan.
    Status observe_source_head(const FactRecord& head){
        if(!initialized_)return common_success();
        for(std::size_t n=seed_count_;n<episodes_.size();++n){
            auto& episode=episodes_[n];
            if(episode.origin.offered_head==&head){
                if(episode.offered_seen)
                    return common_failure("MembershipOfferPrefixRepeated",head.id);
                episode.offered_seen=true;
            }
            if(episode.origin.accepted_head==&head){
                if(!episode.offered_seen||episode.accepted_seen)
                    return common_failure("MembershipAcceptancePrefixOrderMismatch",head.id);
                episode.accepted_seen=true;
            }
        }
        return common_success();
    }

    // Called only with the original parent already selected by the existing
    // Organization branch. Sync and Office parents do not change response.
    Status apply_parent(const FactRecord& head,const OrganizationFact& parent){
        if(!initialized_||parent.kind!="organization_member_response"||
           parent.actor_id!=*actor_||parent.organization_id!=*organization_)
            return common_success();
        if(!valid_organization_member_prefix_response(parent.response))
            return common_failure("InvalidMemberResponseFact",parent.id);
        for(std::size_t n=seed_count_;n<episodes_.size();++n){
            auto& episode=episodes_[n];
            if(episode.origin.joined_head!=&head)continue;
            if(episode.origin.joined_parent!=&parent||episode.present||n!=next_join_||
               !episode.offered_seen||!episode.accepted_seen)
                return common_failure("MembershipJoinPrefixOrderMismatch",parent.id);
            // This is exactly the real join preview's nondeparted-row rule.
            if(active_)return common_failure("MembershipAlreadyActive",*actor_);
            episode.present=true;episode.response=MemberResponse::serving;
            active_=n;++retained_count_;++next_join_;
            return common_success();
        }
        // The authored seed invariant plus real join rule preserve one active
        // recipient. This covers both command selectors and both automatic
        // producers; departed episodes cannot receive a later response.
        if(!active_)
            return common_failure("MemberResponsePrefixRecipientMissing",parent.id);
        auto& episode=episodes_[*active_];
        episode.response=parent.response;
        if(parent.response==MemberResponse::departed)active_.reset();
        return common_success();
    }

    OrganizationMemberBeforePrefix result()const{
        if(!initialized_)return {};
        if(!retained_count_)
            return {OrganizationMemberPrefixState::no_retained_episode,nullptr,{}};
        if(!active_)
            return {OrganizationMemberPrefixState::no_active_episode,nullptr,{}};
        const auto& episode=episodes_[*active_];
        return {OrganizationMemberPrefixState::active_episode,episode.member,episode.response};
    }
};
} // namespace organization_detail
} // namespace hanmo::resident
