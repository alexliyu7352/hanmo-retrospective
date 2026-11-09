#include "field_reinforcement_moment.hpp"
#include "field_segment_settlement.hpp"
#include "resident/domains/travel/movement_boundary_source.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include <limits>
#include <tuple>
namespace hanmo::resident {
Status validate_field_reinforcement_application_time(
    const FieldExactContactReadContext &x, const FieldBoundaryOriginalRead &origin,
    const FieldBattleTime &local, const WorldMoment &applied,
    const FieldBattleParameters &parameters, bool hot, ExactArithmeticBudget b,
    const SourceRef *actual_application) {
  return movement_detail::status([&] {
    using namespace movement_detail;
    if (!origin.contact || !origin.closed ||
        !x.clock.bound_to(x.common, x.military, x.field))
      fail("FieldReinforcementOriginalClockRequired");
    const auto &occurred = origin.contact->occurred_at;
    const auto now = take(x.clock.read_current_moment());
    if (parameters.window_s <= 0 || parameters.step_s <= 0 ||
        parameters.window_s % parameters.step_s || local.tick < 0 ||
        local.t < 0 || local.t >= parameters.window_s ||
        local.t % parameters.step_s ||
        std::tie(local.tick, local.t) <
            std::tie(origin.entry.tick, origin.entry.t) ||
        take(compare_world_moments(occurred, applied, b)) > 0 ||
        take(compare_world_moments(applied, now, b)) > 0 ||
        (hot && !same_moment(now, applied, b)))
      fail("FieldReinforcementActualApplicationOrderRequired",
           origin.contact->id);
    // 旧原同刻入口不变；不是允许任意后来分数时刻重放。
    if (same_moment(occurred, applied, b))
      return;
    if (local.tick == std::numeric_limits<Integer>::max() ||
        applied.half_day_tick != local.tick + 1 ||
        take(compare_fractions(applied.sub_half_day, CanonicalFraction{}, b)) != 0)
      fail("FieldReinforcementActualClosingRequired", origin.contact->id);
    if (hot) {
      if (x.common.tick() != applied.half_day_tick ||
          !same_moment(take(x.clock.read_field_window_closing_moment()),
                       applied, b))
        fail("FieldReinforcementCommittedClosingRequired", origin.contact->id);
      return;
    }
    if (!actual_application)
      fail("FieldReinforcementHistoricalApplicationRequired", origin.contact->id);
    const auto application_cut =
        take(read_actual_common_fact_position(x.common, *actual_application));
    const auto found = x.field.sessions.find(origin.contact->original_field_source.id);
    if (found == x.field.sessions.end())
      fail("FieldReinforcementHistoricalSessionRequired", origin.contact->id);
    const FieldSegmentSettlement *settled = nullptr;
    for (const auto &candidate : found->second.settlements) {
      if (!same_moment(candidate.applied_at, applied, b))
        continue;
      if (settled)
        fail("FieldReinforcementClosingSettlementAmbiguous", origin.contact->id);
      const SourceRef source{"military", "field_battle_segment", candidate.fact_id};
      const auto original = take(read_field_segment_settlement_source(
          x.common, x.military, x.field, source, &x.clock));
      if (original.session != &found->second ||
          original.settlement != &candidate ||
          application_cut >= take(read_actual_common_fact_position(x.common, source)) ||
          std::tie(local.tick, local.t) >
              std::tie(candidate.settled_at.tick, candidate.settled_at.t))
        fail("FieldReinforcementHistoricalClosingMismatch", origin.contact->id);
      settled = &candidate;
    }
    // 当前尚未结段（包括等待玩家）只借当前真实closing；过期记录必须有原段锚。
    if (!settled &&
        (!same_moment(applied, now, b) ||
         x.common.tick() != applied.half_day_tick ||
         !same_moment(take(x.clock.read_field_window_closing_moment()), applied, b)))
      fail("FieldReinforcementHistoricalClosingRequired", origin.contact->id);
  });
}
} // namespace hanmo::resident
