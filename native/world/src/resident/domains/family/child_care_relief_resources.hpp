// 新志愿合同只用提供者自己的未预约粮，原出生hold保持独立，Common仍为唯一库存账。
#pragma once
#include "child_care_relief.hpp"
namespace hanmo::resident {
struct ChildCareReliefSupplyQuote {WorldIdentity identity;ChildCareReliefSource source;Integer quantity=0,inventory_revision=0,available_before=0;};
Outcome<ChildCareReliefSupplyQuote> preview_child_care_relief_supply(const CommonStore&,const ChildCareReliefSource&);
Status validate_child_care_relief_reservation(const CommonStore&,const ChildCareReliefState&,const StableId& accepted_id,Integer remaining);
Outcome<ResourceRecipe> reserve_child_care_relief_recipe(const CommonStore&,const ChildCareReliefState&,const ChildCareReliefSupplyQuote&,const StableId& accepted_id);
Outcome<ResourceRecipe> consume_child_care_relief_recipe(const CommonStore&,const ChildCareReliefState&,const StableId& accepted_id,Integer remaining,Integer consumed);
Outcome<ResourceRecipe> release_child_care_relief_recipe(const CommonStore&,const ChildCareReliefState&,const StableId& accepted_id,Integer remaining);
}
