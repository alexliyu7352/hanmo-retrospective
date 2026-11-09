// 原 Birth4 安排的真实到期预测，不持第二日程或出生结果。
// 完整 Family factory 仍须提供出生、人口、供养、照护及条件载荷的真实写入链。
#pragma once
#include "born_character_source.hpp"
#include "resident/kernel/exact_phase_graph.hpp"

namespace hanmo::resident {
// 纯读实际 accepted/终态原表，在真实当前时刻到 permit_to 间只返回最早一项。
// 同 M 按原 Common 事实顺序，采用后必须重新读取；漏过到期是错误，不补生。
Outcome<std::vector<ExactEventPrediction>>
predict_original_birth_due(const BornCharacterSourceContext &,
                           const WorldMoment &current,
                           const WorldMoment &permit_to);

// 原 writer 采用前重读本刻最早真实安排，校验整份预测及真实 revision。
// 不分 ID、不消费预约、不分派生死结果；实际死因/出生生产者仍独立验证。
Outcome<ScheduledBirthSource>
read_original_birth_due_prediction_source(const BornCharacterSourceContext &,
                                          const ExactEventPrediction &);
} // namespace hanmo::resident
