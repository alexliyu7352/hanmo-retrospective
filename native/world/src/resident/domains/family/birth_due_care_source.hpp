// 出生结果只借真实照护目标与 S0 原 Need；后来的接管不得改写出生时的处置。
#pragma once
#include "care_coverage.hpp"

namespace hanmo::resident {

// 全部引用由同一完整 World lease 拥有，不能使用临时空目标或第二份 Need。
// 这里只反读，不接受 caller 自报的照护、生命、同意或费用成功。
struct BirthDueCareSourceContext {
  const CareGoalSourceContext &care;
  const AffairsState &affairs;
  const StateNeedPolicyRead &policies;
  const StateGoalRead &goals;
  const StateTaskRead &tasks;
};

// CareNeedOpened 必须指该真实出生的第一个 source_episode。
// AcceptedCare 只认原 CareCoverageSatisfied 事实，并核实际同意、交接及覆盖。
Status validate_birth_due_care_disposition(const BirthDueCareSourceContext &,
                                           const BirthFactRef &,
                                           const BirthCareDisposition &);

// 新生初始目标产生后取得保留的原 Need，不因后来失覆盖而换成当前 episode。
// 原 Need 即使已被真实后继结清，出生回执仍保同一个历史 ID。
Outcome<BirthCareDisposition>
read_birth_due_original_care_need(const BirthDueCareSourceContext &,
                                  const BirthFactRef &);

} // namespace hanmo::resident
