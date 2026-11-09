// 舟战实际写口只运行在中央完整World候选内；Common/Naval/Military/Travel失败必须共同回滚。
#pragma once
#include "naval_capture_settlement.hpp"
#include "military_travel_end_source.hpp"
#include "naval_settlement.hpp"
#include "naval_continuation.hpp"
namespace hanmo::resident {
// 原共同动作终止入口由World注入；必须保实际接敌锚点并清理原Travel载荷/参与者，不重扣旅费。
using NavalCapturedJourneyEnd=std::function<Status(const StableId& action_id)>;
// Common船舱产权亦可能关联生产/税债/任务责任；两口必须绑定同一World候选，不能空实现冒证明。
struct NavalCaptureAssetBoundary {
 std::function<Status(const NavalCaptureFact&,const ResourceRecipe&)> before_transfer;
 std::function<Status(const NavalCaptureFact&)> after_transfer;
};
Outcome<NavalCaptureFact> apply_naval_capture(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,NavalState&,const NavalCatalog&,const StableId& actor,const NavalCaptureCommand&,const NavalCapturedJourneyEnd&,const NavalCaptureAssetBoundary&,const MilitaryTravelEndWithSource& end_with_source={});
Outcome<NavalClaimDeclineFact> apply_naval_decline(CommonWrite&,const MilitaryState&,NavalState&,const StableId& actor,const NavalClaimDeclineCommand&);
Outcome<NavalContinueFact> apply_naval_continue(CommonWrite&,const MilitaryState&,const MilitaryBattleCatalog&,NavalState&,const NavalCatalog&,const StableId& actor,const NavalContinueCommand&,const NavalCapturedJourneyEnd&,const MilitaryTravelEndWithSource& end_with_source={});
Outcome<NavalRoundFact> apply_naval_round(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,NavalState&,const NavalCatalog&,const StableId& battle_id,Integer finish_tick,const NavalCapturedJourneyEnd& end_journey={},const MilitaryTravelEndWithSource& end_with_source={});
}
