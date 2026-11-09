// 照护全史末尾的瞬态Action核验；仅正在释放的真实照护Action打开本轮到期窗口。
#pragma once
#include "family_birth.hpp"
namespace hanmo::resident::birth_detail {
// 本函数只核Action接缝，调用者必须先验证完整开工、终态和原粮来源；不能独立授照护资格。
inline Status validate_care_action_window(const CommonStore& c,const FamilyBirthState& s,const std::set<StableId>& ended,const StableId& releasing={}) {
    // 串行处理同一轮的多份到期工单时，其他恰好本tick到期的原Action允许暂留零。
    // 普通命令、冷存和最终World验证不传此ID，任何待结零Action仍被拒绝。
    bool window=false;
    if(!releasing.empty()) {
        const auto* start=s.care_starts.find(releasing);const auto* action=c.action(releasing);
        const auto* caregiver=start?c.actor(start->caregiver_actor_id):nullptr;
        if(ended.count(releasing)&&start&&!action) { /* 已释放的同一终态只作幂等冷读，不打开到期窗口。 */ }
        else {
        if(!start||!action||start->action_id!=releasing||action->kind!="family_child_care"||
           action->actor_id!=start->caregiver_actor_id||action->participants!=std::vector<StableId>{start->caregiver_actor_id}||
           action->start_tick!=start->tick||start->duration<=0||start->tick>2147483647-start->duration||
           c.tick()<start->tick||c.tick()>start->tick+start->duration||
           action->remaining_ticks!=start->tick+start->duration-c.tick()||!caregiver||caregiver->action_id!=releasing)
            return common_failure("ChildCareCompletionWindowRequired",releasing);
        window=true;
        }
    }
    for(const auto& row:s.care_starts.rows()) {
        const auto* action=c.action(row.first);
        if(ended.count(row.first)) {
            if(action&&row.first!=releasing)return common_failure("CompletedChildCareActionStillActive",row.first);
            continue;
        }
        const auto& start=row.second;const auto* caregiver=c.actor(start.caregiver_actor_id);
        if(!action||start.action_id!=row.first||action->kind!="family_child_care"||action->actor_id!=start.caregiver_actor_id||
           action->participants!=std::vector<StableId>{start.caregiver_actor_id}||action->start_tick!=start.tick||
           start.duration<=0||start.tick>2147483647-start.duration||c.tick()<start.tick||c.tick()>start.tick+start.duration||
           action->remaining_ticks!=start.tick+start.duration-c.tick()||(action->remaining_ticks==0&&!window)||
           !caregiver||caregiver->action_id!=row.first)return common_failure("ChildCareActionBindingMismatch",row.first);
    }
    return common_success();
}
}
