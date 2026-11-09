// 新供给政策只给Affairs提供有来源的不可变条款；原合同、旧Need和实际钱货不由该读口改写。
#pragma once
#include "resident/domains/affairs/affairs_model.hpp"
#include "resident/domains/local_economy/local_economy_model.hpp"
namespace hanmo::resident {
inline constexpr const char* economic_supply_policy_module="economic_supply_policies";
inline constexpr const char* economic_supply_policy_prefix="economic_supply_policy";
inline constexpr const char* economic_supply_policy_issue_kind="economic_supply_policy_issued";
inline constexpr const char* economic_supply_observation_prefix="economic_supply_observation";
inline constexpr const char* economic_supply_observation_kind="economic_supply_observed";
// policy.id、source_key和semantic_key均由真实发行事实派生。历史条款总能按原ID读取；
// 许可为false只暂停新Offer/认领，不取消旧合同或改派受益人与结算账户。
struct EconomicSupplyPolicyTermsSource {
    WorldIdentity source_identity;
    StableId policy_fact_id,author_policy_id,registration_fact_id,original_source_key,original_source_fact_id;
    NeedPolicy policy;
    Integer issued_tick=0,earliest_effective_tick=0,through_tick=0,target_amount=0;
    ActingAuthority issuing_authority;StableId issuing_grant_id;Integer maximum_contract_commitment=0;
};
// S6批准只读这份已发行条款，不能反向调用政治整态校验，避免政策与批准互为前置。
class EconomicSupplyPendingPolicyRead {
public:
    virtual ~EconomicSupplyPendingPolicyRead()=default;
    // 供显式adoption绑定精确草案；历史仅允许真实批准边界，不随后来草案重选。
    virtual Outcome<std::optional<EconomicSupplyPolicyTermsSource>> economic_supply_policy_terms(
        const StableId& policy_fact_id,const std::optional<StableId>& before_approval_fact_id={})const=0;
    // 无before_fact为当前待批准最新政策；历史仅接受真实政治批准头，按原事实序选择此前最新发行。
    virtual Outcome<std::optional<EconomicSupplyPolicyTermsSource>> economic_supply_pending_policy(
        const StableId& registration_fact_id,const StableId& original_source_key,
        const std::optional<StableId>& before_approval_fact_id={})const=0;
};
struct EconomicSupplyPolicySource {
    EconomicSupplyPolicyTermsSource issued;
    Integer effective_tick=0,observed_tick=0;
    bool new_contracts_allowed=false;
    StableId approval_fact_id,unavailable_code;
    // 从本账原事实序派生；暂停时仍保历史ID，空表示未发生实际观察。
    StableId last_observation_fact_id;
};
// 观察只描述在原Common真实容器上发生的快照；消费者须逐字段核policy和具名事实头。
struct EconomicSupplyObservationSource {
    WorldIdentity source_identity;
    StableId policy_fact_id;
    LocalSupplyObservation observation;
};
class EconomicSupplyPolicyRead {
public:
    virtual ~EconomicSupplyPolicyRead()=default;
    // 内部完整冷验枚举发行事实，非玩家可见列表；返回唯一原序且不包含作者候选空占位。
    virtual Outcome<std::vector<StableId>> economic_supply_policy_ids()const=0;
    // 当前许可只由本候选已核政治/账户来源派生，不能只凭库存Owner推断可发合同。
    virtual Outcome<std::optional<EconomicSupplyPolicySource>> economic_supply_policy(const StableId& policy_fact_id)const=0;
    // 历史读必须命中实际记录及Common同kind/domain/prefix，不回读当前库存伪造旧数量。
    virtual Outcome<std::optional<EconomicSupplyObservationSource>> economic_supply_observation(const StableId& fact_id)const=0;
};
}
