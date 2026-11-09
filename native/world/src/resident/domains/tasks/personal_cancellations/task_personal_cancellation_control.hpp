// 只读原唯一Family控制链，私人返报销约零时不增加控制写者，也不把当前player当旧事实授权。
#pragma once
#include "task_personal_cancellation_contract.hpp"
namespace hanmo::resident {
struct FamilyState;struct FamilyLegacyState;
Outcome<StableId> task_personal_cancellation_player_before(const CommonStore&,const FamilyState&,const FamilyLegacyState*,Integer tick,const StableId& exact_fact);
}
