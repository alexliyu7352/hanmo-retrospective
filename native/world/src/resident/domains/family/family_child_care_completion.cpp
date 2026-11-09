// 只投影本域已发生的逐次照护，不扫描别的Action并猜测其完成含义。
#include "family_child_care_completion.hpp"
#include <algorithm>
#include <tuple>
namespace hanmo::resident {
Outcome<std::optional<ChildCareCompletionSource>> read_child_care_completion(const CommonStore& c,const FamilyBirthState& state,const StableId& caregiver,const StableId& action) {
    auto bad=[&](const StableId& code)->Outcome<std::optional<ChildCareCompletionSource>>{return {false,{},common_failure(code,action).error};};
    const auto* start=state.care_starts.find(action);
    const ChildCareReceipt* receipt=nullptr;StableId receipt_id;
    for(const auto& row:state.care.rows())if(row.second.action_id==action){if(receipt)return bad("DuplicateChildCareCompletion");receipt=&row.second;receipt_id=row.first;}
    if(!start)return receipt?bad("ChildCareStartRequired"):Outcome<std::optional<ChildCareCompletionSource>>{true,std::nullopt,{}};
    const auto* head=c.state().facts.find(action);
    if(start->kind!="family_child_care_started"||start->action_id!=action||start->caregiver_actor_id!=caregiver||!head||head->id!=action||head->kind!=start->kind||head->domain!="family_birth")return bad("ChildCareCompletionActorSourceMismatch");
    if(!receipt)return {true,std::nullopt,{}};
    const auto* end=c.state().facts.find(receipt_id);const auto& order=c.state().facts.order();
    const auto at=std::find(order.begin(),order.end(),action),after=std::find(order.begin(),order.end(),receipt_id);
    if(!end||end->id!=receipt_id||end->kind!="family_child_care"||end->domain!="family_birth"||receipt->kind!=end->kind||at==order.end()||after==order.end()||at>=after||std::tie(receipt->accepted_id,receipt->child_actor_id,receipt->caregiver_actor_id,receipt->place_id,receipt->started_tick,receipt->period_index)!=std::tie(start->accepted_id,start->child_actor_id,start->caregiver_actor_id,start->place_id,start->tick,start->period_index)||receipt->tick<start->tick||receipt->tick>c.tick()||receipt->tick>start->tick+start->duration)return bad("ChildCareCompletionSourceMismatch");
    const auto* accepted=state.replies.find(start->accepted_id);if(!accepted||!accepted->confirmation)return bad("ChildCareCompletionAcceptanceRequired");
    const bool success=receipt->outcome=="completed";
    if(success){if(!receipt->code.empty()||!receipt->interruption_actor_id.empty()||!receipt->interruption_fact_id.empty()||receipt->food_consumed!=accepted->confirmation->terms.food_per_period||receipt->tick!=start->tick+start->duration)return bad("ChildCareCompletionConsumptionMismatch");}
    else if(receipt->outcome!="cancelled"||receipt->food_consumed!=0)return bad("ChildCareCompletionOutcomeMismatch");
    else if(receipt->code=="CaregiverStopped"){if(!receipt->interruption_actor_id.empty()||!receipt->interruption_fact_id.empty())return bad("ChildCareUnexpectedInterruptionSource");}
    else if(receipt->code=="CaregiverDied"||receipt->code=="ChildDied") {
        const auto* death=c.state().facts.find(receipt->interruption_fact_id);const auto death_at=std::find(order.begin(),order.end(),receipt->interruption_fact_id);
        if(receipt->interruption_actor_id!=(receipt->code=="CaregiverDied"?caregiver:start->child_actor_id)||!death||death->id!=receipt->interruption_fact_id||death->kind!="family_death"||death->domain!="family_life"||death_at==order.end()||death_at>=after)return bad("ChildCareCompletionDeathSourceMismatch");
    } else return bad("ChildCareCompletionOutcomeMismatch");
    return {true,ChildCareCompletionSource{c.state().identity,action,receipt_id,start->accepted_id,caregiver,start->child_actor_id,receipt->code,receipt->interruption_fact_id,start->tick,receipt->tick,receipt->food_consumed,success},{}};
}
}
