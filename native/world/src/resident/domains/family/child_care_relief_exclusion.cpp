// 新志愿照护活跃期间不允许旧计划再对同一儿童开工；关闭后旧预约与剩余期数仍由原Birth账读取。
#include "child_care_relief_exclusion.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
Status validate_original_care_not_replaced(const CommonStore& c,const ChildCareReliefState& state,const StableId& child,Integer tick,const StableId& original){
    if(!c.actor(child)||tick<0||tick>c.tick())return common_failure("ChildCareReliefExclusionActorRequired");const auto& order=c.state().facts.order();auto limit=order.end();
    if(!original.empty()){const auto* head=c.state().facts.find(original);limit=std::find(order.begin(),order.end(),original);if(!head||head->id!=original||head->kind!="family_child_care_started"||head->domain!="family_birth"||limit==order.end())return common_failure("ChildCareReliefOriginalStartBoundaryRequired");}
    else if(tick!=c.tick())return common_failure("ChildCareReliefCurrentBoundaryRequired");
    std::set<StableId> active;
    for(auto at=order.begin();at!=limit;++at){const auto* header=c.state().facts.find(*at);if(!header||header->id!=*at)return common_failure("ChildCareReliefExclusionSourceMismatch");
        if(const auto* row=state.agreements.find(*at)){if(header->domain!="family_care_relief"||header->kind!="family_care_relief_accepted"||row->kind!=header->kind||row->source.tick>tick)return common_failure("ChildCareReliefExclusionSourceMismatch");if(row->source.child_actor_id==child)active.insert(*at);}
        else if(header->domain=="family_care_relief"&&header->kind=="family_care_relief_accepted")return common_failure("ChildCareReliefExclusionPayloadRequired");
        if(const auto* row=state.closures.find(*at)){if(header->domain!="family_care_relief"||header->kind!="family_care_relief_closed"||row->kind!=header->kind||row->tick>tick||!state.agreements.find(row->accepted_id))return common_failure("ChildCareReliefExclusionSourceMismatch");if(row->reason!="provider_withdrew"&&row->reason!="parent_withdrew"&&row->reason!="provider_died"&&row->reason!="child_died"&&row->reason!="finite_care_completed")return common_failure("ChildCareReliefExclusionClosureUnknown");active.erase(row->accepted_id);}
        else if(header->domain=="family_care_relief"&&header->kind=="family_care_relief_closed")return common_failure("ChildCareReliefExclusionPayloadRequired");
    }
    return active.empty()?common_success():common_failure("ChildCareReliefAlreadyActive",*active.begin());
}
}
