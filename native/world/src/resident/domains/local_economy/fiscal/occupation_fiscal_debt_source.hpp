// 新税债沿既有欠据DTO投影；旧财政入口不因此接受新政策或事实头。
#pragma once
#include "fiscal_debt_source.hpp"
#include "occupation_fiscal_settlement.hpp"
namespace hanmo::resident {
class CurrentMomentRead;
// 仅已提交时钟可读；每条付款返回其发生时累计金额，暂停期没有虚构欠款。
Outcome<std::vector<FiscalDebtObservation>> observe_occupation_fiscal_debt_fact(
    const CommonStore&,const LocalEconomyState&,const OccupationFiscalSettlementState&,
    const FiscalSettlementCatalog&,const StableId& fact_id,const CurrentMomentRead* actual_clock=nullptr);
}
