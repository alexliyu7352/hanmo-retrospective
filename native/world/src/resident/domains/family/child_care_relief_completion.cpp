// 原结束源给出确切本人、孩子、粮耗和失败原因；未知Action返回空，坏来源硬拒，读不写世界。
#include "child_care_relief_completion.hpp"
#include <algorithm>
#include <tuple>
namespace hanmo::resident {
Outcome<std::optional<ChildCareReliefCompletionSource>> read_child_care_relief_completion(const CommonStore& c,const ChildCareReliefState& state,const StableId& provider,const StableId& action){
    auto bad=[&](const StableId& code)->Outcome<std::optional<ChildCareReliefCompletionSource>>{return {false,{},common_failure(code,action).error};};
    const auto* start=state.starts.find(action);const ChildCareReliefReceipt* end=nullptr;StableId end_id;
    for(const auto& row:state.care.rows())if(row.second.action_id==action){if(end)return bad("DuplicateChildCareReliefCompletion");end=&row.second;end_id=row.first;}
    if(!start)return end?bad("ChildCareReliefStartRequired"):Outcome<std::optional<ChildCareReliefCompletionSource>>{true,std::nullopt,{}};
    const auto* h=c.state().facts.find(action);if(!h||h->id!=action||h->kind!="family_care_relief_started"||h->domain!="family_care_relief"||start->kind!=h->kind||start->action_id!=action||start->provider_actor_id!=provider)return bad("ChildCareReliefCompletionActorMismatch");
    if(!end)return {true,std::nullopt,{}};const auto* finish=c.state().facts.find(end_id);const auto* accepted=state.agreements.find(start->accepted_id);const auto* accepted_head=c.state().facts.find(start->accepted_id);const auto& order=c.state().facts.order();const auto a=std::find(order.begin(),order.end(),action),b=std::find(order.begin(),order.end(),end_id),agreement=std::find(order.begin(),order.end(),start->accepted_id);
    if(!finish||finish->id!=end_id||finish->kind!="family_care_relief_provided"||finish->domain!="family_care_relief"||end->kind!=finish->kind||!accepted||!accepted_head||accepted_head->kind!="family_care_relief_accepted"||accepted_head->domain!="family_care_relief"||agreement>=a||a>=b||b==order.end()||std::tie(end->accepted_id,end->provider_actor_id,end->child_actor_id,end->place_id,end->started_tick,end->period_index)!=std::tie(start->accepted_id,start->provider_actor_id,start->child_actor_id,start->place_id,start->tick,start->period_index)||end->tick<start->tick||end->tick>start->tick+start->duration||end->tick>c.tick())return bad("ChildCareReliefCompletionSourceMismatch");
    const bool success=end->outcome=="completed";
    if(success){if(end->tick!=start->tick+start->duration||end->food_consumed!=accepted->source.terms.food_per_period||!end->code.empty()||!end->interruption_fact_id.empty())return bad("ChildCareReliefCompletionConsumptionMismatch");}
    else if(end->outcome!="cancelled"||end->food_consumed!=0)return bad("UnknownChildCareReliefOutcome");
    else if(end->code=="CareReliefProviderStopped"){if(!end->interruption_fact_id.empty())return bad("ChildCareReliefUnexpectedDeathSource");}
    else if(end->code=="CareReliefProviderDied"||end->code=="CareReliefChildDied"){
        const auto* death=c.state().facts.find(end->interruption_fact_id);const auto at=std::find(order.begin(),order.end(),end->interruption_fact_id);if(!death||death->id!=end->interruption_fact_id||death->kind!="family_death"||death->domain!="family_life"||at<=a||at>=b)return bad("ChildCareReliefDeathSourceRequired");
    }else return bad("UnknownChildCareReliefOutcome");
    return {true,ChildCareReliefCompletionSource{c.state().identity,action,end_id,start->accepted_id,accepted->source.original_accepted_id,provider,start->child_actor_id,end->code,end->interruption_fact_id,start->tick,end->tick,end->food_consumed,success},{}};
}
}
