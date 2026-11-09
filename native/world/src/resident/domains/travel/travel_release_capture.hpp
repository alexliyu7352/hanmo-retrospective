// 原行动释放前的拥有值，仅供有真实原因的caller在同候选内保留；它本身不是结束来源。
#pragma once
#include "travel_plan_capture.hpp"
namespace hanmo::resident {
struct TravelReleaseSnapshot {
    ActionCoreView action;
    Integer source_tick=0;
    Location actual_location;
    std::variant<TerrainTravelParams,RoadTravelParams,RescueParams> original_payload;
    std::optional<TerrainJourneyFact> terrain_plan;
};
// 只核当前原Action/人物占用/唯一载荷并拷贝，不分配ID、不改位置、不执行release或推定完成。
Outcome<TravelReleaseSnapshot> capture_travel_before_release(
    const CommonRead&,const TravelState&,const StableId& action_id);
}
