#include "growth_fixed_input_changes.hpp"
#include "growth_fixed_actor_input_source.hpp"
#include "growth_fixed_control_inputs.hpp"
#include "growth_fixed_spatial_inputs.hpp"
#include "growth_permanent_contributions.hpp"
#include "growth_practice_inputs.hpp"
#include "growth_trade_grant_read.hpp"
#include "growth_trade_grant_inputs.hpp"
#include "growth_quiet_source.hpp"
#include "growth_skill_qualification_codec_fields.hpp"
#include "growth_skill_qualification_v2.hpp"
#include "resident/domains/actor_foundation/authored_origin.hpp"
#include "resident/domains/actor_foundation/military_availability.hpp"
#include "resident/domains/family/family_life_exact.hpp"
#include "study_permanent_contribution_read.hpp"
#include <algorithm>
#include <array>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
template <class F> Status check(F fn) {
  try {
    fn();
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
bool same_source(const CharacterConditionSourceRef &a,
                 const CharacterConditionSourceRef &b) {
  if (a.index() != b.index())
    return false;
  if (const auto *seed = std::get_if<InitialConditionSeedRef>(&a))
    return seed->seed_key == std::get<InitialConditionSeedRef>(b).seed_key;
  return std::get<ConditionReceiptRefV2>(a).result_id ==
         std::get<ConditionReceiptRefV2>(b).result_id;
}
bool same_head(const ActorConditionHead &a, const ActorConditionHead &b) {
  return a.actor_id == b.actor_id &&
         a.condition_revision == b.condition_revision &&
         a.initial_condition_seed_ref.seed_key ==
             b.initial_condition_seed_ref.seed_key &&
         bool(a.latest_receipt_ref) == bool(b.latest_receipt_ref) &&
         (!a.latest_receipt_ref ||
          a.latest_receipt_ref->result_id == b.latest_receipt_ref->result_id);
}
const LifeFixedActorInputs *
actor_inputs(const LifeFixedServiceBoundary &boundary, const StableId &actor,
             const StableId &role) {
  const LifeFixedActorInputs *found = nullptr;
  for (const auto &input : boundary.input_snapshot.actors)
    if (input.actor_id == actor && input.role == role) {
      if (found)
        fail("DuplicateGrowthFixedActorRole", boundary.boundary_id);
      found = &input;
    }
  return found;
}
const ActorConditionHead &head(const LifeFixedActorInputs &input) {
  if (input.capability_frames.empty())
    fail("GrowthFixedBodyFrameRequired", input.actor_id);
  const auto &result = input.capability_frames.front().condition_head_snapshot;
  for (const auto &frame : input.capability_frames)
    if (frame.actor_id != input.actor_id ||
        !same_head(frame.condition_head_snapshot, result))
      fail("GrowthFixedBodyFramesDisagree", input.actor_id);
  return result;
}
const LifeFixedServiceBoundary &
previous(const GrowthState &state, const LifeFixedServiceBoundary &boundary) {
  if (!boundary.previous_boundary_ref)
    fail("GrowthFixedInputPreviousRequired", boundary.boundary_id);
  const auto *value =
      state.fixed_service_boundaries.find(*boundary.previous_boundary_ref);
  if (!value || value->action_ref != boundary.action_ref ||
      value->boundary_ordinal + 1 != boundary.boundary_ordinal)
    fail("GrowthFixedInputPreviousMismatch", boundary.boundary_id);
  return *value;
}
const ConditionReceiptV2 &receipt(const CharacterConditionSources &sources,
                                  const ConditionReceiptRefV2 &ref) {
  const auto *value =
      sources.state.character_condition_extensions.applied_receipts.find(
          ref.result_id);
  if (!value || value->result_id != ref.result_id ||
      value->effective_at.kind != ActorFoundationAt::Kind::exact_moment)
    fail("GrowthFixedConditionSourceRequired", ref.result_id);
  return *value;
}
const LifeFixedInputChange *change(const LifeFixedServiceBoundary &boundary) {
  if (const auto *actual = std::get_if<LifeActualInputChange>(&boundary.cause))
    return &actual->change;
  if (const auto *terminal =
          std::get_if<LifeOriginalTerminalApplication>(&boundary.cause))
    if (const auto *stop =
            std::get_if<LifeActualStopTrigger>(&terminal->trigger))
      return std::get_if<LifeFixedInputChange>(&stop->source);
  return nullptr;
}
const ActorFoundationRuleRef *rule(const LifeFixedServiceBoundary &boundary) {
  if (const auto *actual = std::get_if<LifeActualInputChange>(&boundary.cause))
    return &actual->transition_rule_ref;
  if (const auto *terminal =
          std::get_if<LifeOriginalTerminalApplication>(&boundary.cause))
    if (const auto *stop =
            std::get_if<LifeActualStopTrigger>(&terminal->trigger))
      return &stop->original_stop_rule_ref;
  return nullptr;
}
const ConditionReceiptV2 &
actual_condition(const CharacterConditionSources &sources,
                 const LifeFixedInputChange &source) {
  if (const auto *condition = std::get_if<LifeConditionInputChange>(&source))
    return receipt(sources, condition->condition_receipt_ref);
  if (const auto *death = std::get_if<LifeDeathInputChange>(&source)) {
    if (!sources.deaths || !sources.life_sources)
      fail("GrowthFixedDeathSourcesRequired", death->death_ref.id);
    auto actual = take(read_verified_death4(
        sources.common, *sources.deaths, *sources.life_sources,
        death->death_ref, sources.actual_now));
    const ConditionReceiptV2 *result = nullptr;
    for (const auto &row :
         sources.state.character_condition_extensions.applied_receipts.rows())
      if (row.second.life_transition_ref &&
          *row.second.life_transition_ref == death->death_ref.id) {
        if (result)
          fail("GrowthFixedDeathConditionAmbiguous", death->death_ref.id);
        result = &row.second;
      }
    if (!result || result->actor_id != actual.actor_id ||
        actual.effective_at.kind != ActorFoundationAt::Kind::exact_moment ||
        take(compare_world_moments(result->effective_at.exact_moment,
                                   actual.effective_at.exact_moment)))
      fail("GrowthFixedDeathConditionRequired", death->death_ref.id);
    return *result;
  }
  fail("GrowthFixedNonConditionInputReaderRequired");
}
void frame(const CharacterConditionSources &sources, const GrowthState &state,
           const LifeFixedServiceBoundary &boundary,
           const FrozenFacilityCapabilityInputs &frozen) {
  std::optional<StableId> skill;
  if (frozen.purpose_ref.id == "study" ||
      frozen.purpose_ref.id == "provide_teaching") {
    const auto *raw = state.facts.find(boundary.action_ref);
    const auto *study = raw ? std::get_if<StudyFact>(raw) : nullptr;
    if (!study ||
        (frozen.actor_id != study->terms.student_id &&
         frozen.actor_id != study->terms.teacher_id) ||
        frozen.purpose_ref.id != (frozen.actor_id == study->terms.student_id
                                      ? "study"
                                      : "provide_teaching"))
      fail("GrowthFixedStudyPurposeMismatch", boundary.boundary_id);
    skill = study->terms.skill_id;
  }
  auto expected = take(read_character_capability_at_head(
      sources, frozen.condition_head_snapshot, frozen.purpose_ref, skill,
      boundary.occurred_at));
  if (frozen.actor_id != expected.actor_id ||
      take(compare_world_moments(frozen.captured_at, boundary.occurred_at)) ||
      !frozen.active_condition_inputs.empty() ||
      frozen.body_scalars.alive != expected.body.alive ||
      frozen.body_scalars.hp != expected.body.hp ||
      frozen.body_scalars.fatigue != expected.body.fatigue ||
      frozen.body_scalars.injury != expected.body.injury ||
      frozen.selected_rule_refs.size() != 1 ||
      !actor_foundation_rule_equal(frozen.selected_rule_refs.front(),
                                   expected.rule_ref) ||
      frozen.result.allowed != expected.allowed ||
      frozen.result.efficiency_bp != expected.efficiency_bp ||
      !actor_foundation_rule_equal(frozen.result.rule_ref, expected.rule_ref) ||
      take(compare_world_moments(frozen.result.observed_at,
                                 boundary.occurred_at)) ||
      frozen.result.limiting_condition_refs.size() !=
          expected.limiting_condition_refs.size())
    fail("GrowthFixedActualCapabilityMismatch", boundary.boundary_id);
  for (std::size_t n = 0; n < expected.limiting_condition_refs.size(); ++n)
    if (!same_source(frozen.result.limiting_condition_refs[n],
                     expected.limiting_condition_refs[n]))
      fail("GrowthFixedCapabilityLimitSourceMismatch", boundary.boundary_id);
}
const GrowthSkillQualificationV2 *
qualification(const LifeFixedActorInputs &actor, const StableId &skill) {
  const GrowthSkillQualificationV2 *result = nullptr;
  for (const auto &value : actor.skill_qualification_views)
    if (value.skill_id == skill) {
      if (result)
        fail("GrowthFixedDuplicateSkillInput", actor.actor_id);
      result = &value;
    }
  return result;
}
void growth_changed(const CharacterConditionSources &sources,
                    const GrowthState &state,
                    const LifeFixedServiceBoundary &boundary,
                    const LifeGrowthInputChange &input) {
  struct SkillChange {
    const StableId *skill = nullptr;
    Integer before = 0, after = 0;
  };
  const StableId *actor = nullptr;
  WorldMoment at;
  std::array<SkillChange, 2> changes{};
  std::size_t count = 0;
  if (const auto *ref =
          std::get_if<StudyLessonResultRef>(&input.original_result_ref)) {
    auto source =
        take(read_study_lesson_permanent_contribution(sources, state, *ref));
    actor = &source.fact->terms.student_id;
    at = source.effective_at;
    changes[count++] = {&source.fact->terms.skill_id, source.xp_before,
                        source.xp_after};
  } else if (const auto *ref = std::get_if<StudyPerformanceAdjustmentRef>(
                 &input.original_result_ref)) {
    if (!sources.growth_catalog)
      fail("GrowthFixedSkillCatalogRequired");
    auto source = take(read_study_adjustment_permanent_contribution(
        sources.common, state, *sources.growth_catalog, *ref,
        sources.actual_now));
    actor = &source.source.fact->terms.student_id;
    at = source.source.exact->settled_at;
    changes[count++] = {&source.source.fact->terms.skill_id, source.xp_before,
                        source.xp_after};
  } else if (const auto *ref =
                 std::get_if<PracticeAwardRef>(&input.original_result_ref)) {
    if (!sources.permanent_growth || !sources.permanent_growth_catalog ||
        !sources.work_practice)
      fail("GrowthPracticeInputSourcesRequired", ref->award_id);
    GrowthPermanentSources permanent{sources, *sources.permanent_growth_catalog,
                                     *sources.permanent_growth,
                                     *sources.work_practice};
    const auto *source = take(read_verified_practice_award(permanent, *ref));
    actor = &source->actor_id;
    at = source->applied_at.exact_moment;
    for (const auto &row : source->skill_rows)
      if (row.actual_xp_gain)
        changes[count++] = {&row.skill_id, row.xp_before, row.xp_after};
  } else if (const auto *ref = std::get_if<GrowthTradeGrantRef>(&input.original_result_ref)) {
    if (!sources.growth_catalog || !sources.shared)
      fail("GrowthTradeGrantSourcesRequired", ref->action_id);
    auto source = take(read_growth_trade_grant_v1(sources.common, state, *sources.growth_catalog, *sources.shared, *ref, sources.actual_now));
    actor = &source.grant->actor_id;
    at = source.terminal->ended_at;
    changes[count++] = {&source.grant->skill_id, source.grant->xp_before, source.grant->xp_after};
  } else
    fail("GrowthFixedContributionReaderRequired", boundary.boundary_id);
  bool positive = false;
  for (std::size_t i = 0; i < count; ++i)
    positive = positive || changes[i].after > changes[i].before;
  if (take(compare_world_moments(at, boundary.occurred_at)) || !positive)
    fail("GrowthFixedActualSkillChangeRequired", boundary.boundary_id);
  const auto &prior = previous(state, boundary);
  bool affected = false, rule_used = false;
  const auto *selected = rule(boundary);
  for (const auto &person : boundary.input_snapshot.actors) {
    const auto *old = actor_inputs(prior, person.actor_id, person.role);
    if (!old || !same_head(head(*old), head(person)))
      fail("GrowthFixedSkillChangeHasUnexplainedBody", boundary.boundary_id);
    if (old->skill_qualification_views.size() !=
        person.skill_qualification_views.size())
      fail("GrowthFixedSkillInputSetChanged", boundary.boundary_id);
    for (const auto &value : person.skill_qualification_views) {
      const auto *previous = qualification(*old, value.skill_id);
      if (!previous)
        fail("GrowthFixedSkillInputSetChanged", boundary.boundary_id);
      const SkillChange *changed = nullptr;
      if (person.actor_id == *actor)
        for (std::size_t i = 0; i < count; ++i)
          if (value.skill_id == *changes[i].skill)
            changed = &changes[i];
      if (changed) {
        affected = true;
        if (previous->minimum_xp != changed->before ||
            value.minimum_xp != changed->after)
          fail("GrowthFixedSkillBeforeAfterMismatch", boundary.boundary_id);
        for (const auto &frame : person.capability_frames)
          if (selected &&
              actor_foundation_rule_equal(*selected, frame.result.rule_ref))
            rule_used = true;
      } else if (previous->minimum_xp != value.minimum_xp ||
                 previous->contribution_refs.size() !=
                     value.contribution_refs.size())
        fail("GrowthFixedUnexplainedSkillChange", boundary.boundary_id);
    }
  }
  if (!affected || !rule_used)
    fail("GrowthFixedActualSkillInputNotRead", boundary.boundary_id);
}
void changed(const CharacterConditionSources &sources, const GrowthState &state,
             const LifeFixedServiceBoundary &boundary,
             const LifeFixedInputChange &ref) {
  if (const auto *growth = std::get_if<LifeGrowthInputChange>(&ref)) {
    growth_changed(sources, state, boundary, *growth);
    return;
  }
  if (const auto *position = std::get_if<LifeActualPositionInputChange>(&ref)) {
    if (!sources.fixed_spatial)
      fail("GrowthFixedSpatialHistoryReaderRequired", boundary.boundary_id);
    const auto &prior = previous(state, boundary);
    for (const auto &person : boundary.input_snapshot.actors) {
      const auto *old = actor_inputs(prior, person.actor_id, person.role);
      if (!old || !same_head(head(*old), head(person)) ||
          old->skill_qualification_views.size() !=
              person.skill_qualification_views.size())
        fail("GrowthFixedPositionUnexplainedBody", boundary.boundary_id);
      for (const auto &skill : person.skill_qualification_views) {
        const auto *old_skill = qualification(*old, skill.skill_id);
        if (!old_skill || old_skill->minimum_xp != skill.minimum_xp ||
            old_skill->contribution_refs.size() !=
                skill.contribution_refs.size())
          fail("GrowthFixedPositionUnexplainedSkill", boundary.boundary_id);
      }
    }
    take(validate_growth_fixed_position_input_change(
        sources, *sources.fixed_spatial, state, boundary, *position));
    return;
  }
  if (const auto *capacity = std::get_if<LifeOccupationInputChange>(&ref)) {
    if (growth_quiet_input_source_shape(capacity->original_source_ref,
                                        capacity->source_schema)) {
      const auto &prior = previous(state, boundary);
      for (const auto &person : boundary.input_snapshot.actors) {
        const auto *old = actor_inputs(prior, person.actor_id, person.role);
        if (!old || !same_head(head(*old), head(person)) ||
            old->skill_qualification_views.size() !=
                person.skill_qualification_views.size() ||
            !growth_fixed_location_heads_equal(*old, person))
          fail("GrowthQuietUnexplainedActorInput", boundary.boundary_id);
        for (const auto &skill : person.skill_qualification_views) {
          const auto *prior_skill = qualification(*old, skill.skill_id);
          if (!prior_skill || prior_skill->minimum_xp != skill.minimum_xp ||
              prior_skill->contribution_refs.size() !=
                  skill.contribution_refs.size())
            fail("GrowthQuietUnexplainedSkill", boundary.boundary_id);
        }
      }
      take(validate_growth_long_rest_quiet_input_change(sources, state,
                                                        boundary, *capacity));
      return;
    }

    if (capacity->source_schema != 2 ||
        capacity->original_source_ref.domain != "actor_foundation" ||
        capacity->original_source_ref.kind != "capacity_transition")
      fail("GrowthFixedOccupationOriginalReaderRequired", boundary.boundary_id);
    const auto *actual = take(read_verified_military_availability_transition(
        sources, capacity->original_source_ref.id));
    const auto *selected = rule(boundary);
    if (!selected || !actor_foundation_rule_equal(*selected, actual->purpose) ||
        take(compare_world_moments(actual->effective_at.exact_moment,
                                   boundary.occurred_at)))
      fail("GrowthFixedAvailabilitySourceMismatch", boundary.boundary_id);
    const auto &prior = previous(state, boundary);
    bool affected = false;
    for (const auto &person : boundary.input_snapshot.actors) {
      const auto *old = actor_inputs(prior, person.actor_id, person.role);
      if (!old || !same_head(head(*old), head(person)) ||
          old->skill_qualification_views.size() !=
              person.skill_qualification_views.size())
        fail("GrowthFixedAvailabilityUnexplainedBody", boundary.boundary_id);
      if (person.actor_id == actual->actor_id)
        affected = true;
      for (const auto &skill : person.skill_qualification_views) {
        const auto *old_skill = qualification(*old, skill.skill_id);
        if (!old_skill || old_skill->minimum_xp != skill.minimum_xp ||
            old_skill->contribution_refs.size() !=
                skill.contribution_refs.size())
          fail("GrowthFixedAvailabilityUnexplainedSkill", boundary.boundary_id);
      }
    }
    bool paused = false;
    Integer ordinal = -1;
    for (const auto &row : state.fixed_service_boundaries.rows())
      if (row.second.action_ref == boundary.action_ref &&
          row.second.boundary_ordinal < boundary.boundary_ordinal &&
          row.second.boundary_ordinal > ordinal &&
          (row.second.kind == LifeFixedBoundaryKind::suspended ||
           row.second.kind == LifeFixedBoundaryKind::resumed)) {
        ordinal = row.second.boundary_ordinal;
        paused = row.second.kind == LifeFixedBoundaryKind::suspended;
      }
    if (!affected || actual->to ||
        (boundary.kind == LifeFixedBoundaryKind::input_rebound && !paused) ||
        (boundary.kind != LifeFixedBoundaryKind::input_rebound &&
         boundary.kind != LifeFixedBoundaryKind::suspended &&
         boundary.kind != LifeFixedBoundaryKind::stopped))
      fail("GrowthFixedAvailabilityDispositionMismatch", boundary.boundary_id);
    return;
  }
  const auto &actual = actual_condition(sources, ref);
  const auto &prior = previous(state, boundary);
  if (take(compare_world_moments(actual.effective_at.exact_moment,
                                 boundary.occurred_at)))
    fail("GrowthFixedInputOccurrenceMismatch", boundary.boundary_id);
  const auto *selected_rule = rule(boundary);
  bool affected = false, rule_used = false, allowed = true;
  for (const auto &input : boundary.input_snapshot.actors) {
    const auto *old = actor_inputs(prior, input.actor_id, input.role);
    if (!old)
      fail("GrowthFixedInputRoleChanged", boundary.boundary_id);
    const auto &before = head(*old);
    const auto &after = head(input);
    if (input.actor_id == actual.actor_id) {
      affected = true;
      if (before.condition_revision != actual.condition_revision_before ||
          after.condition_revision != actual.condition_revision_after ||
          !after.latest_receipt_ref ||
          after.latest_receipt_ref->result_id != actual.result_id ||
          before.initial_condition_seed_ref.seed_key !=
              after.initial_condition_seed_ref.seed_key)
        fail("GrowthFixedConditionChangeChainMismatch", boundary.boundary_id);
      for (const auto &value : input.capability_frames) {
        allowed = allowed && value.result.allowed;
        if (selected_rule &&
            actor_foundation_rule_equal(*selected_rule, value.result.rule_ref))
          rule_used = true;
      }
    } else if (!same_head(before, after))
      fail("GrowthFixedUnexplainedConditionChange", boundary.boundary_id);
  }
  if (!affected || !rule_used)
    fail("GrowthFixedInputRuleNotApplied", boundary.boundary_id);
  bool paused_before = false;
  Integer last_phase_ordinal = -1;
  for (const auto &row : state.fixed_service_boundaries.rows())
    if (row.second.action_ref == boundary.action_ref &&
        row.second.boundary_ordinal < boundary.boundary_ordinal &&
        row.second.boundary_ordinal > last_phase_ordinal &&
        (row.second.kind == LifeFixedBoundaryKind::suspended ||
         row.second.kind == LifeFixedBoundaryKind::resumed)) {
      last_phase_ordinal = row.second.boundary_ordinal;
      paused_before = row.second.kind == LifeFixedBoundaryKind::suspended;
    }
  if (boundary.kind == LifeFixedBoundaryKind::input_rebound && !paused_before &&
      !allowed)
    fail("GrowthFixedUnavailableInputCannotContinue", boundary.boundary_id);
  if (boundary.kind == LifeFixedBoundaryKind::resumed && !allowed)
    fail("GrowthFixedUnavailableInputCannotResume", boundary.boundary_id);
  if (std::holds_alternative<LifeDeathInputChange>(ref) &&
      boundary.kind != LifeFixedBoundaryKind::stopped)
    fail("GrowthFixedDeathMustStopOriginalService", boundary.boundary_id);
}
} // namespace
Status validate_growth_fixed_input_change_reference(
    const LifeFixedInputChange &source) {
  return check([&] {
    if (const auto *value =
            std::get_if<LifeActualPositionInputChange>(&source)) {
      if (value->original_source_ref.id.empty() ||
          take(actor_spatial_history_source_schema(
              value->original_source_ref)) != value->source_schema)
        fail("GrowthFixedActualPositionSourceRequired");
      return;
    }
    if (const auto *value = std::get_if<LifeOccupationInputChange>(&source)) {
      if (growth_quiet_input_source_shape(value->original_source_ref,
                                          value->source_schema))
        return;
      if (value->source_schema != 2 ||
          value->original_source_ref.domain != "actor_foundation" ||
          value->original_source_ref.kind != "capacity_transition" ||
          value->original_source_ref.id.empty())
        fail("GrowthFixedOccupationOriginalReaderRequired");
      return;
    }
    if (const auto *value = std::get_if<LifeConditionInputChange>(&source)) {
      if (value->condition_receipt_ref.result_id.empty())
        fail("GrowthFixedConditionSourceRequired");
      return;
    }
    if (const auto *value = std::get_if<LifeDeathInputChange>(&source)) {
      if (value->death_ref.domain != "family_life" ||
          value->death_ref.kind != "family_death" ||
          value->death_ref.id.empty())
        fail("GrowthFixedDeathSourceRequired");
      return;
    }
    if (const auto *value = std::get_if<LifeGrowthInputChange>(&source)) {
      if (const auto *ref =
              std::get_if<StudyLessonResultRef>(&value->original_result_ref)) {
        if (ref->action_id.empty() || ref->lesson_ordinal < 1)
          fail("GrowthFixedActualStudyContributionRequired");
        return;
      }
      if (const auto *ref = std::get_if<StudyPerformanceAdjustmentRef>(
              &value->original_result_ref)) {
        if (ref->action_id.empty() || ref->lesson_ordinal < 1)
          fail("GrowthFixedActualStudyContributionRequired");
        return;
      }
      if (const auto *ref =
              std::get_if<PracticeAwardRef>(&value->original_result_ref)) {
        if (ref->award_id.empty())
          fail("GrowthFixedActualPracticeContributionRequired");
        return;
      }
      if (const auto *ref = std::get_if<GrowthTradeGrantRef>(&value->original_result_ref)) {
        if (ref->action_id.empty()) fail("GrowthFixedActualTradeGrantRequired");
        return;
      }
      fail("GrowthFixedContributionReaderRequired");
    }
    fail("GrowthFixedNonConditionInputReaderRequired");
  });
}
Status validate_growth_fixed_input_change_source(
    const CharacterConditionSources &sources, const GrowthState &state,
    const StableId &id) {
  return check([&] {
    if (sources.growth != &state)
      fail("GrowthFixedInputSameCandidateRequired", id);
    const auto *boundary = state.fixed_service_boundaries.find(id);
    if (!boundary || boundary->boundary_id != id)
      fail("GrowthFixedActualInputBoundaryRequired", id);
    const auto *actual = change(*boundary);
    if (!actual)
      fail("GrowthFixedActualInputChangeRequired", id);
    take(validate_growth_fixed_input_change_reference(*actual));
    for (const auto &person : boundary->input_snapshot.actors) {
      head(person);
      for (const auto &value : person.capability_frames)
        frame(sources, state, *boundary, value);
    }
    changed(sources, state, *boundary, *actual);
  });
}
// 原安装门中的实际冻结身体/技艺核验抽为同边界叶，不复制规则或反入全世界。
Status validate_growth_fixed_boundary_actor_inputs(
    const CharacterConditionSources &sources, const GrowthState &state,
    const LifeFixedServiceBoundary &boundary) {
  return check([&] {
    if (sources.growth != &state ||
        state.fixed_service_boundaries.find(boundary.boundary_id) != &boundary)
      fail("GrowthFixedActualOwnedBoundaryRequired", boundary.boundary_id);
    for (const auto &input : boundary.input_snapshot.actors) {
      head(input);
      for (const auto &value : input.capability_frames)
        frame(sources, state, boundary, value);
      const auto *required = take(
          read_growth_fixed_required_skill(sources, state, boundary, input));
      if (required) {
        if (input.skill_qualification_views.size() != 1 ||
            input.skill_qualification_views.front().actor_id !=
                input.actor_id ||
            input.skill_qualification_views.front().skill_id != *required)
          fail("GrowthFixedActualSkillInputsRequired", boundary.boundary_id);
      } else if (!input.skill_qualification_views.empty())
        fail("GrowthFixedUnreadSkillInput", boundary.boundary_id);
      for (const auto &skill : input.skill_qualification_views)
        take(validate_growth_skill_qualification_at_fixed_boundary_v2(
            sources, state, boundary.boundary_id, skill));
    }
  });
}
Status validate_growth_fixed_condition_input_closure(
    const CharacterConditionSources &sources, const GrowthState &state) {
  return check([&] {
    if (sources.growth != &state)
      fail("GrowthFixedInputSameCandidateRequired");
    take(validate_complete_character_conditions(sources));
    for (const auto &row : state.fixed_service_boundaries.rows()) {
      const auto &boundary = row.second;
      if (row.first != boundary.boundary_id)
        fail("GrowthFixedInputBoundaryKeyMismatch", row.first);
      take(validate_growth_fixed_boundary_actor_inputs(sources, state,
                                                       boundary));
      if (const auto *ref = change(boundary)) {
        take(validate_growth_fixed_input_change_reference(*ref));
        changed(sources, state, boundary, *ref);
      }
    }
    // 每个实际 actor/role
    // 的全部身体前驱都必须反映；同Moment按冻结revision截面，不用ID字典序。
    for (const auto &row : state.fixed_service_boundaries.rows()) {
      const auto &start = row.second;
      if (start.kind != LifeFixedBoundaryKind::started)
        continue;
      const LifeFixedServiceBoundary *last = &start;
      for (const auto &candidate : state.fixed_service_boundaries.rows())
        if (candidate.second.action_ref == start.action_ref &&
            candidate.second.boundary_ordinal > last->boundary_ordinal)
          last = &candidate.second;
      const bool terminal = last->kind == LifeFixedBoundaryKind::completed ||
                            last->kind == LifeFixedBoundaryKind::stopped;
      for (const auto &initial : start.input_snapshot.actors) {
        const auto &first = head(initial);
        const auto *end = actor_inputs(*last, initial.actor_id, initial.role);
        if (!end)
          fail("GrowthFixedFinalRoleMissing", start.action_ref);
        const auto *current = sources.state.character_condition_extensions
                                  .actor_condition_heads.find(initial.actor_id);
        if (!current)
          fail("GrowthFixedActorConditionHeadMissing", initial.actor_id);
        const auto ceiling = terminal ? head(*end).condition_revision
                                      : current->condition_revision;
        for (const auto &row : sources.state.character_condition_extensions
                                   .capacity_transitions.rows()) {
          const auto &capacity = row.second;
          if (!is_original_availability_transition(capacity) ||
              capacity.actor_id != initial.actor_id)
            continue;
          const auto original_capacity = take(
              read_original_availability_cause_position(sources, capacity));
          const auto after_start = take(compare_world_moments(
              capacity.effective_at.exact_moment, start.occurred_at));
          const auto before_end = take(compare_world_moments(
              capacity.effective_at.exact_moment,
              terminal ? last->occurred_at : sources.actual_now));
          if (after_start < 0 || before_end > 0)
            continue;
          // Terminal publication is appended by the original owner immediately
          // after its real result and S0 terminal. Use that actual Common
          // insertion cut, not a guessed same-M ordering or an unconditional
          // exclusion of end M.
          if (terminal && before_end == 0) {
            const GrowthTerminalPublication *publication = nullptr;
            for (const auto &row : state.terminal_publications.rows())
              if (row.second.action_id == start.action_ref) {
                if (publication)
                  fail("GrowthFixedTerminalPublicationAmbiguous",
                       start.action_ref);
                publication = &row.second;
              }
            if (!publication)
              fail("GrowthFixedTerminalPublicationRequired", start.action_ref);
            const auto &order = sources.common.state().facts.order();
            const auto ended = std::find(order.begin(), order.end(),
                                         publication->publication_id),
                       cause = std::find(order.begin(), order.end(),
                                         original_capacity.source->id);
            if (ended == order.end() || cause == order.end())
              fail("GrowthFixedAvailabilityOriginalCutRequired", row.first);
            if (cause > ended)
              continue;
          }
          if (after_start == 0) {
            const auto &order = sources.common.state().facts.order();
            const auto began = std::find(order.begin(), order.end(),
                                         start.action_ref),
                       cause = std::find(order.begin(), order.end(),
                                         original_capacity.source->id);
            if (began == order.end() || cause == order.end())
              fail("GrowthFixedAvailabilityOriginalCutRequired", row.first);
            if (cause < began)
              continue;
          }
          std::size_t count = 0;
          for (const auto &candidate : state.fixed_service_boundaries.rows())
            if (candidate.second.action_ref == start.action_ref)
              if (const auto *c = change(candidate.second))
                if (const auto *ref = std::get_if<LifeOccupationInputChange>(c);
                    ref && ref->source_schema == 2 &&
                    same_source_ref(
                        ref->original_source_ref,
                        military_availability_transition_source(row.first)))
                  ++count;
          if (count != 1)
            fail("GrowthFixedAvailabilityInputCoverageMismatch", row.first);
        }
        if (!terminal) {
          const auto *person =
              sources.common.state().actors.find(initial.actor_id);
          if (!person)
            fail("GrowthFixedCurrentActorMissing", initial.actor_id);
          for (const auto &skill : end->skill_qualification_views)
            if (skill.minimum_xp !=
                growth_skill_xp(person->skills, skill.skill_id))
              fail("GrowthFixedPermanentInputCoverageMismatch",
                   last->boundary_id);
        }
        for (const auto &source : sources.state.character_condition_extensions
                                      .applied_receipts.rows()) {
          const auto &actual = source.second;
          if (actual.actor_id != initial.actor_id ||
              actual.condition_revision_after <= first.condition_revision ||
              actual.condition_revision_after > ceiling)
            continue;
          std::size_t count = 0;
          for (const auto &candidate : state.fixed_service_boundaries.rows())
            if (candidate.second.action_ref == start.action_ref)
              if (const auto *ref = change(candidate.second)) {
                if (const auto *condition =
                        std::get_if<LifeConditionInputChange>(ref)) {
                  if (condition->condition_receipt_ref.result_id ==
                      actual.result_id)
                    ++count;
                } else if (const auto *death =
                               std::get_if<LifeDeathInputChange>(ref);
                           death && actual.life_transition_ref &&
                           *actual.life_transition_ref == death->death_ref.id)
                  ++count;
              }
          if (count != 1)
            fail("GrowthFixedConditionInputCoverageMismatch", source.first);
        }
      }
    }
  });
}
} // namespace hanmo::resident
namespace hanmo::resident {
Status validate_complete_growth_fixed_sources(
    const CharacterConditionSources &body, const GrowthState &state,
    const GrowthMarketExactSources *market_exact) {
  if (body.growth != &state || !body.growth_catalog || !body.shared ||
      !body.elapsed || !body.scale || !body.action_registrations)
    return common_failure("CompleteGrowthFixedSourceLeaseRequired");
  GrowthActionTimeRead timing(
      body.common, state, *body.growth_catalog, *body.shared, *body.elapsed,
      *body.scale, body.catalog.identity, body.actual_now, {}, body.interaction,
      body.interaction_catalog, market_exact);
  struct Sink final : ActionTimeOwnerIdSink {
    const GrowthActionTimeRead &timing;
    const ActionElapsedStore &elapsed;
    const ActionTimeRegistration &registration;
    Sink(const GrowthActionTimeRead &t, const ActionElapsedStore &e,
         const ActionTimeRegistration &r)
        : timing(t), elapsed(e), registration(r) {}
    Status append(std::string_view id) override {
      auto proof = timing.read_action_time_owner_borrowed(registration, id);
      if (!proof.ok)
        return {false, {}, std::move(proof.error)};
      const auto *actual = find_fixed_elapsed(elapsed, proof.value.action_ref);
      if (!actual)
        return common_failure("GrowthFixedElapsedCoverageMissing",
                              proof.value.action_ref);
      return timing.validate_retained_fixed_action_sources(
          registration, proof.value, *actual);
    }
  };
  for (const auto &registration : *body.action_registrations) {
    if (registration.owner_domain != "growth")
      continue;
    Sink sink(timing, *body.elapsed, registration);
    auto owners = timing.enumerate_action_time_owners(registration, sink);
    if (!owners.ok)
      return owners;
  }
  auto control = validate_growth_fixed_control_inputs(body, state);
  if (!control.ok)
    return control;
  auto condition = validate_growth_fixed_condition_input_closure(body, state);
  if (!condition.ok)
    return condition;
  auto practice = validate_growth_practice_input_coverage(body, state);
  if (!practice.ok)
    return practice;
  auto trade = validate_growth_trade_grant_input_coverage(body, state);
  if (!trade.ok)
    return trade;
  auto quiet = validate_growth_long_rest_quiet_input_coverage(body, state);
  if (!quiet.ok)
    return quiet;
  if (!state.fixed_service_boundaries.empty()) {
    if (!body.fixed_spatial)
      return common_failure("GrowthFixedSpatialHistoryReaderRequired");
    return validate_growth_fixed_spatial_inputs(body, *body.fixed_spatial,
                                                state);
  }
  return common_success();
}
} // namespace hanmo::resident
