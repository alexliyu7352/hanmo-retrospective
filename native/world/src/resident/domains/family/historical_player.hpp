// 已完整验证世界中的历史玩家；事实边界保同tick内的真实先后。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
class CommonStore;
struct FamilyState;
// before_fact_id必须是本世界已存在的Common事实；返回该事实写入前、指定tick的合法玩家。
// 本读口独立核有序控制链与所指事实；其他World/Family语义仍由完整准入校验，不授控制或财产。
Outcome<StableId> family_player_before_fact(const CommonStore&,const FamilyState&,Integer tick,const StableId& before_fact_id);
}
