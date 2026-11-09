#include "field_army_exit_return.hpp"
#include "field_ability_sources.hpp"
#include "field_exact_contact_source.hpp"
#include "resident/domains/actor_foundation/character_availability_read.hpp"
#include "resident/domains/actor_foundation/condition_application_history.hpp"
#include "resident/domains/military/military_field_command_source.hpp"
#include "resident/domains/military/training_exact.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/storage/common_store.hpp"
#include <limits>
namespace hanmo::resident {
using namespace movement_detail;
namespace {
// 真业务头前的原指挥及完整生命/可用链，只证明CommanderUnavailable，不吞其它Required。
bool unavailable_before(const FieldExactContactReadContext &x, const StableId &army,
    const SourceRef &boundary, const WorldMoment &at) {
  const auto command = take(query_military_army_command_source_before_at(x.common, x.military,
      army, boundary.id, take(x.clock.read_current_moment()), &x.formations));
  const auto body = take(read_character_condition_before_application_fact(x.conditions,
      command.commander_actor_id, boundary, at));
  const auto availability = take(read_character_availability_before_fact_provenance_v2(x.conditions,
      command.commander_actor_id, boundary, at, x.clock));
  if (!availability.present) fail("MilitaryFiniteReturnActualCommanderRequired", command.commander_actor_id);
  return !body.body.alive || !availability.alive || !availability.available;
}
// 当前否定也从原应用前缀/availability还原，不以今日旗号替历史理由。
bool unavailable_now(const FieldExactContactReadContext &x, const StableId &army) {
  const auto now = take(x.clock.read_current_moment());
  const auto command = take(query_military_army_command_source_at(x.common, x.military,
      army, x.clock, &x.formations));
  const auto *actor = x.common.actor(command.commander_actor_id);
  if (!actor || !x.conditions.deaths || !x.conditions.life_sources)
    fail("MilitaryFiniteReturnActualCommanderRequired", command.commander_actor_id);
  const auto borrowed = take(read_current_common_fact_prefix(x.common));
  CommonActualFactPrefix prefix{borrowed.common_fact_count, {}};
  if (borrowed.last_common_fact) prefix.last_common_fact = SourceRef{
      borrowed.last_common_fact->domain, borrowed.last_common_fact->kind, borrowed.last_common_fact->id};
  const auto presence = take(read_character_applied_presence_at_prefix_v2(x.common,
      x.conditions.state, x.conditions.catalog, *x.conditions.deaths, *x.conditions.life_sources,
      command.commander_actor_id, prefix, prefix, now, x.clock, x.conditions.family_catalog,
      x.conditions.shared, x.conditions.born_sources));
  take(validate_military_availability_actor_history(x.conditions, command.commander_actor_id));
  const auto body = take(read_current_character_condition(x.conditions, command.commander_actor_id));
  if (!presence.present || actor->alive != presence.alive)
    fail("MilitaryFiniteReturnActualCommanderRequired", command.commander_actor_id);
  return !body.body.alive || !presence.alive || !actor->available;
}
// 逐已经发布的旧closing证明等待；不能删开程后任意把采用拖到多年以后。
void prior_waits(const FieldExactContactReadContext &x, const FieldArmyExitReadyRead &ready,
    const WorldMoment &until) {
  if (!x.periods) fail("FieldArmyExitOriginalWindowRequired", ready.plan->fact_id);
  if (take(compare_world_moments(ready.adopted_at, until)) > 0)
    fail("FieldArmyExitAdoptionBeforeReady", ready.plan->fact_id);
  if (until.half_day_tick <= ready.adopted_at.half_day_tick) return;
  if (!unavailable_before(x, ready.plan->army_id, *ready.plan->ready_source, ready.adopted_at))
    fail("FieldArmyExitWaitingSourceRequired", ready.plan->fact_id);
  const bool inside_period = take(compare_fractions(until.sub_half_day, CanonicalFraction{})) > 0;
  for (Integer tick = ready.adopted_at.half_day_tick + 1;
       tick < until.half_day_tick || (inside_period && tick == until.half_day_tick); ++tick) {
    const auto period = take(x.periods->read_training_period(tick));
    if (period.from_tick != tick || period.original_opening.source_schema != 1 ||
        period.opening_at.half_day_tick != tick ||
        take(compare_fractions(period.opening_at.sub_half_day, CanonicalFraction{})) != 0 ||
        !unavailable_before(x, ready.plan->army_id, period.original_opening.source_ref, period.opening_at))
      fail("FieldArmyExitWaitingSourceRequired", ready.plan->fact_id);
    if (tick == until.half_day_tick) break; // 最大整数tick也不溢出。
  }
}
// 来源M必须属于真实原期末；热时读实际closing，冷时用下一Opening头证明写入切点。
void closing(const FieldExactContactReadContext &x, const WorldMoment &at,
    const std::optional<SourceRef> &actual) {
  const auto now = take(x.clock.read_current_moment());
  if (!x.periods || at.half_day_tick <= 0 ||
      take(compare_fractions(at.sub_half_day, CanonicalFraction{})) != 0 ||
      take(compare_world_moments(at, now)) > 0)
    fail("FieldArmyExitActualAdoptionClosingRequired");
  const auto period = take(x.periods->read_training_period(at.half_day_tick - 1));
  if (period.from_tick != at.half_day_tick - 1 || period.to_tick != at.half_day_tick ||
      period.original_opening.source_schema != 1 ||
      period.opening_at.half_day_tick != at.half_day_tick - 1 ||
      take(compare_fractions(period.opening_at.sub_half_day, CanonicalFraction{})) != 0)
    fail("FieldArmyExitActualAdoptionClosingRequired");
  if (actual && take(read_actual_common_fact_position(x.common, period.original_opening.source_ref)) >=
      take(read_actual_common_fact_position(x.common, *actual)))
    fail("FieldArmyExitActualAdoptionClosingRequired", actual->id);
  if (same_moment(at, now)) {
    if (!same_moment(at, take(x.clock.read_field_window_closing_moment())))
      fail("FieldArmyExitActualAdoptionClosingRequired");
  } else {
    if (!actual) fail("FieldArmyExitActualAdoptionClosingRequired");
    const auto after = take(x.periods->read_training_period(at.half_day_tick));
    if (!same_moment(after.opening_at, at) || after.original_opening.source_schema != 1 ||
        take(read_actual_common_fact_position(x.common, after.original_opening.source_ref)) <=
            take(read_actual_common_fact_position(x.common, *actual)))
      fail("FieldArmyExitActualAdoptionClosingRequired", actual->id);
  }
}
}
// 几何到齐不是主将资格；只有原Unavailable否定可以保留尚未采用的ready。
Outcome<bool> read_field_army_exit_return_wait(const FieldExactContactReadContext &x,
    const FieldBattleCatalog &catalog, const FieldBattleAbilityReadContext &a, const SourceRef &source) {
  return guard<bool>([&] {
    const auto ready = take(read_field_army_exit_ready_source(x, catalog, a, source));
    if (ready.plan->actual_continuation) fail("FieldArmyExitReturnAlreadyAdopted", source.id);
    if (!unavailable_now(x, ready.plan->army_id)) return false;
    if (!unavailable_before(x, ready.plan->army_id, source, ready.adopted_at))
      fail("FieldArmyExitWaitingSourceRequired", source.id);
    prior_waits(x, ready, take(x.clock.read_current_moment()));
    return true;
  });
}
// 新writer前的时钟准入；不修改ready时刻，不产生新PeriodOpening或空开程。
Status validate_field_army_exit_current_adoption_window(const FieldExactContactReadContext &x,
    const FieldBattleCatalog &catalog, const FieldBattleAbilityReadContext &a, const SourceRef &source) {
  return status([&] {
    const auto ready = take(read_field_army_exit_ready_source(x, catalog, a, source));
    const auto now = take(x.clock.read_current_moment());
    closing(x, now, {}); prior_waits(x, ready, now);
  });
}
// 反读实际取消/开程发生的期末，真实头必须在ready之后且仍属同军原链。
Status validate_field_army_exit_adoption_boundary(const FieldExactContactReadContext &x,
    const FieldBattleCatalog &catalog, const FieldBattleAbilityReadContext &a, const SourceRef &source,
    const SourceRef &actual, const WorldMoment &at) {
  return status([&] {
    const auto ready = take(read_field_army_exit_ready_source(x, catalog, a, source));
    if (actual.domain != "travel" || (actual.kind != "group_movement_opening" && actual.kind != "group_movement_control") ||
        take(read_actual_common_fact_position(x.common, source)) >= take(read_actual_common_fact_position(x.common, actual)))
      fail("FieldArmyExitReturnContinuationOrderRequired", actual.id);
    const auto *control = actual.kind == "group_movement_control" ? x.groups.controls.find(actual.id) : nullptr;
    const auto *opening = actual.kind == "group_movement_opening" ? x.groups.openings.find(actual.id) : nullptr;
    const auto *journey = opening ? x.carriers.journeys.find(opening->journey_id) : nullptr;
    const auto *army = journey ? std::get_if<ArmyCarrierRef>(&journey->carrier) : nullptr;
    if (control ? (control->journey_id != ready.original_journey->journey_id ||
                   !same_moment(control->occurred_at, at) || !same_source(control->authority_source, source)) :
        (!opening || !army || army->army_id != ready.plan->army_id || !same_moment(opening->occurred_at, at)))
      fail("FieldArmyExitReturnContinuationSourceMismatch", actual.id);
    closing(x, at, actual); prior_waits(x, ready, at);
  });
}
} // namespace hanmo::resident
