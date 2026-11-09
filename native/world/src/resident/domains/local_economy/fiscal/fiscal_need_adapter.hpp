// 财政只把唯一欠据投影进既有Affairs；实际税额、付款和预留仍由财政/共同资源持有。
#pragma once
#include "resident/domains/local_economy/fiscal/fiscal_debt_source.hpp"
#include "resident/domains/affairs/affairs_model.hpp"
namespace hanmo::resident {
struct OccupationFiscalSettlementState;
// 作者精确选一种税源单位；征收主体来自该期冻结政策，不能由作者另指定收款人。
struct FiscalNeedPolicy {
    StableId id,district_id,source_id,semantic_key;
    FiscalDebtUnit unit=FiscalDebtUnit::cash;
};
struct FiscalNeedCatalog { Integer version=1;OrderedTable<FiscalNeedPolicy> policies; };
// 共享Affairs校验只借真实财政读取者，不接受“已验证”布尔或空成功回调。
class FiscalNeedValidationRead {
public:
    virtual ~FiscalNeedValidationRead()=default;
    virtual Status validate_fiscal_need_state(const CommonStore&,const AffairsState&)const=0;
};
// 本访问器绑定同一候选的唯一财政状态；恢复只校验，不从漏掉的Need自动补账。
class FiscalNeedAccess final:public FiscalNeedValidationRead {
public:
    FiscalNeedAccess(const LocalEconomyState&,const FiscalSettlementState&,const FiscalSettlementCatalog&,const FiscalNeedCatalog&,const CurrentMomentRead* actual_clock=nullptr);
    FiscalNeedAccess(const LocalEconomyState&,const FiscalSettlementState&,const FiscalSettlementCatalog&,const FiscalNeedCatalog&,const OccupationFiscalSettlementState&,const CurrentMomentRead* actual_clock=nullptr);
    Status validate_fiscal_need_state(const CommonStore&,const AffairsState&)const override;
private:
    const LocalEconomyState& economy_;const FiscalSettlementState& fiscal_;
    const FiscalSettlementCatalog& taxes_;const FiscalNeedCatalog& catalog_;
    const OccupationFiscalSettlementState* occupation_=nullptr;const CurrentMomentRead* actual_clock_=nullptr;
};
Status validate_fiscal_need_catalog(const FiscalNeedCatalog&,const FiscalSettlementCatalog&);
Outcome<FiscalNeedCatalog> decode_fiscal_need_catalog(const BoundaryValue&);
// 必须位于clock_commit后、原Affairs关闭事项之前；传入该步真实财政effects.fact_ids。
// 本函数承接外层候选：失败由调用者整体丢弃候选，不自行嵌套savepoint。
Status reconcile_fiscal_needs_in_candidate(CommonStore&,AffairsState&,const LocalEconomyState&,
    const FiscalSettlementState&,const FiscalSettlementCatalog&,const FiscalNeedCatalog&,
    const std::vector<StableId>& fact_ids,const CurrentMomentRead* actual_clock=nullptr);
// 显式新账重载消费同候选两类事实；旧调用仍严格拒绝新事实头。
Status reconcile_fiscal_needs_in_candidate(CommonStore&,AffairsState&,const LocalEconomyState&,
    const FiscalSettlementState&,const FiscalSettlementCatalog&,const FiscalNeedCatalog&,
    const std::vector<StableId>& fact_ids,const OccupationFiscalSettlementState&,const CurrentMomentRead* actual_clock=nullptr);
}
