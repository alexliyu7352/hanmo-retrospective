#include "native_work_practice_source.hpp"
#include "growth_practice_inputs.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include "resident/domains/tasks/work_execution/work_growth_contribution.hpp"
#include <type_traits>
namespace hanmo::resident {
Outcome<WorkPracticeContributionView>
NativeWorkPracticeContributionRead::read_actual_work_contribution(
    const StableId &actor, const WorkPracticeContribution &source,
    const WorldMoment &now) const {
  auto actual = read_work_growth_contribution_source(
      common_, tasks_, actor, source.result, source.participation_refs, now);
  if (!actual.ok)
    return {false, {}, actual.error};
  const auto &read = actual.value;
  if (!read.result || !read.episode || !read.original_result ||
      !read.last_actual_period)
    return {
        false,
        {},
        common_failure("PracticeActualWorkContributionRequired", actor).error};
  return {true,
          {read.result->finished_at,
           read.result->kind,
           read.actor_credited_half_days,
           {read.original_result->domain, read.original_result->kind,
            read.original_result->id},
           source.participation_refs},
          {}};
}
} // namespace hanmo::resident

namespace hanmo::resident {
namespace {
class SelectedWorkGrowth final : public WorkGrowthContributorVisitor {
  CommonStore &common;
  GrowthState &growth;
  GrowthPermanentState &state;
  const GrowthPermanentSources &sources;
  ActionElapsedStore &elapsed;
  const WorkResultSource &result;
  GrowthCapabilityRevisionWrite &capability;

public:
  SelectedWorkGrowth(CommonStore &c, GrowthState &g, GrowthPermanentState &s,
                     const GrowthPermanentSources &p, ActionElapsedStore &e,
                     const WorkResultSource &r,
                     GrowthCapabilityRevisionWrite &w)
      : common(c), growth(g), state(s), sources(p), elapsed(e), result(r),
        capability(w) {
  }
  Status visit(const StableId &actor,
               std::span<const StableId> participation) override {

    if (participation.size() > SIZE_MAX / sizeof(StableId))
      throw std::bad_alloc();


    std::visit(
        [&](const auto &r) {
          using T = std::decay_t<decltype(r)>;
          if constexpr (std::is_same_v<T, LegacyAppeaseWork>)
            (void)0;
          else
            (void)0;
        },
        result);
    auto memory = CheckpointMemoryCharge(0);
    WorkPracticeContribution actual{
        result, {participation.begin(), participation.end()}};
    auto original = sources.work.read_actual_work_contribution(
        actor, actual, sources.condition.actual_now);
    if (!original.ok)
      return {false, {}, original.error};
    for (const auto &row : sources.catalog.practice_rules.rows()) {
      const auto &rule = row.second;
      if (rule.criterion.work_kind != original.value.work_kind ||
          rule.criterion.minimum_credited_half_days >
              original.value.credited_half_days)
        continue;
      auto applied = apply_work_practice_in_candidate(
          common, state, sources, actor, rule.rule_ref, actual, capability);
      if (!applied.ok)
        return {false, {}, applied.error};
      auto adopted = adopt_growth_practice_inputs_in_candidate(
          common, growth, sources.condition, elapsed, applied.value.result);
      if (!adopted.ok)
        return adopted;
    }
    for (const auto &row : sources.catalog.milestone_rules.rows()) {
      const auto &rule = row.second;
      if (rule.criterion.work_kind != original.value.work_kind ||
          rule.criterion.minimum_credited_half_days >
              original.value.credited_half_days)
        continue;
      Integer count = 0;
      bool already = false;
      for (const auto &earned : state.milestones.rows())
        if (earned.second.actor_id == actor &&
            earned.second.attribute_id == rule.attribute_id) {
          ++count;
          if (earned.second.unique_milestone_id == rule.unique_milestone_id)
            already = true;
        }
      // The original per-attribute cap cannot turn a completed Work result into
      // a failed work action. No sixth milestone or alternate renamed award is
      // made.
      if (already || count >= 5)
        continue;
      auto applied = apply_work_attribute_milestone_in_candidate(
          common, state, sources, actor, rule.rule_ref, actual, capability);
      if (!applied.ok)
        return {false, {}, applied.error};
    }
    return common_success();
  }
};
} // namespace
Status apply_selected_work_growth_in_candidate(
    CommonStore &common, GrowthState &growth, GrowthPermanentState &state,
    const GrowthPermanentSources &sources, const TaskState &tasks,
    ActionElapsedStore &elapsed, const WorkResultSource &result,
    GrowthCapabilityRevisionWrite &capability) {
  if (!sources.condition.shared || &sources.condition.common != &common ||
      sources.condition.growth != &growth ||
      sources.condition.elapsed != &elapsed || &sources.state != &state ||
      !capability.bound_to(common, state))
    return common_failure("WorkGrowthActualCandidateRequired");
  auto rules = validate_growth_permanent_catalog(sources.catalog,
                                                 *sources.condition.shared);
  if (!rules.ok)
    return rules;
  SelectedWorkGrowth consumer(common, growth, state, sources, elapsed, result,
                              capability);
  return visit_work_growth_contributors(common, tasks, result,
                                        sources.condition.actual_now, consumer);
}
} // namespace hanmo::resident

namespace hanmo::resident {
bool NativeWorkCompletedGrowthConsumer::bound_to(
    const CommonStore &c, const TaskState &t) const noexcept {
  return &c == &common_ && &t == &tasks_ &&
         capability_.bound_to(common_, permanent_);
}
Status NativeWorkCompletedGrowthConsumer::consume_completed_result(
    const WorkResultSource &ref, const WorldMoment &at) {
  auto clock = compare_world_moments(at, sources_.condition.actual_now);
  if (!clock.ok)
    return {false, {}, clock.error};
  if (clock.value)
    return common_failure("WorkGrowthActualCompletionClockRequired");
  return apply_selected_work_growth_in_candidate(common_, growth_, permanent_,
                                                 sources_, tasks_, elapsed_,
                                                 ref, capability_);
}
} // namespace hanmo::resident
