// 整军实际到齐的有限S1接头；公开值只借同World原件，调用者不能自填值授取消权。
#pragma once
#include "field_army_exit_rally.hpp"
#include "resident/domains/travel/movement_exact_model.hpp"
namespace hanmo::resident {
struct GroupMovementJourney;
struct GroupMovementOpeningSource;
struct GroupContactMemberReleaseSource;
struct FieldBattleRoadBinding;
struct FieldArmyExitReadyRead {
  const FieldArmyExitPlan *plan = nullptr;
  const FieldBattleSession *session = nullptr;
  const FieldBattleRoadBinding *original_binding = nullptr;
  const GroupMovementJourney *original_journey = nullptr;
  const GroupMovementOpeningSource *original_opening = nullptr;
  const MovementPrefixState *original_prefix = nullptr;
  const MovementUnitCloseSource *original_close = nullptr;
  SourceRef original_contact;
  MovementAnchor actual_anchor;
  FieldBattleTime actual_exit_at;
  // 原几何ready应用的closing；不是可能晚到的S1实际采用时刻。
  WorldMoment adopted_at;
  // 全部幸存具名人物与仍受旧contact hold者分开；旧真实有序退出不可撤回。
  std::vector<StableId> named_participants, held_named_participants;
  std::vector<const GroupContactMemberReleaseSource *> prior_orderly_releases;
};
// 完整原choice/terminal/全场有序step/实际closing proof；不改原cancel锚，不开新路。
Outcome<FieldArmyExitReadyRead> read_field_army_exit_ready_source(
    const FieldExactContactReadContext &, const FieldBattleCatalog &,
    const FieldBattleAbilityReadContext &, const SourceRef &ready_source,
    ExactArithmeticBudget = {});
} // namespace hanmo::resident

namespace hanmo::resident {
struct GroupMovementControlSource;
struct FieldArmyExitContinuationRead {
  FieldArmyExitReadyRead ready;
  const GroupMovementControlSource *cancellation = nullptr;
  const GroupMovementOpeningSource *opening = nullptr;
  const GroupMovementJourney *journey = nullptr;
  // 真出口已是原目标聚落：只采用stable点和旧取消，不构造零路程或假卸装。
  bool already_at_destination = false;
};
// 唯一旧contact取消→ready实际点→新finite_return采用。具名原Field终点只覆盖
// ready.held_named_participants；原先有序退出者保自己的旧退出，不产生第二终点。
Outcome<FieldArmyExitContinuationRead> read_field_army_exit_continuation_source(
    const FieldExactContactReadContext &, const FieldBattleCatalog &,
    const FieldBattleAbilityReadContext &, const SourceRef &actual_continuation,
    ExactArithmeticBudget = {});
} // namespace hanmo::resident
