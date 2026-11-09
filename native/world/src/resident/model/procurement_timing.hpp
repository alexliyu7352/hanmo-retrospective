// 采购与交付共用既有时间规则；只检最短必要时耗，不推测路线、NPC等待或将来价格。
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include <limits>
namespace hanmo::resident {
inline constexpr Integer trade_action_ticks=1;
inline constexpr Integer task_handover_ticks=1;
inline constexpr Integer authored_task_late_grace_ticks=2;
// 避免now+duration或deadline+grace溢出；时间刚好能装入窗口时允许。
inline bool task_finish_within(Integer now,Integer duration,Integer deadline,Integer grace=0){
    if(now<0||duration<0||deadline<0||grace<0||now>std::numeric_limits<Integer>::max()-duration)return false;
    if(now<=deadline){const auto remaining=deadline-now;return duration<=remaining||duration-remaining<=grace;}
    const auto overdue=now-deadline;return overdue<=grace&&duration<=grace-overdue;
}
}
