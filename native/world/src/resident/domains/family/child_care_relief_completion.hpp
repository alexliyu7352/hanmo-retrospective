// 玩家计划/共同Completion只读自己的具名接手照护终态；不以Action消失或对白等待猜已完成。
#pragma once
#include "child_care_relief.hpp"
namespace hanmo::resident {
struct ChildCareReliefCompletionSource {WorldIdentity source_identity;StableId action_id,fact_id,accepted_id,original_birth_accepted_id,provider_actor_id,child_actor_id,code,interruption_fact_id;Integer started_tick=0,completed_tick=0,food_consumed=0;bool success=false;};
// 同候选Birth/Life/本域完整史先验；本口额外核唯一开始、结束、原事实序和逐期数量。
Outcome<std::optional<ChildCareReliefCompletionSource>> read_child_care_relief_completion(const CommonStore&,const ChildCareReliefState&,const StableId& provider_actor_id,const StableId& action_id);
}
