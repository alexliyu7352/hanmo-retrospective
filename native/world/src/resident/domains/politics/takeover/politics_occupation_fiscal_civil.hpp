// 民心只认完整新财政评税结果；钱粮总额与税债仍由经济唯一账负责。
#pragma once
#include "politics_occupation_fiscal_contract.hpp"
#include "resident/domains/politics/politics_governance_sources.hpp"
namespace hanmo::resident {
struct OccupationFiscalCivilAssessment {
    WorldIdentity source_identity;
    StableId period_fact_id;
    Integer due_tick=0;
    FiscalPolicySnapshot policy;
};
class OccupationFiscalCivilRead {
public:
    virtual ~OccupationFiscalCivilRead()=default;
    virtual Outcome<std::optional<OccupationFiscalCivilAssessment>> occupation_fiscal_civil_period(const StableId&)const=0;
};
Outcome<PoliticalCivilSource> read_occupation_fiscal_civil_source(const CommonStore&,const PoliticsState&,const PoliticsGovernanceState&,const FiscalSettlementState*,const OccupationFiscalCivilRead&,const StableId&);
Outcome<StableId> apply_occupation_fiscal_civil_in_transaction(CommonStore&,const PoliticsState&,PoliticsGovernanceState&,const PoliticsGovernanceCatalog&,const FiscalSettlementState*,const OccupationFiscalCivilRead&,const StableId&);
}
