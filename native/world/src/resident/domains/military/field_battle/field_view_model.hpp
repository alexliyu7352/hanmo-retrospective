// §9/§13 的有限公开值；不暴露内部 RNG、原名册或未送达军令结果。
#pragma once
#include "field_control_acceptance.hpp"
#include "field_council_model.hpp"
#include "field_result_model.hpp"
namespace hanmo::resident {
struct FieldBattleViewIdentity {
  StableId battle_id, observer_actor_id;
  Integer segment = 0, revision = 0;
  FieldBattleTime now;
  FieldBattleState state = FieldBattleState::Council;
};
struct FieldCellView {
  StableId terrain;
  Integer height_dm = 0;
};
struct FieldBattleFieldView {
  FieldAnchor anchor;
  StableId field_hash;
  Integer length_m = 0, width_m = 0, cell_m = 0;
  std::optional<std::vector<FieldCellView>> cells;
};
struct FieldBattleSideView {
  Integer side = 0;
  StableId display_name, faction_id, controller;
  bool is_observer_side = false;
  std::vector<std::size_t> deploy_zone, exits;
};
// 公开任务有唯一 kind；各有限分支只允许它自己的目标字段。
struct FieldMissionView {
  StableId kind;
  std::optional<PointI> point_dm;
  std::optional<Integer> facing_mdeg, exit_side;
  std::optional<StableId> target_formation_id;
};
struct FieldCommanderView {
  StableId actor_id, display_name, state;
};
struct FieldTacticView {
  StableId tactic_id, kind;
  Integer point_cost = 0;
  std::optional<FieldBattleTime> ready_at;
};
struct FieldStatusView {
  StableId kind;
  std::optional<FieldBattleTime> until;
};
struct FieldFormationView {
  StableId formation_id, army_id, display_name, category, troop_type_id;
  Integer side = 0;
  PointI position_dm;
  Integer facing_mdeg = 0, width_dm = 0, depth_dm = 0;
  StableId state, control;
  FieldMissionView mission;
  Integer fit = 0, wounded = 0, dead = 0, captured = 0, deserted = 0,
          morale = 0;
  FieldCommanderView commander;
  std::optional<StableId> deputy_actor_id;
  std::vector<StableId> traits;
  std::vector<FieldTacticView> tactics;
  Integer tactic_points = 0;
  std::optional<FieldCouncilWave> wave;
  std::vector<FieldStatusView> statuses;
};
// 观察者只能拿到已知的状态；unconfirmed 不把内部 status/report_due 泄给客户端。
struct FieldOrderView {
  StableId order_id, recipient, status;
  std::optional<FieldMissionView> mission;
  std::optional<StableId> tactic_id;
  FieldBattleTime issued;
  std::optional<std::array<FieldBattleTime, 2>> eta_range;
};
struct FieldBattleEventView {
  Integer sequence = 0;
  FieldBattleTime at;
  StableId kind;
  std::optional<StableId> formation_id, target_formation_id, tactic, stratagem;
  std::optional<Integer> count;
  std::optional<bool> success;
};
// 公开军议只含合同字段；不把持久 command_result 或内部应用来源送往表现层。
struct FieldCouncilProposalView {
  StableId proposal_id, speaker_actor_id;
  FieldCouncilStance stance = FieldCouncilStance::Steady;
  FieldCouncilPlan plan;
  StableId text_key;
};
struct FieldCouncilRequestView {
  StableId actor_id;
  FieldCouncilRequestValue value = FieldCouncilRoleRequest{};
  FieldCouncilRequestStatus status = FieldCouncilRequestStatus::Pending;
  StableId reason;
};
struct FieldCouncilView {
  std::vector<FieldCouncilParticipant> participants;
  FieldCouncilIntelSource intel;
  std::vector<FieldCouncilProposalView> proposals;
  std::vector<FieldCouncilRequestView> requests;
  std::optional<StableId> adopted_proposal_id;
  bool closed = false;
};
struct FieldBattleView {
  FieldBattleViewIdentity identity;
  FieldBattleFieldView field;
  std::vector<FieldBattleSideView> sides;
  std::vector<FieldFormationView> formations;
  std::optional<FieldCouncilView> council;
  std::vector<FieldOrderView> orders;
  std::vector<FieldBattleEventView> events;
  std::optional<FieldBattleResult> result;
};
// 纯读仅借原 World；不得通过成功提交顺带新增 request/history/revision。
struct FieldBattleReadQuote {
  FieldBattleView view;
};
struct FieldBattleAdvanceResult {
  StableId battle_id;
  FieldBattleTime from, to;
  std::vector<FieldBattleEventView> events;
  std::optional<FieldBattleResult> result;
};
// 其它公开命令回执只给本次实际结果引用；envelope kind
// 决定哪些可选字段必须存在。
struct FieldDeploymentApplied {
  PointI position_dm;
  Integer facing_mdeg = 0;
  FieldFormationControl control = FieldAutonomousControl{};
  std::optional<StableId> deputy_actor_id;
  FieldMission immediate_mission = FieldHold{};
  std::optional<FieldMission> wave_mission;
};
struct FieldBattleMutationResult {
  StableId battle_id;
  FieldBattleState state = FieldBattleState::Council;
  FieldBattleTime at;
  std::optional<StableId> formation_id, order_id, council_fact_id, proposal_id,
      stratagem_id, tactic_id;
  std::optional<StableId> activation_id;
  std::optional<Integer> mission_revision, request_index;
  // 原命令实际接受的任务和生效边界；冷验不靠今日 Formation 反猜旧请求。
  std::optional<FieldMission> mission;
  std::optional<FieldBattleTime> effective_at;
  // Accepted Council attachment payload before Common result binding.
  std::optional<FieldCouncilStratagemPlan> council_stratagem;
  std::optional<FieldDeploymentApplied> deployment;
  // Captured only by the actual five control writers before their first write.
  std::optional<FieldControlAcceptanceBoundary> actual_control_acceptance;
  // 单口选择专用结果槽；不挪用编队/军令ID伪装另一种成功。
  std::optional<StableId> army_exit_choice_id;
  std::optional<Integer> selected_exit_cell;
};
// Common 仍是原成功结果/History 唯一 owner，不在军事域再建命令结果账。
using FieldBattleReceiptValue =
    std::variant<FieldBattleView, FieldBattleAdvanceResult,
                 FieldBattleMutationResult>;
struct FieldBattleReceipt {
  FieldBattleReceiptValue result = FieldBattleMutationResult{};
};
} // namespace hanmo::resident
