// 仅公开命令值；不含世界 owner 或旧棋盘字段。
#pragma once
#include "field_council_model.hpp"
#include "field_result_model.hpp"
#include <string_view>
namespace hanmo::resident {
// 布阵只能指定直控或战前自主；派出时刻属于另一条正式 Dispatch 命令。
enum class FieldDeploymentControl { Direct, Autonomous };
struct FieldBattleSetDeployment {
  StableId battle_id, formation_id;
  PointI position_dm;
  Integer facing_mdeg = 0;
  FieldMission mission = FieldHold{};
  FieldDeploymentControl control = FieldDeploymentControl::Autonomous;
  std::optional<StableId> deputy_actor_id;
};
struct FieldBattleStart {
  StableId battle_id;
};
// 玩家实际发言值；计划不是已采纳结果，须走主将正式 Adopt 才影响编队。
using FieldCouncilSpeech =
    std::variant<FieldCouncilRoleRequest, FieldCouncilTargetRequest,
                 FieldCouncilPlan>;
struct FieldCouncilSpeak {
  StableId battle_id;
  FieldCouncilSpeech speech = FieldCouncilRoleRequest{};
};
struct FieldCouncilAdopt {
  StableId battle_id;
  // 主将必选真实提案；部将只可空值听令，不能借该字段替 AI 主将定案。
  std::optional<StableId> proposal_id;
  std::vector<FieldCouncilAssignment> overrides;
};
// 纯读游标由调用方回传，不把“上次读”写入权威世界或消费随机流。
struct FieldBattleRead {
  StableId battle_id, known_field_hash;
  Integer after_event_sequence = 0;
};
struct FieldBattleDirect {
  StableId battle_id, formation_id;
  Integer expected_mission_revision = 0;
  FieldMission mission = FieldHold{};
};
struct FieldBattleOrder {
  StableId battle_id, formation_id;
  FieldMission mission = FieldHold{};
};
struct FieldBattleDispatch {
  StableId battle_id, formation_id;
  FieldMission mission = FieldHold{};
};
struct FieldBattleRejoin {
  StableId battle_id, formation_id;
};
struct FieldBattleDelegate {
  StableId battle_id;
};
struct FieldBattleAdvance {
  StableId battle_id;
  FieldBattleTime to;
};
struct FieldBattleTactic {
  StableId battle_id, formation_id, tactic_id;
};
struct FieldBattleStratagem {
  StableId battle_id;
  FieldStratagemKind kind = FieldStratagemKind::Ambush;
  std::optional<StableId> formation_id, target_formation_id;
  std::optional<PointI> area_dm;
  // Council only; absent resolves the commander's actual current proposal.
  std::optional<StableId> proposal_id;
};
// §5.2 全部有限 DTO 一次登记；有类型不代表对应 writer 已实现。
using FieldDeploymentCommand =
    std::variant<FieldBattleSetDeployment, FieldBattleStart, FieldCouncilSpeak,
                 FieldCouncilAdopt, FieldBattleRead, FieldBattleDirect,
                 FieldBattleOrder, FieldBattleDispatch, FieldBattleRejoin,
                 FieldBattleDelegate, FieldBattleAdvance, FieldBattleTactic,
                 FieldBattleStratagem>;

// 全部十三个公开名称的唯一 typed 对应；不接受旧棋盘命令别名。
bool field_deployment_command_kind(std::string_view) noexcept;
const char *
field_deployment_command_kind(const FieldDeploymentCommand &) noexcept;
} // namespace hanmo::resident
