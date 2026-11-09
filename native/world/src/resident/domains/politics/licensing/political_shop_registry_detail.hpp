#pragma once
#include "political_shop_registry_rules.hpp"
#include "political_shop_licensing_detail.hpp"
namespace hanmo::resident::shop_registry_detail {
Outcome<PoliticalShopRegistryEndpoint> endpoint(const PoliticalShopRegistryContext&,const PoliticalShopRegistrySuccessorPolicy&);
Status authority_before(const PoliticalShopRegistryContext&,const PoliticalShopRegistryEndpoint&,const ActingAuthority&,const StableId& grant,const StableId& boundary,Integer tick);
Outcome<StableId> authority_now(const PoliticalShopRegistryContext&,const PoliticalShopRegistryEndpoint&,const ActingAuthority&);
const PoliticalShopRegistryAcceptance* latest(const PoliticalShopRegistryContext&,const StableId& court,Integer sequence);
}
