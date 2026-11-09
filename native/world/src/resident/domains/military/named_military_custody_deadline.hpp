// 原期限只作 S0 一次性事件的来源，不在本域建立另一个当前时钟或 timer 表。
#pragma once
#include "named_military_custody_release_read.hpp"
namespace hanmo::resident {
Outcome<std::vector<const NamedMilitaryCustodyTerm *>> read_due_named_military_custody_terms(
    const NamedMilitaryCustodyReleaseContext &, const WorldMoment &actual_from,
    const WorldMoment &requested_to);
} // namespace hanmo::resident
