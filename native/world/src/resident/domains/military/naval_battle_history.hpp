// 实船与战斗事实同序重放的正式Naval2冷门；不能只验当前frame或玩家自称before。
#pragma once
#include "naval_contact_history.hpp"
#include "naval_asset_history.hpp"
namespace hanmo::resident {
// 先完成Common/Military/Travel/Family原域合法性，再调用本门；不回写任何候选。
// Naval6专用早结构门：机械重放仍完整，但XP证明必须在同World最终Growth门后补核。
Status validate_naval_battle_structure_for_gunnery(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const TravelState&,const NavalState&,const NavalCatalog&,const NavalHistoryAccess&);
Status validate_naval_battle_history(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const TravelState&,const NavalState&,const NavalCatalog&,const NavalHistoryAccess&);
// Original Naval battle replay with explicit exact-repair sources in the same
// asset stream. This does not reinterpret legacy rounds or create new battles.
Status validate_naval_battle_history_with_exact_repairs(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const TravelState&,const NavalState&,const NavalCatalog&,const NavalHistoryAccess&,const ExactNavalAssetHistoryContext&);
using NavalBattlePrefixObserver=std::function<Status(const NavalBattleState&)>;
// Synchronous borrowed original battle predecessor at a verified C21 Common
// start/result cut. The actual repair owner verifies its terms and actual M.
Status visit_naval_battles_before_exact_repair_source(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const TravelState&,const NavalState&,const NavalCatalog&,const NavalHistoryAccess&,const ExactNavalAssetHistoryContext&,const SourceRef&,Integer schema,const NavalBattlePrefixObserver&);

// Original-source provenance during the immediate contact/crew consequence.
// All original Naval events are replayed and then borrowed to the observer;
// final current load/battle equality remains mandatory at stable publication.
Status visit_naval_battle_source_history_with_exact_repairs(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const TravelState&,const NavalState&,const NavalCatalog&,const NavalHistoryAccess&,const ExactNavalAssetHistoryContext&,const NavalBattlePrefixObserver&);

using NavalOriginalPrefixObserver=std::function<Status(const NavalAssetState&,const NavalBattleState&)>;
Status visit_naval_history_at_actual_prefix(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const TravelState&,const NavalState&,const NavalCatalog&,const NavalHistoryAccess&,const ExactNavalAssetHistoryContext&,const CommonActualFactPrefix&,const WorldMoment& at,const NavalOriginalPrefixObserver&);
// Literal original nonclosed bindings -> fleet -> army -> commander + members.
// No alive/active/contact-roster filter; all missing source reads remain errors.
Outcome<bool> naval_actor_held_at_actual_prefix(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const TravelState&,const NavalState&,const NavalCatalog&,const NavalHistoryAccess&,const ExactNavalAssetHistoryContext&,const StableId& actor_id,const CommonActualFactPrefix&,const WorldMoment& at);
}
