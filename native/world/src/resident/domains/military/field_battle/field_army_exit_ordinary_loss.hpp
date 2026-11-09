// 原截止步的普通兵伤亡父源；复用 MilitaryCasualtyFact，不伪造战斗段或新身体账。
#pragma once
#include "field_army_exit_model.hpp"
#include "field_segment_settlement.hpp"
#include "field_member_loss_desertion.hpp"
namespace hanmo::resident {
struct FieldExactContactReadContext;
struct FieldBattleAbilityReadContext;
// 只借同World实际正文；底层不调用完整几何/名册历史，避免原名册读口自递归。
struct FieldArmyExitOrdinaryCasualtyRead {
  const FieldArmyExitPlan *plan = nullptr;
  const FieldArmyExitStep *step = nullptr;
  const FieldBattleSession *session = nullptr;
  const MilitaryCasualtyFact *casualties = nullptr;
};
// 核真实step/Common头、唯一伤亡回执和终局名单的整普通兵差量。
// 本口不替代外层完整step几何验证，也不证明S4/S1已应用。
Outcome<FieldArmyExitOrdinaryCasualtyRead> read_field_army_exit_ordinary_casualty_source(
    const CommonStore &, const MilitaryState &, const FieldBattleWorldState &,
    const SourceRef &actual_casualty, const FieldSegmentApplicationRead &);
// 正反覆盖每个有ordinary departure的step及引用step父的伤亡正文。
Status validate_field_army_exit_ordinary_casualty_sources(
    const CommonStore &, const MilitaryState &, const FieldBattleWorldState &,
    const FieldSegmentApplicationRead &);
// 完整全表反覆盖由原Scope调用；有后果无真实S4/S1采用必拒。
Status validate_field_army_exit_ordinary_loss_applications(const FieldGroupHandoffReadContext &);
// 完整原候选批次：先验全量真来源，后写全部S4，最后绑定全部原S1证据。
// 不暴露可伪造prepared值；错误由中央丢弃整候选，不发布半套后果。
struct FieldArmyExitOrdinaryLossApplication {
  // 本真实step没有ordinary departure时为nullopt，两effects向量也为空。
  std::optional<SourceRef> actual_casualty;
  std::vector<BoundActorAvailabilityWrite> availability;
  std::vector<BoundSpatialLocationWrite> locations;
};
Outcome<FieldArmyExitOrdinaryLossApplication>
apply_field_army_exit_ordinary_losses_with_capture_in_candidate(
    CommonStore &, MilitaryState &, AuthoredActorFoundationState &,
    MovementExactState &, GroupMovementExecutionState &,
    const FieldGroupHandoffReadContext &, const SourceRef &actual_step,
    const CurrentReadStampRead &, ReadStampAuthority &,
    const DomainReadVersionSource &, CheckpointMemoryCharge &,
    ExactArithmeticBudget = {});
} // namespace hanmo::resident
