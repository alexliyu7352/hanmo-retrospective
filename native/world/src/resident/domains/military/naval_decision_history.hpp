// 同一战果后零时长选择的冷来源；不执行资源recipe，不改原军籍或船主。
#pragma once
#include "naval_contact_history.hpp"
namespace hanmo::resident {
Status validate_naval_capture_history(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const NavalAssetState&,const NavalCatalog&,const NavalBattleState& before,const NavalCaptureFact&,const NavalNavigationState* =nullptr);
Status validate_naval_decline_history(const CommonStore&,const MilitaryState&,const NavalBattleState& before,const NavalClaimDeclineFact&);
Outcome<NavalBattleRecord> naval_continuation_history_source(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const NavalAssetState&,const NavalCatalog&,const NavalBattleState& before,const NavalContinueFact&,const NavalGunneryPreparation* =nullptr,const NavalNavigationState* =nullptr);
Status validate_naval_capture_history(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const NavalAssetState&,const NavalCatalog&,const NavalBattleState&,const NavalCaptureFact&,const NavalNavigationState*,const NavalOriginalSourceContext*);
Status validate_naval_decline_history(const CommonStore&,const MilitaryState&,const NavalBattleState&,const NavalClaimDeclineFact&,const NavalOriginalSourceContext*);
Outcome<NavalBattleRecord> naval_continuation_history_source(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const NavalAssetState&,const NavalCatalog&,const NavalBattleState&,const NavalContinueFact&,const NavalGunneryPreparation*,const NavalNavigationState*,const NavalOriginalSourceContext*);
}
