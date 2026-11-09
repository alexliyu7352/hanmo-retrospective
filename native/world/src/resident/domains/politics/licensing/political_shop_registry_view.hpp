// 接册办理页只向到场持专项权的本人显示有限原册入口，不在页面改变案件或处分。
#pragma once
#include "political_shop_registry_rules.hpp"
namespace hanmo::resident {
Outcome<BoundaryValue> query_political_shop_registry_successions(const PoliticalShopRegistryContext&,const PoliticalTakeoverRead&,const StableId& place,Integer cursor=0);
BoundaryValue encode_political_shop_registry_acceptance_quote(const PoliticalShopRegistryAcceptanceQuote&);
}
