// 原看管方位置的有限历史读口；借同 World 原根，不另存 carrier 位置账。
#pragma once
#include "military_return_source.hpp"
#include "named_military_custody_release_read.hpp"
#include "resident/domains/travel/group_movement.hpp"
namespace hanmo::resident {
class FieldBattleExitPlacementRead;
struct NamedMilitaryCustodyCarrierHistoryContext {
  const NamedMilitaryCustodyReleaseContext &original;
  const GroupMovementExecutionState &group_executions;
  const MilitaryReturnSourceRead *returns = nullptr;
  const FieldBattleExitPlacementRead *field_exits = nullptr;
};
struct OriginalNamedMilitaryCustodyCarrierPoint {
  MovementAnchor anchor;
  std::vector<MomentOwnerSource> original_sources;
};
// 当前入口只接受同根真 term 与实际 clock/prefix，不接受自报的观察时刻。
Outcome<OriginalNamedMilitaryCustodyCarrierPoint>
read_current_named_military_custody_carrier_position(
    const NamedMilitaryCustodyCarrierHistoryContext &,
    const NamedMilitaryCustodyTerm &);
// 永久历史只接受同根真 closure；先借其原 keeper 链再读真实 S1 因果截面。
Outcome<OriginalNamedMilitaryCustodyCarrierPoint>
read_original_named_military_custody_carrier_before_release(
    const NamedMilitaryCustodyCarrierHistoryContext &,
    const NamedMilitaryCustodyRelease &);
// Real historical carrier point, NOT ordinary stationary-service permission.
// S1's original adopting owner supplies the associated actual M/prefix.
Outcome<OriginalNamedMilitaryCustodyCarrierPoint>
read_named_military_custody_carrier_at_original_prefix(
    const NamedMilitaryCustodyCarrierHistoryContext &,
    const NamedMilitaryCustodyTerm &, const WorldMoment &,
    const CommonActualFactPrefix &);
} // namespace hanmo::resident
