// 原座所有限办理页只读本人签受和当地明确可见人物，不将远方现任或私下变化投影到页面。
#pragma once
#include "political_governance_agency_business.hpp"
namespace hanmo::resident {
BoundaryValue encode_political_governance_agency_quote(const PoliticalGovernanceAgencyQuote&);
Outcome<BoundaryValue> query_political_governance_agencies(const PoliticalGovernanceAgencyBusinessContext&,const StableId& place,Integer cursor=0);
}
