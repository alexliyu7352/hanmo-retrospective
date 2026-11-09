// 溃退指令由原轮初公开状态确定并保存为自动意图，玩家不能改写目标或借它再次攻击。
#pragma once
#include "military_rout_retreat.hpp"
#include "military_rout_status.hpp"
namespace hanmo::resident {
Outcome<MilitaryOrder> make_military_rout_order(const MilitaryBattleRecord&,const MilitaryBattleCatalog&,const StableId&);
Outcome<MilitaryOrderQuote> quote_military_rout_order(const MilitaryBattleRecord&,const MilitaryBattleCatalog&,const StableId&,const MilitaryOrder&);
}
