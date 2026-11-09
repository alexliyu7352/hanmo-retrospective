// PlayerPlan/2沿原预算精确累计，只接受原成功advance的真实跨度；无第二run或墙钟。
#pragma once
#include "plan_model.hpp"
namespace hanmo::resident {
Status validate_player_plan_time_budget(const PlayerPlanState&,ExactArithmeticBudget budget={});
Outcome<HalfDayDuration> read_player_plan_remaining_duration(const PlayerPlanState&,ExactArithmeticBudget budget={});
Status player_plan_consume_exact_budget(PlayerPlanState&,const HalfDayDuration& actual_advance,Integer gross_spend,ExactArithmeticBudget budget={});
// 只用于中央已完整验证的旧世界显式/1→/2迁移；本口不注册profile、不重授步骤或预算。
Outcome<PlayerPlanState> migrate_validated_player_plan_time2(const PlayerPlanState&,ExactArithmeticBudget budget={});
}
