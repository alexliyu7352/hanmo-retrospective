// 续签计划只引用已完整核验的政治与供给账；缺省目录继续拒绝新模板和marker。
#pragma once
#include "politics_supply_renewal_director.hpp"
#include "politics_supply_renewal_catalog.hpp"
namespace hanmo::resident {
Status validate_political_supply_renewal_director(const DirectorCatalog&,const DirectorState&,
    const PoliticalTakeoverContext*,const EconomicSupplyPolicyContext*);
Status validate_political_supply_renewal_modules(bool enabled,const std::vector<std::pair<StableId,StableId>>&);
}
