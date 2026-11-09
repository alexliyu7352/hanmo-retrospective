// 海战回合事实只由原完整候选产生；冷读重放已冻结输入，不能靠重算外层hash掩盖改写结果。
#pragma once
#include "naval_battle_state.hpp"
namespace hanmo::resident {
Outcome<NavalRoundFact> make_naval_round_fact(const NavalAssetState&,const NavalAssetCatalog&,const NavalBattlePolicy&,const MilitaryBattleCatalog&,const NavalBattleRecord&,const StableId& fact_id,Integer finish_tick,const NavalGunneryPreparation* =nullptr);
// source_assets/source_battle必须是调用者已校验的接敌或前一事实结果；不接受fact自称的起点。
Status validate_naval_round_fact(const NavalAssetState& source_assets,const NavalAssetCatalog&,const NavalBattlePolicy&,const MilitaryBattleCatalog&,const NavalBattleRecord& source_battle,const NavalRoundFact&,const NavalGunneryPreparation* =nullptr);
}
