// 精确认领凭据保留到原会话结束后；永不由今日岗位任职或一个格式正确的ID补造过去认领。
#include "visit_reception_claims.hpp"
#include "resident/domains/interaction/occupancy/actor_social_occupancy.hpp"
#include <charconv>
#include <set>
namespace hanmo::resident {namespace {
std::optional<Integer> ordinal(const StableId& id,const StableId& prefix,Integer next){const auto head=prefix+":";if(id.rfind(head,0)!=0)return {};auto text=id.substr(head.size());Integer n=0;auto parsed=std::from_chars(text.data(),text.data()+text.size(),n);if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()||n<=0||n>=next||std::to_string(n)!=text)return {};return n;}
const VisitMessagePolicy* policy(const VisitMessageCatalog& c,const StableId& id){for(const auto& p:c.policies)if(p.id==id)return &p;return nullptr;}
const DialogueDefinition* dialogue(const InteractionCatalog& c,const StableId& id){for(const auto& d:c.dialogues)if(d.id==id)return &d;return nullptr;}
bool matches(const VisitReceptionClaim& c,const InteractionSession& s){return c.session_id==s.id&&c.actor_id==s.actor_id&&c.provider_actor_id==s.context.provider_actor_id&&c.role_id==s.context.role_id&&c.place_id==s.context.place_id&&c.dialogue_id==s.dialogue_id&&!s.context.kind&&!s.context.encounter_id;}
Outcome<Integer> actual_tick(const CommonStore&c,const CurrentMomentRead*clock){if(!clock)return {true,c.tick(),{}};auto now=clock->read_current_moment();return now.ok?Outcome<Integer>{true,now.value.half_day_tick,{}}:Outcome<Integer>{false,0,now.error};}
Status opening(const CommonStore&c,const InteractionState&active,const NativeActorSocialOccupancyRead*reader,const StableId&actor,const StableId&session,Integer tick,const StableId*claim,const CurrentMomentRead*clock){
 if(!active.social_occupancy)return reader?common_failure("UnselectedVisitReceptionOccupancySources"):common_success();
 if(!reader||!clock)return common_failure("VisitReceptionActualOccupancyRequired",{},ErrorKind::unsupported_capability);
 if(!reader->bound_to(c,active))return common_failure("VisitReceptionOccupancyCandidateMismatch",{},ErrorKind::internal);
 const ActorSocialOccupancyTransition*start=nullptr;for(const auto&row:active.social_occupancy->transitions.rows())if(row.second.actor_id==actor&&row.second.original_context_id==session&&row.second.cause==ActorSocialOccupancyCause::dialogue_open){if(start)return common_failure("AmbiguousVisitReceptionTalkSource",session);start=&row.second;}
 if(!start||start->initiator_actor_id!=actor||start->before.interaction_id||start->after.interaction_id!=std::optional<StableId>{session}||start->occurred_at.half_day_tick!=tick)return common_failure("VisitReceptionActualTalkSourceRequired",session);
 auto actual=reader->read_source_moment({"interaction","actor_social_occupancy_changed",start->id},1);if(!actual.ok)return {false,{},actual.error};
 if(!claim){auto now=clock->read_current_moment();if(!now.ok)return {false,{},now.error};auto equal=compare_world_moments(actual.value,now.value);if(!equal.ok)return {false,{},equal.error};if(equal.value)return common_failure("VisitReceptionTalkAdoptionMomentMismatch",session);return common_success();}
 // The claim remains Calendar. Its true Common cut must see this same original
 // session, with no intervening close/replacement; no fractional claim time is invented.
 bool started=false,found=false;for(const auto&id:c.state().facts.order()){if(id==*claim){found=true;break;}if(id==start->id){started=true;continue;}if(started){const auto*t=active.social_occupancy->transitions.find(id);if(t&&t->actor_id==actor&&t->after.interaction_id!=std::optional<StableId>{session})return common_failure("VisitReceptionTalkClosedBeforeClaim",*claim);}}
 return started&&found?common_success():common_failure("VisitReceptionTalkSourceOrderMismatch",*claim);
}

}
const VisitReceptionClaim* read_visit_reception_claim(const VisitReceptionState& state,const StableId& session,const StableId& actor,const StableId& intent){const auto at=state.by_session.find(session);const auto* found=at==state.by_session.end()?nullptr:state.claims.find(at->second);return found&&found->session_id==session&&found->actor_id==actor&&found->intent_id==intent?found:nullptr;}
Status validate_visit_reception_claims(const CommonStore& common,const VisitReceptionState& state,const VisitReceptionCatalog& catalog,const VisitMessageCatalog& messages,const InteractionCatalog& interactions,const InteractionState& active,const CurrentMomentRead*clock,const NativeActorSocialOccupancyRead*occupancy){
    auto now=actual_tick(common,clock);if(!now.ok)return {false,{},now.error};if(active.social_occupancy&&(!clock||!occupancy))return common_failure("VisitReceptionActualOccupancyRequired",{},ErrorKind::unsupported_capability);
    if(state.version!=1||state.claims.size()>65536)return common_failure("InvalidVisitReceptionClaims");auto valid=validate_visit_reception_catalog(catalog,messages,interactions);if(!valid.ok)return valid;
    std::set<StableId> sessions;std::size_t count=0;Integer previous=0,previous_tick=-1;
    for(const auto& row:common.state().facts.rows()){
        const auto& h=row.second;if(h.kind!=visit_reception_claim_kind&&h.domain!=visit_reception_claim_module)continue;
        const auto* c=state.claims.find(row.first);const auto n=ordinal(row.first,"visit_reception_claim",common.state().next_instance_id);const auto sn=c?ordinal(c->session_id,"interaction",common.state().next_instance_id):std::optional<Integer>{};
        const auto* i=c?visit_reception_intent(catalog,c->intent_id):nullptr;const auto* p=i?policy(messages,i->message_policy_id):nullptr;const auto* role=p?interactions.common->roles.find(p->role_id):nullptr;const auto* d=p?dialogue(interactions,p->dialogue_id):nullptr;
        if(!c||h.id!=row.first||h.kind!=visit_reception_claim_kind||h.domain!=visit_reception_claim_module||c->fact_id!=row.first||!n||!sn||(active.social_occupancy?*n<=*sn:*n!=*sn+1)||*n<=previous||!sessions.insert(c->session_id).second||!i||!p||!role||!d||c->actor_id!=i->actor_id||!common.actor(c->actor_id)||!common.actor(c->provider_actor_id)||c->policy_id!=p->id||c->provider_actor_id!=p->custodian_actor_id||c->role_id!=p->role_id||c->dialogue_id!=p->dialogue_id||c->entry_node_id!=d->entry||c->place_id!=role->place_id||c->tick<0||c->tick>now.value||c->tick<previous_tick)return common_failure("InvalidVisitReceptionClaim",row.first);
        valid=opening(common,active,occupancy,c->actor_id,c->session_id,c->tick,&c->fact_id,clock);if(!valid.ok)return valid;
        for(const auto& s:active.interactions)if(s.id==c->session_id&&!matches(*c,s))return common_failure("VisitReceptionSessionSourceMismatch",row.first);
        previous=*n;previous_tick=c->tick;++count;
    }
    if(count!=state.claims.size())return common_failure("MissingVisitReceptionClaimFact");for(const auto& row:state.claims.rows())if(row.first!=row.second.fact_id)return common_failure("InvalidVisitReceptionClaimKey",row.first);
    decltype(state.by_session) expected;for(const auto&row:state.claims.rows())expected.emplace(row.second.session_id,row.first);if(expected!=state.by_session)return common_failure("VisitReceptionClaimIndexMismatch");return common_success();
}
Status record_visit_reception_claim_in_candidate(CommonStore& common,VisitReceptionState& state,const VisitReceptionCatalog& catalog,const VisitMessageCatalog& messages,const InteractionCatalog& interactions,const InteractionState& active,const StableId& actor,const StableId& intent,const CurrentMomentRead*clock,const NativeActorSocialOccupancyRead*occupancy){
    auto valid=validate_visit_reception_claims(common,state,catalog,messages,interactions,active,clock,occupancy);if(!valid.ok)return valid;if(state.claims.size()>=65536)return common_failure("VisitReceptionHistoryLimit");
    const auto* i=visit_reception_intent(catalog,intent);const auto* p=i?policy(messages,i->message_policy_id):nullptr;const auto* role=p?interactions.common->roles.find(p->role_id):nullptr;const auto* d=p?dialogue(interactions,p->dialogue_id):nullptr;const InteractionSession* s=nullptr;for(const auto& row:active.interactions)if(row.actor_id==actor){if(s)return common_failure("VisitReceptionTalkSourceMismatch");s=&row;}
    const auto sn=s?ordinal(s->id,"interaction",common.state().next_instance_id):std::optional<Integer>{};
    if(!i||i->actor_id!=actor||!p||!role||!d||!s||!sn||(!active.social_occupancy&&*sn!=common.state().next_instance_id-1)||s->context.provider_actor_id!=p->custodian_actor_id||s->context.role_id!=p->role_id||s->context.place_id!=role->place_id||s->context.kind||s->context.encounter_id||s->dialogue_id!=p->dialogue_id||s->node_id!=d->entry||state.by_session.count(s->id))return common_failure("VisitReceptionTalkSourceMismatch",intent,ErrorKind::internal);
    auto now=actual_tick(common,clock);if(!now.ok)return {false,{},now.error};valid=opening(common,active,occupancy,actor,s->id,now.value,nullptr,clock);if(!valid.ok)return valid;
    auto id=common.next_id("visit_reception_claim");if(!id.ok)return {false,{},id.error};valid=common.append_fact({id.value,visit_reception_claim_kind,visit_reception_claim_module});if(!valid.ok)return valid;
    VisitReceptionClaim claim{id.value,intent,p->id,s->id,actor,p->custodian_actor_id,p->role_id,p->dialogue_id,d->entry,role->place_id,now.value};if(!state.claims.insert(id.value,std::move(claim)))return common_failure("DuplicateVisitReceptionClaim",id.value,ErrorKind::internal);rebuild_visit_reception_indexes(state);return common_success();
}
}
