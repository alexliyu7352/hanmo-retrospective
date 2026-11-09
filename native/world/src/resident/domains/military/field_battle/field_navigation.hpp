// 原 §7 八邻格网寻路与射线；只读同一重建战场，不持有第二地图或世界时钟。
#pragma once
#include "field_session.hpp"
#include <map>
namespace hanmo::resident {
class CheckpointMemoryCharge;
// 路径仅为本次调用的缓存；空路径代表真实不连通，不代表已到达。
struct FieldNavigationPath {
  bool reachable = false;
  std::vector<std::size_t> cells;
};
// 真实首尾格必须在场内；不可通行目标只返回不连通，不自动换目标或瞬移。
Outcome<FieldNavigationPath>
read_field_navigation_path(const GeneratedBattleField &, FieldTroopCategory,
                           PointI from_dm, PointI target_dm);
// 单段直线沿所有真实编队外接圆截断；不写位置、不把终点无重叠当作途中无穿透。
Outcome<PointI> read_field_motion_contact_point(const FieldBattleSession &,
                                                const FieldFormation &,
                                                PointI proposed_dm);
struct FieldNavigationStep {
  PointI from_dm, to_dm;
  bool reached = false, blocked = false;
};
// Derived read-only history projection, not an authoritative world or setter.
// IDs/fit/positions are reconstructed from actual initial and local sources.
struct FieldMotionPrefixFormation {
  PointI position_dm;
  Integer fit = 0;
  bool routing = false, withdrawn = false;
  bool withdrawing = false; // derived from the actual mission at this prefix
};
struct FieldMotionPrefixRead {
  FieldBattleTime at;
  const std::map<StableId, FieldMotionPrefixFormation> &formations;
};
Outcome<FieldNavigationStep> read_field_navigation_step_at_prefix(
    const FieldBattleSession &, const GeneratedBattleField &, const StableId &,
    PointI target_dm, const FieldBattleWindowSlice &,
    const FieldMotionPrefixRead * = nullptr);
// 唯一局部步内位置写者；只改原 Formation，不写 Common/S1 锚或推进 now。
// 速度按本步起点格和原兵种/溃退倍率计算，路径/碰撞失败由整候选撤回。
Outcome<FieldNavigationStep> move_field_formation_in_candidate(
    FieldBattleSession &, const GeneratedBattleField &,
    const StableId &formation_id, PointI target_dm,
    const FieldBattleWindowSlice &one_step,
    CheckpointMemoryCharge &actual_world_owner);
// 射线遮挡按原 Forest/Village/blocks_sight 和实际高度，不读取表现插值。
Outcome<bool> read_field_line_of_sight(const GeneratedBattleField &,
                                       PointI from_dm, PointI target_dm);
} // namespace hanmo::resident
