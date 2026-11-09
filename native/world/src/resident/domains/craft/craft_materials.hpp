// Common精确选批是唯一料量writer。本叶保存实际接受/工序所需的最小选批映射。
#pragma once
#include "craft_job_model.hpp"
namespace hanmo::resident {
struct CraftMaterialReadContext {
 const CommonStore& common;const CommonCatalog& common_catalog;const CraftJobsState& jobs;const CraftRecipeCatalog& recipes;
 const ItemInstancesState& instances;const LotPropertiesState& properties;const ItemServiceCatalog& items;
};
// 开工纯核：完整逐材料数量/替代/性质、重复选批与原holder；权源/保管另由原接受源核。
Status validate_craft_material_selection(const CraftMaterialReadContext&,const CraftTerms&,const WorldMoment&);
// 只接本候选已经入唯一job表的接受行；任何错误由外层撤整个World候选。
Outcome<std::vector<CraftReservedMaterial>> reserve_craft_materials_in_candidate(CommonStore&,const CraftMaterialReadContext&,const StableId& job_id);
// 阈值与真实gate核验归专业gate writer，本口仅该唯一实际stage写原精确Consume。
Outcome<std::vector<CraftConsumedMaterial>> consume_craft_stage_materials_in_candidate(CommonStore&,const CraftMaterialReadContext&,const StableId& job_id,Integer stage_ordinal,const WorldMoment&);
// 按实际已采用工序重放，不读取今天料量代替已消耗历史。
Outcome<Integer> read_craft_consumed_selection_quantity(const CraftMaterialReadContext&,const CraftJob&,Integer selection_ordinal);
Status validate_craft_material_history(const CraftMaterialReadContext&,const CraftJob&);
}
