#pragma once
#include "field_group_handoff.hpp"
#include "field_segment_settlement.hpp"
#include "field_army_exit_ordinary_loss.hpp"
namespace hanmo::resident {
struct FieldMemberLossGroupOrigin {
  FieldSegmentSettlementSourceRead segment;
  // 真新step父独立保存；旧segment仅旧分支有效，不构造影子settlement。
  std::optional<FieldArmyExitOrdinaryCasualtyRead> ordinary_loss;
  const FieldBattleSession *session = nullptr;
  const WorldMoment *applied_at = nullptr;
  const MilitaryActorChange *actor_change = nullptr;
  Integer casualty_ordinal = 0;
  const FieldBattleRoadBinding *battle_binding = nullptr;
  const FieldBattleMemberBinding *member = nullptr;
  const GroupMovementJourney *journey = nullptr;
  const GroupMovementOpeningSource *opening = nullptr;
  const MovementUnitCloseSource *closed = nullptr;
  const SourceRef *original_contact = nullptr;
  const WorldMoment *entered_at = nullptr;
};
// Real original entry/closed group ancestry only. No claim that Life/body,
// spatial binding, or member release has been applied; no current alive flags.
Outcome<FieldMemberLossGroupOrigin> read_field_member_loss_group_origin(
    const FieldGroupHandoffReadContext &, const SourceRef &actual_casualty,
    const StableId &actor, ExactArithmeticBudget = {});
// 逃散世界落点只借该原编队已发生的唯一离场记录；不会借死亡入口锚点。
Outcome<const FieldBattleExitRecord *>
read_field_member_desertion_exit_source(const FieldGroupHandoffReadContext &,
                                        const FieldMemberLossGroupOrigin &,
                                        ExactArithmeticBudget = {});
} // namespace hanmo::resident
