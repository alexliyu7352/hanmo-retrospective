// 舟战航向与水流只约束原六角移动报价；不拥有世界航程、船体或随机流。
#pragma once
#include "battle_model.hpp"
namespace hanmo::resident {
// 只对作者逐格指定的水流加价，缺少流向的合法水格视作静水。
struct NavalTacticalCurrent { HexPoint position;Integer heading=0; };
struct NavalTacticalNavigationPolicy {
 StableId profile="hex_helm_current_v1";
 Integer quarter_turn_cost=1,upstream_cost=1;
 std::vector<NavalTacticalCurrent> currents;
};
// 无战场时只验有限形状；有战场时逐条核真实水格，不能用场外流向补造航路。
Status validate_naval_tactical_navigation(const NavalTacticalNavigationPolicy&,const BattlefieldDefinition* =nullptr);
// 每次60度转向累计至整条路径，含最终朝向；向上取整90度单位后收一次显式单价。
// final_heading为空时返回各格最便宜的到达航向，否则连最终转向一起计价。
Outcome<std::vector<HexPath>> naval_tactical_navigation_routes(
 const BattlefieldDefinition&,const NavalTacticalNavigationPolicy&,HexPoint from,Integer heading,Integer budget,
 const std::vector<HexPoint>& occupied,const std::vector<HexPoint>& stops,
 std::optional<Integer> final_heading=std::nullopt);
}
