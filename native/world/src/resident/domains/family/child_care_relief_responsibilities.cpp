// 同一原事实序计算当前与历史未结责任，后来的取消/完成不能倒填旧交割时点。
#include "child_care_relief_responsibilities.hpp"
#include <algorithm>
#include <charconv>
namespace hanmo::resident {namespace {
// 实际分配的继产实例号和精确头；任意同tick标记不能成为资产责任边界。
bool inheritance_boundary(const CommonStore& c,const StableId& id){const StableId prefix="family_inheritance:";if(id.rfind(prefix,0)!=0)return false;const auto raw=id.substr(prefix.size());Integer number=0;auto parsed=std::from_chars(raw.data(),raw.data()+raw.size(),number);const auto* fact=c.state().facts.find(id);return parsed.ec==std::errc{}&&parsed.ptr==raw.data()+raw.size()&&number>0&&number<c.state().next_instance_id&&raw==std::to_string(number)&&fact&&fact->id==id&&fact->kind=="family_inheritance_settled"&&fact->domain=="family_inheritance";}
// 完整本域验证在外层，内部仍拒绝缺载荷/错头与未来事件，不通过当前Actor字段猜当年援助承诺。
Outcome<ChildCareReliefResponsibilities> read(const CommonStore& c,const ChildCareReliefState& state,const StableId& actor,Integer tick,const StableId& boundary,bool historical){
    auto bad=[&](const StableId& code)->Outcome<ChildCareReliefResponsibilities>{return {false,{},common_failure(code,actor).error};};
    if(!c.actor(actor)||tick<0||tick>c.tick())return bad("ChildCareReliefResponsibilityActorRequired");
    const auto& order=c.state().facts.order();const auto limit=historical?std::find(order.begin(),order.end(),boundary):order.end();
    if(historical&&(!inheritance_boundary(c,boundary)||limit==order.end()))return bad("ChildCareReliefInheritanceBoundaryRequired");
    ChildCareReliefResponsibilities out{c.state().identity,actor,{}};std::set<StableId> active;
    for(auto at=order.begin();at!=limit;++at){const auto* fact=c.state().facts.find(*at);if(!fact)return bad("ChildCareReliefResponsibilityFactMissing");
        if(const auto* accepted=state.agreements.find(*at)){
            if(fact->domain!="family_care_relief"||fact->kind!="family_care_relief_accepted"||fact->id!=*at||accepted->kind!=fact->kind||accepted->source.tick>tick)return bad("ChildCareReliefResponsibilitySourceMismatch");
            const auto& source=accepted->source;
            if(actor==source.parent_actor_id||actor==source.provider_actor_id)active.insert(*at);
        }else if(fact->domain=="family_care_relief"&&fact->kind=="family_care_relief_accepted")return bad("ChildCareReliefResponsibilityPayloadRequired");
        if(const auto* close=state.closures.find(*at)){
            if(fact->domain!="family_care_relief"||fact->id!=*at||fact->kind!="family_care_relief_closed"||close->kind!=fact->kind||close->tick>tick)return bad("ChildCareReliefResponsibilitySourceMismatch");
            if(close->reason!="provider_withdrew"&&close->reason!="parent_withdrew"&&close->reason!="provider_died"&&close->reason!="child_died"&&close->reason!="finite_care_completed")return bad("ChildCareReliefResponsibilityClosureUnknown");
            active.erase(close->accepted_id);
        } else if(fact->domain=="family_care_relief"&&fact->kind=="family_care_relief_closed")return bad("ChildCareReliefResponsibilityPayloadRequired");
    }
    // 保原接受事实顺序；StableId字典序不冒时间序，调用方可逐项展示真实来源。
    for(auto at=order.begin();at!=limit;++at)if(active.count(*at))out.pending_acceptance_ids.push_back(*at);
    return {true,std::move(out),{}};
}
}
Outcome<ChildCareReliefResponsibilities> read_child_care_relief_responsibilities(const CommonStore& c,const ChildCareReliefState& state,const StableId& actor){return read(c,state,actor,c.tick(),{},false);}
Outcome<ChildCareReliefResponsibilities> read_child_care_relief_responsibilities_before_inheritance(const CommonStore& c,const ChildCareReliefState& state,const StableId& actor,Integer tick,const StableId& fact){return read(c,state,actor,tick,fact,true);}
}
