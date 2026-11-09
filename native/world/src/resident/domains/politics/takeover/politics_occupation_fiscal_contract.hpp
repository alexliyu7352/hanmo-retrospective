// 新任税政显式来源；旧财政段、旧税债和原余数桶不被本读口改写。
#pragma once
#include "politics_takeover_contract.hpp"
#include "resident/domains/politics/politics_fiscal_contract.hpp"
namespace hanmo::resident {
inline constexpr const char* political_occupation_fiscal_command_kind="political_set_occupation_tax";
inline constexpr const char* political_occupation_fiscal_fact_kind="political_occupation_fiscal_policy_changed";
inline constexpr const char* political_occupation_fiscal_id_prefix="politics_occupation_fiscal";
struct OccupationFiscalPolicySource {
    WorldIdentity source_identity;
    FiscalPolicySnapshot policy;
    StableId registration_fact_id,occupation_fact_id;
    PoliticalOccupationRuleSource occupation_rule;
    Integer occupation_tick=0,integration_until_tick=0,income_basis_points=10000;
    std::optional<FiscalPolicyOccupationBoundary> control_loss;
};
class OccupationFiscalPolicyRead {
public:
    virtual ~OccupationFiscalPolicyRead()=default;
    // due沿原地区周期，不由新签政策重新起算；未来tick仅可经严格原单步上下文读取。
    virtual Outcome<std::optional<OccupationFiscalPolicySource>> occupation_fiscal_policy_for_due_tick(const StableId& site_id,Integer due_tick)const=0;
    // 精确旧期来源，不允许任意旧/未来政策。due==后次占领tick时，S5还须证明
    // 原period/assessment事实分配在该occupation之前；普通新期读在此边界为空。
    virtual Outcome<std::optional<OccupationFiscalPolicySource>> occupation_fiscal_policy_for_assessment_source(const StableId& site_id,Integer due_tick,const StableId& policy_source_fact_id)const=0;
};
}
