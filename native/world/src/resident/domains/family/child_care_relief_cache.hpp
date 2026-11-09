// 冷缓存必须属于本次真实分配区间；首保留history也不能换挂同tick旧本人来源。
#pragma once
#include "child_care_relief.hpp"
#include <charconv>
namespace hanmo::resident::care_relief_detail {
// 实例号只用于已分配result的边界，不替代Common业务事实的先后校验。
inline Integer instance_ordinal(const StableId& id,const StableId& prefix){const auto head=prefix+":";if(id.rfind(head,0)!=0)return 0;const auto digits=id.substr(head.size());Integer value=0;auto parsed=std::from_chars(digits.data(),digits.data()+digits.size(),value);return parsed.ec==std::errc{}&&parsed.ptr==digits.data()+digits.size()&&value>0&&std::to_string(value)==digits?value:0;}
// 完整本域史已先验；Advance可有多本人完成，不把后一个儿童/工单当成前一个来源的替代。
inline Status validate_cached_allocation(const CommonStore& common,const ChildCareReliefState& state,const StableId& result_id,const StableId& source_id){
    const HistoryRecord* previous=nullptr;const HistoryRecord* selected=nullptr;const HistoryRecord* before=nullptr;
    for(const auto& history:common.state().history){if(history.result_id==result_id){if(selected)return common_failure("CareReliefCachedHistoryMismatch");selected=&history;before=previous;}previous=&history;}
    if(!selected||selected->tick<0||selected->tick>common.tick())return common_failure("CareReliefCachedHistoryMismatch");const bool started=selected->kind=="family_child_care_relief",advance=selected->kind=="advance";const StableId prefix=started?"action":"family_care_relief";
    const auto upper=instance_ordinal(result_id,"result"),lower=before?instance_ordinal(before->result_id,"result"):0,source=instance_ordinal(source_id,prefix);
    if(upper<=lower||upper>=common.state().next_instance_id||(before&&!lower)||source<=lower||source>=upper)return common_failure("CareReliefCachedAllocationMismatch",source_id);
    auto matches=[&](const StableId& id){
        if(started){const auto* p=state.starts.find(id);return p&&p->provider_actor_id==selected->actor_id&&p->tick==selected->tick;}
        if(selected->kind=="family_care_relief_accept"){const auto* p=state.agreements.find(id);return p&&p->source.parent_actor_id==selected->actor_id&&p->source.tick==selected->tick;}
        if(selected->kind=="family_care_relief_withdraw"){const auto* p=state.closures.find(id);return p&&p->actor_id==selected->actor_id&&p->tick==selected->tick&&(p->reason=="parent_withdrew"||p->reason=="provider_withdrew");}
        if(selected->kind=="family_care_relief_cancel"){const auto* p=state.care.find(id);return p&&p->provider_actor_id==selected->actor_id&&p->tick==selected->tick&&p->outcome=="cancelled"&&p->code=="CareReliefProviderStopped";}
        if(advance){const auto* p=state.care.find(id);return p&&p->tick==selected->tick&&p->code!="CareReliefProviderStopped";}
        return false;
    };
    if(!matches(source_id))return common_failure("CareReliefCachedSourceMismatch",source_id);
    // history可能只保后缀，永久领域来源仍完整。相同本人/同类命令的更晚来源可否定换挂旧源。
    if(!advance){for(const auto& fact:common.state().facts.rows()){const auto later=instance_ordinal(fact.first,prefix);if(later>source&&later<upper&&matches(fact.first))return common_failure("CareReliefCachedAllocationMismatch",source_id);}}
    return common_success();
}
}
