// 接管为独立零时长选择；本轮明确放弃后可以正常继续，不把资产处置失败塞回半日。
#pragma once
#include "naval_capture_settlement.hpp"
namespace hanmo::resident {
struct NavalDeclineSettlement { NavalState naval;NavalClaimDeclineFact fact;bool replayed=false; };
struct NavalContinuation { NavalState naval;NavalContinueFact fact;bool replayed=false; };
// 允许原军团统帅放弃其本部已产生的权利；不授予已失去军职者新的接管权限。
Outcome<NavalDeclineSettlement> settle_naval_decline_candidate(const CommonRead&,const MilitaryState&,const NavalState&,const StableId& actor,const NavalClaimDeclineCommand&,const StableId& fact_id);
// 调用者是当前真实玩家控制者，或没有可用玩家控制者时由原World自动结算入口调用。
Outcome<NavalContinuation> settle_naval_continue_candidate(const CommonRead&,const MilitaryState&,const MilitaryBattleCatalog&,const NavalState&,const NavalCatalog&,const StableId& actor,const NavalContinueCommand&,const StableId& fact_id);
}
