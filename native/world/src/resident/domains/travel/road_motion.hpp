// 行动前真实道路轨迹读口；个人接触与军团接触共用原一半日预演，不复制费用规则。
#pragma once
#include "travel.hpp"
namespace hanmo::resident {
// 输入必须来自同一 CommonRead/TravelState；缺人、错路或非规则错误显式拒绝。
Outcome<std::optional<RoadTravelMotion>>
original_road_travel_motion(const CommonRead &, const TravelCatalog &,
                            const TravelServices &, const ActionCoreView &,
                            const RoadTravelParams &,
                            const std::vector<StableId> &hold = {});
} // namespace hanmo::resident
