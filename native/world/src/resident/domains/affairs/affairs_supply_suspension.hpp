// 暂停只消费本候选S5原供给责任投影，不创建继受供给政策、第二账或付款权。
#pragma once
#include "affairs_model.hpp"
#include "resident/domains/local_economy/takeover/responsibility_execution.hpp"
#include <set>
namespace hanmo::resident {
// World必须从同一已commit候选读取EconomicSupplySuspension，不能接受命令自报列表。
inline Outcome<std::set<StableId>> read_affairs_suspended_policies(const CommonStore& common,const AffairsCatalog& catalog,const std::vector<EconomicSupplySuspension>* sources,const std::set<StableId>* issued_suspended_policy_ids=nullptr){
    std::set<StableId> result,seen;
    // 新政策暂停只来自同候选S5具名读取；不能以旧来源暂停伪造新issuer或发行身份。
    if(issued_suspended_policy_ids)for(const auto& id:*issued_suspended_policy_ids){const auto* policy=catalog.policies.find(id);const auto* fact=common.state().facts.find(id);if(!policy||policy->id!=id||policy->source_key!=id||policy->semantic_key!=id||id.rfind("economic_supply_policy:",0)!=0||!fact||fact->id!=id||fact->kind!="economic_supply_policy_issued"||fact->domain!="economic_supply_policies")return {false,{},common_failure("InvalidAffairsIssuedSupplySuspension",id).error};result.insert(id);}
    if(!sources)return {true,std::move(result),{}};
    for(const auto& source:*sources){const auto* fact=common.state().facts.find(source.registration_fact_id);
        if(source.source_key.empty()||source.original_organization_id.empty()||source.site_id.empty()||source.inventory_id.empty()||source.item_id.empty()||source.code.empty()||!seen.insert(source.source_key).second||!fact||fact->kind!="political_public_responsibilities_pending"||fact->domain!="politics")return {false,{},common_failure("InvalidAffairsSupplySuspension",source.source_key).error};
        for(const auto& row:catalog.policies.rows()){const auto& policy=row.second;if(policy.source_key!=source.source_key)continue;
            if(policy.source_domain!="local_economy"||policy.organization_id!=source.original_organization_id||policy.site_id!=source.site_id||policy.inventory_id!=source.inventory_id||policy.item_id!=source.item_id)return {false,{},common_failure("AffairsSupplySuspensionSourceMismatch",policy.id).error};
            result.insert(policy.id);
        }
    }return {true,std::move(result),{}};
}
// 零耗时政治改判只收紧已存在的投影；恢复由原半日刷新，不能顺带执行旧合同。
inline Status project_affairs_supply_suspensions_in_candidate(const CommonStore& common,AffairsState& state,const AffairsCatalog& catalog,TaskState& tasks,const std::vector<EconomicSupplySuspension>& sources){
    auto paused=read_affairs_suspended_policies(common,catalog,&sources);if(!paused.ok)return {false,{},paused.error};
    std::set<StableId> affair_ids,need_ids;
    for(const auto& row:state.needs.rows()){const auto& need=row.second;if(!paused.value.count(need.policy_id))continue;const auto* policy=catalog.policies.find(need.policy_id);const auto* affair=state.affairs.find(need.affair_id);
        if(!policy||!affair||need.source_domain!=policy->source_domain||need.source_key!=policy->source_key||need.beneficiary.kind!=SubjectKind::organization||need.beneficiary.id!=policy->organization_id||need.target_inventory_id!=policy->inventory_id||need.item_id!=policy->item_id||affair->semantic_key!=policy->semantic_key||affair->principal.kind!=SubjectKind::organization||affair->principal.id!=policy->organization_id)return common_failure("AffairsSupplySuspensionSourceMismatch",need.id);
        if(!affair->closed_tick)affair_ids.insert(affair->id);need_ids.insert(need.id);
    }
    // 先核所有关联Offer再改phase/status，坏源不留下部分派生投影。
    for(const auto& row:tasks.offer_instances.rows()){const auto& offer=row.second;if(!need_ids.count(offer.source_need_id))continue;const auto* need=state.needs.find(offer.source_need_id);const auto* policy=catalog.policies.find(need->policy_id);
        if(offer.template_id!=policy->template_id||offer.issuer.kind!=Owner::Kind::organization||offer.issuer.id!=policy->organization_id||offer.funding_wallet_id!=policy->funding_wallet_id||offer.reward_wallet_id!=policy->reward_wallet_id)return common_failure("AffairsSupplySuspensionOfferMismatch",offer.id);
    }
    for(const auto& id:affair_ids)state.affairs.find(id)->phase=AffairPhase::suspended;
    for(auto& row:tasks.offer_instances.rows()){auto& offer=row.second;if(!need_ids.count(offer.source_need_id))continue;const auto* need=state.needs.find(offer.source_need_id);const bool terminal=need->status==NeedStatus::fulfilled||need->status==NeedStatus::expired||need->status==NeedStatus::cancelled;offer.status=terminal?OfferStatus::closed:OfferStatus::withdrawn;}
    return common_success();
}
// 原Affair已有暂停枚举；本辅助只读派生状态，受领合同/受益人仍来自原Need。
inline bool affair_source_suspended(const AffairsState& state,const Need& need){const auto* affair=state.affairs.find(need.affair_id);return affair&&affair->phase==AffairPhase::suspended;}
}
