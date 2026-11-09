#include "work_condition.hpp"
#include "authored_origin.hpp"
#include "condition_application_fact.hpp"
#include "condition_capacity_contract.hpp"
#include "resident/domains/growth/growth_internal.hpp"
#include <array>
#include <limits>
namespace hanmo::resident {
struct GrowthWorkConditionBoundary::State {
  const CommonStore *common = nullptr;
  const AuthoredActorFoundationState *foundation = nullptr;
  const GrowthState *growth = nullptr;
  std::shared_ptr<const ActorConditionCaptureIdentity> capture;
  ActorConditionSavepointBoundary savepoint;
  WorldIdentity identity;
  StableId action_id;
  WorkOrStudyConditionCause::Kind kind =
      WorkOrStudyConditionCause::Kind::study_lesson;
  Integer ordinal = 0;
  WorldMoment at;
  std::size_t draft_offset = 0, count = 0;
  bool consumed = false;
  struct Actor {
    StableId id;
    ActorConditionHead head;
    Integer fatigue = 0, increment = 0;
  };
  std::array<Actor, 2> actors;
};
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> x) {
  if (!x.ok)
    throw Failure{std::move(x.error)};
  return std::move(x.value);
}
template <class T, class F> Outcome<T> guarded(F fn) {
  try {
    return {true, fn(), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
bool identity(const WorldIdentity &a, const WorldIdentity &b) {
  return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch &&
         a.revision == b.revision && a.tick == b.tick;
}
void rule(const CommonStore &common, const ActorFoundationCatalog &catalog) {
  if (!catalog.work_or_study_rule ||
      catalog.work_or_study_rule->id != "original_work_or_study_exertion" ||
      catalog.work_or_study_rule->version != 1 ||
      catalog.work_or_study_rule->catalog_digest !=
          common.state().metadata.content_digest)
    fail("GrowthWorkConditionRuleRequired");
}
Integer fatigue_after(Integer before, Integer increment) {
  if (before < 0 || before > 100 || increment < 0 || increment > 72000)
    fail("GrowthWorkFatigueRangeInvalid");
  return increment >= 100 - before ? 100 : before + increment;
}
struct Source {
  WorldMoment at;
  Integer increment = 0;
  std::optional<Integer> original_before;
};
Source source(const CommonStore &common, const GrowthState &growth,
              const GrowthCatalog &catalog, const CommonCatalog &shared,
              const ActionElapsedStore &elapsed, const TimeScaleRef &scale,
              const CatalogIdentity &catalog_identity,
              const ActionTimeRegistration &registration,
              const WorldMoment &now, const WorkOrStudyConditionCause &cause,
              const InteractionState *interaction,
              const InteractionCatalog *interaction_catalog) {
  const auto *raw = growth.facts.find(cause.action_id);
  if (!raw || cause.actor_id.empty() || cause.ordinal < 1)
    fail("GrowthWorkConditionOriginalSourceRequired", cause.action_id);
  GrowthActionTimeRead reader(common, growth, catalog, shared, elapsed, scale,
                              catalog_identity, now, {}, interaction,
                              interaction_catalog);
  auto proof =
      take(reader.read_action_time_owner(registration, cause.action_id));
  const auto *entry = find_fixed_elapsed(elapsed, cause.action_id);
  if (!entry)
    fail("GrowthWorkConditionElapsedMissing", cause.action_id);
  take(reader.validate_retained_fixed_action_sources(registration, proof,
                                                     *entry));
  Source out;
  Integer completed = 0;
  if (cause.kind == WorkOrStudyConditionCause::Kind::study_lesson) {
    const auto *f = std::get_if<StudyFact>(raw);
    if (!f || cause.ordinal > static_cast<Integer>(f->lessons.size()) ||
        !growth::study_action_kind(registration.action_kind))
      fail("GrowthWorkConditionLessonMissing", cause.action_id);
    const auto &lesson =
        f->lessons[static_cast<std::size_t>(cause.ordinal - 1)];
    if (cause.actor_id == f->terms.student_id) {
      out.increment = 6;
      out.original_before = lesson.fatigue_before;
    } else if (cause.actor_id == f->terms.teacher_id)
      out.increment = 2;
    else
      fail("GrowthWorkConditionActorMismatch", cause.actor_id);
    completed = cause.ordinal;
  } else if (cause.kind ==
             WorkOrStudyConditionCause::Kind::training_completion) {
    const auto *f = std::get_if<TrainingFact>(raw);
    if (!f || f->phase != "completed" || !f->success || cause.ordinal != 1 ||
        registration.action_kind != f->action_kind)
      fail("GrowthWorkConditionTrainingMissing", cause.action_id);
    take(
        reader.validate_adopted_terminal_source(registration, cause.action_id));
    if (cause.actor_id == f->training.student_id)
      out.increment = f->training.student_fatigue;
    else if (!f->training.teacher_id.empty() &&
             cause.actor_id == f->training.teacher_id)
      out.increment = f->training.teacher_fatigue;
    else
      fail("GrowthWorkConditionActorMismatch", cause.actor_id);
    completed = f->training.duration;
  } else
    fail("GrowthWorkConditionKindInvalid", cause.action_id);
  out.at = take(read_fixed_service_credit_boundary(
      elapsed, cause.action_id,
      take(fixed_duration_work_units(completed, scale.units_per_half_day)), now,
      scale));
  return out;
}
bool same_key(const ConditionReceiptV2 &row, const StableId &action,
              Integer ordinal, const StableId &actor) {
  const auto *key =
      std::get_if<WorkOrStudyConditionBusinessKey>(&row.business_key);
  return key && key->action_id == action && key->ordinal == ordinal &&
         key->actor_id == actor;
}
} // namespace
Outcome<GrowthWorkConditionBoundary> begin_growth_work_condition(
    CommonStore &common, const AuthoredActorFoundationState &foundation,
    const ActorFoundationCatalog &catalog, const GrowthState &growth,
    const ActionCoreView &action, const GrowthFixedStepPermit &gate) {
  return guarded<GrowthWorkConditionBoundary>([&] {
    rule(common, catalog);
    take(validate_growth_fixed_step_permit(gate, common, growth, action));
    const auto *raw = growth.facts.find(action.id);
    if (!raw)
      fail("GrowthWorkConditionOriginalSourceRequired", action.id);
    auto capture = common.actor_condition_capture_identity();
    if (!capture)
      fail("GrowthWorkConditionCaptureRequired", action.id);
    auto state = std::make_shared<GrowthWorkConditionBoundary::State>();
    state->common = &common;
    state->foundation = &foundation;
    state->growth = &growth;
    state->capture = std::move(capture);
    state->savepoint = take(common.actor_condition_savepoint_boundary());
    state->identity = common.state().identity;
    state->action_id = action.id;
    state->at = gate.at();
    state->draft_offset = common.actor_condition_execution_drafts().size();
    if (const auto *f = std::get_if<StudyFact>(raw)) {
      state->kind = WorkOrStudyConditionCause::Kind::study_lesson;
      state->ordinal = gate.ordinal();
      if (f->lessons.size() + 1 != static_cast<std::size_t>(state->ordinal))
        fail("GrowthWorkConditionLessonBoundaryMismatch", action.id);
      state->actors[0].id = f->terms.student_id;
      state->actors[0].increment = 6;
      state->actors[1].id = f->terms.teacher_id;
      state->actors[1].increment = 2;
      state->count = 2;
    } else if (const auto *f = std::get_if<TrainingFact>(raw)) {
      if (!gate.final_gate())
        fail("GrowthWorkConditionTrainingGateRequired", action.id);
      state->kind = WorkOrStudyConditionCause::Kind::training_completion;
      state->ordinal = 1;
      state->actors[0].id = f->training.student_id;
      state->actors[0].increment = f->training.student_fatigue;
      state->count = 1;
      if (!f->training.teacher_id.empty()) {
        state->actors[1].id = f->training.teacher_id;
        state->actors[1].increment = f->training.teacher_fatigue;
        state->count = 2;
      }
    } else
      fail("GrowthWorkConditionOriginalKindMismatch", action.id);
    for (std::size_t i = 0; i < state->count; ++i) {
      auto &saved = state->actors[i];
      const auto *actor = common.actor(saved.id);
      const auto *head =
          foundation.character_condition_extensions.actor_condition_heads.find(
              saved.id);
      const auto *origin =
          foundation.character_origin_records.origins.find(saved.id);
      if (!actor || !head || !origin || origin->actor_id != saved.id ||
          head->actor_id != saved.id || head->condition_revision < 0 ||
          head->condition_revision == std::numeric_limits<Integer>::max() ||
          !foundation.actor_start_context.initial_condition_seeds.find(
              head->initial_condition_seed_ref.seed_key))
        fail("GrowthWorkConditionActorSourceRequired", saved.id);
      for (std::size_t j = 0; j < i; ++j)
        if (state->actors[j].id == saved.id)
          fail("GrowthWorkConditionDuplicateActor", saved.id);
      saved.head = *head;
      saved.fatigue = actor->fatigue;
      fatigue_after(saved.fatigue, saved.increment);
      for (const auto &row :
           foundation.character_condition_extensions.applied_receipts.rows())
        if (same_key(row.second, action.id, state->ordinal, saved.id))
          fail("GrowthWorkConditionAlreadyApplied", action.id);
    }
    GrowthWorkConditionBoundary out;
    out.state_ = std::move(state);
    return out;
  });
}
Status validate_growth_work_condition_receipt(
    const CommonStore &common, const ActorFoundationCatalog &foundation,
    const GrowthState &growth, const GrowthCatalog &catalog,
    const CommonCatalog &shared, const ActionElapsedStore &elapsed,
    const TimeScaleRef &scale, const CatalogIdentity &catalog_identity,
    const ActionTimeRegistration &registration, const WorldMoment &now,
    const ConditionReceiptV2 &receipt, const InteractionState *interaction,
    const InteractionCatalog *interaction_catalog) {
  return guarded<Done>([&] {
    rule(common, foundation);
    const auto *cause = std::get_if<WorkOrStudyConditionCause>(&receipt.cause);
    const auto *key =
        std::get_if<WorkOrStudyConditionBusinessKey>(&receipt.business_key);
    if (!cause || !key || receipt.actor_id != cause->actor_id ||
        key->action_id != cause->action_id ||
        key->actor_id != cause->actor_id || key->ordinal != cause->ordinal ||
        !actor_foundation_rule_equal(receipt.rule_ref,
                                     *foundation.work_or_study_rule) ||
        receipt.effective_at.kind != ActorFoundationAt::Kind::exact_moment ||
        receipt.condition_revision_before < 0 ||
        receipt.condition_revision_before ==
            std::numeric_limits<Integer>::max() ||
        receipt.condition_revision_after !=
            receipt.condition_revision_before + 1 ||
        receipt.scalar_changes.size() != 1 ||
        receipt.selected_injury_result.kind !=
            SelectedInjuryResult::Kind::not_an_injury ||
        !receipt.created_effect_keys.empty() ||
        !receipt.recovery_resolution_refs.empty() ||
        receipt.life_transition_ref ||
        !original_condition_capacity_refs_shape(foundation, receipt))
      fail("GrowthWorkConditionReceiptMismatch", receipt.result_id);
    auto actual = source(common, growth, catalog, shared, elapsed, scale,
                         catalog_identity, registration, now, *cause,
                         interaction, interaction_catalog);
    const auto &change = receipt.scalar_changes.front();
    if (take(compare_world_moments(actual.at,
                                   receipt.effective_at.exact_moment)) ||
        change.field != ConditionScalarChange::Field::fatigue ||
        change.after != fatigue_after(change.before, actual.increment) ||
        (actual.original_before && change.before != *actual.original_before))
      fail("GrowthWorkConditionFormulaMismatch", receipt.result_id);
    return Done{};
  });
}
Outcome<std::vector<BoundActorConditionWrite>> bind_growth_work_condition(
    CommonStore &common, AuthoredActorFoundationState &foundation,
    const ActorFoundationCatalog &foundation_catalog, const GrowthState &growth,
    const GrowthCatalog &catalog, const CommonCatalog &shared,
    const ActionElapsedStore &elapsed, const TimeScaleRef &scale,
    const CatalogIdentity &catalog_identity,
    const ActionTimeRegistration &registration, const WorldMoment &now,
    GrowthWorkConditionBoundary &boundary) {
  return guarded<std::vector<BoundActorConditionWrite>>([&] {
    rule(common, foundation_catalog);
    auto state = boundary.state_;
    if (!state || state->consumed || state->common != &common ||
        state->foundation != &foundation || state->growth != &growth ||
        state->capture != common.actor_condition_capture_identity() ||
        !identity(state->identity, common.state().identity))
      fail("GrowthWorkConditionBoundaryExpired");
    take(common.validate_actor_condition_savepoint_boundary(state->savepoint));
    if (take(compare_world_moments(state->at, now)))
      fail("GrowthWorkConditionActualMomentChanged", state->action_id);
    const auto *raw = growth.facts.find(state->action_id);
    if (!raw)
      fail("GrowthWorkConditionOriginalSourceRequired", state->action_id);
    bool applied = false;
    if (state->kind == WorkOrStudyConditionCause::Kind::study_lesson) {
      const auto *f = std::get_if<StudyFact>(raw);
      if (!f)
        fail("GrowthWorkConditionKindChanged");
      if (f->lessons.size() == static_cast<std::size_t>(state->ordinal))
        applied = true;
      else if (f->lessons.size() + 1 !=
                   static_cast<std::size_t>(state->ordinal) ||
               f->phase != "completed" || f->success || f->code.empty())
        fail("GrowthWorkConditionLessonNotApplied", state->action_id);
    } else {
      const auto *f = std::get_if<TrainingFact>(raw);
      if (!f || f->phase != "completed")
        fail("GrowthWorkConditionTrainingNotApplied", state->action_id);
      applied = f->success;
    }
    const auto count = applied ? state->count : 0;
    const auto &drafts = common.actor_condition_execution_drafts();
    if (drafts.size() != state->draft_offset + count)
      fail("GrowthWorkConditionActualWriteCountMismatch", state->action_id);
    std::vector<BoundActorConditionWrite> bindings;
    bindings.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
      const auto &before = state->actors[i];
      const auto &draft = drafts[state->draft_offset + i];
      auto *head =
          foundation.character_condition_extensions.actor_condition_heads.find(
              before.id);
      const auto *actor = common.actor(before.id);
      if (!head || !actor ||
          head->condition_revision != before.head.condition_revision ||
          head->latest_receipt_ref.has_value() !=
              before.head.latest_receipt_ref.has_value() ||
          (head->latest_receipt_ref &&
           head->latest_receipt_ref->result_id !=
               before.head.latest_receipt_ref->result_id) ||
          !identity(draft.identity, state->identity) ||
          draft.sequence != Integer(state->draft_offset + i + 1) ||
          draft.kind != ActorConditionExecutionDraft::Kind::mutation ||
          draft.actor_id != before.id || !draft.before ||
          draft.before->fatigue != before.fatigue || !draft.fatigue_written ||
          draft.hp_written || draft.injury_written || draft.alive_written ||
          draft.after.fatigue != actor->fatigue ||
          draft.after.fatigue !=
              fatigue_after(before.fatigue, before.increment) ||
          draft.next_instance_before != draft.next_instance_after)
        fail("GrowthWorkConditionActualWriteMismatch", before.id);
      ConditionReceiptV2 receipt;
      receipt.result_id = take(common.next_id("condition"));
      receipt.actor_id = before.id;
      receipt.business_key = WorkOrStudyConditionBusinessKey{
          state->action_id, before.id, state->ordinal};
      receipt.cause = WorkOrStudyConditionCause{state->kind, state->action_id,
                                                before.id, state->ordinal};
      receipt.rule_ref = *foundation_catalog.work_or_study_rule;
      receipt.effective_at.exact_moment = now;
      receipt.condition_revision_before = head->condition_revision;
      receipt.condition_revision_after = head->condition_revision + 1;
      receipt.scalar_changes = {{ConditionScalarChange::Field::fatigue,
                                 before.fatigue,
                                 actor->fatigue}}; // 热绑定消费写前sealed
                                                   // gate＋同capture真实原写；此时terminal
                                                   // FSL可能尚未追加。
      // 源冷反验在原terminal采用后由完整coverage统一执行，不能因先绑收据而倒造FSL来源。
      if (registration.owner_domain != "growth" ||
          registration.action_kind.empty())
        fail("GrowthWorkConditionRegistrationMissing", state->action_id);
      const auto *period = find_fixed_elapsed(elapsed, state->action_id);
      if (!period || take(compare_world_moments(period->last_settled_at, now)))
        fail("GrowthWorkConditionGateChanged", state->action_id);
      const Integer completed =
          state->kind == WorkOrStudyConditionCause::Kind::study_lesson
              ? state->ordinal
              : std::get<TrainingFact>(*raw).training.duration;
      auto expected_credit =
          take(fixed_duration_work_units(completed, scale.units_per_half_day));
      if (take(compare_fractions(period->credited_units.value,
                                 expected_credit.value)))
        fail("GrowthWorkConditionGateChanged", state->action_id);
      if (state->kind == WorkOrStudyConditionCause::Kind::study_lesson &&
          i == 0 &&
          std::get<StudyFact>(*raw)
                  .lessons[static_cast<std::size_t>(state->ordinal - 1)]
                  .fatigue_before != before.fatigue)
        fail("GrowthWorkConditionLessonBeforeMismatch", state->action_id);
      const auto id = receipt.result_id;
      if (!foundation.character_condition_extensions.applied_receipts.insert(
              id, std::move(receipt)))
        fail("DuplicateConditionReceipt", id);
      head->condition_revision += 1;
      head->latest_receipt_ref = ConditionReceiptRefV2{id};
      BoundActorConditionWrite actual{
          {id}, state->capture, Integer(state->draft_offset + i + 1)};
      take(append_condition_application_fact_in_candidate(
          common, foundation, foundation_catalog, actual));
      bindings.push_back(std::move(actual));
    }
    state->consumed = true;
    return bindings;
  });
}
Status validate_growth_work_condition_coverage(
    const CommonStore &common, const AuthoredActorFoundationState &foundation,
    const ActorFoundationCatalog &foundation_catalog, const GrowthState &growth,
    const GrowthCatalog &catalog, const CommonCatalog &shared,
    const ActionElapsedStore &elapsed, const TimeScaleRef &scale,
    const CatalogIdentity &catalog_identity,
    const std::vector<ActionTimeRegistration> &registrations,
    const WorldMoment &now, const InteractionState *interaction,
    const InteractionCatalog *interaction_catalog) {
  return guarded<Done>([&] {
    for (const auto &row : growth.facts.rows()) {
      const StudyFact *study = std::get_if<StudyFact>(&row.second);
      const TrainingFact *training = std::get_if<TrainingFact>(&row.second);
      if (!study && !training)
        continue;
      const StableId kind =
          study ? (study->terms.study_teaching_agreement ? "teach_study"
                                                         : "study_skill")
                : training->action_kind;
      const ActionTimeRegistration *registration = nullptr;
      for (const auto &candidate : registrations)
        if (candidate.owner_domain == "growth" &&
            candidate.action_kind == kind) {
          if (registration)
            fail("DuplicateGrowthWorkConditionRegistration", kind);
          registration = &candidate;
        }
      if (!registration)
        fail("GrowthWorkConditionRegistrationMissing", kind);
      Integer count =
          study ? static_cast<Integer>(study->lessons.size())
                : (training->phase == "completed" && training->success ? 1 : 0);
      for (Integer ordinal = 1; ordinal <= count; ++ordinal) {
        const std::array<const StableId *, 2> actors =
            study ? std::array<const StableId *, 2>{&study->terms.student_id,
                                                    &study->terms.teacher_id}
                  : std::array<const StableId *, 2>{
                        &training->training.student_id,
                        &training->training.teacher_id};
        for (const auto *actor : actors) {
          if (actor->empty())
            continue;
          std::size_t matched = 0;
          for (const auto &receipt : foundation.character_condition_extensions
                                         .applied_receipts.rows())
            if (same_key(receipt.second, row.first, ordinal, *actor)) {
              take(validate_growth_work_condition_receipt(
                  common, foundation_catalog, growth, catalog, shared, elapsed,
                  scale, catalog_identity, *registration, now, receipt.second,
                  interaction, interaction_catalog));
              ++matched;
            }
          if (matched != 1)
            fail("GrowthWorkConditionCoverageMismatch", row.first);
        }
      }
    }
    for (const auto &row :
         foundation.character_condition_extensions.applied_receipts.rows())
      if (const auto *cause =
              std::get_if<WorkOrStudyConditionCause>(&row.second.cause);
          cause &&
          (cause->kind == WorkOrStudyConditionCause::Kind::study_lesson ||
           cause->kind ==
               WorkOrStudyConditionCause::Kind::training_completion)) {
        const auto *raw = growth.facts.find(cause->action_id);
        if (!raw)
          fail("OrphanGrowthWorkConditionReceipt", row.first);
        const auto *study = std::get_if<StudyFact>(raw);
        const auto *training = std::get_if<TrainingFact>(raw);
        const StableId kind =
            study      ? (study->terms.study_teaching_agreement ? "teach_study"
                                                                : "study_skill")
            : training ? training->action_kind
                       : StableId{};
        const ActionTimeRegistration *registration = nullptr;
        for (const auto &r : registrations)
          if (r.owner_domain == "growth" && r.action_kind == kind)
            registration = &r;
        if (!registration)
          fail("GrowthWorkConditionRegistrationMissing", row.first);
        take(validate_growth_work_condition_receipt(
            common, foundation_catalog, growth, catalog, shared, elapsed, scale,
            catalog_identity, *registration, now, row.second, interaction,
            interaction_catalog));
      }
    return Done{};
  });
}
} // namespace hanmo::resident
