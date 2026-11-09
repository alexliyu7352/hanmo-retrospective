// 到期前真实亲代死亡的原安排终止；不为已出生儿童推断免责或自动改照护人。
#pragma once
#include "family_birth.hpp"
namespace hanmo::resident {
// 调用方须完成clock_commit与实际FamilyLife登记；仅未出生的原接受安排可以释放剩余私粮。
// 已有真实终态重入复用原ID，不再分配事实或释放第二次。
Outcome<StableId> close_unborn_arrangement_after_death(CommonStore&,const InteractionCatalog&,const FamilyCatalog&,const BirthArrangementCatalog&,FamilyBirthState&,const BirthArrangementAccess&,const StableId& accepted_id);
// 原World候选/Common保存点专用，失败由外层Common＋FamilyBirth写集完整回滚。
Outcome<StableId> close_unborn_arrangement_after_death_in_candidate(CommonStore&,const InteractionCatalog&,const FamilyCatalog&,const BirthArrangementCatalog&,FamilyBirthState&,const BirthArrangementAccess&,const StableId& accepted_id);
}
