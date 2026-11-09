// 原照护Action终态的有限纯读合同，供共同Completion/PlayerPlan消费；不接受通用等待或对白替身。
#pragma once
#include "family_birth.hpp"
namespace hanmo::resident {
struct ChildCareCompletionSource {
    WorldIdentity source_identity;
    StableId action_id,fact_id,accepted_id,caregiver_actor_id,child_actor_id,code,interruption_fact_id;
    Integer started_tick=0,completed_tick=0,food_consumed=0;
    bool success=false;
};
// 调用方已完整验证同一候选FamilyBirth和FamilyLife历史；本口额外核实际头、唯一结束、原Action及事实序。
// 未结束的原照护返回空；错误人、伪造头、重复或未知结局返回error，不补写Completion或消费粮食。
Outcome<std::optional<ChildCareCompletionSource>> read_child_care_completion(const CommonStore&,const FamilyBirthState&,const StableId& caregiver_actor_id,const StableId& action_id);
}
