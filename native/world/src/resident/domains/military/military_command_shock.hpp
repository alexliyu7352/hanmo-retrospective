// 原同步轮次在真实伤害/人物损失之后、溃退归类之前只结算一次；无随机数和独立时钟。
#pragma once
#include "battle_model.hpp"
#include "military_command_shock_contract.hpp"
namespace hanmo::resident {
struct MilitaryCommandShockResolution {
 MilitaryBattleRecord battle;MilitaryCommandShockState state;std::vector<MilitaryBattleEvent> events;
};
Status validate_military_command_shock_rule(const MilitaryCommandShockRule&);
Status validate_military_command_shock_shape(const MilitaryBattleRecord&,const MilitaryCommandShockState&);
Outcome<MilitaryCommandShockResolution> resolve_military_command_shock(
 const MilitaryBattleRecord& before,const MilitaryBattleRecord& after_damage,
 const MilitaryOrders& actual_orders,const MilitaryBattleCatalog&,
 const MilitaryCommandShockRule&,const MilitaryCommandShockState&);
// 显式顺序轮按实际单次行动后的差分结算，接受本轮已经发生的恰一次源。
Outcome<MilitaryCommandShockResolution> resolve_military_sequential_command_shock(
 const MilitaryBattleRecord& before_action,const MilitaryBattleRecord& after_action,
 const MilitaryOrders& actual_order,const MilitaryBattleCatalog&,
 const MilitaryCommandShockRule&,const MilitaryCommandShockState&);
}
