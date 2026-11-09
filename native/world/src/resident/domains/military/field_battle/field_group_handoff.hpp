// Field exit hands only actual exiting members out of an original paused group.
// This is not ArmyReturn and grants no new journey, cargo split or position
// root.
#pragma once
#include "field_settlement_exit_source.hpp"
#include "resident/domains/travel/group_movement.hpp"
namespace hanmo::resident {
class FieldBattleExitPlacementRead;
class PreparedFieldGroupMemberHandoff;
struct CharacterConditionSources;
struct FamilyDeathState;
struct FamilyLifeCauseSources4;
struct FieldBattleAbilityReadContext;
struct FieldGroupHandoffReadContext {
  const CommonStore &common;
  const MilitaryState &military;
  const FieldBattleWorldState &field;
  const TravelCatalog &travel;
  const FieldBattleCatalog &catalog;
  const MovementExactState &movement;
  const SpatialCarrierState &carriers;
  const GroupMovementExecutionState &groups;
  const FieldSegmentApplicationRead &clock;
  const FieldExactContactReadContext *exact_contacts = nullptr;
  const PoliticalRelationsReadContext *diplomacy = nullptr;
  const PoliticalTakeoverTerritoryAccess *territory = nullptr;
  const CharacterConditionSources *original_body = nullptr;
  const FamilyDeathState *original_deaths = nullptr;
  const FamilyLifeCauseSources4 *original_life = nullptr;
  // 同owner最终构成的完整Field来源；低层名册/身体不回调，只有外层新step冷验证借用。
  const FieldBattleAbilityReadContext *original_abilities = nullptr;
};
// 终局逐编队的原退出腿。只借原正文/释放行，绝不将不同出口合并成军团长位置。
// 指针只在本次同World owner且没有后续写入时有效；actual_anchor由原退出reader推导。
struct FieldTerminalReturnLegRead {
  const FieldBattleExitRecord *exit = nullptr;
  const FieldFormation *formation = nullptr;
  const GroupContactMemberReleaseSource *member_release = nullptr;
  MovementAnchor actual_anchor;
};
// 真terminal、每份实际exit、真实S1成员释放与幸存名单全覆盖；不创建返程、取消旧路或收费。
Outcome<std::vector<FieldTerminalReturnLegRead>>
read_field_terminal_return_legs(
    const FieldGroupHandoffReadContext &, const SourceRef &terminal_segment,
    const StableId &army_id, const GroupContactMemberReleaseRead &,
    const FieldBattleExitPlacementRead &, ExactArithmeticBudget = {});
struct MovementBoundaryContact;
struct FieldSingleGroupExitSourceRead {
  const MovementBoundaryContact *contact = nullptr;
  const GroupMovementJourney *journey = nullptr;
  const GroupMovementOpeningSource *opening = nullptr;
  const MovementUnitCloseSource *closed = nullptr;
};
// Original single-prefix entry ancestry for an already verified actual exit.
// No placement/actor write or release adoption is implied by this read.
Outcome<FieldSingleGroupExitSourceRead>
read_field_single_group_exit_source(const FieldGroupHandoffReadContext &,
                                    const FieldBattleExitRecord &,
                                    ExactArithmeticBudget = {});
// A default/foreign/moved permit carries no authority. Prepared before the real
// location mutation, consumed after its actual same-capture Bound writes.
class PreparedFieldGroupMemberHandoff {
  const CommonStore *common_ = nullptr;
  const FieldBattleExitRecord *exit_ = nullptr;
  const GroupMovementJourney *journey_ = nullptr;
  const GroupMovementOpeningSource *opening_ = nullptr;
  const MovementUnitCloseSource *original_close_ = nullptr;
  std::shared_ptr<const ActorLocationCaptureIdentity> capture_;
  Integer first_sequence_ = 0;
  friend Outcome<PreparedFieldGroupMemberHandoff>
  prepare_field_group_member_handoff(const FieldGroupHandoffReadContext &,
                                     const SourceRef &, ExactArithmeticBudget);
  friend Outcome<SourceRef> adopt_field_group_member_handoff_in_candidate(
      CommonStore &, MovementExactState &, GroupMovementExecutionState &,
      const FieldGroupHandoffReadContext &, PreparedFieldGroupMemberHandoff &&,
      const std::vector<BoundSpatialLocationWrite> &,
      const FieldBattleExitPlacementRead &, CheckpointMemoryCharge &,
      ExactArithmeticBudget);

public:
  PreparedFieldGroupMemberHandoff() = default;
  PreparedFieldGroupMemberHandoff(const PreparedFieldGroupMemberHandoff &) =
      delete;
  PreparedFieldGroupMemberHandoff &
  operator=(const PreparedFieldGroupMemberHandoff &) = delete;
  PreparedFieldGroupMemberHandoff(PreparedFieldGroupMemberHandoff &&) = default;
};
Outcome<PreparedFieldGroupMemberHandoff>
prepare_field_group_member_handoff(const FieldGroupHandoffReadContext &,
                                   const SourceRef &,
                                   ExactArithmeticBudget = {});
Outcome<SourceRef> adopt_field_group_member_handoff_in_candidate(
    CommonStore &, MovementExactState &, GroupMovementExecutionState &,
    const FieldGroupHandoffReadContext &, PreparedFieldGroupMemberHandoff &&,
    const std::vector<BoundSpatialLocationWrite> &,
    const FieldBattleExitPlacementRead &, CheckpointMemoryCharge &,
    ExactArithmeticBudget = {});
// Only the real original exit controls the members and target. The caller owns
// the whole candidate; any error discards it, including prior actor mutations.
Outcome<SourceRef> apply_field_exit_group_handoff_in_candidate(
    CommonStore &, MovementExactState &, GroupMovementExecutionState &,
    const FieldGroupHandoffReadContext &, const SourceRef &actual_exit,
    const FieldBattleExitPlacementRead &,
    std::vector<BoundSpatialLocationWrite> &, CheckpointMemoryCharge &,
    ExactArithmeticBudget = {});
} // namespace hanmo::resident
