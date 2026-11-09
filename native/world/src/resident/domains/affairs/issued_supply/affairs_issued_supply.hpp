// 已发行供给只生成栈上政策投影；原Need、Offer、合同与钱货仍由原领域唯一writer持有。
#pragma once
#include "resident/domains/affairs/affairs_model.hpp"
#include "resident/domains/local_economy/supply/supply_policy_contract.hpp"
#include <set>
namespace hanmo::resident {
// 历史条款完整保留；不可新签仅形成派生暂停集合，不持久化第二套政策或许可。
struct AffairsIssuedSupplyProjection {
    AffairsCatalog catalog;
    OrderedTable<EconomicSupplyPolicySource> issued;
    std::set<StableId> unavailable_policy_ids;
    std::vector<SupplyObservation> fresh_observations;
};
// 只消费同候选的S5具名读，按Common原序覆盖全部发行事实，未知来源或身份直接失败。
Outcome<AffairsIssuedSupplyProjection> read_affairs_issued_supply_projection(
    const CommonStore&,const AffairsCatalog&,const TaskCatalog&,
    const EconomicSupplyPolicyRead*,const DynamicStepInput* phase=nullptr);
// 原Need引用它自己的历史观察；当前容器或最新另一张观察不能替代过去记录。
Status validate_affairs_issued_supply_history(const CommonStore&,const AffairsState&,
    const AffairsIssuedSupplyProjection&,const EconomicSupplyPolicyRead*,const DynamicStepInput* phase=nullptr);
// 零耗时失效只收紧既有phase/status；恢复留原半日刷新，不造ID、货物或合同。
Status project_affairs_issued_supply_suspensions(const CommonStore&,AffairsState&,TaskState&,
    const AffairsIssuedSupplyProjection&);
// 只在原刷新边界加入真实新观察；所有认领、资金预约与贡献仍走原Affairs/Task规则。
Status reconcile_affairs_with_issued_supply(CommonStore&,AffairsState&,const AffairsCatalog&,TaskState&,
    const TaskCatalog&,const std::vector<SupplyObservation>&,const DynamicResultRead&,Integer tick,
    const EconomicSupplyPolicyRead&,const TaskExecutionServices* execution=nullptr,
    const std::vector<EconomicSupplySuspension>* supply_suspensions=nullptr,const DynamicStepInput* original_calendar_phase=nullptr);
Status publish_need_offers_with_issued_supply(CommonStore&,AffairsState&,const AffairsCatalog&,TaskState&,
    const TaskCatalog&,Integer tick,const EconomicSupplyPolicyRead&,const DynamicStepInput* original_calendar_phase=nullptr);
// 冷验先验证发行/原观察，然后复用原全量责任与合同校验，不屏蔽未知Need。
Status validate_affairs_with_issued_supply(const CommonStore&,const AffairsState&,const AffairsCatalog&,
    const TaskState&,const TaskCatalog&,const EconomicSupplyPolicyRead&,
    const DynamicResultRead* results=nullptr,const FiscalNeedValidationRead* fiscal_needs=nullptr,
    const CohortWageNeedValidationRead* cohort_wages=nullptr,
    const std::vector<EconomicSupplySuspension>* supply_suspensions=nullptr,const DynamicStepInput* original_calendar_phase=nullptr,const WorldMoment* actual_moment=nullptr);
// 政治命令成功后/实际占领phase沿同候选重读，立即撤下不可新签的原Offer。
Status project_affairs_issued_supply_in_candidate(const CommonStore&,AffairsState&,const AffairsCatalog&,
    TaskState&,const TaskCatalog&,const EconomicSupplyPolicyRead&,const DynamicStepInput* phase=nullptr);

// 新受领的每次预览与真正commit都重读当前许可，旧quote和尚未重绘Offer不授新签权。
Status validate_affairs_issued_supply_claim(const CommonStore&,const Need&,const OfferInstance&,
    const EconomicSupplyPolicyRead*);

// 只收紧原知情列表，不发现新委托；当前许可暂停时不展示可领份额，坏来源仍报错。
Outcome<std::vector<OpportunityView>> filter_affairs_issued_supply_opportunities(
    const CommonStore&,const AffairsState&,const TaskState&,const std::vector<OpportunityView>&,
    const EconomicSupplyPolicyRead*);
// 页面和NPC共用该薄读，先按原知情/缓存筛选，再在计候选预算前核当前许可。
Outcome<std::vector<OpportunityView>> query_opportunities_with_issued_supply(
    const CommonStore&,const AffairsState&,const TaskState&,const KnownSubjectView&,
    const EconomicSupplyPolicyRead*);

}
