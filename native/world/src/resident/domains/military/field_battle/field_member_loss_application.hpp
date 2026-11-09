// Actual body application evidence, not a second custody/body/world account.
#pragma once
#include "field_segment_settlement.hpp"
#include "resident/domains/actor_foundation/condition_capture.hpp"
namespace hanmo::resident {
struct CharacterConditionSources;
struct ActorLocationExecutionDraft;
struct ActorConditionExecutionDraft;
struct FieldCapturedMemberApplicationRead;
class FieldCapturedMemberApplication {
  struct Impl;
  std::shared_ptr<const Impl> evidence_;
  friend Outcome<FieldCapturedMemberApplication>
  apply_field_captured_member_in_candidate(
      CommonStore &, AuthoredActorFoundationState &,
      const CharacterConditionSources &, const MilitaryState &,
      const FieldBattleWorldState &, const SourceRef &, const StableId &,
      const FieldSegmentApplicationRead &, const ReadStamp &,
      const ReadStampAuthority &, const DomainReadVersionSource &);
  friend Outcome<FieldCapturedMemberApplicationRead>
  read_actual_field_captured_member_application(
      const CommonStore &, const AuthoredActorFoundationState &,
      const CharacterConditionSources &, const MilitaryState &,
      const FieldBattleWorldState &, const FieldSegmentApplicationRead &,
      const FieldCapturedMemberApplication &);

public:
  FieldCapturedMemberApplication() = default;
};
struct FieldCapturedMemberApplicationRead {
  FieldSegmentCaptivitySourceRead original;
  const BoundActorAvailabilityWrite *availability = nullptr;
  const CapacityTransitionV2 *transition = nullptr;
  const ActorConditionExecutionDraft *condition_write = nullptr;
  const ActorLocationExecutionDraft *location_write = nullptr;
  const SourceRef *actual_casualty = nullptr;
  const SourceRef *actual_capacity_transition = nullptr;
};
// S4 prepare -> the original military available/location single mutation ->
// S4 bind. Caller supplies the real read stamp; no arbitrary before-values.
// Failure requires discarding the enclosing candidate, not publishing a half.
Outcome<FieldCapturedMemberApplication>
apply_field_captured_member_in_candidate(
    CommonStore &, AuthoredActorFoundationState &,
    const CharacterConditionSources &, const MilitaryState &,
    const FieldBattleWorldState &, const SourceRef &actual_casualty,
    const StableId &actor_id, const FieldSegmentApplicationRead &,
    const ReadStamp &, const ReadStampAuthority &,
    const DomainReadVersionSource &);
// Same live capture/savepoint only. Actual S1 ownership release remains a
// separate consumer of this proof, never inferred from current Actor flags.
Outcome<FieldCapturedMemberApplicationRead>
read_actual_field_captured_member_application(
    const CommonStore &, const AuthoredActorFoundationState &,
    const CharacterConditionSources &, const MilitaryState &,
    const FieldBattleWorldState &, const FieldSegmentApplicationRead &,
    const FieldCapturedMemberApplication &);
} // namespace hanmo::resident
