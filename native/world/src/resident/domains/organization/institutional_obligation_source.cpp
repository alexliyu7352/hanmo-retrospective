// 只证成原责任的本期计提，不授权消费已转属公库，也不修改旧债主体。
#include "institutional_obligation_source.hpp"
#include "organization_internal.hpp"
namespace hanmo::resident {
bool institutional_issuer_obligation_accrual_allowed(const CommonStore& common,const OrganizationState& state,const OrganizationCatalog& catalog,const LocalEconomyState& economy,const StableId& issuer_id,const StableId& obligation_id,const StableId& principal,const StableId& source,Integer amount,Integer tick){
    if(economy.membership_wage_bindings.find(issuer_id))return false;
    const auto* issuer=catalog.institutional_issuers.find(issuer_id);const auto* organization=state.organizations.find(principal);const auto* obligation=economy.obligations.find(obligation_id);
    if(!issuer||!organization||organization->status==OrganizationStatus::dissolved||issuer->organization_id!=principal||amount<0||amount>issuer->amount_limit||tick<0||!organization_detail::has(issuer->obligation_ids,obligation_id)||!organization_detail::has(issuer->source_container_ids,source)||!obligation||obligation->principal.kind!=SubjectKind::organization||obligation->principal.id!=principal||obligation->source_wallet_or_inventory!=source||obligation->authority_ref!=issuer_id)return false;
    if(organization->status!=OrganizationStatus::active&&issuer->continuity_policy_id!="continue_under_organization")return false;
    // 原容器必须仍存在；它的当前Owner和实际支付资格由独立执行读口逐项检查。
    return common.wallet(source)||common.inventory(source);
}
}
