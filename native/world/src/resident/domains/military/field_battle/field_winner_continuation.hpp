#pragma once
#include "field_exact_contact_source.hpp"
namespace hanmo::resident {
struct FieldBattleSession;
struct FieldSegmentSettlement;
struct FieldBattleRoadBinding;
struct MovementContactContinuationSource;
struct MovementArrivalSource;
struct MovementPrefixState;
struct MovementUnitState;
struct FieldWinnerContinuationRead {
  const FieldBattleSession *session = nullptr;
  const FieldSegmentSettlement *terminal = nullptr;
  const FieldBattleRoadBinding *binding = nullptr;
  FieldExactRoadContactRead original;
  Integer endpoint = 0;
  const MovementPrefixState *prefix = nullptr;
  const MovementUnitState *closed_unit = nullptr;
  const MovementContactContinuationSource *continuation = nullptr;
  const MovementArrivalSource *arrival = nullptr;
  WorldMoment opening_at;
};
// 初始双编组真接触的胜方保留原道路停止锚点。下一半日只借原经济
// Opening 头与 M；不从 Field 秒造世界时刻，不重发军令或另开旅程。
// 热调用只用于同一未发布候选，失败必须整候选撤销；冷读只反读原源，
// 不调用 whole Field/Group/Complete。S4/S1 全部原后果仍须组合根全验。
// 原路最终端点使用既有 movement_arrival（arrival 借行）；有正余程或
// 原路径下一边则使用 movement_contact_continuation，不制造零长 unit。
Outcome<FieldWinnerContinuationRead> read_field_winner_continuation(
    const FieldExactContactReadContext &, const FieldBattleCatalog &,
    const StableId &battle_id, const StableId &army_id,
    const SourceRef *actual_continuation = nullptr, ExactArithmeticBudget = {});
Status validate_field_winner_continuations(const FieldExactContactReadContext &,
                                           const FieldBattleCatalog &,
                                           ExactArithmeticBudget = {});
} // namespace hanmo::resident
