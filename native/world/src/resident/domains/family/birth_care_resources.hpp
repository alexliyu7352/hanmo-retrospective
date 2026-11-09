// 生育安排的有限私粮预留与真实照护消耗；不把预留本身称为已喂养，不碰共同/公有资产。
#pragma once
#include "birth_arrangement_source.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
struct BirthCareSupplyQuote {
    WorldIdentity identity;
    BirthCareTerms terms;
    Integer quantity=0,inventory_revision=0,available_before=0;
};
// 仅供已验本人意愿的生育账调用；首次承诺足额个人可用粮，不通过预览分配事实或批次。
Outcome<BirthCareSupplyQuote> preview_birth_care_supply(const CommonStore&,const BirthCareTerms&);
// 接受事实须已在同一候选原址登记并受共同保存点保护，原reservation resolver才会承认它。
Outcome<ResourceRecipe> reserve_birth_care_recipe(const CommonStore&,const BirthCareSupplyQuote&,const StableId& accepted_fact_id);
// 原计划自己的精确contract预留：未定义钱款、他人货物、异地容器或额外预留都拒绝。
Status validate_birth_care_reservation(const CommonStore&,const BirthCareTerms&,const StableId& accepted_fact_id,Integer expected_remaining);
// 只由已核真实照护Action完成入口调用。此叶核资源，不替代照护者本人/儿童/地点/时间证明。
Outcome<ResourceRecipe> consume_birth_care_recipe(const CommonStore&,const BirthCareTerms&,const StableId& accepted_fact_id,Integer remaining_before,Integer consumed);
// 取消或有限照护终态仅释放未消费原粮；已消耗不返还、不生成替代品、不转给另一人物。
Outcome<ResourceRecipe> release_birth_care_recipe(const CommonStore&,const BirthCareTerms&,const StableId& accepted_fact_id,Integer expected_remaining);
}
