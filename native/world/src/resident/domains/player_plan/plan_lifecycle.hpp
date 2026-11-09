// 生命周期只编辑当前意向或登记原命令边界，绝不启动、取消或结算原领域行动。
#pragma once
#include "plan_model.hpp"
namespace hanmo::resident {
Outcome<StableId> edit_player_plan(CommonStore&,PlayerPlanState&,const StableId& actor_id,const PlayerPlanEdit&);
const PlayerPlanStep* next_player_plan_step(const PlayerPlanState&);
PlayerPlanStep* next_player_plan_step(PlayerPlanState&);
// 相同真实边界重放无副作用；不同action/result不能覆盖已登记的引用。
Status player_plan_record_started(PlayerPlanState&,const StableId& plan_id,const StableId& step_id,const std::optional<StableId>& action_id,const StableId& result_id,Integer tick);
Status player_plan_record_completed(PlayerPlanState&,const StableId& plan_id,const StableId& step_id,const std::optional<StableId>& action_id,const StableId& result_id,Integer tick,bool success,const StableId& code={});
Status player_plan_stop(PlayerPlanState&,const StableId& step_id,const StableId& reason,bool needs_confirmation=false);
// 手动操作与读档停止驱动的持久解释，不撤销原行动或当前步骤的已接受事实。
void player_plan_pause(PlayerPlanState&,const StableId& reason);
Status player_plan_set_budget(PlayerPlanState&,Integer tick,Integer max_additional_ticks,Integer max_gross_spend);
// 支出取共同钱包负变动观察，收入不得抵扣；越界返回失败并且不改计划。
Status player_plan_consume_budget(PlayerPlanState&,Integer additional_ticks,Integer gross_spend);
} // 调用方必须和实际领域副作用共同提交或共同丢弃。
