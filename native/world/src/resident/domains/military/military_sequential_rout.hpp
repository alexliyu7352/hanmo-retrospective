// 只在显式顺序轮的真实自身行动结算；不会让别部行动增加本部被堵轮数。
#pragma once
#include "military_rout_disposition.hpp"
namespace hanmo::resident {
Status synchronize_military_sequential_rout(MilitaryBattleRecord&,const MilitaryBattleCatalog&);
Outcome<MilitaryRoutDisposition> resolve_military_sequential_rout_turn(const MilitaryBattleRecord&,const MilitaryBattleCatalog&,const StableId& unit_id);
}
