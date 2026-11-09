// 新供给规则内部同候选检查，不对外提供任意Owner或自由资金写口。
#pragma once
#include "supply_policy_rules.hpp"
#include "../local_economy_internal.hpp"
#include "resident/domains/affairs/affairs_policy_validation.hpp"
namespace hanmo::resident::economic_supply_detail {
inline bool identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
inline bool authority(const ActingAuthority& a,const ActingAuthority& b){return a.organization_id==b.organization_id&&a.office_id==b.office_id&&a.actor_id==b.actor_id&&a.authority_epoch==b.authority_epoch;}
inline bool mandate_equal(const PoliticalSupplyPolicyMandate& a,const PoliticalSupplyPolicyMandate& b){return a.registration_fact_id==b.registration_fact_id&&a.original_source_key==b.original_source_key&&a.organization_id==b.organization_id&&a.site_id==b.site_id&&a.inventory_id==b.inventory_id&&a.funding_wallet_id==b.funding_wallet_id&&a.reward_wallet_id==b.reward_wallet_id&&a.maximum_contract_commitment==b.maximum_contract_commitment;}
inline Outcome<PoliticalSupplyPolicyMandate> mandate(const StableId& registration,const StableId& source,const NeedPolicy& p){auto cap=quote_need_policy_commitment(p);if(!cap.ok)return {false,{},cap.error};return {true,{registration,source,p.organization_id,p.site_id,p.inventory_id,p.funding_wallet_id,p.reward_wallet_id,cap.value.total},{}};}
inline Integer observed_tick(const EconomicSupplyPendingContext& x){return x.phase?x.phase->to_tick:x.common.tick();}
inline EconomicSupplyPolicyTermsSource terms(const EconomicSupplyPolicyContext& x,const EconomicSupplyPolicyRecord& r){return {x.ledger.common.state().identity,r.id,r.author_policy_id,r.registration_fact_id,r.original_source_key,r.original_source_fact_id,r.policy,r.issued_tick,r.earliest_effective_tick,r.through_tick,r.target_amount,r.authority,r.grant_id,quote_need_policy_commitment(r.policy).value.total};}
inline bool before(const CommonStore& c,const StableId& a,const StableId& b){bool seen=false;for(const auto& f:c.state().facts.rows()){if(f.first==b)return seen;if(f.first==a)seen=true;}return false;}
inline const EconomicSupplyPolicyObservation* latest(const EconomicSupplyPolicyContext& x,const StableId& policy){const EconomicSupplyPolicyObservation* found=nullptr;for(const auto& row:x.ledger.state.observations.rows())if(row.second.policy_fact_id==policy)found=&row.second;return found;}
// S6来源仍须逐精确条款和原世界二次核，不只检查返回ok。
inline Status proof(const EconomicSupplyPolicyContext& x,const PoliticalSupplyPolicyAuthoritySource& p,const PoliticalSupplyPolicyMandate& expected,const ActingAuthority& signer,const StableId& grant,Integer tick){
 const auto& r=p.registration;
 if(!identity(x.ledger.common.state().identity,p.source_identity)||!mandate_equal(p.mandate,expected)||!authority(p.authority,signer)||p.grant_id.empty()||(!grant.empty()&&p.grant_id!=grant)||p.issued_tick!=tick||r.registration_fact_id!=expected.registration_fact_id||r.site_id!=expected.site_id||r.successor_organization_id!=expected.organization_id||!local_economy_detail::has(r.public_inventory_ids,expected.inventory_id))return common_failure("EconomicSupplyAuthoritySourceMismatch",expected.registration_fact_id);return common_success();
}
inline Status adoption(const EconomicSupplyPolicyContext& x,const EconomicSupplyPolicyRecord& r,const PoliticalSupplyAdoptionSource& a,Integer tick){
 if(!identity(x.ledger.common.state().identity,a.source_identity)||a.policy_fact_id!=r.id||a.registration_fact_id!=r.registration_fact_id||a.original_source_key!=r.original_source_key||a.organization_id!=r.policy.organization_id||a.site_id!=r.policy.site_id||a.approval_fact_id.empty()||a.decision_fact_id.empty()||a.effective_tick<r.issued_tick||a.observed_tick!=tick||!before(x.ledger.common,r.id,a.approval_fact_id))return common_failure("EconomicSupplyAdoptionSourceMismatch",r.id);
 const auto* fact=x.ledger.common.state().facts.find(a.approval_fact_id);if(!fact||fact->kind!="political_public_responsibilities_assumed"||fact->domain!="politics"||a.approval_fact_id.rfind("political_takeover:",0)!=0)return common_failure("InvalidEconomicSupplyApprovalBoundary",a.approval_fact_id);
 if(a.new_contracts_allowed?!a.unavailable_code.empty():a.unavailable_code.empty())return common_failure("EconomicSupplyAdoptionPermissionMismatch",r.id);return common_success();
}
inline Outcome<Integer> physical(const EconomicSupplyPolicyContext &x,
                                 const NeedPolicy &p) {
  const auto *inventory = x.ledger.common.inventory(p.inventory_id);
  if (!inventory)
    return {
        false, 0,
        common_failure("UnknownEconomicSupplyInventory", p.inventory_id).error};
  auto quantity = count_inventory_physical_items(
      x.ledger.common, p.inventory_id, p.item_id, inventory->owner);
  if (!quantity.ok && quantity.error.original.code == "ProjectionOverflow")
    return {false, 0,
            common_failure("EconomicSupplyQuantityOverflow", p.id).error};
  return quantity;
}
// 该函数只在完整历史已验后使用；过期/失权正常暂停，损坏来源返回错误。
Outcome<EconomicSupplyPolicySource> current(const EconomicSupplyPolicyContext&,const EconomicSupplyPolicyRecord&);
}
