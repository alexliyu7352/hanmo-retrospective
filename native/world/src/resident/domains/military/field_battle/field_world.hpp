// 野战的唯一在战绑定；原 Common OnRoute 只保冻结锚点，不是当前战场位置。
#pragma once
#include "field_contact.hpp"
#include "field_army_exit_model.hpp"
#include "field_reinforcement_model.hpp"
#include "field_session.hpp"
#include "field_spatial_view.hpp"
namespace hanmo::resident {
class FieldBattleActiveMembershipRead;
// 原部队与真实具名成员的绑定；不从人物姓名或单独计数推定归属。
struct FieldBattleMemberBinding {
  StableId actor_id, formation_id;
};
// 初始两军绑定与后来穿入本场的实际运动来源互斥，不共用假 endpoint。
struct FieldInitialContactBinding {
  StableId contact_id;
  Integer source_endpoint = 0;
};
struct FieldBoundaryContactBinding {
  StableId contact_id;
};
using FieldBattleBindingSource =
    std::variant<FieldInitialContactBinding, FieldBoundaryContactBinding>;
// 恢复世界位置只借同场真实接触；初始两军/援军不建立第二种在战权威。
struct FieldBattleRoadBinding {
  StableId army_id, battle_id, action_id, commander_actor_id;
  FieldBattleBindingSource source = FieldInitialContactBinding{};
  std::vector<FieldBattleMemberBinding> members;
};
// 原行动的单方轨迹穿过战场边界，不伪造第二个行动或两军相遇 ContactHit。
struct FieldExactBoundaryOrigin {
  SourceRef actual_contact;
};
using FieldBoundaryMotion = std::variant<RoadContactEndpoint, ContactEndpoint,
                                         FieldExactBoundaryOrigin>;
enum class FieldBoundaryStatus { Stopped, Entered, Released };
struct FieldBattleBoundaryRecord {
  StableId contact_id, battle_id;
  Integer source_tick = 0, time_num = 0, time_den = 1;
  FieldBoundaryMotion motion = RoadContactEndpoint{};
  PointI entry_point_dm;
  std::optional<StableId> army_id;
  // null 是真实未能归到任何一侧的第三方，不默认补成 side 0。
  std::optional<Integer> side;
  std::vector<StableId> participant_actor_ids;
  // 同势力不需条约；异势力只能持实际 P3 结果，不以可通行推定结盟。
  std::vector<SourceRef> diplomacy_sources;
  FieldBoundaryStatus status = FieldBoundaryStatus::Stopped;
  std::optional<FieldBattleTime> entered_at, released_at;
  // 穿边接触不是到时入场；此处只记真正初始化编队的那次应用头。
  std::optional<SourceRef> entry_application_source;
  std::optional<Integer> entered_observed_tick;
  std::optional<WorldMoment> entered_actual_at;
  // Append-only original membership union, extended only by each actual entry.
  // Each admitted formation retains its own immutable header/M in the queue.
  // The separate army_bindings table remains the sole active battle authority.
  std::optional<FieldBattleRoadBinding> entered_binding;
  // 31ef: only this original boundary owns the ordered admission queue.
  std::optional<FieldReinforcementQueue> reinforcement_queue;
  // Exact never-admitted continuation belongs to the original S1 source.
  // No invented post-terminal Field seconds or duplicate actual World M.
  std::optional<SourceRef> exact_continuation_source;
};
// 单编队的实际离场交接；不创建新 Army，不代回城，也不分割军费/军粮。
// 仍在战的其它单位不在 named_actor_ids 中，不能被整军 ArmyReturn 一并带走。
struct FieldBattleExitRecord {
  StableId fact_id, battle_id, formation_id, army_id;
  FieldBattleTime at;
  Integer recorded_tick = 0;
  PointI field_point_dm;
  // 道路/越野回原路线；实际 Gate 或村中心出口回该据点，不能强塞 OnRoute。
  std::variant<OnRouteLocation, SettlementLocation> location =
      OnRouteLocation{};
  StableId source_action_id;
  SourceRef origin;
  std::vector<StableId> named_actor_ids;
  // 原返回/军令真实接收后才有来源；空值不表示已经返回或获得新行军授权。
  std::optional<SourceRef> continuation;
  // The original application boundary, never derived from Field seconds.
  WorldMoment applied_at;
  // Original road progress units (duration_ticks * 100), not a 0..1 fraction.
  // Present exactly for OnRoute; location.progress is its compatibility floor.
  std::optional<CanonicalFraction> exact_route_progress;
};
// 原 probe 与原 actions_step
// 核验后的接触出处；安装后只读，不靠存档重演造一条接触。
// The exact branch retains only its real S1 source identity. Geometry, clock,
// unit closure and carrier membership stay owned by the original travel roots.
struct FieldExactGroupContactOrigin {
  SourceRef actual_contact;
};
using FieldContactOrigin =
    std::variant<FieldRoadContact, FieldExactGroupContactOrigin>;
struct FieldBattleContactRecord {
  StableId contact_id, battle_id;
  FieldContactOrigin source;
  // Frozen original action/member identities, not another position or Army.
  // Retained after the active army binding is released or a later battle
  // starts.
  std::array<FieldBattleRoadBinding, 2> original_bindings;
};
// 空初态仅代表尚未接战；安装/校验/存读必须同版接通才能发布此 owner。
struct FieldBattleWorldState {
  Integer schema = 1;
  CheckpointMap<StableId, FieldBattleSession> sessions;
  CheckpointMap<StableId, FieldBattleContactRecord> contacts;
  CheckpointMap<StableId, FieldBattleBoundaryRecord> boundary_contacts;
  CheckpointMap<StableId, FieldBattleRoadBinding> army_bindings;
  CheckpointMap<StableId, FieldBattleExitRecord> exits;
  CheckpointMap<StableId, FieldCouncilFact> council_facts;
  CheckpointMap<StableId, FieldArmyExitPlan> army_exit_plans;
};
// 真实写者在当前局部时刻追加有序事件；Read 只传游标，不改这个计数器。
Status append_field_battle_event(FieldBattleSession &, FieldBattleEvent);
// 局部尚未结算的会话先阻止下一半日；不靠旧军战 planning 判断新时钟。
enum class FieldWorldAdvanceReason { allowed, local_settlement, player_input };
struct FieldWorldAdvanceGate {
  FieldWorldAdvanceReason reason = FieldWorldAdvanceReason::allowed;
  StableId battle_id;
};
Outcome<FieldWorldAdvanceGate>
read_field_world_advance_gate(const FieldBattleWorldState &,
                              const StableId &player_actor_id,
                              Integer committed_tick);
// 无绑定才返回 null；已绑定而会话/原军团/具名归属损坏必须显式失败。
Outcome<std::optional<FieldBattleSpatialView>> read_field_battle_actor_position(
    const CommonRead &, const MilitaryState &, const FieldBattleWorldState &,
    const StableId &actor_id,
    const FieldBattleActiveMembershipRead *membership = nullptr);
// phase hold 取所有实际具名成员；逐项沿同一 reader 验证，不能静默丢掉坏绑定。
Outcome<std::vector<StableId>> read_field_battle_held_actors(
    const CommonRead &, const MilitaryState &, const FieldBattleWorldState &,
    const FieldBattleActiveMembershipRead *membership = nullptr);
// 接敌候选排除真实已绑军团；不借旧 MilitaryBattleRecord 判断新 profile。
Outcome<std::vector<StableId>> read_field_battle_held_armies(
    const CommonRead &, const MilitaryState &, const FieldBattleWorldState &,
    const FieldBattleActiveMembershipRead *membership = nullptr);
} // namespace hanmo::resident
