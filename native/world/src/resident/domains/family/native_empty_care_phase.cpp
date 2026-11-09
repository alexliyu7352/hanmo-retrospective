#include "native_empty_care_phase.hpp"
#include "native_empty_care_sources.hpp"
#include "resident/domains/growth/native_recovery_absence.hpp"
#include "resident/kernel/native_deferred_arcana_absence.hpp"
#include "resident/kernel/native_character_injury_state.hpp"
#include "resident/kernel/world_bundle.hpp"
#include "resident/kernel/exact_complete_world.hpp"
#include <limits>

namespace hanmo::resident {
namespace {
// 这里的空指针只对应当前 World 根结构中确实不存在的拥有槽；若未来新增根，
// 必须成套改本同世界组合，不能把已选模块交给空读取器。
Status empty_world(const WorldBundle &w) {
  if (!w.catalog || !w.actor_foundation || !w.action_elapsed)
    return common_failure("EmptyCareActualWorldRootsRequired");
  if (w.family_birth_enabled() || w.family_care_relief_enabled())
    return common_failure("NOT_IMPLEMENTED", "birth_or_care",
                          ErrorKind::unsupported_capability);
  for (const auto &module : w.catalog->identity.module_versions)
    if (module.first == "family_birth" ||
        module.first == "family_care_relief" ||
        module.first == "care_requirements" ||
        module.first == "travel_dependents")
      return common_failure("EmptyCareSelectedBusinessNotAllowed", module.first,
                            ErrorKind::unsupported_capability);
  NativeEmptyCareSourceRead source{{w.common, &w.family_birth, nullptr,
                                    &*w.actor_foundation, &*w.action_elapsed,
                                    nullptr, &w.child_care_relief}};
  return source.validate_sources();
}
// 原永久伤效只做正向全源验证；未选的到期/疗程/法效仍走原缺席门。
// 每次直接借同 World 的真实身体来源，不能按当前 episode 是否为空选分支。
Status remaining_world(const WorldBundle &w) {
  auto valid = validate_native_deferred_arcana_absence(w);
  if (!valid.ok) return valid;
  valid = validate_native_growth_recovery_absence(w);
  if (!valid.ok) return valid;
  auto owner = w.complete_exact_time_bindings();
  if (!owner.ok) return {false, {}, std::move(owner.error)};
  if (!owner.value) return common_failure("RecoveryAbsenceActualOwnerRequired");
  auto body = owner.value->reads().character_conditions.sources();
  if (!body.ok) return {false, {}, std::move(body.error)};
  auto injury =
      selected_native_character_injury_source_version(w.catalog->identity);
  if (!injury.ok) return {false, {}, std::move(injury.error)};
  if (injury.value == 1)
    return validate_native_permanent_injury_calendar_sources(w, body.value);
  return validate_native_condition_due_absence(w, body.value);
}
// 真日历期尾与同一次写上下文，不把空阶段当作自由调用的成功回执。
Status empty_calendar_stage(WorldBundle &w, const ExactPeriodContext &period,
                            ExactPhaseWriteContext &context) {
  if (!w.world_timeline ||
      w.world_timeline->boundary_state != PeriodBoundaryState::open ||
      w.world_timeline->period_id != period.period_id ||
      period.opening_at.half_day_tick != w.common.tick() ||
      period.period_id.empty() || period.opening_at.half_day_tick < 0 ||
      period.opening_at.half_day_tick == std::numeric_limits<Integer>::max() ||
      period.through_tick != period.opening_at.half_day_tick + 1 ||
      w.active_exact_write_context() != &context ||
      !w.common.actor_condition_capture_identity())
    return common_failure("EmptyCareActualCalendarBoundaryRequired");
  auto integral = compare_fractions(period.opening_at.sub_half_day, {});
  if (!integral.ok)
    return {false, {}, integral.error};
  if (integral.value != 0)
    return common_failure("EmptyCareActualCalendarBoundaryRequired");
  auto actual = w.actual_phase_moment();
  if (!actual.ok)
    return {false, {}, actual.error};
  auto same =
      compare_world_moments(actual.value, WorldMoment{period.through_tick, {}});
  if (!same.ok)
    return {false, {}, same.error};
  if (same.value != 0)
    return common_failure("EmptyCareActualCalendarBoundaryRequired");
  return empty_world(w);
}
} // namespace

Outcome<ExactPhaseRegistration>
make_native_empty_care_family_phase_registration(
    LifeAgeExactWorldFactory factory) {
  auto registration = make_life_age_exact_phase_registration(factory);
  registration.stages.push_back(ExactStage::birth_due);
  registration.stages.push_back(ExactStage::guardianship_monitor);
  registration.stages.push_back(ExactStage::condition_due);
  registration.stages.push_back(ExactStage::growth_recovery);
  registration.stages.push_back(ExactStage::arcane_expiry);
  registration.calendar_stages = registration.stages;
  // 唯一 age writer 键照旧；五个空阶段没有写入、周期源、due 或业务成果。
  registration.validate_author_selection = [factory](const WorldBundle &w) {
    auto empty = empty_world(w);
    if (!empty.ok) return empty;
    auto original = validate_original_life_age_author_selection(w, factory);
    return original.ok ? remaining_world(w) : original;
  };
  registration.validate_world_sources = [factory](const WorldBundle &w,
                                                  const WorldMoment &at) {
    auto empty = empty_world(w);
    if (!empty.ok) return empty;
    auto original = validate_original_life_age_world_sources(w, at, factory);
    return original.ok ? remaining_world(w) : original;
  };
  registration.apply_calendar_stage = [factory](
                                          WorldBundle &w, ExactStage stage,
                                          const ExactPeriodContext &period,
                                          ExactPhaseWriteContext &context) {
    auto empty = empty_world(w);
    if (!empty.ok)
      return empty;
    if (stage == ExactStage::life_age)
      return apply_original_life_age_calendar_in_candidate(w, period, context,
                                                           factory);
    if (stage == ExactStage::birth_due ||
        stage == ExactStage::guardianship_monitor)
      return empty_calendar_stage(w, period, context);
    if (stage == ExactStage::condition_due ||
        stage == ExactStage::growth_recovery || stage == ExactStage::arcane_expiry) {
      auto boundary = empty_calendar_stage(w, period, context);
      return boundary.ok ? remaining_world(w) : boundary;
    }
    return common_failure("EmptyCareFamilyCalendarStageMismatch");
  };
  return {true, std::move(registration), {}};
}
} // namespace hanmo::resident
