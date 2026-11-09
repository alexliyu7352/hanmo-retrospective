// 意图只新增目录marker；冷计划须回到已验证原接册来源，不能借当前任职改写历史。
#pragma once
#include "political_shop_registry_director.hpp"
namespace hanmo::resident {
Status validate_political_shop_registry_intent_modules(bool,const std::vector<std::pair<StableId,StableId>>&);
Status validate_political_shop_registry_intent_director(const DirectorCatalog&,const DirectorState&,const PoliticalShopRegistryContext*);
}
