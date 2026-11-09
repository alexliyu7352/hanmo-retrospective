// 原出生和逐次照护沿实际Common序重读；未来照护、结清和死亡不会倒改已冻结的接手前责任。
#include "child_care_origin.hpp"
#include "family_child_care_completion.hpp"
#include <algorithm>
#include <charconv>
#include <set>
namespace hanmo::resident {namespace {
bool boundary(const CommonStore& c,const StableId& id){const StableId prefix="family_care_relief:";if(id.rfind(prefix,0)!=0)return false;const auto raw=id.substr(prefix.size());Integer n=0;const auto parsed=std::from_chars(raw.data(),raw.data()+raw.size(),n);const auto* h=c.state().facts.find(id);return parsed.ec==std::errc{}&&parsed.ptr==raw.data()+raw.size()&&n>0&&n<c.state().next_instance_id&&raw==std::to_string(n)&&h&&h->id==id&&h->kind=="family_care_relief_accepted"&&h->domain=="family_care_relief";}
Outcome<ChildCareOriginRead> read(const CommonStore& c,const FamilyBirthState& state,const FamilyDeathState& life,const StableId& child,Integer tick,const StableId& before,bool historical){
    auto bad=[&](const StableId& code)->Outcome<ChildCareOriginRead>{return {false,{},common_failure(code,child).error};};
    const auto* actor=c.state().actors.find(child);if(!actor||tick<0||tick>c.tick())return bad("ChildCareOriginActorRequired");
    const auto& order=c.state().facts.order();const auto limit=historical?std::find(order.begin(),order.end(),before):order.end();if(historical&&(!boundary(c,before)||limit==order.end()))return bad("ChildCareReliefBoundaryRequired");
    const FamilyBirthRecord* born=nullptr;StableId birth;
    for(const auto& row:state.births.rows())if(row.second.actor_id==child){if(born)return bad("ChildCareOriginDuplicateBirth");born=&row.second;birth=row.first;}
    if(!born)return bad("ChildCareOriginBirthRequired");const auto* accepted=state.replies.find(born->accepted_id);const auto at=std::find(order.begin(),order.end(),birth),created=std::find(order.begin(),order.end(),child),approved=std::find(order.begin(),order.end(),born->accepted_id);
    const auto* bh=c.state().facts.find(birth);const auto* ch=c.state().facts.find(child);const auto* ah=c.state().facts.find(born->accepted_id);
    if(born->kind!="family_birth_recorded"||!accepted||!accepted->confirmation||accepted->kind!="family_birth_accepted"||accepted->decision!="accepted"||!bh||bh->id!=birth||bh->kind!="family_birth_recorded"||bh->domain!="family_birth"||!ch||ch->id!=child||ch->kind!="actor_creation"||ch->domain!="family_birth"||!ah||ah->id!=born->accepted_id||ah->kind!=accepted->kind||ah->domain!="family_birth"||approved>=at||at>=created||created>=limit||born->tick>tick||accepted->tick>born->tick||actor->birth_tick!=std::optional<Integer>{born->tick}||actor->created_tick!=std::optional<Integer>{born->tick})return bad("ChildCareOriginSourceMismatch");
    const auto& source=*accepted->confirmation;const auto& terms=source.terms;
    if(terms.food_per_period<1||terms.food_per_period>10000||terms.prepaid_care_periods<1||terms.prepaid_care_periods>360||accepted->reserved_food!=terms.food_per_period*terms.prepaid_care_periods||born->biological_parent_ids!=std::vector<StableId>{source.speaker_actor_id,source.listener_actor_id})return bad("ChildCareOriginTermsMismatch");
    ChildCareOriginRead out;out.source_identity=c.state().identity;out.child_actor_id=child;out.birth_fact_id=birth;out.accepted_fact_id=born->accepted_id;out.original_caregiver_actor_id=terms.caregiver_actor_id;out.original_payer_actor_id=terms.payer_actor_id;out.place_id=terms.place_id;out.source_inventory_id=terms.source_inventory_id;out.food_item_id=terms.food_item_id;out.biological_parent_ids=born->biological_parent_ids;out.before_fact_id=before;out.observed_tick=tick;out.birth_tick=born->tick;out.prepaid_periods=terms.prepaid_care_periods;out.food_per_period=terms.food_per_period;out.remaining_reserved_food=accepted->reserved_food;
    std::set<StableId> pending;StableId last_success;
    for(auto pos=created;pos!=limit;++pos){
        if(const auto* start=state.care_starts.find(*pos))if(start->child_actor_id==child){const auto* h=c.state().facts.find(*pos);if(!h||h->id!=*pos||h->kind!="family_child_care_started"||h->domain!="family_birth"||start->action_id!=*pos||start->accepted_id!=out.accepted_fact_id||start->caregiver_actor_id!=terms.caregiver_actor_id||start->tick>tick||start->period_index!=out.completed_periods+1||!pending.empty()||out.original_schedule_closed)return bad("ChildCareOriginStartMismatch");pending.insert(*pos);}
        if(const auto* done=state.care.find(*pos))if(done->child_actor_id==child){if(done->tick>tick||!pending.erase(done->action_id)||done->accepted_id!=out.accepted_fact_id||done->period_index!=out.completed_periods+1||out.original_schedule_closed)return bad("ChildCareOriginCompletionMismatch");auto proof=read_child_care_completion(c,state,terms.caregiver_actor_id,done->action_id);if(!proof.ok)return {false,{},proof.error};if(!proof.value||proof.value->fact_id!=*pos||proof.value->completed_tick!=done->tick)return bad("ChildCareOriginCompletionMismatch");out.last_care_fact_id=*pos;out.last_care_action_id=done->action_id;out.last_care_code=proof.value->code;if(proof.value->success){if(++out.completed_periods>out.prepaid_periods)return bad("ChildCareOriginCompletionOverflow");out.remaining_reserved_food-=terms.food_per_period;last_success=*pos;}}
        if(const auto* close=state.closures.find(*pos))if(close->accepted_id==out.accepted_fact_id){const auto* h=c.state().facts.find(*pos);if(!h||h->id!=*pos||h->kind!="family_birth_closed"||h->domain!="family_birth"||close->tick>tick||close->reason!="prepaid_care_completed"||out.original_schedule_closed||!pending.empty()||out.completed_periods!=out.prepaid_periods||close->released_food!=0||close->source_fact_id!=last_success)return bad("ChildCareOriginClosureMismatch");out.original_schedule_closed=true;}
    }
    if(!pending.empty())out.pending_action_id=*pending.begin();
    auto death=[&](const StableId& person){return historical?read_actor_death_before_fact(c,life,person,tick,before):read_actor_death(c,life,person);};
    auto caregiver_death=death(terms.caregiver_actor_id),child_death=death(child);if(!caregiver_death.ok)return {false,{},caregiver_death.error};if(!child_death.ok)return {false,{},child_death.error};if(caregiver_death.value)out.caregiver_death_fact_id=caregiver_death.value->death_fact_id;if(child_death.value)out.child_death_fact_id=child_death.value->death_fact_id;
    for(const auto& parent:out.biological_parent_ids){auto d=death(parent);if(!d.ok)return {false,{},d.error};if(!d.value)out.living_parent_ids.push_back(parent);}
    if(!historical){auto reserve=validate_birth_care_reservation(c,terms,out.accepted_fact_id,out.remaining_reserved_food);if(!reserve.ok)return {false,{},reserve.error};if(!out.pending_action_id.empty()){const auto* action=c.action(out.pending_action_id);if(!action||action->actor_id!=terms.caregiver_actor_id||action->kind!="family_child_care")return bad("ChildCareOriginLiveActionRequired");}}
    return {true,std::move(out),{}};
}
}
Outcome<ChildCareOriginRead> read_child_care_origin(const CommonStore& c,const FamilyBirthState& s,const FamilyDeathState& life,const StableId& child){return read(c,s,life,child,c.tick(),{},false);}
Outcome<ChildCareOriginRead> read_child_care_origin_before_relief(const CommonStore& c,const FamilyBirthState& s,const FamilyDeathState& life,const StableId& child,Integer tick,const StableId& fact){return read(c,s,life,child,tick,fact,true);}
}
