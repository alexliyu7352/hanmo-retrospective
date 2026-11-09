// 原当面道别在两端Travel复行之后才落原resolved/result；不将单边复行视作完整离开。
#pragma once
#include "resident/domains/travel/travel_contact_capture.hpp"
#include "resident/domains/interaction/interaction_model.hpp"
#include <memory>
namespace hanmo::resident {
class CommonStore;
class PrivateShopTravelEncounterLeaveBoundary {
public:PrivateShopTravelEncounterLeaveBoundary()=default;
private:
 struct State;std::shared_ptr<const State> state_;
 friend Outcome<PrivateShopTravelEncounterLeaveBoundary> capture_private_shop_travel_encounter_leave_boundary(const CommonStore&,const TravelState&,const InteractionState&,const StableId&,const EncounterLeave&,const InteractionQuote&);
 friend struct PrivateShopTravelEncounterLeaveSourceAccess;
};
struct PrivateShopTravelEncounterLeaveSource {
 WorldIdentity source_identity;
 StableId original_actor_id;
 EncounterRecord original_encounter,resolved_encounter;
 std::array<TravelEncounterResumeDraft,2> actual_resumptions;
 InteractionEffect actual_effect;
};
// 完整原Interaction/Travel史及原preview先验；只取实际pending记录、参与人和两端活动旅行。
// 必须在整个apply_interaction(EncounterLeave)之前捕获；不保留set_journey_encounter回调。
Outcome<PrivateShopTravelEncounterLeaveBoundary> capture_private_shop_travel_encounter_leave_boundary(
 const CommonStore&,const TravelState&,const InteractionState&,const StableId& actual_actor,
 const EncounterLeave& original_command,const InteractionQuote& original_quote);
// 必须在原整个apply成功后，提交缓存分配之前消费。两份draft按原Encounter.action_ids顺序。
// 只接受原同tick、恰一个内部result、两端旧paused恢复/encounter清除，钱货和Action仍原值。
// 单边callback成功、末result失败或原记录未resolved均拒；由外层完整候选撤回暂态结果。
// 该内部result不是玩家提交结果，本口也不授付款、到达、商业归属或永久历史资格。
Outcome<PrivateShopTravelEncounterLeaveSource> read_private_shop_travel_encounter_leave(
 const CommonStore&,const TravelState&,const InteractionState&,
 const PrivateShopTravelEncounterLeaveBoundary&,
 const std::array<TravelEncounterResumeDraft,2>& actual_resumptions,const InteractionEffect& actual_effect);
}
