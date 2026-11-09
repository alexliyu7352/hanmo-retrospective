// 原生日到达成年阈值只开放本人行动资格，不赠技能、财产、关系或玩家控制。
#pragma once
#include "family_birth.hpp"
namespace hanmo::resident {
// 已有成熟记录重入返回原ID。实际自由处境不可证时只记年龄到达，不强行解除他域限制。
Outcome<StableId> record_child_maturation(CommonStore&,const InteractionCatalog&,const FamilyCatalog&,const BirthArrangementCatalog&,FamilyBirthState&,const BirthArrangementAccess&,const StableId& child_actor_id);
// 原World候选专用，写集仅Common＋FamilyBirth；晚失败必须整体回滚。
Outcome<StableId> record_child_maturation_in_candidate(CommonStore&,const InteractionCatalog&,const FamilyCatalog&,const BirthArrangementCatalog&,FamilyBirthState&,const BirthArrangementAccess&,const StableId& child_actor_id);
}
