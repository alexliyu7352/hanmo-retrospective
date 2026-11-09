// NPC经营续签另由显式作者目录选择；旧供给pair不会自动生成任何经营意图。
#pragma once
#include "politics_supply_renewal.hpp"
namespace hanmo::resident {
inline constexpr const char* political_supply_renewal_module="political_supply_renewals";
struct PoliticalSupplyRenewalCatalog {
    Integer version=1;
    OrderedTable<PoliticalSupplyRenewalIntent> intents;
};
Outcome<PoliticalSupplyRenewalCatalog> decode_political_supply_renewals(const BoundaryValue&);
BoundaryValue encode_political_supply_renewals(const PoliticalSupplyRenewalCatalog&);
// 引用只能落在同一原Office的接管规则与采办方案；同源重复意图拒绝，以免轮次间互相改判。
Status validate_political_supply_renewals(const PoliticalSupplyRenewalCatalog&,
    const PoliticalTakeoverCatalog&,const OrganizationCatalog&,const EconomicSupplyPolicyCatalog&);
}
