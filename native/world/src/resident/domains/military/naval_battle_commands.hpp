// 统一四种公开意图的军事桥；仍调用唯一原规则和实际资源事务，不保存通用 JSON。
#pragma once
#include "naval_battle_command_codec.hpp"
#include "naval_battle_effects.hpp"
namespace hanmo::resident {
struct NavalBattlePreviewAccess {
 // 只读责任门供公开preview给真实拒因，不能写任何候选或外部服务。
 std::function<Status(const NavalCaptureFact&,const ResourceRecipe&)> inspect_capture;
 bool journey_termination_available=false,capture_boundary_available=false;
};
struct NavalBattleCommandAccess : NavalBattlePreviewAccess {
 NavalCapturedJourneyEnd end_journey;
 NavalCaptureAssetBoundary capture_boundary;
 // 真实执行源转发，preview只沿原独立availability门；不把新回调当授权。
 MilitaryTravelEndWithSource end_journey_with_source;
};
Outcome<NavalBattleCommandQuote> preview_naval_battle_command(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalState&,const NavalCatalog&,const StableId& actor,const NavalBattleCommand&,const NavalBattlePreviewAccess&);
// 外层必须是完整 World 候选；内部任何晚 ID、资源、责任或事实失败都不可发布局部变化。
Outcome<NavalBattleCommandReceipt> apply_naval_battle_command(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,NavalState&,const NavalCatalog&,const StableId& actor,const NavalBattleCommand&,const NavalBattleCommandAccess&);
}
