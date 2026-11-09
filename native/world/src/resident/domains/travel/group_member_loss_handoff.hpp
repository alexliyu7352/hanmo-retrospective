// 原接触暂停队列仅移交真实伤亡中的死亡、被俘或逃散成员；不另造身体账。
// 逃散子型来源：總設計師 2026-10-29 Slack 批准，1793331657.217219。
#pragma once
#include "hanmo/resident/world_moment.hpp"
#include "resident/domains/family/death_ref.hpp"
#include "resident/kernel/source_ref.hpp"
#include <variant>
namespace hanmo::resident {
struct GroupMemberDeathApplication {
  DeathRef actual_death;
};
struct GroupMemberCaptivityApplication {
  SourceRef actual_capacity_transition;
};
// 逃散只引用原 S4 CapacityTransition，与被俘保持不同的有限 tag。
struct GroupMemberDesertionApplication {
  SourceRef actual_capacity_transition;
};
struct GroupMemberLossHandoffSource {
  StableId id, journey_id, actor_id;
  SourceRef original_contact, actual_casualty;
  Integer casualty_ordinal = 0;
  WorldMoment occurred_at;
  std::variant<GroupMemberDeathApplication, GroupMemberCaptivityApplication,
               GroupMemberDesertionApplication>
      actual_body;
};
class CommonStore;
struct MovementExactState;
struct GroupMovementExecutionState;
// A same-World concrete owner must validate the original casualty, original
// entry/member ancestry and actual Life/Capacity cause, not current flags.
class GroupMemberLossHandoffRead {
public:
  virtual ~GroupMemberLossHandoffRead() = default;
  virtual bool bound_to(const CommonStore &, const MovementExactState &,
                        const GroupMovementExecutionState &) const noexcept = 0;
  virtual Status validate_actual_group_member_loss(
      const GroupMemberLossHandoffSource &) const = 0;
};
Status validate_group_member_loss_handoffs(const CommonStore &,
                                           const MovementExactState &,
                                           const GroupMovementExecutionState &,
                                           const GroupMemberLossHandoffRead *);
} // namespace hanmo::resident
