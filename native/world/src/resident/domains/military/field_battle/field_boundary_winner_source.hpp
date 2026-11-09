#pragma once
#include "field_boundary_source.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {
struct PoliticalRelationsReadContext;
struct FieldOriginalRosterReadContext;
struct FieldReinforcementQueue;
struct FieldBattleBoundaryRecord;
struct FieldBattleSession;
struct FieldBattleRoadBinding;
struct FieldSegmentSettlement;
struct MovementBoundaryContinuationSource;
// 只读覆盖分类，不是继续权或另一支军；partial 绝不默认为整军已齐。
enum class FieldBoundaryWinnerCoverage { partial_admission, fully_admitted };
struct FieldBoundaryWinnerRead {
  CheckpointMemoryCharge copied_sources;
  const FieldBattleBoundaryRecord *boundary = nullptr;
  const FieldBattleSession *session = nullptr;
  const FieldReinforcementQueue *queue = nullptr;
  const FieldBattleRoadBinding *binding = nullptr;
  const FieldSegmentSettlement *terminal = nullptr;
  const MovementBoundaryContinuationSource *continuation = nullptr;
  FieldBoundaryOriginalRead original;
  SourceRef next_opening_source;
  WorldMoment opening_at;
  FieldBoundaryWinnerCoverage coverage = FieldBoundaryWinnerCoverage::partial_admission;
  // 全部指针只借同一原World：入场原并集、原basis未入场差集、实际仍held名单。
  CheckpointVector<const StableId *> admitted_original_members;
  CheckpointVector<const StableId *> unadmitted_original_members;
  CheckpointVector<const StableId *> active_admitted_members;
  CheckpointVector<const StableId *> active_unadmitted_members;
};
// 仅证明尚未续行的边界赢家原因：原单prefix截停/入场/终局/真实Opening。
// actual_continuation仅读既存原单prefix续程；热准入与历史采用分别验，不回写时钟。
// 不产生持久回执，不把它伪装为初始两军endpoint；partial需另有整军合流权威。
Outcome<FieldBoundaryWinnerRead> read_field_boundary_winner_resume_cause(
    const FieldExactContactReadContext &, const PoliticalRelationsReadContext &,
    const FieldBattleCatalog &, const FieldOriginalRosterReadContext &,
    const SourceRef &actual_boundary_contact, ExactArithmeticBudget = {},
    const SourceRef *actual_continuation = nullptr);
} // namespace hanmo::resident
