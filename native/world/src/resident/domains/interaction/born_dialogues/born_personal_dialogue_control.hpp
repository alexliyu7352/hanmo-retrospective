// 本域打开事实前的原控制链；与Legacy2消费形成有向事实序，不调用对方完整validator。
#pragma once
#include "born_personal_dialogue_state.hpp"
namespace hanmo::resident {
struct FamilyState;struct FamilyLegacyState;
Outcome<StableId> born_personal_dialogue_player_before(const CommonStore&,const FamilyState&,const FamilyLegacyState*,Integer,const StableId& opening_fact);
}
