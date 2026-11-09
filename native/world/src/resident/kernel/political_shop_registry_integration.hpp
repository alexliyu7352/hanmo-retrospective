// 接册World入口只借同候选完整接管权源；法律纯读仍持独立原册context。
#pragma once
#include "political_shop_licensing_integration.hpp"
#include "resident/domains/politics/licensing/political_shop_registry_profile.hpp"
#include "resident/domains/politics/licensing/political_shop_registry_view.hpp"
namespace hanmo::resident {
Status validate_political_shop_registry_world(const WorldBundle&);
Outcome<PoliticalShopRegistryAcceptanceQuote> prepare_political_shop_registry_world(const WorldBundle&,const StableId&,const PoliticalAcceptShopRegistry&);
Outcome<StableId> commit_political_shop_registry_world(WorldBundle&,const StableId&,const PoliticalAcceptShopRegistry&,const PoliticalShopRegistryAcceptanceQuote&);
Outcome<BoundaryValue> query_political_shop_registry_world(const WorldBundle&,const StableId& place,Integer cursor=0);
}
