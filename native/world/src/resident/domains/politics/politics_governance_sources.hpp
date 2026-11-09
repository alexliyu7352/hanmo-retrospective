// 政治内部的只读完成来源，供采用与冷验证复用同一规则；不是玩家命令或持久状态。
#pragma once
#include "politics_governance_consequences.hpp"
namespace hanmo::resident {
struct PoliticalCivilSource {StableId jurisdiction_id,source_domain,source_fact_id,plan_id;Integer source_tick=0,delta=0,cap=100;};
Outcome<std::optional<PoliticalCivilSource>> read_completed_governance_civil_source(const CommonStore&,const PoliticsState&,const PoliticsGovernanceState&,const PoliticsGovernanceCatalog&,const TaskGovernanceRead&,const PublicReliefResultRead&,const StableId&);
Outcome<PoliticalCivilSource> read_assessed_fiscal_civil_source(const CommonStore&,const PoliticsState&,const PoliticsGovernanceState&,const PoliticsGovernanceCatalog&,const PoliticalFiscalState&,const FiscalSettlementState&,const StableId&);
}
