// 只借原回书永久账与真实Advance；不新开收信、不重写原Task报告和资金结算。
#include "plan_binding.hpp"
#include "resident/model/procurement_timing.hpp"
#include "resident/kernel/world_bundle.hpp"
namespace hanmo::resident {
// 旧Action已被完整回书门验证，仍逐本人/开工/剩余半日精确匹配。
bool player_plan_message_return_continuation(const CommonStore& common,const TaskMessageReturnState& state,const ActionCoreView& action){
    if(action.kind!=task_message_return_collect_command)return false;
    auto source=query_task_message_return_action(common,state,action.actor_id,action.id);
    return source.ok&&source.value&&source.value->status=="active"&&source.value->actor_id==action.actor_id&&source.value->action_id==action.id&&source.value->started_tick==action.start_tick&&source.value->remaining_ticks==action.remaining_ticks&&action.remaining_ticks==task_handover_ticks;
}
// 仅显式选中的回书模块可续办，缺独立账时严格拒绝。
bool player_plan_message_return_continuation(const WorldBundle& w,const ActionCoreView& action){
    return w.catalog->task_message_returns&&w.tasks.message_returns&&player_plan_message_return_continuation(w.common,*w.tasks.message_returns,action);
}
// 原账唯一完成与实际结果已在提交尾绑定；这里只读，不递归验证World。
Outcome<std::optional<TaskMessageReturnActionSource>> player_plan_message_return_completion(const WorldBundle& w,const StableId& actor,const StableId& action,const ReceiptDetails& receipt,Integer tick,const StableId& result_id){
    if(!w.catalog->task_message_returns)return {true,std::nullopt,{}};
    if(!w.tasks.message_returns)return {false,{},common_failure("TaskMessageReturnProfileRequired").error};
    return query_task_message_return_completion(w.common,*w.tasks.message_returns,actor,action,receipt,tick,result_id);
}
}
