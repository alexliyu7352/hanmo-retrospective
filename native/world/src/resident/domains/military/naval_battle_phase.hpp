// 舟战只接原世界半日，规划/结算待决守卫不创造第二时钟。
#pragma once
#include "naval_battle_effects.hpp"
namespace hanmo::resident {
Status preflight_naval_battles(const CommonRead&,const NavalState&,const NavalCatalog&,Integer finish_tick);
// 原供给阶段后、同半日接敵安装前调用；新接敵战场从下一半日才结算。
Outcome<std::vector<StableId>> step_naval_battles(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,NavalState&,const NavalCatalog&,Integer finish_tick,const NavalCapturedJourneyEnd& end_journey={},const MilitaryTravelEndWithSource& end_with_source={});
}
