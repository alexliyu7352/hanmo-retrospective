// 原已交命令因本半日真实逃散失去执行者/目标时，生成具名取消事件；不重算AI或抽新随机数。
#pragma once
#include "military_supply_attrition_rules.hpp"
namespace hanmo::resident {
struct MilitaryAttritionBattlePreparation {MilitaryBattleRecord battle;std::vector<MilitaryBattleEvent> interrupted_orders;};
Outcome<MilitaryAttritionBattlePreparation> prepare_military_supply_attrition_battle(const MilitaryState&,const MilitaryCatalog&,const MilitaryBattleRecord&,Integer finish_tick);
}
