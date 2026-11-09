// 匿名工资借原Affairs表达责任，来源、付款和家庭资产仍由原经济账持有。
#pragma once
#include "cohort_wage_source.hpp"
#include "resident/domains/affairs/affairs_model.hpp"
namespace hanmo::resident {
// 作者只选择已有工资义务；实际收款主体与钱包不由映射再次指定。
struct CohortWageNeedPolicy { StableId id,obligation_id,organization_id,semantic_key; };
struct CohortWageNeedCatalog { Integer version=1;OrderedTable<CohortWageNeedPolicy> policies; };
// 共享校验须调用真实源读取者，不能以空成功回调替代原欠据覆盖。
class CohortWageNeedValidationRead {
public:
    virtual ~CohortWageNeedValidationRead()=default;
    virtual Status validate_cohort_wage_need_state(const CommonStore&,const AffairsState&)const=0;
};
// 访问器绑定同一世界候选的原经济状态，冷恢复只验缺失而不自动补项。
class CohortWageNeedAccess final:public CohortWageNeedValidationRead {
public:
    CohortWageNeedAccess(const LocalEconomyState&,const LocalEconomyCatalog&,const CohortWageNeedCatalog&,const CurrentMomentRead* actual_clock=nullptr);
    CohortWageNeedAccess(const LocalEconomyState&,const LocalEconomyCatalog&,const CohortWageNeedCatalog&,const EconomicTakeoverReadContext*,const EconomicTakeoverExecutionState*,const CurrentMomentRead* actual_clock=nullptr);
    Status validate_cohort_wage_need_state(const CommonStore&,const AffairsState&)const override;
private:
    const LocalEconomyState& economy_;const LocalEconomyCatalog& economy_catalog_;const CohortWageNeedCatalog& catalog_;
    const EconomicTakeoverReadContext* takeover_=nullptr;const EconomicTakeoverExecutionState* execution_=nullptr;const CurrentMomentRead* actual_clock_=nullptr;
};
Status validate_cohort_wage_need_catalog(const CohortWageNeedCatalog&,const LocalEconomyCatalog&);
Outcome<CohortWageNeedCatalog> decode_cohort_wage_need_catalog(const BoundaryValue&);
// 承接原外层事务；在clock_commit之后消费本步真实经济事实，失败由候选整体回滚。
Status reconcile_cohort_wage_needs_in_candidate(CommonStore&,AffairsState&,const LocalEconomyState&,
    const LocalEconomyCatalog&,const CohortWageNeedCatalog&,const std::vector<StableId>& fact_ids,const CurrentMomentRead* actual_clock=nullptr);
Status reconcile_cohort_wage_needs_in_candidate(CommonStore&,AffairsState&,const LocalEconomyState&,
    const LocalEconomyCatalog&,const CohortWageNeedCatalog&,const std::vector<StableId>&,
    const EconomicTakeoverReadContext*,const EconomicTakeoverExecutionState*,const CurrentMomentRead* actual_clock=nullptr);
}
