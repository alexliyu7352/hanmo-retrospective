// 战后当前载荷仅排除有真实接管事实的原单位，军旅人口和历史名册仍保留原ID。
#pragma once
#include "naval_battle_state.hpp"
namespace hanmo::resident {
class CommonStore;
// 冷行旅只核原行动、共同事实序与启程船型预算；不授当前出航权，捕获后沿真实前后绑定回溯。
Outcome<std::optional<NavalTravelProof>> query_naval_journey_source(const CommonStore&,const MilitaryState&,const NavalAssetState&,const NavalAssetCatalog&,const TravelState&,const StableId& actor,const StableId& action_id,const StableId& plan_id,const NavalBattleState* combat_history=nullptr);
Outcome<NavalFormationObservation> observe_naval_combat_manifest(const CommonRead&,const MilitaryState&,const NavalAssetState&,const NavalBattleState&,const NavalFleetRecord&);
Outcome<NavalFormationQuote> validate_naval_combat_manifest(const CommonRead&,const MilitaryState&,const NavalState&,const NavalCatalog&,const NavalFleetRecord&);
Outcome<std::optional<NavalTravelProof>> preview_naval_combat_travel(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalState&,const NavalCatalog&,const StableId& actor);
}
