// 原创建初始HP/疲劳，只复用Installed投影及唯一Family身份来源验证。
#pragma once
#include "growth_personal_condition_contract.hpp"
#include "growth_personal_mastery_history.hpp"
#include "growth_skill_origin.hpp"
namespace hanmo::resident {
// initial由正规选中scenario.initial构造并随PreparedCatalog持有。其actor集合只作原种子存在证明；
// learned不会被读为身体来源。四个已审原创建写者均原始100HP/0疲劳，当前值不作初态证明。
// 前置完整Common/Family/Creation/Birth；稳定态调用，动态create尚未落原事实时必须失败。
Outcome<GrowthPersonalConditionActorOrigin> read_growth_condition_actor_origin(
    const CommonStore&,const GrowthSkillOriginContext&,const GrowthPersonalMasteryInitialCatalog&,
    const StableId& actor);
}
