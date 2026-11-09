// 民心只消费真实完成/评税来源；调用方持同一候选，不能用玩家给定数值写结果。
#pragma once
#include "politics_governance.hpp"
#include "politics_fiscal.hpp"
namespace hanmo::resident {
class PublicReliefResultRead;struct FiscalSettlementState;
// 初始民心必须由作者逐辖区明确提供，注册只发生在原生新局tick0。
Status initialize_political_civil_state_in_transaction(CommonStore&,const PoliticsState&,PoliticsGovernanceState&,const PoliticsGovernanceCatalog&);
// 内层必须与S3最终fact/S5实际收据处于同一外层候选。取消/失败没有民心结果。
Outcome<std::optional<StableId>> apply_governance_civil_consequence_in_transaction(CommonStore&,const PoliticsState&,PoliticsGovernanceState&,const PoliticsGovernanceCatalog&,const TaskGovernanceRead&,const PublicReliefResultRead&,const StableId& final_fact_id);
// 整个辖区税期只施加一次；无政策或未完成评税不能冒作收入/民心来源。
Outcome<StableId> apply_fiscal_civil_consequence_in_transaction(CommonStore&,const PoliticsState&,PoliticsGovernanceState&,const PoliticsGovernanceCatalog&,const PoliticalFiscalState&,const FiscalSettlementState&,const StableId& period_fact_id);
}
