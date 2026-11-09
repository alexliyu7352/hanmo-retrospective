// 先验有限整数再结算恢复点，剩余点只属于同一实际伤势，不生成药物或未来免费治疗。
#include "growth_recovery_progress.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {
namespace {
bool valid(const GrowthRecoveryProgress &p) {
  return p.injury >= 0 && p.injury <= 3 && p.points >= 0 && p.points < 4 &&
         (p.injury != 0 || p.points == 0);
}
} // namespace
Outcome<GrowthRecoveryCredit>
apply_growth_recovery_points(const GrowthRecoveryProgress &p, Integer offered) {
  if (!valid(p) || offered < 1 || offered > 4)
    return {false, {}, common_failure("InvalidGrowthRecoveryPoints").error};
  GrowthRecoveryCredit out;
  out.before = p;
  out.after = p;
  out.offered_points = offered;
  out.used_points = std::min<Integer>(offered, 4 * p.injury - p.points);
  const auto total = p.points + out.used_points;
  out.after.injury = p.injury - total / 4;
  out.after.points = out.after.injury ? total % 4 : 0;
  return {true, std::move(out), {}};
}
Outcome<GrowthRecoveryProgress>
apply_growth_injury_increase(const GrowthRecoveryProgress &p, Integer injury) {
  if (!valid(p) || injury < p.injury || injury > 3)
    return {false, {}, common_failure("InvalidGrowthInjuryIncrease").error};
  return {true, {injury, injury > p.injury ? 0 : p.points}, {}};
}
Integer growth_injury_hp_limit(Integer injury) {
  constexpr Integer limits[] = {100, 90, 75, 50};
  return injury >= 0 && injury <= 3 ? limits[injury] : -1;
}
} // namespace hanmo::resident

namespace hanmo::resident {
Outcome<GrowthRecoveryCredit>
apply_growth_terminal_recovery_points_v1(const GrowthRecoveryProgress &before,
                                         Integer method,
                                         Integer actual_half_days) {
  if (!valid(before) || (method != 1 && method != 2) || actual_half_days < 1 ||
      actual_half_days > std::numeric_limits<Integer>::max() / method)
    return {false,
            {},
            common_failure("InvalidTerminalRecoveryPointsVersion1").error};
  GrowthRecoveryCredit result;
  result.before = before;
  result.after = before;
  result.offered_points = method * actual_half_days;
  result.used_points = std::min<Integer>(result.offered_points,
                                         4 * before.injury - before.points);
  const auto total = before.points + result.used_points;
  result.after.injury = before.injury - total / 4;
  result.after.points = result.after.injury ? total % 4 : 0;
  return {true, std::move(result), {}};
}
Outcome<GrowthRecoveryProgress>
apply_growth_battle_injury_v1(const GrowthRecoveryProgress &before,
                              Integer incoming) {
  if (!valid(before) || incoming < 1 || incoming > 3)
    return {false, {}, common_failure("InvalidBattleInjuryVersion1").error};
  const Integer injury =
      before.injury > 0
          ? std::min<Integer>(3, std::max(before.injury, incoming) + 1)
          : incoming;
  // Every positive rehit either increases the grade or repeats grade3. Both
  // cases reset earned progress under the explicitly approved source/1 rule.
  return {true, {injury, 0}, {}};
}
} // namespace hanmo::resident

namespace hanmo::resident {
Outcome<GrowthRecoveryProgress>
apply_growth_rest_recovery_v1(const GrowthRecoveryProgress &before,
                              bool original_reduces_injury) {
  if (!valid(before))
    return {
        false, {}, common_failure("InvalidRestRecoveryProgressVersion1").error};
  if (!original_reduces_injury)
    return {true, before, {}};
  GrowthRecoveryProgress after{std::max<Integer>(0, before.injury - 1), 0};
  // Four points reduce a level. Retain at most three, never trigger a second
  // reduction at this same rest completion; no future injury credit at zero.
  if (after.injury)
    after.points = std::min<Integer>(before.points, 3);
  return {true, after, {}};
}
} // namespace hanmo::resident
