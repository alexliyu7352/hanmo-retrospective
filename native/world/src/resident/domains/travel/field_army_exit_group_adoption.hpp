// 野战整军退出的原 S1 组合入口；独立声明不扩大通用 Group 公共依赖。
#pragma once
#include "group_movement.hpp"
namespace hanmo::resident {
struct FieldBattleAbilityReadContext;
struct MilitaryState;
class FieldBattleExitPlacementRead;
// 仅真实整军ready可触发；同候选取消旧未来并开原finite_return，不开放自由列表/改锚。
Outcome<SourceRef> adopt_field_army_exit_ready_in_candidate(
    CommonStore &, MilitaryState &, SpatialCarrierState &, MovementExactState &,
    GroupMovementExecutionState &, FieldBattleWorldState &,
    const GroupMovementContext &, const FieldExactContactReadContext &,
    const FieldBattleCatalog &, const FieldBattleAbilityReadContext &,
    const FieldBattleExitPlacementRead &,
    const SourceRef &actual_ready, const ArmyGroupJourneyOpeningInput &,
    MilitaryGroupOpeningAdoption &, std::vector<BoundSpatialLocationWrite> &,
    CheckpointMemoryCharge &, ExactArithmeticBudget = {});
} // namespace hanmo::resident
