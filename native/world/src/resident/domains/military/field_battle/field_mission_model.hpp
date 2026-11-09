// Field 公开任务与编队枚举；不包含内部事件、会话或世界拥有者。
#pragma once
#include "field_time.hpp"
#include <variant>
namespace hanmo::resident {
// 连续任务只携带合同规定的目标，路径缓存不进入持久状态。
struct FieldMoveTo {
  PointI point_dm;
  Integer facing_mdeg = 0;
};
struct FieldHold {
  PointI point_dm;
  Integer facing_mdeg = 0;
};
struct FieldEngage {
  StableId target_formation_id;
};
struct FieldSupport {
  StableId ally_formation_id;
};
struct FieldWithdraw {
  Integer exit_side = 0;
};
using FieldMission = std::variant<FieldMoveTo, FieldHold, FieldEngage,
                                  FieldSupport, FieldWithdraw>;
enum class FieldFormationState {
  Deploying,
  Moving,
  Holding,
  Engaged,
  Routing,
  Withdrawn,
  Destroyed
};
enum class FieldTroopCategory { Infantry, Archer, Cavalry };
enum class FieldTacticKind { Charge, Volley, Rally, Guard };
enum class FieldStratagemKind { Ambush, FeintStrength, Confuse };
struct FieldDirectControl {
  StableId controller_actor_id;
};
struct FieldAutonomousControl {
  std::optional<FieldBattleTime> dispatched_at;
};
using FieldFormationControl =
    std::variant<FieldDirectControl, FieldAutonomousControl>;
} // namespace hanmo::resident
