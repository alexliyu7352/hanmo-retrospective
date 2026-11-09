// 10-31整军单口的真实选择与终局后汇合；不复制Army库存、钱包或世界路线。
#pragma once
#include "field_control_acceptance.hpp"
#include "field_time.hpp"
#include <optional>
#include <vector>
namespace hanmo::resident {
// 原选择边界逐编队的实际位置与幸存兵员权重；来源仍须沿原局部事件反核。
struct FieldArmyExitWeight {
  StableId formation_id;
  PointI position_dm;
  Integer fit = 0, wounded = 0;
};
// 终局后每个真实一步的几何结果；普通兵截止逃散与活将领保留严格分列。
struct FieldArmyExitMotion {
  StableId formation_id;
  PointI from_dm, to_dm;
  bool reached = false, blocked = false;
  std::vector<StableId> deserted_member_ids;
};
// 一次原World候选采用的局部步；applied_at只取同根时钟，不从局部秒推造。
struct FieldArmyExitStep {
  StableId fact_id;
  FieldBattleTime from, to;
  WorldMoment applied_at;
  std::vector<FieldArmyExitMotion> formations;
};
// Ended原战斗位置不重写；这张表是终局后真实移动的唯一持久位置来源。
struct FieldArmyExitRallyFormation {
  StableId formation_id;
  PointI initial_position_dm;
  std::vector<StableId> ordinary_member_ids, retained_officer_ids;
  // 有序提前退出保原Exit事实，不把已退出者重新拉回战场移动。
  std::optional<SourceRef> original_exit;
};
// 唯一选择事实键；玩家由原成功CommandResult封口，AI由真实原控制cut授权。
// 无terminal时只锁定后续有序撤退；Routing近口逃散不采用或改写本选择。
struct FieldArmyExitPlan {
  StableId fact_id, battle_id, army_id, commander_actor_id;
  StableId field_hash, destination_settlement_id, return_garrison_id;
  std::size_t exit_cell = 0;
  FieldBattleTime selected_at;
  WorldMoment applied_at;
  SourceRef original_journey_opening;
  FieldControlAcceptanceBoundary selection_boundary;
  std::optional<SourceRef> player_command_result;
  std::vector<FieldArmyExitWeight> weights;
  std::optional<SourceRef> terminal_segment;
  std::optional<FieldBattleTime> ordinary_deadline;
  std::vector<FieldArmyExitRallyFormation> rally_formations;
  std::vector<FieldArmyExitStep> steps;
  // ready事实仅由全体保留成员在单口齐备的真实一步产生；不是付费或已开程。
  std::optional<SourceRef> ready_source;
  std::optional<FieldBattleTime> ready_at;
  std::optional<WorldMoment> ready_applied_at;
  std::optional<SourceRef> actual_continuation;
};
} // namespace hanmo::resident
