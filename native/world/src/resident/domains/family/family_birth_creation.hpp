// 生育计划在真实到期且原照护条件仍成立时唯一兑现；不是收养、重新抽人或世代跳过。
#pragma once
#include "family_birth_identity.hpp"
#include "resident/domains/interaction/interaction_model.hpp"
#include "resident/domains/social/social_model.hpp"
namespace hanmo::resident {
// The original fixed/legacy newborn appearance algorithm, shared by both selectors.
Outcome<std::shared_ptr<const AppearanceProfile>> make_original_birth_appearance(const IdentityPrototype&);
Status validate_original_birth_appearance(const IdentityPrototype&,const AppearanceProfile&);
// 返回原出生的人物ID；已有记录的重入只核历史，不抽随机、改名或补发初始资产。
Outcome<StableId> fulfill_birth_arrangement(CommonStore&,const InteractionCatalog&,const FamilyCatalog&,const BirthArrangementCatalog&,FamilyBirthState&,InteractionState&,SocialState&,const BirthArrangementAccess&,const StableId& accepted_id);
// 整个World候选或外层Common保存点专用；调用方必须备份全部typed写集，任一失败整体丢弃/回滚。
// 不自行开启或提交保存点；不得把失败的候选继续用于后续命令。
// 本入口写Common、FamilyBirth、Interaction知识索引和Social记忆索引。
Outcome<StableId> fulfill_birth_arrangement_in_candidate(CommonStore&,const InteractionCatalog&,const FamilyCatalog&,const BirthArrangementCatalog&,FamilyBirthState&,InteractionState&,SocialState&,const BirthArrangementAccess&,const StableId& accepted_id);
}
