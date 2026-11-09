// 野战ready的唯一有限返程reader/adopter；原出征和到营卸装均不被冒充。
#pragma once
#include "field_army_exit_ready.hpp"
#include "resident/domains/military/military_finite_return_source.hpp"
#include "resident/domains/travel/group_movement.hpp"
namespace hanmo::resident {
class FieldArmyExitFiniteReturnRead final : public MilitaryFiniteReturnSourceRead {
  const FieldExactContactReadContext &x_;
  const FieldBattleCatalog &catalog_;
  const FieldBattleAbilityReadContext &abilities_;
public:
  FieldArmyExitFiniteReturnRead(const FieldExactContactReadContext &,
      const FieldBattleCatalog &, const FieldBattleAbilityReadContext &);
  bool bound_to(const CommonStore &, const MilitaryState &, const SpatialCarrierState &,
      const MovementExactState &, const GroupMovementExecutionState &) const noexcept override;
  Outcome<MilitaryFiniteReturnOriginalRead> read_original_finite_return(const SourceRef &) const override;
  Outcome<MilitaryFiniteReturnOriginalRead> read_actual_finite_return_continuation(const SourceRef &) const override;
};
// 原点已到目标Settlement时，中央先做真实整军stable/capture；这里只采用同源
// returning/plan绑定，不启动未选ArmyReturn，也不造零movement opening。
Status adopt_field_army_exit_at_destination_in_candidate(
    CommonStore &, MilitaryState &, FieldBattleWorldState &,
    const FieldExactContactReadContext &, const FieldBattleCatalog &,
    const FieldBattleAbilityReadContext &, const SourceRef &actual_ready,
    const SourceRef &actual_cancellation, CheckpointMemoryCharge &);
// 仅真实CommanderUnavailable可保留未采用ready；其它错误不变成等待。
Outcome<bool> read_field_army_exit_return_wait(const FieldExactContactReadContext &,
    const FieldBattleCatalog &, const FieldBattleAbilityReadContext &, const SourceRef &actual_ready);
// writer在分配cancel前核实际当前期末；历史boundary另核真实头及全部等待期。
Status validate_field_army_exit_current_adoption_window(const FieldExactContactReadContext &,
    const FieldBattleCatalog &, const FieldBattleAbilityReadContext &, const SourceRef &actual_ready);
Status validate_field_army_exit_adoption_boundary(const FieldExactContactReadContext &,
    const FieldBattleCatalog &, const FieldBattleAbilityReadContext &, const SourceRef &actual_ready,
    const SourceRef &actual_continuation, const WorldMoment &actual_adopted_at);
class FieldArmyExitOpeningAdoption final : public MilitaryGroupOpeningAdoption {
  MilitaryState &military_;
  FieldBattleWorldState &field_;
  const FieldExactContactReadContext &x_;
  const FieldBattleCatalog &catalog_;
  const FieldBattleAbilityReadContext &abilities_;
  const MilitaryFiniteReturnSourceRead &returns_;
  SourceRef ready_;
  CheckpointMemoryCharge &owner_;
public:
  FieldArmyExitOpeningAdoption(MilitaryState &, FieldBattleWorldState &,
      const FieldExactContactReadContext &, const FieldBattleCatalog &,
      const FieldBattleAbilityReadContext &, const MilitaryFiniteReturnSourceRead &,
      SourceRef actual_ready, CheckpointMemoryCharge &);
  Status adopt_original_order_in_candidate(CommonStore &, SpatialCarrierState &,
      MovementExactState &, GroupMovementExecutionState &, const StableId &journey_id) override;
};
} // namespace hanmo::resident
