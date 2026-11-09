#include "growth_capability_chain.hpp"
#include "growth_trade_grant_read.hpp"
#include "growth_complete_validation.hpp"
#include "growth_skill_qualification_v2.hpp"
// 验证原固定条款、课时证据和完成因果；只读检查不补收费、补课或重发经验。
#include "growth_equal.hpp"
#include "growth_internal.hpp"
#include "growth_life_results.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
bool range(Integer v, Integer low, Integer high) {
  return v >= low && v <= high;
}
Integer learned(const ActorRecord *a, const StableId &skill) {
  if (!a)
    return -1;
  auto *m = a->combat.learned.find(skill);
  return m ? *m : -1;
}
// 冷输入可含任意int64；先界定每个新数值，再允许暂停和总贡献函数做加减。
Status prevalidate_study_segments(const StudyFact &f, Integer now) {
  auto adjusted = innate_study_lesson_xp(
      growth_lesson_xp(0, 0),
      f.terms.innate_source ? f.terms.innate_source->bonus_bp : 0);
  if (!adjusted.ok)
    return {false, {}, adjusted.error};
  const auto normal = adjusted.value, maximum = normal * 12000 / 10000,
             bonus = maximum - normal;
  for (const auto &lesson : f.lessons) {
    if (!lesson.segment)
      return common_failure("InvalidStudySegment");
    const auto &part = *lesson.segment;
    if (!range(lesson.xp, 0, normal) || !range(part.ordinal, 1, 6) ||
        !range(part.settled_tick, 0, now) || !range(part.xp_before, 0, 1200) ||
        !range(part.performance_bp, 10000, 12000) ||
        !range(part.xp_gained, 0, maximum))
      return common_failure("InvalidStudySegment");
    if (part.adjustment) {
      const auto &a = *part.adjustment;
      if (!range(a.settled_tick, 0, now) || !range(a.xp_before, 0, 1200) ||
          !range(a.performance_bp, 10000, 12000) ||
          !range(a.xp_gained, 0, bonus))
        return common_failure("InvalidStudyAdjustment");
    }
  }
  if (f.pauses)
    for (const auto &pause : *f.pauses)
      if (pause.credited_xp && !range(*pause.credited_xp, 0, 6 * maximum))
        return common_failure("InvalidStudyPause");
  return common_success();
}
// 分段证据按真实边界重放只读账本；教学、迟答补差与恢复基线各有唯一位置。
Status validate_study_segments(const StudyFact &f, Integer now) {
  struct Credit {
    Integer tick, ordinal, before, gained;
    bool adjustment;
  };
  std::vector<Credit> credits;
  const auto final_bp = f.challenge && f.challenge->result
                            ? f.challenge->result->performance_bp
                            : 10000;
  Integer previous_tick = f.start_tick;
  for (std::size_t n = 0; n < f.lessons.size(); ++n) {
    const auto &lesson = f.lessons[n];
    if (!lesson.segment)
      return common_failure("InvalidStudySegment");
    const auto &part = *lesson.segment;
    if (now < f.start_tick || Integer(n) + 1 > now - f.start_tick)
      return common_failure("InvalidStudySegment");
    Integer tick = f.start_tick + Integer(n) + 1;
    if (f.pauses)
      for (const auto &pause : *f.pauses)
        if (pause.end_tick >= 0 && pause.after_lessons <= Integer(n)) {
          const auto elapsed = pause.end_tick - pause.start_tick;
          if (elapsed > now - tick)
            return common_failure("InvalidStudySegment");
          tick += elapsed;
        }
    if (part.ordinal != Integer(n) + 1 || part.settled_tick != tick ||
        part.settled_tick <= previous_tick || part.settled_tick > now ||
        !range(part.xp_before, 0, 1200) ||
        !range(part.performance_bp, 10000, 12000) ||
        part.xp_gained !=
            std::max<Integer>(
                0, std::min<Integer>(lesson.xp * part.performance_bp / 10000,
                                     f.terms.xp_limit - part.xp_before)))
      return common_failure("InvalidStudySegment");
    if (part.performance_pending) {
      if (!f.challenge || f.challenge->frozen.mode != "manual" ||
          part.performance_bp != 10000)
        return common_failure("InvalidStudySegment");
    } else if (part.performance_bp != final_bp ||
               (f.challenge && !f.challenge->result))
      return common_failure("InvalidStudySegment");
    const bool expected_adjustment =
        part.performance_pending && final_bp > 10000;
    if (bool(part.adjustment) != expected_adjustment)
      return common_failure("InvalidStudyAdjustment");
    credits.push_back({part.settled_tick, part.ordinal, part.xp_before,
                       part.xp_gained, false});
    if (part.adjustment) {
      const auto &a = *part.adjustment;
      if (!range(a.settled_tick, part.settled_tick, now) ||
          !range(a.xp_before, 0, 1200) || a.performance_bp != final_bp ||
          a.xp_gained !=
              std::max<Integer>(
                  0, std::min<Integer>(lesson.xp * final_bp / 10000 - lesson.xp,
                                       f.terms.xp_limit - a.xp_before)))
        return common_failure("InvalidStudyAdjustment");
      credits.push_back(
          {a.settled_tick, part.ordinal, a.xp_before, a.xp_gained, true});
    }
    previous_tick = part.settled_tick;
  }
  std::sort(credits.begin(), credits.end(), [](const auto &a, const auto &b) {
    return std::tie(a.tick, a.adjustment, a.ordinal) <
           std::tie(b.tick, b.adjustment, b.ordinal);
  });
  Integer total = 0;
  for (const auto &credit : credits) {
    Integer expected = f.terms.xp_before + total;
    if (f.pauses)
      for (const auto &pause : *f.pauses)
        if (pause.end_tick >= 0 && pause.end_tick <= credit.tick &&
            pause.credited_xp && *pause.credited_xp <= total &&
            (credit.adjustment || pause.after_lessons < credit.ordinal))
          expected = pause.student_xp + total - *pause.credited_xp;
    if (credit.before != expected)
      return common_failure("InvalidStudySegmentBaseline");
    total += credit.gained;
  }
  if (f.pauses)
    for (const auto &pause : *f.pauses)
      if (pause.end_tick >= 0) {
        Integer before = 0, at = 0;
        for (const auto &credit : credits) {
          if (!credit.adjustment) {
            if (credit.ordinal <= pause.after_lessons)
              before += credit.gained;
          } else if (credit.tick < pause.end_tick)
            before += credit.gained;
          else if (credit.tick == pause.end_tick)
            at += credit.gained;
        }
        if (!pause.credited_xp ||
            (*pause.credited_xp != before && *pause.credited_xp != before + at))
          return common_failure("InvalidStudyPause");
      }
  return common_success();
}
Status validate_study_fact(const CommonStore &common, const GrowthState &state,
                           const GrowthCatalog &catalog,
                           const CommonCatalog &shared,
                           const GrowthAccess &access, const StableId &id,
                           const StudyFact &f,
                           const GrowthCompleteValidationSources *exact) {
  const auto &t = f.terms;
  if (t.study_teaching_agreement) {
    if (!catalog.study_teaching_requests.enabled ||
        !access.validate_study_teaching_agreement)
      return common_failure("StudyTeachingCapabilityRequired", id);
    const auto &source = *t.study_teaching_agreement;
    if (source.accepted_tick != f.start_tick ||
        source.student_actor_id != f.actor_id ||
        source.student_actor_id != t.student_id ||
        source.teacher_actor_id != t.teacher_id ||
        (t.challenge && t.challenge->mode != "automatic"))
      return common_failure("StudyTeachingFactMismatch", id);
    auto valid = access.validate_study_teaching_agreement(source, t);
    if (!valid.ok)
      return valid;
  }
  const bool innate =
      t.settlement_policy && *t.settlement_policy == "innate_per_lesson_v1";
  if (innate != bool(t.innate_source) || (innate && !catalog.innate_learning))
    return common_failure("InnateStudyProfileRequired", id);
  if (innate) {
    const auto &source = *t.innate_source;
    if (!access.validate_innate_study_source ||
        source.actor_id != t.student_id || source.skill_id != t.skill_id ||
        source.observed_tick != f.start_tick)
      return common_failure("InnateStudySourceMismatch", id);
    auto valid = access.validate_innate_study_source(source, id);
    if (!valid.ok)
      return valid;
  }
  const bool segmented = growth::study_segmented(f);
  if (t.settlement_policy && (!catalog.study_segments || !segmented))
    return common_failure("InvalidStudySettlementPolicy", id);
  auto *student = common.state().actors.find(t.student_id);
  auto *teacher = common.state().actors.find(t.teacher_id);
  if (!growth_skill_defined(t.skill_id, shared.extended_skills) ||
      (t.mode != "facility" && t.mode != "teacher") || !student || !teacher ||
      t.student_id == t.teacher_id || f.actor_id != t.student_id ||
      !range(t.duration, 2, 6) || !range(t.xp_before, 0, 1199) ||
      !range(t.teacher_xp, 0, 1200) || !range(t.student_fatigue, 0, 100) ||
      !range(t.teacher_fatigue, 0, 100))
    return common_failure("InvalidStudyTerms", id);
  if (growth_skill_xp(student->skills, t.skill_id) < 0 ||
      growth_skill_xp(teacher->skills, t.skill_id) < 0)
    return common_failure("InvalidStudyActor", id);
  auto *role = shared.roles.find(t.context.role_id);
  if (t.context.kind || t.context.encounter_id ||
      t.context.provider_actor_id != t.teacher_id || !role)
    return common_failure("InvalidStudyContext", id);
  auto *course = growth::course(catalog, t.context.role_id, t.skill_id);
  if (!course || course->mode != t.mode ||
      course->wallet_id != t.recipient_wallet_id ||
      role->place_id != t.context.place_id ||
      std::find(role->services.begin(), role->services.end(), "study_skill") ==
          role->services.end())
    return common_failure("StudyTermsMismatch", id);
  if (t.settlement_policy != course->settlement_policy)
    return common_failure("StudySettlementPolicyMismatch", id);
  if (bool(course->challenge) != bool(t.challenge) ||
      bool(t.challenge) != bool(f.challenge))
    return common_failure("StudyChallengeTermsMismatch", id);
  if (t.challenge) {
    const auto &author = *course->challenge;
    const auto &frozen = *t.challenge;
    if (!growth::challenge_context_equal(author.context, frozen.context) ||
        (frozen.context && frozen.context->quote) ||
        author.field != frozen.field || author.kind != frozen.kind ||
        author.id != frozen.challenge_id ||
        frozen.skill_level != growth_skill_level(t.xp_before) ||
        !growth::frozen_equal(frozen, f.challenge->frozen) ||
        f.challenge->actor_id != f.actor_id || f.challenge->source_id != id ||
        f.challenge->consumed != (f.phase == "completed" &&
                                  (!exact || !f.study_service_terminal ||
                                   !growth::challenge_pending(*f.challenge))))
      return common_failure("StudyChallengeBindingMismatch", id);
    auto valid = growth::challenge_validate(catalog, *f.challenge);
    if (!valid.ok)
      return valid;
  }
  const auto limit =
      t.mode == "facility"
          ? 300
          : growth::original_skill_threshold(growth_skill_level(t.teacher_xp));
  if (t.xp_limit != limit || t.xp_before >= limit ||
      (t.mode == "teacher" &&
       growth_skill_level(t.teacher_xp) <= growth_skill_level(t.xp_before)) ||
      t.fee != (growth::original_study_lesson_cost(t.xp_before)) * t.duration)
    return common_failure("InvalidStudyGrowth", id);
  auto *payer = common.wallet(t.payer_wallet_id);
  auto *recipient = common.wallet(t.recipient_wallet_id);
  const Owner owner =
      role->organization_id
          ? Owner{Owner::Kind::organization, *role->organization_id, {}, {}}
          : Owner{Owner::Kind::actor, t.teacher_id, {}, {}};
  if (!payer || !recipient || payer->id == recipient->id ||
      student->wallet_id != payer->id ||
      !owner_equal(payer->owner,
                   Owner{Owner::Kind::actor, t.student_id, {}, {}}) ||
      !owner_equal(recipient->owner, owner))
    return common_failure("InvalidStudyPayment", id);
  if (exact)
    return validate_complete_study_result_sources(
        common, state, catalog, shared, access, id, f, *exact);
  if (f.lessons.size() > static_cast<std::size_t>(t.duration) ||
      !range(f.start_tick, 0, common.tick()))
    return common_failure("InvalidStudyProgress", id);
  if (segmented) {
    auto valid = prevalidate_study_segments(f, common.tick());
    if (!valid.ok)
      return valid;
  }
  // 暂停必须贴合真实课时边界；只有最后一段可开放，历史不允许插空或倒退。
  Integer paused_ticks = 0, previous_lessons = 0, previous_xp = t.xp_before,
          previous_credited = 0;
  const bool has_pause_field =
      std::find(f.field_order.begin(), f.field_order.end(), "pauses") !=
      f.field_order.end();
  if (bool(f.pauses) != has_pause_field)
    return common_failure("InvalidStudyPause", id);
  if (f.pauses) {
    if (f.pauses->empty() || !growth::study_pause_supported(f))
      return common_failure("InvalidStudyPause", id);
    for (std::size_t index = 0; index < f.pauses->size(); ++index) {
      const auto &pause = (*f.pauses)[index];
      if (!range(pause.after_lessons, previous_lessons,
                 Integer(f.lessons.size())) ||
          pause.after_lessons >= t.duration ||
          !range(pause.start_tick, f.start_tick, common.tick()))
        return common_failure("InvalidStudyPause", id);
      const auto elapsed = pause.start_tick - f.start_tick;
      if (elapsed < pause.after_lessons ||
          elapsed - pause.after_lessons != paused_ticks)
        return common_failure("InvalidStudyPause", id);
      if (bool(pause.credited_xp) != (segmented && pause.end_tick >= 0))
        return common_failure("InvalidStudyPause", id);
      if (pause.end_tick == -1) {
        if (index + 1 != f.pauses->size() || f.phase != "started" ||
            pause.after_lessons != Integer(f.lessons.size()) ||
            pause.student_fatigue != -1 || pause.teacher_fatigue != -1 ||
            pause.student_xp != -1)
          return common_failure("InvalidStudyPause", id);
      } else {
        if (!range(pause.end_tick, pause.start_tick, common.tick()) ||
            !range(pause.student_fatigue, 0, 100) ||
            !range(pause.teacher_fatigue, 0, 100) ||
            !range(pause.student_xp, previous_xp, 1200))
          return common_failure("InvalidStudyPause", id);
        if (segmented) {
          if (!range(*pause.credited_xp, previous_credited,
                     growth::study_xp_gained(f)) ||
              pause.student_xp <
                  previous_xp + *pause.credited_xp - previous_credited)
            return common_failure("InvalidStudyPause", id);
          previous_credited = *pause.credited_xp;
        }
        previous_xp = pause.student_xp;
      }
      paused_ticks += (pause.end_tick == -1 ? common.tick() : pause.end_tick) -
                      pause.start_tick;
      previous_lessons = pause.after_lessons;
    }
  }
  const bool has_settlement_field =
      std::find(f.field_order.begin(), f.field_order.end(),
                "xp_settlement_before") != f.field_order.end();
  if (bool(f.xp_settlement_before) != has_settlement_field ||
      bool(f.xp_settlement_before) !=
          (!segmented && f.pauses && f.phase == "completed") ||
      (f.xp_settlement_before && *f.xp_settlement_before != previous_xp))
    return common_failure("InvalidStudyGrowth", id);
  for (std::size_t n = 0; n < f.lessons.size(); ++n) {
    const auto &lesson = f.lessons[n];
    if (bool(lesson.segment) != segmented)
      return common_failure("InvalidStudySegment", id);
    auto innate_xp = innate_study_lesson_xp(
        growth_lesson_xp(lesson.fatigue_before, lesson.injury),
        t.innate_source ? t.innate_source->bonus_bp : 0);
    if (!innate_xp.ok)
      return {false, {}, innate_xp.error};
    if (lesson.fatigue_before != growth::study_fatigue(f, n) ||
        !range(lesson.injury, 0, 3) || lesson.xp != innate_xp.value ||
        (lesson.injury == 3 &&
         (t.skill_id == "infantry" || t.skill_id == "cavalry" ||
          t.skill_id == "archery" || t.skill_id == "martial")))
      return common_failure("InvalidStudyLesson", id);
  }
  if (segmented) {
    auto valid = validate_study_segments(
        f, f.phase == "completed" ? f.completed_tick : common.tick());
    if (!valid.ok)
      return valid;
  }
  const auto *action = common.action(id);
  const auto *payload = state.actions.find(id);
  auto count = Integer(f.lessons.size());
  if (f.phase == "started") {
    if (f.completed_tick != -1 || f.success || !f.code.empty())
      return common_failure("InvalidStudyFact", id);
    if (!action || !payload ||
        action->kind !=
            (t.study_teaching_agreement ? "teach_study" : "study_skill"))
      return common_failure("MissingStudyAction", id);
    auto *terms = std::get_if<StudyTerms>(&payload->quote.data);
    const bool paused = growth::study_paused(f);
    const auto participants =
        paused ? std::vector<StableId>{} : growth::study_participants(t);
    if (!terms || !growth::study_equal(t, *terms) ||
        action->actor_id != growth::study_initiator(t) ||
        action->start_tick != f.start_tick ||
        action->participants != participants)
      return common_failure("StudyFactMismatch", id);
    if (t.study_teaching_agreement &&
        (!payload->quote.context ||
         !growth::context_equal(*payload->quote.context,
                                t.study_teaching_agreement->request_context) ||
         payload->request.kind != "teach_study"))
      return common_failure("StudyTeachingActionMismatch", id);
    if (action->remaining_ticks != t.duration - count ||
        !range(action->remaining_ticks, 1, t.duration) ||
        common.tick() - f.start_tick - count != paused_ticks)
      return common_failure("InvalidStudyProgress", id);
    const auto current_xp = growth_skill_xp(student->skills, t.skill_id);
    const auto baseline = growth::study_xp_baseline(f);
    if (paused ? current_xp < baseline : current_xp != baseline)
      return common_failure("StudyGrowthChanged", id);
    if (growth::reserved(payer, id) !=
        t.fee - (growth::original_study_lesson_cost(t.xp_before)) * count)
      return common_failure("StudyReservationMismatch", id);
    if (paused && (student->action_id == id || teacher->action_id == id))
      return common_failure("StudyOccupationMissing", id);
    for (const auto &person : action->participants) {
      auto *a = common.actor(person);
      if (!a || a->action_id != id)
        return common_failure("StudyOccupationMissing", id);
    }
  } else if (f.phase == "completed") {
    if (action || payload || f.completed_tick < f.start_tick ||
        f.completed_tick > common.tick() || growth::reserved(payer, id) != -1)
      return common_failure("InvalidStudyCompletion", id);
    if (f.success) {
      if (!f.code.empty() || count != t.duration ||
          f.completed_tick - f.start_tick - t.duration != paused_ticks)
        return common_failure("InvalidStudyCompletion", id);
    } else if (f.code == "StudyCancelled") {
      if (count >= t.duration ||
          f.completed_tick - f.start_tick - count != paused_ticks)
        return common_failure("InvalidStudyCompletion", id);
    } else if (f.code.empty() || count >= t.duration ||
               f.completed_tick - f.start_tick - count - 1 != paused_ticks)
      return common_failure("InvalidStudyCompletion", id);
  } else
    return common_failure("InvalidStudyFact", id);
  return common_success();
}
Status validate_training(const CommonStore &common,
                         const GrowthCatalog &catalog,
                         const CommonCatalog &shared,
                         const GrowthAccess &access, const StableId &id,
                         const TrainingFact &f, const GrowthAction *payload,
                         const GrowthState &state,
                         const GrowthCompleteValidationSources *exact) {
  const auto &t = f.training;
  // 结束后的邀请来源仍与原唯一训练事实绑定，不因离场或新执行代重新询问NPC。
  if (catalog.teaching_requests.enabled && f.action_kind == "teach_skill") {
    if (!t.teaching_agreement || !access.validate_teaching_agreement)
      return common_failure("TeachingAgreementRequired", id);
    const auto &agreed = *t.teaching_agreement;
    if (agreed.teacher_actor_id != t.teacher_id ||
        agreed.student_actor_id != t.student_id ||
        agreed.skill_id != t.skill_id ||
        agreed.teacher_actor_id != f.actor_id ||
        agreed.accepted_tick != f.start_tick ||
        agreed.context.place_id != t.place_id || !f.context ||
        !growth::context_equal(agreed.context, *f.context))
      return common_failure("TeachingAgreementFactMismatch", id);
    auto valid = access.validate_teaching_agreement(agreed);
    if (!valid.ok)
      return valid;
  } else if (t.teaching_agreement)
    return common_failure("UnexpectedTeachingAgreement", id);
  auto *student = common.state().actors.find(t.student_id);
  auto skill =
      access.personal_skill ? access.personal_skill(t.skill_id) : std::nullopt;
  if (!skill || !shared.places.find(t.place_id) || !student)
    return common_failure("UnknownTrainingReference", id);
  if (!range(t.duration, 1, 30) || !range(t.mastery_before, -1, 100) ||
      !range(t.mastery_after, 0, 100) || !range(t.student_fatigue, 0, 180) ||
      !range(t.teacher_fatigue, 0, 60) || !range(t.fee, 0, 10000))
    return common_failure("InvalidTrainingTerms", id);
  if (exact) {
    auto precise = validate_complete_training_result_sources(
        common, state, catalog, shared, id, f, *exact);
    if (!precise.ok)
      return precise;
  }
  if (f.phase == "completed") {
    if (common.action(id) || payload ||
        (!exact && (f.completed_tick < f.start_tick + t.duration ||
                    f.completed_tick > common.tick())))
      return common_failure("InvalidTrainingCompletion", id);
    return common_success();
  }
  if (f.phase != "started" || f.completed_tick != -1 || f.success)
    return common_failure("InvalidTrainingFact", id);
  auto *action = common.action(id);
  if (!action || !payload)
    return common_failure("MissingTrainingAction", id);
  auto *terms = std::get_if<TrainingTerms>(&payload->quote.data);
  if (!terms || !growth::training_equal(t, *terms) ||
      !growth::contexts_equal(f.context, payload->quote.context) ||
      f.action_kind != action->kind || f.actor_id != action->actor_id ||
      f.start_tick != action->start_tick)
    return common_failure("TrainingFactMismatch", id);
  if ((!exact && !range(action->start_tick, 0, common.tick())) ||
      !range(action->remaining_ticks, 1, t.duration))
    return common_failure("InvalidTrainingTime", id);
  if (!exact && action->start_tick + t.duration - action->remaining_ticks !=
                    common.tick())
    return common_failure("TrainingClockMismatch", id);
  std::vector<StableId> participants{action->actor_id};
  if (action->kind == "practice_skill") {
    if (action->actor_id != t.student_id || !t.teacher_id.empty() ||
        f.context || t.duration > 8 || t.mastery_before < 0 ||
        skill->practice_ticks < 1)
      return common_failure("InvalidPracticeTerms", id);
    auto gain = ((100 + skill->practice_ticks - 1) / skill->practice_ticks) *
                t.duration;
    if ((t.mastery_after != std::min<Integer>(100, t.mastery_before + gain) &&
         t.mastery_after !=
             std::min<Integer>(100, t.mastery_before +
                                        std::max<Integer>(1, gain / 2))) ||
        t.student_fatigue != t.duration * 6 || t.teacher_fatigue != 0)
      return common_failure("InvalidPracticeGrowth", id);
  } else {
    if (!f.context || f.context->kind ||
        !shared.roles.find(f.context->role_id) ||
        f.context->place_id != t.place_id)
      return common_failure("InvalidTrainingContext", id);
    auto *teacher = common.state().actors.find(t.teacher_id);
    auto provider = action->kind == "learn_skill" ? t.teacher_id : t.student_id;
    if (f.context->provider_actor_id != provider || !teacher ||
        t.teacher_id == t.student_id)
      return common_failure("InvalidTrainingTeacher", id);
    participants.push_back(provider);
    if (t.student_fatigue != t.duration * 4 ||
        t.teacher_fatigue != t.duration * 2)
      return common_failure("InvalidTrainingFatigue", id);
    if (action->kind == "learn_skill") {
      if (action->actor_id != t.student_id || t.mastery_before != -1 ||
          t.mastery_after != 0)
        return common_failure("InvalidLessonGrowth", id);
      auto *payer = common.wallet(t.payer_wallet_id);
      auto *recipient = common.wallet(t.recipient_wallet_id);
      auto *role = shared.roles.find(f.context->role_id);
      Owner owner =
          role->organization_id
              ? Owner{Owner::Kind::organization, *role->organization_id, {}, {}}
              : Owner{Owner::Kind::actor, t.teacher_id, {}, {}};
      if (!payer || !recipient || payer->id != student->wallet_id ||
          payer->id == recipient->id ||
          !owner_equal(payer->owner,
                       Owner{Owner::Kind::actor, action->actor_id, {}, {}}) ||
          !owner_equal(recipient->owner, owner))
        return common_failure("InvalidLessonPayment", id);
      auto *offering = catalog.roles.find(f.context->role_id);
      bool found = false;
      if (offering)
        for (const auto &lesson : offering->lessons)
          if (lesson.skill_id == t.skill_id && lesson.fee == t.fee &&
              lesson.study_ticks == t.duration &&
              lesson.wallet_id == t.recipient_wallet_id)
            found = true;
      if (!found)
        return common_failure("LessonTermsMismatch", id);
    } else if (action->kind != "teach_skill" ||
               action->actor_id != t.teacher_id || t.duration != 2 ||
               t.mastery_after !=
                   std::min(learned(teacher, t.skill_id),
                            std::max<Integer>(0, t.mastery_before) + 10))
      return common_failure("InvalidTeachingGrowth", id);
  }
  if (action->kind != "learn_skill" &&
      (t.fee != 0 || !t.payer_wallet_id.empty() ||
       !t.recipient_wallet_id.empty()))
    return common_failure("UnexpectedTrainingPayment", id);
  if (action->participants != participants)
    return common_failure("InvalidTrainingParticipants", id);
  for (const auto &person : participants) {
    auto *actor = common.actor(person);
    if (!actor || actor->action_id != id)
      return common_failure("TrainingOccupationMissing", id);
  }
  if (learned(student, t.skill_id) != t.mastery_before)
    return common_failure("TrainingGrowthChanged", id);
  return common_success();
}
} // namespace
static Status
validate_growth_impl(const CommonStore &common, const GrowthState &state,
                     const GrowthCatalog &catalog, const CommonCatalog &shared,
                     const GrowthAccess &access,
                     const GrowthCompleteValidationSources *exact) {
  if (catalog.complete_study_sources != bool(exact))
    return common_failure("CompleteGrowthSourceValidatorRequired");
  if (!catalog.complete_capability_application_chain && !state.trade_grants.empty())
    return common_failure("UnexpectedGrowthTradeGrant");
  if (catalog.cards.enabled != state.cards.has_value())
    return common_failure(catalog.cards.enabled ? "MissingGrowthCards"
                                                : "CardsCapabilityRequired");
  std::set<StableId> bases, consumed, pending_students;
  std::set<std::tuple<StableId, StableId, StableId, StableId>> study_sources;
  for (const auto &row : state.facts.rows()) {
    Status r;
    if (auto *f = std::get_if<StudyFact>(&row.second)) {
      r = validate_study_fact(common, state, catalog, shared, access, row.first,
                              *f, exact);
      if (f->terms.study_teaching_agreement) {
        const auto &source = *f->terms.study_teaching_agreement;
        if (!study_sources
                 .emplace(source.request_id, source.session_id,
                          source.teacher_actor_id, source.student_actor_id)
                 .second)
          return common_failure("DuplicateStudyTeachingConsumption", row.first);
      }
      if (f->phase == "started" && !pending_students.insert(f->actor_id).second)
        return common_failure("InvalidStudyProgress", row.first);
    } else if (auto *rest = std::get_if<RestFact>(&row.second))
      r = exact
              ? validate_complete_rest_result_sources(
                    common, state, catalog, shared, row.first, *rest, *exact)
              : growth::validate_rest(common, state, access, row.first, *rest);
    else if (auto *bargain = std::get_if<BargainFact>(&row.second)) {
      r = growth::validate_bargain(common, state, catalog, shared, row.first,
                                   *bargain);
      if (!bases.insert(bargain->basis_sha256).second)
        return common_failure("DuplicateBargainQuote");
      if (!bargain->consumed_action_id.empty() &&
          !consumed.insert(bargain->consumed_action_id).second)
        return common_failure("DuplicateBargainConsumption");
    } else
      r = validate_training(common, catalog, shared, access, row.first,
                            std::get<TrainingFact>(row.second),
                            state.actions.find(row.first), state, exact);
    if (!r.ok)
      return r;
  }
  for (const auto &row : state.actions.rows()) {
    auto *action = common.action(row.first);
    if (!action || !growth_action_kind(action->kind) ||
        action->kind != row.second.request.kind)
      return common_failure("ResidentActionPayloadMismatch", row.first);
    if (action->kind == "service" || action->kind == "trade") {
      auto r = growth::validate_service_trade(common, catalog, shared, *action,
                                              row.second);
      if (!r.ok)
        return r;
    } else if (!state.facts.find(row.first))
      return common_failure("MissingGrowthFact", row.first);
  }
  if (state.cards) {
    auto valid = catalog.complete_study_sources
                     ? validate_retained_legacy_growth_card_rows(
                           common.state(), state, *state.cards, catalog.cards)
                     : validate_growth_cards(common.state(), state,
                                             *state.cards, catalog.cards);
    if (!valid.ok)
      return valid;
  }
  if (exact) {
    const auto &body = exact->condition;
    if (!body.elapsed || !body.scale || !body.action_registrations)
      return common_failure("CompleteLifeTimeSourcesRequired");
    GrowthActionTimeRead reader(common, state, catalog, shared, *body.elapsed,
                                *body.scale, body.catalog.identity,
                                body.actual_now, {}, body.interaction,
                                body.interaction_catalog, exact->market_exact);
    return validate_life_activities_at_moment(
        common, state, catalog, shared, reader, *body.action_registrations,
        body.actual_now);
  }
  return validate_life_activities(common, state, catalog, shared);
}
Status validate_growth(const CommonStore &common, const GrowthState &state,
                       const GrowthCatalog &catalog,
                       const CommonCatalog &shared,
                       const GrowthAccess &access) {
  return validate_growth_impl(common, state, catalog, shared, access, nullptr);
}
Status validate_growth_with_complete_sources(
    const CommonStore &common, const GrowthState &state,
    const GrowthCatalog &catalog, const CommonCatalog &shared,
    const GrowthAccess &access, const GrowthCompleteValidationSources &exact) {
  auto valid =
      validate_growth_impl(common, state, catalog, shared, access, &exact);
  if (!valid.ok)
    return valid;
  const auto &body = exact.condition;
  valid = validate_growth_trade_grants_v1(common, state, catalog, shared, body.actual_now);
  if (!valid.ok || !catalog.complete_capability_application_chain)
    return valid;
  if (!body.permanent_growth || !body.permanent_growth_catalog ||
      !body.work_practice)
    return common_failure("GrowthPermanentCompleteSourcesRequired");
  GrowthPermanentSources sources{body, *body.permanent_growth_catalog,
                                 *body.permanent_growth, *body.work_practice};
  valid = validate_growth_permanent_results(sources);
  if (!valid.ok)
    return valid;
  valid = validate_growth_capability_chain(sources, state);
  if (!valid.ok)
    return valid;
  for (const auto &actor : common.state().actors.rows())
    for (const auto &skill : growth_skill_ids(shared.extended_skills)) {
      auto net =
          read_verified_growth_skill_net_v2(body, state, actor.first, skill);
      if (!net.ok)
        return {false, {}, net.error};
    }
  return common_success();
}

} // namespace hanmo::resident
