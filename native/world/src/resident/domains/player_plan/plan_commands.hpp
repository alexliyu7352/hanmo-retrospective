// 玩家计划只允许已声明的编辑、预算与内部单步，不接任意原生命令包。
#pragma once
#include "plan_model.hpp"
namespace hanmo::resident {
struct PlayerPlanApproveBudget { Integer ticks=0,spend=0; };
struct PlayerPlanInternalStep { StableId plan_id,step_id; };
using PlayerPlanCommandData=std::variant<PlayerPlanEdit,PlayerPlanApproveBudget,PlayerPlanInternalStep>;
struct PlayerPlanCommand { StableId kind;PlayerPlanCommandData data; };
struct PlayerPlanExecution;
// 执行报价只在一次同源候选中存活，不能写进世界或存档。
struct PlayerPlanQuote { PlayerPlanCommand command;std::optional<PlayerPlanTerms> terms;std::shared_ptr<const PlayerPlanExecution> execution; };
bool player_plan_command_kind(const StableId&);
Outcome<PlayerPlanCommand> decode_player_plan_command(const StableId&,const BoundaryValue&);
} // 内部single-step类型只能由现run controller构造，JSON入口不授予它。
