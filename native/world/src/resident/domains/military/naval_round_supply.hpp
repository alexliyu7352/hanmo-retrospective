// 舟战复用原军粮阶段的真实供给回执，不额外扣粮、推进时间或重算已冻结 AI。
#pragma once
#include "naval_round_history.hpp"
namespace hanmo::resident {
struct NavalSuppliedBattle { NavalBattleRecord battle;std::vector<StableId> source_ids; };
Outcome<NavalSuppliedBattle> naval_round_supply_source(const MilitaryState&,const NavalBattleRecord&,Integer finish_tick);
Outcome<NavalRoundFact> make_naval_supplied_round(const MilitaryState&,const NavalAssetState&,const NavalAssetCatalog&,const NavalBattlePolicy&,const MilitaryBattleCatalog&,const NavalBattleRecord&,const StableId& id,Integer finish_tick,const NavalGunneryPreparation* =nullptr);
Status validate_naval_supplied_round(const MilitaryState&,const NavalAssetState&,const NavalAssetCatalog&,const NavalBattlePolicy&,const MilitaryBattleCatalog&,const NavalBattleRecord&,const NavalRoundFact&,const NavalGunneryPreparation* =nullptr);
}
