// 舟战跨域只绑定当前唯一World；公开命令不接收客户端权源或第二份资产状态。
#pragma once
#include "resident/domains/military/naval_battle_commands.hpp"
#include "resident/domains/military/naval_contact_history.hpp"
namespace hanmo::resident {
struct WorldBundle;
NavalBattlePreviewAccess naval_preview_access(const WorldBundle&);
NavalBattleCommandAccess naval_command_access(WorldBundle&);
NavalHistoryAccess naval_history_access(const WorldBundle&);
Status run_naval_settlement_decisions(WorldBundle&);
// 显式维修计划只通过原命令总线和完整NPC候选执行。
Status run_naval_repair_decisions(WorldBundle&);
Status validate_naval_command_history(const WorldBundle&);
}
