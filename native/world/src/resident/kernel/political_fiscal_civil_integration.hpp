// 只消费本次真实财政阶段新产生的周期；旧期不会被扫描或补算。
#pragma once
#include "resident/domains/politics/politics_governance_consequences.hpp"
#include "resident/domains/local_economy/fiscal/fiscal_settlement.hpp"
namespace hanmo::resident {
struct WorldBundle;
// 调用者持同一完整世界候选，财政阶段之后、clock_commit之后、治理任务最终结算之前调用。
Status settle_political_fiscal_civil_in_transaction(CommonStore&,const PoliticsState&,PoliticsGovernanceState&,const PoliticsGovernanceCatalog&,const PoliticalFiscalState&,const FiscalSettlementState&,const DynamicStepInput&,const LocalEconomyEffects&);
Status settle_political_fiscal_civil_world(WorldBundle&,const DynamicStepInput&,const LocalEconomyEffects&);
}
