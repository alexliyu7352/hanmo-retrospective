// 原接触发生 M 与局部入场实际应用 M 分开核证；不构造或回拨时钟。
#pragma once
#include "field_boundary_source.hpp"
namespace hanmo::resident {
// hot 为真正写入前的准入；冷读只验原记录与当前真实上界，不伪造历史clock。
// 旧同 M 路径保持；延迟入场必须在该 local 窗对应的真实宏观 closing 应用。
Status validate_field_reinforcement_application_time(
    const FieldExactContactReadContext &, const FieldBoundaryOriginalRead &,
    const FieldBattleTime &local, const WorldMoment &applied,
    const FieldBattleParameters &, bool hot, ExactArithmeticBudget = {},
    const SourceRef *actual_application = nullptr);
} // namespace hanmo::resident
