// field_battle/1 独立会话类型；不采用旧六角回合或全世界子半日骨架。
#pragma once
#include "field_control_acceptance.hpp"
#include "field_time.hpp"
#include "field_council_model.hpp"
#include "field_result_model.hpp"
#include "field_event.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/kernel/source_ref.hpp"
#include "resident/kernel/world_timeline.hpp"
#include <variant>
namespace hanmo::resident {
enum class FieldCouncilFactKind {
  Adopted,
  RequestGranted,
  RequestDenied,
  OrderDelivered,
  PlayerOrder,
  AiOrder
};
// 双方各持一份军议，闭议必须有真实采纳方案，不能空标 closed。
struct FieldCouncilRecord {
  // 安装时实际在场的玩家；保军议等待与命令出处，不读未来接控者倒填。
  std::optional<StableId> player_actor_id;
  std::vector<FieldCouncilParticipant> participants{};
  FieldCouncilIntelSource intel{};
  std::vector<FieldCouncilProposal> proposals{};
  std::vector<FieldCouncilRequest> requests{};
  std::optional<StableId> adopted_proposal_id{};
  bool closed{};
};
// 同 Common 事实头的军议结果；记录准驳和定案，不承担关系值变更。
struct FieldCouncilFact {
  StableId fact_id{};
  StableId battle_id{};
  Integer side{};
  FieldBattleTime at{};
  Integer recorded_tick = 0;
  FieldCouncilFactKind kind = FieldCouncilFactKind::Adopted;
  FieldCouncilRecord council{};
  std::optional<StableId> order_id{};
  std::optional<SourceRef> command_result;
  // 部将实际命令的有限载荷；不用后来 Formation 的任务倒填历史。
  std::optional<StableId> player_command_kind, player_actor_id, formation_id;
  std::optional<StableId> tactic_id, activation_id;
  std::optional<FieldMission> player_mission;
  std::optional<Integer> mission_revision;
  bool ai_pursuit = false;
  std::optional<WorldMoment> adopted_actual_at;
  // AI Direct/Order have a real council source rather than a player result.
  std::optional<FieldControlAcceptanceBoundary> ai_control_acceptance;
};

// 特性与战法只保原习得来源的有限快照；不是新人物属性或额外兵种定义。
enum class FieldTrait {
  CavalryMaster,
  InfantryMaster,
  ArcheryMaster,
  Inspiring,
  StaffWork,
  Valiant
};
struct FieldAvailableTactic {
  StableId tactic_id, learned_tactic_id, acquisition_fact_id;
  FieldTacticKind kind = FieldTacticKind::Rally;
  Integer point_cost = 0, power_bp = 10000, morale_effect = 0;
};
// 战法的就绪时刻随会话保存；读口不临时补扣点或结束冷却。
struct FieldTacticReadiness {
  FieldAvailableTactic source;
  std::optional<FieldBattleTime> ready_at;
};
// 已发动效果保实际起点与脉冲余数；齐射按脉冲计，不伪装为秒数。
struct FieldTacticActivation {
  StableId tactic_id;
  StableId activation_id, issuer_actor_id, commander_actor_id;
  FieldAvailableTactic accepted_source;
  bool ai_issued = false;
  FieldTacticKind kind = FieldTacticKind::Rally;
  FieldBattleTime started;
  std::optional<FieldBattleTime> ends;
  Integer remaining_pulses = 0;
  bool first_contact_consumed = false;
  std::optional<SourceRef> command_result;
  std::optional<StableId> order_id;
};
enum class FieldStatusKind { Confused, Suppressed, Guarding, Charging, Hidden };
// hidden 到真实现身才结束；只有有期限的效果持有 until，不编造伏兵的超时。
struct FieldFormationStatus {
  FieldStatusKind kind = FieldStatusKind::Hidden;
  StableId source_id;
  std::optional<FieldBattleTime> until;
};
// 能力源来自实际指挥者；副手接替必须重读其自身来源，不沿用逝者习得。
// Actual deputy takeover is not a Family death fact. The local Lost event
// precedes this original Common application head at the same S0 moment.
struct FieldCommanderChange {
  SourceRef source_ref;
  StableId previous_actor_id, commander_actor_id;
  FieldBattleTime at;
  Integer observed_tick = 0, lost_event_sequence = 0;
  WorldMoment applied_at;
};
struct FieldFormationAbilityState {
  StableId commander_actor_id;
  // 冻结来源是实际安装/入场的 Common 头和宏观观察边界，不是物理战场秒。
  SourceRef source_ref;
  Integer observed_tick = 0;
  std::vector<FieldTrait> traits;
  std::vector<FieldTacticReadiness> tactics;
  Integer capacity = 0, points = 0;
  FieldBattleTime last_point_recovery;
  // 保实际发动记录供冷读反验点数/冷却；已到期效果由 ends/pulses 判定，不重发。
  std::vector<FieldTacticActivation> active;
  std::vector<FieldCommanderChange> commander_changes;
};
// 直控令只在下一步生效；保真正接受版本和请求序号，不增第二套全局钟。
struct FieldPendingDirectMission {
  StableId issuer_actor_id;
  Integer request_sequence = 0, accepted_revision = 0;
  FieldMission mission = FieldHold{};
  FieldBattleTime apply_at;
  bool ai_pursuit = false;
  // 原场内决策来源；与玩家成功 CommandResult 严格互斥，冷验实际统帅控制。
  bool ai_issued = false;
  std::optional<StableId> ai_source_id;
  std::optional<SourceRef> command_result;
};
// 自主战法以同一传令记录承载；到达采用时才扣点。
struct FieldTacticIntent {
  StableId tactic_id;
};
using FieldOrderPayload = std::variant<FieldMission, FieldTacticIntent>;
// 用计按实际发令序列判定；resolved_at/success 均空表示尚未消费随机流。
struct FieldStratagemRecord {
  // 采用时冻结真实来源与实际两人；120 秒后换将不重选概率来源。
  SourceRef adopted_source;
  Integer observed_tick = 0;
  StableId strategist_actor_id;
  std::optional<StableId> target_commander_actor_id;
  // 概率用四分之一百分点保原 /2、/4，不在中间截断。军议确定型计策为 0。
  Integer success_chance_quarters = 0;
  StableId stratagem_id, issuer_actor_id;
  Integer side = 0, issued_sequence = 0;
  FieldStratagemKind kind = FieldStratagemKind::Ambush;
  std::optional<StableId> formation_id, target_formation_id;
  std::optional<PointI> area_dm;
  FieldBattleTime issued;
  std::optional<FieldBattleTime> resolve_at, resolved_at;
  std::optional<bool> success;
  // 埋伏的现身与首个接战脉冲分别只消费一次；知情不等于伤害已经应用。
  std::optional<FieldBattleTime> revealed_at;
  bool first_contact_consumed = false;
  std::optional<SourceRef> command_result;
  WorldMoment applied_at;
};
// A rule-derived mission is not a new player/AI order. Keep the original
// source pins and an ordered local transition chain for cold replay.




// Actual adoption only, not an accepted-but-superseded pending request.


// 步内真实结果的顺序事件；读口传游标取增量，读取本身不推进会话或知情。

// 段提交持有唯一真实父头与原伤亡/终局引用，不另存一份人产余额。
// 暂定：recorded_tick 只取同 Common 提交边界，settled_at 仍是局部战场秒。
struct FieldSegmentSettlement {
  StableId fact_id;
  Integer recorded_tick = 0;
  // 原 S0 采用栈冻结的写回时刻，不是把局部战场秒转换为世界时刻。
  WorldMoment applied_at;
  Integer segment = 0;
  FieldBattleTime settled_at;
  std::vector<StableId> casualty_fact_ids;
  std::optional<StableId> battle_fact_id;
};
// 独立将领不占兵员余额；实际脉冲只保单调伤/死状态，段提交交原人物落账。
struct FieldCommanderCasualty {
  StableId actor_id;
  Integer loss = 0; // 原 exposure 结果：1 受伤，2 阵亡。
  FieldBattleTime at;
};
// 原五类成员 ID 原样进入会话；没有可独立增减的兵员余额。
struct FieldFormation {
  StableId formation_id, army_id, troop_type_id, commander_actor_id;
  std::optional<StableId> deputy_actor_id;
  Integer side = 0, fit_start = 0;
  FieldBattleTime joined_at;
  FieldTroopCategory category = FieldTroopCategory::Infantry;
  std::vector<StableId> fit, wounded, dead, captured, deserted;
  // 俘虏归属是原成员→俘获军团，不能到段末按胜方反猜。
  std::vector<std::pair<StableId, StableId>> captors;
  std::vector<FieldCommanderCasualty> commander_casualties;
  std::optional<FieldBattleTime> commander_lost_at;
  PointI position_dm;
  Integer facing_mdeg = 0, ranks = 0, morale = 0, morale_start = 0;
  FieldFormationState state = FieldFormationState::Deploying;
  FieldFormationControl control = FieldAutonomousControl{};
  FieldMission mission = FieldHold{};
  // No selected mission for a newly admitted player-led reinforcement.
  // Inert variant storage is not Hold authority; forced local rules still run.
  bool pending_player_entry_order = false;
  Integer mission_revision = 0;
  std::optional<Integer> mission_transition_sequence;
  std::optional<StableId> adopted_order_id;
  std::optional<FieldCouncilWave> wave;
  std::optional<FieldMission> wave_mission, followup_mission;
  // Index in the actually adopted immutable original council plan.
  std::optional<Integer> flank_assignment_index;
  std::optional<FieldPendingDirectMission> pending_direct;
  // 当前任务/控制仍反读原成功封口；新军令采用时释放被替代命令的 pin。
  std::optional<SourceRef> mission_command_result, control_command_result;
  // Actual successful Dispatch/Rejoin/Delegate result references, in original
  // accepted revision order. Required for old control cuts after replacement;
  // not a second control state and never reconstructed from today's control.
  CheckpointVector<SourceRef> control_change_sources;
  // Last actual pre-battle placement; retained after later orders replace
  // mission.
  std::optional<SourceRef> deployment_command_result;
  // 原 AI Direct 的不可变 military_council/AiOrder 头；不是空玩家回执。
  std::optional<StableId> mission_ai_source_id;
  std::optional<FieldFormationAbilityState> ability;
  std::vector<FieldFormationStatus> statuses;
  std::optional<FieldBattleTime> stopped_since, hold_since;
  Integer casualty_ordinal = 0;
  bool has_engaged = false;
  std::optional<FieldBattleTime> withdrawn_at;
  // A* 路径不存；只保实际重算时选定的动态目标，使冷读不改成目标的新位置。
  std::optional<PointI> navigation_target_dm;
  std::optional<FieldBattleTime> navigation_replanned_at;
  std::optional<Integer> navigation_observation_sequence, last_motion_sequence;
  // 仅 §6.2 追击决策设置；玩家 Engage 不受 AI 的 1500 米追击限界冒管。
  bool ai_pursuit = false;
};
struct FieldPlayerController {
  StableId actor_id;
};
struct FieldAiController {};
struct FieldBattleSide {
  Integer side = 0;
  StableId faction_id, commander_actor_id;
  std::variant<FieldPlayerController, FieldAiController> controller =
      FieldAiController{};
  std::vector<std::size_t> deploy_zone, exits;
  std::optional<StableId> strategist_actor_id, feint_strength_id,
      delegated_by_actor_id;
  Integer stratagem_points = 0, initial_stratagem_points = 0;
  std::optional<SourceRef> delegation_command_result;
};
enum class FieldOrderStatus {
  InTransit,
  Adopted,
  Delivered,
  NotResponding,
  Superseded,
  Undeliverable
};
struct FieldOrderRecord {
  StableId order_id, issuer_actor_id, target_formation_id;
  FieldOrderPayload payload = FieldMission{FieldHold{}};
  FieldBattleTime issued, due;
  Integer delay_s = 0, distance_m = 0;
  FieldOrderStatus status = FieldOrderStatus::InTransit;
  std::optional<FieldBattleTime> status_at, report_due;
  // Actual arrival/status application, distinct from delayed OrderArrived
  // report.
  std::optional<Integer> resolution_event_sequence;
  std::optional<FieldControlAcceptanceBoundary> resolution_boundary;
  StableId reason, superseded_by;
  bool ai_pursuit = false;
  bool ai_issued = false;
  std::optional<SourceRef> command_result;
  // Only the original successful mission adoption writes this exact revision.
  std::optional<Integer> adopted_mission_revision;
};
struct FieldPendingEntry {
  StableId army_id, contact_id;
  Integer side = 0;
  FieldBattleTime entry;
  PointI entry_point_dm;
};
enum class FieldAiTrigger {
  FirstEngagement,
  Rout,
  CommanderLost,
  Reinforcement
};
struct FieldAiState {
  Integer side = 0;
  FieldBattleTime last_eval;
  // 语义为集合；写入和冷读必须保持枚举升序且不重复。
  CheckpointVector<FieldAiTrigger> pending_triggers;
};
// 仅细时间、原名册、任务和回执属于会话；生成格由内容及最终锚点重建。
// 当前为严格类型骨架，尚未有公共安装/推进 writer，不能据此宣称 BW06 成功。
struct FieldBattleSession {
  StableId battle_id, field_hash;
  // created_tick 只记世界安装提交与排序；now.tick 是接触所归属的物理窗口。
  Integer created_tick = 0, segment = 0;
  // 当前 segment 已结到窗末时 now 为下一窗零秒，尚未开放下一段。
  bool window_complete = false;
  FieldBattleTime now;
  FieldBattleState state = FieldBattleState::Council;
  std::optional<FieldBattleTime> started_at;
  // NPC 借实际 Session 安装头；玩家借实际 Start 写入头，不能用同 tick
  // 代替先后。
  std::optional<SourceRef> started_source;
  Integer started_observed_tick = 0;
  std::optional<WorldMoment> started_actual_at;
  std::optional<StableId> started_by_actor_id;
  FieldAnchor anchor;
  std::array<FieldBattleSide, 2> sides;
  std::array<FieldCouncilRecord, 2> councils;
  CheckpointMap<StableId, FieldFormation> formations;
  CheckpointMap<StableId, FieldOrderRecord> orders;
  CheckpointMap<StableId, FieldStratagemRecord> stratagems;
  std::vector<FieldBattleEvent> events;
  std::vector<FieldSegmentSettlement> settlements;
  Integer next_event_sequence = 1, next_stratagem_sequence = 1;
  std::vector<FieldPendingEntry> pending_entries;
  std::array<FieldAiState, 2> ai;
  StableId rng_seed, rng_state;
  CheckpointMap<std::pair<StableId, StableId>, Integer> pulse_remainders;
  CheckpointMap<StableId, Integer> morale_remainders, flank_remainders;
  std::optional<FieldBattleResult> result;
};
} // namespace hanmo::resident
