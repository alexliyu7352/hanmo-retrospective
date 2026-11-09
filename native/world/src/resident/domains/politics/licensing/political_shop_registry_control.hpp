// 接册只读真实控制链和原批准头；不反向枚举经济/任务/私铺责任。
#pragma once
#include "political_shop_registry_contract.hpp"
#include "political_shop_licensing.hpp"
namespace hanmo::resident {
struct PoliticalShopRegistryControlSource {
 WorldIdentity source_identity;
 StableId organization_id,registration_fact_id,occupation_fact_id,approval_fact_id;
 Integer revision=0,occupation_tick=0,observed_tick=0;
};
// 原政治完整批准/逐责任合法性由World先验，实际接册热门另调用PoliticalTakeoverRead。
// 本口负责真实军事清单、控制链、Common精确事实序和批准链的纯来源投影。
Outcome<PoliticalShopRegistryControlSource> read_political_shop_registry_control(
 const PoliticalShopControlContext&,const PoliticsState&,const PoliticsCatalog&,
 const PoliticalShopCourt&,const StableId& actual_boundary={},std::optional<Integer> tick=std::nullopt);
}
