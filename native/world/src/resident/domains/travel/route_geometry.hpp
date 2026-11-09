// 原作者路网几何的唯一读口；不生成替代路线或地图尺度。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct TravelCatalog;
struct TravelRoad;
struct TravelSettlement;
// 纯作者叶只接受真实道路及其两个匹配端点；ID 不符显式拒绝。
Outcome<std::vector<PointI>>
original_travel_road_polyline(const TravelSettlement &origin,
                              const TravelRoad &,
                              const TravelSettlement &destination);
// 输入为已校验目录内真实道路；仍防御缺端点，失败不返回空折线冒成功。
Outcome<std::vector<PointI>>
original_travel_road_polyline(const TravelCatalog &, const TravelRoad &);
// 两端必须有同一个真实作者地图，路自身若声明地图也须一致；缺失不按坐标猜。
Outcome<StableId> original_travel_road_world_map(const TravelSettlement &,
                                                 const TravelRoad &,
                                                 const TravelSettlement &);
// 只接受目录中真实道路与端点；显式地图必须确实存在于同一目录。
Outcome<StableId> original_travel_road_world_map(const TravelCatalog &,
                                                 const TravelRoad &);
} // namespace hanmo::resident
