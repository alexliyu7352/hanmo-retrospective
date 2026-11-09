// 远方战斗的冻结战果由原统帅作同tick战术选择；不创建NPC行旅行动或第二只时钟。
#pragma once
#include "naval_battle_effects.hpp"
namespace hanmo::resident {
using NavalSettlementCommand=std::variant<NavalCaptureCommand,NavalClaimDeclineCommand,NavalContinueCommand>;
struct NavalSettlementIntent {StableId actor_id,battle_id,claim_vessel_id;NavalSettlementCommand command;bool system_continuation=false;};
// failure_claims只含同一候选刚经历整World回滚的接管尝试；其后明确放弃，不能无限重试。
// 返回空代表剩余决定应由玩家处理。调用者每执行一条后重查，循环上界为原claims数加battle数。
Outcome<std::optional<NavalSettlementIntent>> next_naval_npc_settlement(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const NavalState&,const NavalCatalog&,const StableId& player_actor_id,const std::vector<std::pair<StableId,StableId>>& failure_claims={});
}
