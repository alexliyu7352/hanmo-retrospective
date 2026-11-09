// 接册行为只来自显式作者意图；配置不授司法权，不撤销旧案，也不替代原公共批准。
#pragma once
#include "resident/domains/politics/licensing/political_shop_registry_rules.hpp"
namespace hanmo::resident {
inline constexpr const char* political_shop_registry_intents_module="political_shop_registry_intents";
inline constexpr const char* political_shop_registry_intent_template="shop_registry_acceptance";
struct PoliticalShopRegistryIntent {StableId id,actor_id,registry_policy_id;};
struct PoliticalShopRegistryIntentCatalog {Integer version=1;OrderedTable<PoliticalShopRegistryIntent> intents;};
Outcome<PoliticalShopRegistryIntentCatalog> decode_political_shop_registry_intents(const BoundaryValue&);
BoundaryValue encode_political_shop_registry_intents(const PoliticalShopRegistryIntentCatalog&);
Status validate_political_shop_registry_intents(const PoliticalShopRegistryIntentCatalog&,const CommonCatalog&,const PoliticalShopRegistrySuccessionCatalog&);
}
