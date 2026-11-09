// 只在原候选协调点消费目标通知；钱款仍由原 Task 结算，贡献仍由 Affairs
// 唯一表拥有。
#pragma once
#include "resident/domains/tasks/objectives/objective_result_committed.hpp"
namespace hanmo::resident {
struct AffairsState;
// 通知先精确反读原结果，再复用既有贡献 once 键；没有第二支付或验工账。
Status apply_objective_result_committed_in_candidate(
    const CommonStore &, AffairsState &, const TaskState &,
    const ObjectiveResultCommitted &, Integer through_tick);
} // namespace hanmo::resident
