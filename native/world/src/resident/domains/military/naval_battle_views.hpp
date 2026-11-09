// 舟战页面只公开本人真实参与战场；AI冻结计划、完整库存与随机状态不出只读边界。
#pragma once
#include "naval_plan_command.hpp"
#include "naval_battle_commands.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
// 当前页只沿真实参战来源选唯一未闭战场；没有战场返回active=false。
Outcome<BoundaryValue> naval_current_battle_page(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalState&,const NavalCatalog&,const StableId& actor,const NavalBattlePreviewAccess& application={});
Outcome<BoundaryValue> naval_battle_page(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalState&,const NavalCatalog&,const StableId& actor,const StableId& battle_id,const NavalBattlePreviewAccess& application={});
Outcome<BoundaryValue> naval_battle_draft_page(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalState&,const NavalCatalog&,const StableId& actor,const NavalPlanCommand&,const StableId& selected_unit);
}
