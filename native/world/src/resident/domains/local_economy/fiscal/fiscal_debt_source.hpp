// 税欠账只投影原评税/付款事实，供既有Need接入；本叶不拥有资产或另一份可写欠额。
#pragma once
#include "resident/domains/local_economy/fiscal/fiscal_settlement.hpp"
namespace hanmo::resident {
class CurrentMomentRead;
enum class FiscalDebtUnit { cash, grain };
// 同一assessment中的钱/粮欠据分别保稳定ID，付款事实与原币物单位不可互换。
struct FiscalDebtObservation {
    StableId source_fact_id,assessment_id,period_id,source_id,obligation_id,arrear_id;
    StableId jurisdiction_id,site_id,policy_fact_id,item_id;
    SubjectRef taxpayer,beneficiary;
    FiscalDebtUnit unit=FiscalDebtUnit::cash;
    Integer fact_tick=0,due_tick=0,due_amount=0,paid_amount=0,outstanding_amount=0;
};
// 仅内部来源适配器调用，不能绕过玩家页权限直接公开；已提交tick才可读，不借阶段to偷看未来。
// registration/period事实没有逐户欠据，成功返回空；未知/非财政事实明确报错。
Outcome<std::vector<FiscalDebtObservation>> observe_fiscal_debt_fact(
    const CommonStore&,const LocalEconomyState&,const FiscalSettlementState&,
    const FiscalSettlementCatalog&,const StableId& fact_id,const CurrentMomentRead* actual_clock=nullptr);
}
