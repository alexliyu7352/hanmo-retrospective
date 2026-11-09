// 逃散的实际 S4 单写与 S1 采用；结果只借本次 capture，不新增持久位置或身体账。
#pragma once
#include "field_group_handoff.hpp"
#include "resident/domains/actor_foundation/condition_capture.hpp"
#include "resident/domains/travel/spatial_location_capture.hpp"
namespace hanmo::resident {
struct CharacterConditionSources;
struct FieldDesertedMemberApplication {
  BoundActorAvailabilityWrite availability;
  BoundSpatialLocationWrite location;
};
// 原段必须已经证明逃散、原编队已经产生真实 exit；失败由外层弃整候选。
Outcome<FieldDesertedMemberApplication>
apply_field_deserted_group_member_with_capture_in_candidate(
    CommonStore &, AuthoredActorFoundationState &, MovementExactState &,
    GroupMovementExecutionState &, const FieldGroupHandoffReadContext &,
    const SourceRef &actual_casualty, const StableId &actor, const ReadStamp &,
    const ReadStampAuthority &, const DomainReadVersionSource &,
    CheckpointMemoryCharge &, ExactArithmeticBudget = {});
} // namespace hanmo::resident
