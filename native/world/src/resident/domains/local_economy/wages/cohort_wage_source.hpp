// 匿名工资只读取原欠据与支付收据，不把群体展开为人物或复制可花余额。
#pragma once
#include "resident/domains/local_economy/local_economy_model.hpp"
namespace hanmo::resident {
class CurrentMomentRead;
// 金额单位固定为原工资的钱；主体和家庭资产身份来自唯一义务及群体。
struct CohortWageObservation {
    StableId source_fact_id,obligation_id,arrear_id,authority_ref;
    StableId from_wallet_id,to_wallet_id,household_group_id;
    SubjectRef principal,beneficiary;
    Integer fact_tick=0,due_tick=0,due_amount=0,paid_amount=0,outstanding_amount=0;
};
// 供事务/Need适配使用；调用方须先通过完整local_economy/organization验证。
// 只允许已提交时钟的真实wage_due或obligation_payment；未知来源明确失败。
// 本函数没有玩家访问授权语义，不能直接注册成任意事实查询。
Outcome<CohortWageObservation> observe_cohort_wage_fact(
    const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,const StableId& fact_id,const CurrentMomentRead* actual_clock=nullptr);
// 整体Need校验/阶段适配一次读取所选义务的真实历史，避免每张收据重复扫描世界。
// 输出遵循共同事实顺序，仍是临时只读快照；不增加持久索引或冷字段。
Outcome<std::vector<CohortWageObservation>> observe_cohort_wage_history(
    const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,
    const std::vector<StableId>& obligation_ids,const CurrentMomentRead* actual_clock=nullptr);
struct EconomicTakeoverReadContext;struct EconomicTakeoverExecutionState;
// 显式接管profile才传同候选真源；旧入口仍严格拒绝非原authority的支付。
Outcome<CohortWageObservation> observe_cohort_wage_fact(
    const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,const StableId&,
    const EconomicTakeoverReadContext*,const EconomicTakeoverExecutionState*,const CurrentMomentRead* actual_clock=nullptr);
Outcome<std::vector<CohortWageObservation>> observe_cohort_wage_history(
    const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,const std::vector<StableId>&,
    const EconomicTakeoverReadContext*,const EconomicTakeoverExecutionState*,const CurrentMomentRead* actual_clock=nullptr);
}
