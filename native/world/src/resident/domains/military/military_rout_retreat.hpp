// 退路报价不读取玩家草案、不重掷AI、不改军籍；只借原地图地形和同轮真实占格。
#pragma once
#include "military_rout_model.hpp"
namespace hanmo::resident {
Status validate_military_rout_rule(const MilitaryRoutRetreatRule&,const BattlefieldDefinition&);
Outcome<MilitaryRoutRetreatQuote> quote_military_rout_retreat(const MilitaryBattleRecord&,
 const MilitaryBattleCatalog&,const MilitaryRoutRetreatRule&,const StableId& unit_id);
// 当轮落点仍须交原同步移动器；只有实际到达自己的出口才能退出，报价不能直接迁走单位。
}
