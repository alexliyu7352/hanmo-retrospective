// 具名原Family边界的控制纯读；只校两种真实控制链的前序，不建立许可或第二控制账。
#pragma once
#include "family_legacy.hpp"
namespace hanmo::resident {
// 原Family/Legacy机械链先验；接受/完成/撤回载荷tick由调用域核，完成头自身不计入提交前玩家。
Outcome<StableId> read_voluntary_player_before_fact(const CommonStore&,const FamilyState&,
 const FamilyLegacyState*,Integer tick,const StableId& actual_fact);
}
