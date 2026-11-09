// 新税账原责任只读；不回调政治政策validator，避免政治接管冷验与财政来源循环。
#pragma once
#include "occupation_fiscal_settlement.hpp"
#include "../takeover/responsibility_history.hpp"
namespace hanmo::resident {
Outcome<EconomicResponsibilitySource> read_occupation_fiscal_responsibility_source(const CommonStore&,const LocalEconomyState&,const FiscalSettlementCatalog&,const OccupationFiscalSettlementState&,const EconomicPublicAccountSelection&,const EconomicResponsibilityRef&,const DynamicStepInput* phase=nullptr);
Outcome<std::vector<EconomicResponsibilityRef>> read_occupation_fiscal_public_account_keys(const CommonStore&,const LocalEconomyState&,const FiscalSettlementCatalog&,const OccupationFiscalSettlementState&,const MilitaryOccupationFact&,const DynamicStepInput* phase=nullptr);
Outcome<EconomicResponsibilitySource> read_occupation_fiscal_public_account_history(const CommonStore&,const LocalEconomyState&,const FiscalSettlementCatalog&,const OccupationFiscalSettlementState&,const MilitaryOccupationFact&,const EconomicResponsibilityRef&,const DynamicStepInput* phase=nullptr);
Outcome<std::vector<EconomicFiscalDependency>> read_occupation_fiscal_public_account_dependencies(const CommonStore&,const LocalEconomyState&,const FiscalSettlementCatalog&,const OccupationFiscalSettlementState&,const EconomicPublicAccountSelection&,const DynamicStepInput* phase=nullptr);
}
