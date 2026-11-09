// 正式舟战计划沿原军权写唯一事实；热草案和 AI 冻结边界各自独立。
#pragma once
#include "naval_battle_state.hpp"
namespace hanmo::resident {
struct NavalPlanQuote { bool replayed=false;NavalPlanFact fact;NavalBattleRecord candidate; };
Outcome<NavalPlanQuote> preview_naval_plan(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const NavalState&,const NavalCatalog&,const StableId& actor,const NavalPlanCommand&);
// 本口写 Common ID/事实与 Naval，任何晚失败都要求外层丢弃整个世界候选。
Outcome<NavalPlanFact> apply_naval_plan(CommonWrite&,const MilitaryState&,const MilitaryCatalog&,NavalState&,const NavalCatalog&,const StableId& actor,const NavalPlanCommand&);
// 半日结算必须认原正式计划，不接受直接改 pending_orders 的悬空状态。
Status validate_naval_pending_plan(const NavalState&,const NavalBattleRecord&);
}
