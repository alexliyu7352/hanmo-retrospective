// 只读原唯一Family控制链，回书完成phase不增加控制写者，也不把当前player当旧事实授权。
#pragma once
#include "task_message_return_contract.hpp"
namespace hanmo::resident {
struct FamilyState;struct FamilyLegacyState;
Outcome<StableId> task_message_return_player_before(const CommonStore&,const FamilyState&,const FamilyLegacyState*,Integer tick,const StableId& exact_fact,const DynamicStepInput* phase=nullptr);
}
