// 中央串行接原领域与有限计划；所有持久变化都留在原事务候选内。
#pragma once
#include "world_bundle.hpp"
#include "resident/domains/player_plan/plan_execution.hpp"
namespace hanmo::resident {
class CompleteExactCommandCapture;
Outcome<PlayerPlanQuote> preview_player_plan_command(const WorldBundle&,const StableId&,const PlayerPlanCommand&);
Outcome<ReceiptDetails> apply_player_plan_command(WorldBundle&,const StableId&,const PlayerPlanQuote&,CompleteExactCommandCapture* capture=nullptr);
Status finish_player_plan_commit(WorldBundle&,const StableId&,const PlayerPlanExecution*,const ReceiptDetails&,const StableId&);
bool player_plan_pins_result(const PlayerPlanState&,const StableId&);
} // 这里不创建第二个世界、钱包、线程或领域模拟器。
