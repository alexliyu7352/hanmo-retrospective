#pragma once
#include "spatial_location_capture.hpp"
#include <variant>
namespace hanmo::resident {
// 只分类原 Field 实际退出头；分类本身不证明位置或成员已经交接。
inline bool
is_field_battle_exit_placement_source(const SourceRef &ref) noexcept {
  return ref.domain == "military" && ref.kind == "field_battle_exit";
}
// 仅分类独立 member-loss 原头，不代表身体、空间或成员账已经应用。
inline bool
is_field_member_loss_placement_source(const SourceRef &ref) noexcept {
  return ref.domain == "travel" && ref.kind == "group_member_loss_handoff";
}
struct MovementContactInstallationSource;
struct MovementContactContinuationSource;
struct MovementBoundaryContinuationSource;
struct MovementArrivalSource;
struct GroupMovementOpeningSource;
struct GroupMovementControlSource;
struct ExactMovementContact;
struct MovementBoundaryContact;
struct GroupContactMemberReleaseSource;
struct GroupMemberLossHandoffSource;
// 原双 prefix 接触安装：旧入场证明保持真实安装行与完整接触行。
struct FieldActorDoubleContactEntry {
  const MovementContactInstallationSource *installation = nullptr;
  const ExactMovementContact *contact = nullptr;
};
// 原单 prefix 边界并不等于已入场。31ef 逐支队列必须借本 actor 所属
// 编队自己的真实 Entered 头和 admission.applied_at；排队时只占原 S1 hold，
// 不能返回这条参战来源。所有指针只借同 World 原行，不借临时 fold/view。
struct FieldActorSingleBoundaryEntry {
  const MovementBoundaryContact *contact = nullptr;
  const FactRecord *field_contact_head = nullptr;
  const FactRecord *entered_head = nullptr;
  const WorldMoment *entered_at = nullptr;
};
using FieldActorEngagementEntry =
    std::variant<FieldActorDoubleContactEntry, FieldActorSingleBoundaryEntry>;
// 终点只允许原退出/死俘逃散交接，或胜方原余程/真实到达采用；monostate
// 表示仍参战。 variant 只借同 World 实际行，不新增参战状态或用 casualty 结果冒
// S1 采用；整军有限返程只借真实新opening，必须先证明旧hold取消与ready同源。
using FieldActorEngagementRelease =
    std::variant<std::monostate, const GroupContactMemberReleaseSource *,
                 const GroupMemberLossHandoffSource *,
                 const MovementContactContinuationSource *,
                 const MovementBoundaryContinuationSource *,
                 const MovementArrivalSource *,
                 const GroupMovementOpeningSource *,
                 const GroupMovementControlSource *>;
// 只借原已入场 actor 与已采用的成员交接；release 未给实际行表示尚未完成
// 原 capture；胜方续程则保持原 Army carrier，不产生静置 placement。
// 完整来源 reader 必须验 finite variant 各指针/原几何/唯一本支
// admission/实际 Common 前序；空默认值不是成功证明。不保存第二参战区间。
struct FieldActorEngagementRead {
  const StableId *battle_id = nullptr;
  const StableId *journey_id = nullptr;
  FieldActorEngagementEntry entry;
  FieldActorEngagementRelease release;
};
// 原死俘交接的完整业务来源与实际位置投影。actual_placement 复用原同 job
// 拥有值；original_loss、carried_army_id 只借同 World 原行。被俘分支的
// physical anchor 来自真实俘获方的原 group close，Common 投影则仍为
// InArmy(实际 captor)，绝不把 carrier 当 physical anchor。死亡分支无
// carried_army_id，只退到原被暂停 group 的真实世界锚，不宣称战场死亡 dm 点。
struct FieldActorMemberLossPlacementRead {
  const GroupMemberLossHandoffSource *original_loss = nullptr;
  ActualActorSpatialWriteRead actual_placement;
  const StableId *carried_army_id = nullptr;
};
// 只读稳定到点证明；actual保真cancel+ready两头，Army指针只借原plan，不复制军队。
struct FieldActorArmyExitStablePlacementRead {
  ActualActorSpatialWriteRead actual;
  const StableId *army_id = nullptr;
};
// 复用原有实际写读值，不另存位置、军籍、成员或世界钟。
// 单笔读负责真退出行/原来源/时钟；实际 Common mutation 必须再经 capture。
// 历史读只枚举已被原 S1 group member handoff 采用的真实 placement，
// 不得仅因退出行存在或 formation Withdrawn 就解除全军 hold。
class FieldBattleExitPlacementRead {
public:
  virtual ~FieldBattleExitPlacementRead() = default;
  virtual bool bound_to(const CommonStore &,
                        const MovementExactState &) const noexcept = 0;
  virtual Outcome<ActualActorSpatialWriteRead>
  read_field_exit_placement(const MomentOwnerSource &actual_exit,
                            const StableId &actor_id) const = 0;
  virtual Outcome<CheckpointVector<ActualActorSpatialWriteRead>>
  read_field_exit_placement_history(const StableId &actor_id) const = 0;
  // 逐笔原死俘来源必核真实 Life / Capacity、所属入场与原 close；成功只给
  // 期望投影。实际本次 Common mutation 仍由 shared capture 唯一绑定。
  virtual Outcome<FieldActorMemberLossPlacementRead>
  read_field_member_loss_placement(const MomentOwnerSource &actual_handoff,
                                   const StableId &actor_id) const = 0;
  // 历史只枚举真实 member_loss_handoffs 已采用行，不把裸伤亡结果视为交接。
  virtual Outcome<CheckpointVector<FieldActorMemberLossPlacementRead>>
  read_field_member_loss_placement_history(const StableId &actor_id) const = 0;
  // 同目标Settlement的真实ready采用：generic cancel不获授权；仍属原Army而非卸装。
  virtual Outcome<FieldActorArmyExitStablePlacementRead>
  read_field_army_exit_stable_placement(const MomentOwnerSource &actual_cancel,
                                       const StableId &actor_id) const = 0;
  // 包括早期orderly退出后真实重绑的具名成员；历史只输出实际采用过的点与双原头。
  virtual Outcome<CheckpointVector<ActualActorSpatialWriteRead>>
  read_field_army_exit_stable_placement_history(const StableId &actor_id) const = 0;
  // 枚举真实入场名册中的本 actor；终点为已采用成员交接或真实胜方余程。
  // 完整原来源验证必须先完成；裸退出行不构成结束，未退出区间保持开放。
  virtual Outcome<CheckpointVector<FieldActorEngagementRead>>
  read_field_actor_engagement_history(const StableId &actor_id) const = 0;
};
} // namespace hanmo::resident
