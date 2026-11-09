// 全状态只读验证沿原批准、工作、最终收据和民心链，不重算历史技能或补造完成。
#pragma once
#include "politics_governance_sources.hpp"
namespace hanmo::resident {
class OccupationFiscalCivilRead;
Status validate_political_governance(const CommonStore&,const CommonCatalog&,const PoliticsState&,const PoliticsGovernanceState&,const PoliticsGovernanceCatalog&,const OrganizationState&,const OrganizationCatalog&,const ActivityRead&,const TaskGovernanceRead&,const PublicReliefResultRead&,const PoliticalFiscalState* policies=nullptr,const FiscalSettlementState* fiscal=nullptr);
Status validate_political_governance(const CommonStore&,const CommonCatalog&,const PoliticsState&,const PoliticsGovernanceState&,const PoliticsGovernanceCatalog&,const OrganizationState&,const OrganizationCatalog&,const ActivityRead&,const TaskGovernanceRead&,const PublicReliefResultRead&,const PoliticalFiscalState*,const FiscalSettlementState*,const OccupationFiscalCivilRead*);
// 独占代理批准只有显式同栈历史读才能进入原治理全史，旧重载仍拒该新来源。
Status validate_political_governance(const CommonStore&,const CommonCatalog&,const PoliticsState&,const PoliticsGovernanceState&,const PoliticsGovernanceCatalog&,const OrganizationState&,const OrganizationCatalog&,const ActivityRead&,const TaskGovernanceRead&,const PublicReliefResultRead&,const PoliticalFiscalState*,const FiscalSettlementState*,const OccupationFiscalCivilRead*,const PoliticalGovernanceAgencyRead*);
}
