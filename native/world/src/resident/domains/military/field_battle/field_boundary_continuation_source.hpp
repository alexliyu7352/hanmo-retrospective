#pragma once
#include "field_boundary_source.hpp"
namespace hanmo::resident {
struct PoliticalRelationsReadContext;
struct FieldBattleBoundaryRecord;
struct FieldSegmentSettlement;
enum class FieldBoundaryResumeTiming { after_terminal, next_half_day };
struct FieldBoundaryContinuationRead {
  const FieldBattleBoundaryRecord *boundary = nullptr;
  FieldBoundaryOriginalRead original;
  const FieldSegmentSettlement *terminal = nullptr;
  FieldBoundaryResumeTiming timing = FieldBoundaryResumeTiming::after_terminal;
  WorldMoment actual_now;
};
// 从未入场分支保原拒门；已入场仅接受完整真实赢家/下一Opening原cause。
// 部分入场、仍在战或败方不能用本读口冒整军合流/返回权限。
Outcome<FieldBoundaryContinuationRead> read_field_boundary_continuation_source(
    const FieldExactContactReadContext &, const PoliticalRelationsReadContext &,
    const FieldBattleCatalog &, const SourceRef &actual_field_contact,
    ExactArithmeticBudget = {}, const SourceRef *actual_continuation = nullptr);
} // namespace hanmo::resident
