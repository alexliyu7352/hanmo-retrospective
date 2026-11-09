// S1 真道路弧长读口；只作几何报价，不改旅行耗时、行动、费用或地图。
#pragma once
#include "movement_exact_model.hpp"
namespace hanmo::resident {
struct TravelCatalog;
struct TravelRoad;
// 单位为同一作者地图坐标单位。逐原 shape 段整开方，绝不拿端点直线或耗时当长度。
// 原 S1 尚无弧长量化读口；此整数精度为内部 PROVISIONAL，待中央审准后才用于释放。
Outcome<Integer> read_original_road_geometry_length(const TravelCatalog &, const TravelRoad &);
struct OriginalRoadDistance {
  StableId settlement_id;
  CanonicalFraction distance;
  std::vector<StableId> road_ids;
  bool initial_returning = false;
};
struct OriginalRoadDistanceTree {
  StableId world_map_id;
  std::vector<OriginalRoadDistance> reachable;
};
// origin 须由调用方的真实位置来源证明。此纯叶只接世界 Settlement/原 OnRoute。
// OnRoute 的进度仅作为原道路比例，乘真实弧长；不是把 duration_ticks 当距离权重。
// 完整查原有向路网及原封路规则，缺几何为失败，不冒充合法“没有候选”。
Outcome<OriginalRoadDistanceTree> read_original_road_distance_tree(
    const TravelCatalog &, const Location &origin, Integer observed_tick,
    ExactArithmeticBudget = {});
// 精确 S1 原锚直接用有理进度，不能先投影成整数 OnRoute 再选最近据点。
Outcome<OriginalRoadDistanceTree> read_original_road_distance_tree(
    const TravelCatalog &, const MovementAnchor &origin, Integer observed_tick,
    ExactArithmeticBudget = {});
} // namespace hanmo::resident
