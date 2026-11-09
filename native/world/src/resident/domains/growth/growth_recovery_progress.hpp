// 原伤势的恢复点数学；Common仍是伤势唯一现值，本结构只用于逐笔来源的临时重放。
#pragma once
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
struct GrowthRecoveryProgress {
  Integer injury = 0;
  Integer points =
      0; // 当前伤级已实际挣得的0..3点；完全康复后不储存未来伤势抵用额。
};
struct GrowthRecoveryCredit {
  GrowthRecoveryProgress before, after;
  Integer offered_points = 0, used_points = 0;
};
// 4点降一级；轻/中/重由零进度开始分别需4/8/12点，无副作用和时间推进。
Outcome<GrowthRecoveryCredit>
apply_growth_recovery_points(const GrowthRecoveryProgress &,
                             Integer offered_points);
// 只接受原领域给出的单调加重结果；确实加重才丢弃旧级部分进度，重复未加重不重置。
// 这是新显式profile的再伤政策，不倒改旧service/rest完成即减一级的历史。
Outcome<GrowthRecoveryProgress>
apply_growth_injury_increase(const GrowthRecoveryProgress &,
                             Integer new_injury);
Integer growth_injury_hp_limit(Integer injury);
} // namespace hanmo::resident

namespace hanmo::resident {
// character-injury-sources/1 only. The caller must prove the successful
// original action's actual supplied half-days, not a declared duration.
// Old per-period apply_growth_recovery_points keeps its original 1..4 bound.
Outcome<GrowthRecoveryCredit>
apply_growth_terminal_recovery_points_v1(const GrowthRecoveryProgress &,
                                         Integer method_points_per_half_day,
                                         Integer actual_completed_half_days);
// Approved whole-person merge. A genuine battle source must prove incoming
// grade1..3. No death decision or new current balance is created here.
Outcome<GrowthRecoveryProgress>
apply_growth_battle_injury_v1(const GrowthRecoveryProgress &,
                              Integer incoming_injury);
} // namespace hanmo::resident

namespace hanmo::resident {
// 2026-10-27 approved ordinary RestProvided carry. Only the original service
// terms decide reduces_injury; no offered treatment points or second downgrade.
Outcome<GrowthRecoveryProgress>
apply_growth_rest_recovery_v1(const GrowthRecoveryProgress &,
                              bool original_reduces_injury);
} // namespace hanmo::resident
