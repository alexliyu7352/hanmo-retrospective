// 舟战作者明确启用新政策，原海军资产作者解码入口保持严格段1语义。
#pragma once
#include "naval_battle_state.hpp"
#include "naval_authoring.hpp"
namespace hanmo::resident {
struct NavalCombatAuthoringSeed { NavalCatalog catalog;NavalState state; };
Outcome<std::optional<NavalCombatAuthoringSeed>> decode_naval_combat_profile(const BoundaryValue& data,const BoundaryValue& scenario,const MilitaryBattleCatalog&);
}
