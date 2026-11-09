// 私人雇用的唯一业务合同；会面/任务只借此来源，不另造接受、工时或工资账。
#pragma once
#include "resident/model/common_model.hpp"
namespace hanmo::resident {
inline constexpr const char* estate_employment_module="estate_employment";
// 条款在雇主邀约时冻结；工资按实际服务半日计，终止时未满一钱的已赚部分向上结清。
struct EstateEmploymentTerms {
    StableId policy_id,estate_id,employer_actor_id,worker_actor_id,job;
    StableId rate_profile,payer_wallet_id,payee_wallet_id;
    Integer wage_numerator=0,wage_denominator=1,service_ticks=0,offer_expires_tick=0,reservation_cap=0;
};
// 本人命令与他人的表态分开；accept/decline/leave只允许原worker，withdraw/dismiss只允许原雇主。
struct EstateEmploymentOffer { StableId policy_id,worker_actor_id; };
struct EstateEmploymentDecision { StableId contract_id,decision; };
// 求职者只请求原雇主声明的邀约；不会同时替自己接受。
struct EstateEmploymentRequestOffer { StableId policy_id; };
using EstateEmploymentCommand=std::variant<EstateEmploymentOffer,EstateEmploymentDecision,EstateEmploymentRequestOffer>;
// 只读快照明确观察者和世界身份；due/paid来自唯一LocalEconomyState，不是另一可花余额。
struct EstateEmploymentSource {
    WorldIdentity source_identity;
    StableId observer_actor_id,contract_id,status,offer_fact_id,acceptance_fact_id,terminal_fact_id,obligation_id;
    EstateEmploymentTerms terms;
    Integer offered_tick=0,accepted_tick=-1,through_tick=-1,observed_tick=0,served_ticks=0,earned_cash=0,paid_cash=0,reserved_cash=0;
};
// 当前真实雇主会面的有界作者邀约项；未冻结contract，不把可见条款当已成立责任。
struct EstateEmploymentPolicySource {
    WorldIdentity source_identity;StableId observer_actor_id,provider_actor_id,policy_id,estate_id,job,rate_profile;
    Integer wage_numerator=0,wage_denominator=1,service_ticks=0,offer_valid_ticks=0,reservation_cap=0;
    StableId employer_actor_id,worker_actor_id;bool provider_offer=false;
};
// 任务手记只引用这个具名合同；未授权观察者返回空值，来源损坏返回错误而非已结清。
class EstateEmploymentRead {
public:
    virtual ~EstateEmploymentRead()=default;
    virtual Outcome<std::optional<EstateEmploymentSource>> estate_employment_source(const StableId& observer_actor_id,const StableId& contract_id)const=0;
    virtual Outcome<std::vector<StableId>> estate_employment_contract_ids(const StableId& observer_actor_id,Integer offset,Integer limit)const=0;
    virtual Outcome<std::vector<EstateEmploymentPolicySource>> estate_employment_policies(const StableId& observer_actor_id,const StableId& provider_actor_id)const=0;
};
}
