// 新供给草案的公职签权与批准分开；签权本身不生成Need、Offer、付款或债权改派。
#pragma once
#include "resident/domains/politics/takeover/politics_takeover_contract.hpp"
namespace hanmo::resident {
inline constexpr const char* political_supply_adoption_module="political_supply_adoptions";
inline constexpr const char* political_supply_policy_grant="political_issue_supply_policy";
// 经济域必须从真实作者政策导出完整单合同最高承诺；同钱包本金与报酬仍相加。
struct PoliticalSupplyPolicyMandate {
    StableId registration_fact_id,original_source_key;
    StableId organization_id,site_id,inventory_id,funding_wallet_id,reward_wallet_id;
    Integer maximum_contract_commitment=0;
};
struct PoliticalSupplyPolicyAuthoritySource {
    WorldIdentity source_identity;
    PoliticalSupplyPolicyMandate mandate;
    ActingAuthority authority;
    StableId grant_id;
    PoliticalTakeoverRegistrationSource registration;
    Integer issued_tick=0;
};
// 当前允许在pending登记上签草案。历史只接受S5实际economic_supply_policy_issued头，
// 由S5另核该事实载荷确为这份条款；不会借今天Owner或Office重写当时授权。
class PoliticalSupplyPolicyAuthorityRead {
public:
    virtual ~PoliticalSupplyPolicyAuthorityRead()=default;
    virtual Outcome<PoliticalSupplyPolicyAuthoritySource> political_supply_policy_authority(
        const ActingAuthority&,const PoliticalSupplyPolicyMandate&)const=0;
    virtual Outcome<PoliticalSupplyPolicyAuthoritySource> political_supply_policy_authority_before_issue(
        const ActingAuthority&,const PoliticalSupplyPolicyMandate&,const StableId& grant_id,
        const StableId& policy_issue_fact_id,Integer issued_tick)const=0;
};
// 绑定表只引用原接管decision和精确已发行政策；后发草案不能替换旧批准。
struct PoliticalSupplyAdoptionSource {
    WorldIdentity source_identity;
    StableId policy_fact_id,registration_fact_id,original_source_key;
    StableId approval_fact_id,decision_fact_id,organization_id,site_id;
    Integer effective_tick=0,observed_tick=0;
    bool new_contracts_allowed=false;
    StableId unavailable_code;
};
class PoliticalSupplyAdoptionRead {
public:
    virtual ~PoliticalSupplyAdoptionRead()=default;
    virtual Outcome<std::optional<PoliticalSupplyAdoptionSource>> political_supply_adoption(
        const StableId& policy_issue_fact_id)const=0;
    virtual Outcome<std::optional<PoliticalSupplyAdoptionSource>> political_supply_adoption_before_observation(
        const StableId& policy_issue_fact_id,const StableId& observation_fact_id,Integer tick)const=0;
};
}
