#include "director_native_subjects2.hpp"
#include "director_actor_read.hpp"
#include "director_appointment_wake2.hpp"
#include "director_task_wake2.hpp"
#include "resident/domains/tasks/work_execution/work_supervision_execution_terms.hpp"
#include "resident/kernel/native_task_work_catalog.hpp"
#include "resident/domains/actor_foundation/authored_origin.hpp"
#include "resident/kernel/original_calendar.hpp"
#include <algorithm>
#include <functional>
namespace hanmo::resident {
// CPP 内部接线声明；不改变共享上下文、公开旧签名或任何持久布局。
Outcome<std::optional<SubjectAdmissionRead2>>
read_native_director_actor_admission2(
    const CommonStore &, const DirectorState &, const DirectorCatalog &,
    const OrganizationRead &, const TaskState &, const ActivityRead &,
    const StableId &, const DirectorConversationResponse &,
    const std::function<Outcome<std::optional<Integer>>(const StableId &)> &);
namespace {
// 只反验已有 authored 声明年龄，不推造生日；未知动态来源继续原拒绝。
Outcome<std::optional<Integer>>
read_original_authored_director_age(const CharacterConditionSources &body,
                                    const StableId &actor) {
  const auto *definition = body.catalog.actors.find(actor);
  const auto *basis =
      body.state.actor_start_context.actor_age_bases.find(actor);
  const auto *origin = body.state.character_origin_records.origins.find(actor);
  if (!definition && !basis &&
      (!origin ||
       origin->origin_kind != CharacterOriginRecordV2::Kind::authored))
    return {true, std::nullopt, {}};
  auto original = read_authored_character_origin_v2(
      body.common.state(), body.state, body.catalog, actor);
  if (!original.ok)
    return {false, {}, std::move(original.error)};
  if (!definition || !basis || basis->actor_id != actor ||
      !actor_foundation_origin_equal(basis->scenario_origin_ref,
                                     definition->scenario_origin_ref))
    return {false,
            {},
            common_failure("DirectorAuthoredAgeBasisMismatch", actor,
                           ErrorKind::internal)
                .error};
  const auto &age = basis->age_basis;
  const auto &expected = definition->age_basis;
  if (age.kind != AuthoredActorAgeBasis::Kind::declared_age_at_start ||
      age.kind != expected.kind || age.birth_tick != expected.birth_tick ||
      age.birth_source_ref != expected.birth_source_ref ||
      age.years != expected.years || age.as_of_tick != expected.as_of_tick ||
      age.calendar_year_ordinal != expected.calendar_year_ordinal ||
      !actor_foundation_origin_equal(age.source, expected.source))
    return {false,
            {},
            common_failure("DirectorAuthoredAgeBasisMismatch", actor,
                           ErrorKind::internal)
                .error};
  // 与原 CharacterFamily 无年检模块时同一已登记固定历法；不另算 years/720。
  auto calendar = OriginalCalendarRead::admit(
      body.catalog.identity,
      {original_calendar_rule_id, 1, body.catalog.identity.content_digest});
  if (!calendar.ok)
    return {false, {}, std::move(calendar.error)};
  auto years = calendar.value.declared_age_at(
      age.years, age.as_of_tick, age.calendar_year_ordinal, body.actual_now);
  if (!years.ok)
    return {false, {}, std::move(years.error)};
  return {true, std::optional<Integer>{years.value}, {}};
}

template <class T>
Outcome<T> fail(const char *c, const StableId &id = {},
                ErrorKind kind = ErrorKind::rule) {
  return {false, {}, common_failure(c, id, kind).error};
}
bool same(const SourceRef &a, const SourceRef &b) {
  return a.domain == b.domain && a.kind == b.kind && a.id == b.id;
}
bool work_kind(const StableId &k) {
  return k == "work_task_offered" || k == "work_task_accepted" ||
         k == "work_accepted" || k == "work_period_receipt" ||
         k == "work_actual_start" || k == "work_leader_changed" ||
         k == "patrol_work_result" || k == "supervised_work_result" ||
         supervision_source_kind(k);
}
} // namespace
bool NativeDirectorSubjectOwnerRead::bound_to(
    const CommonStore &c, const DirectorState &d) const noexcept {
  return &x_.actors.common == &c && &x_.actors.state == &d;
}
Status NativeDirectorSubjectOwnerRead::binding() const {
  const auto &a = x_.actors;
  if (a.family_post)
    return common_failure("NOT_IMPLEMENTED", "director_family_autonomy",
                          ErrorKind::unsupported_capability);
  if (!a.state.subject2 || !a.catalog.subjects2 ||
      !x_.actor_effects.bound_to(a.common) ||
      &x_.authorities.common != &a.common || &x_.conditions.common != &a.common)
    return common_failure("DirectorOriginalOwnerBindingRequired", {},
                          ErrorKind::internal);
  // 外层原生租约反验本身体目录确属同一 Prepared owner；不从表非空推断 Work
  // 选择。
  auto selected =
      selected_native_task_work_version(x_.conditions.catalog.identity);
  if (!selected.ok)
    return {false, {}, selected.error};
  if (bool(selected.value) != bool(x_.work) ||
      bool(selected.value) != bool(a.tasks.work_execution) ||
      (a.tasks.work_execution &&
       a.tasks.work_execution->version != selected.value))
    return common_failure("DirectorOriginalWorkSelectionMismatch", {},
                          ErrorKind::unsupported_capability);
  if (x_.work && (&x_.work->common != &a.common || &x_.work->tasks != &a.tasks))
    return common_failure("DirectorOriginalOwnerBindingRequired", {},
                          ErrorKind::internal);
  if (!x_.work) {
    auto valid = validate_task_work_original_fact_headers(a.common, nullptr);
    if (!valid.ok)
      return valid;
    valid = validate_task_work_original_action_headers(a.common, nullptr);
    if (!valid.ok)
      return valid;
  }
  auto now = x_.clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  for (const auto *p : {&x_.authorities.actual, &x_.conditions.actual_now}) {
    auto same_at = compare_world_moments(*p, now.value);
    if (!same_at.ok)
      return {false, {}, same_at.error};
    if (same_at.value)
      return common_failure("StaleDirectorOriginalOwnerScope", {},
                            ErrorKind::internal);
  }
  if (x_.work) {
    auto same_at = compare_world_moments(x_.work->actual, now.value);
    if (!same_at.ok)
      return {false, {}, same_at.error};
    if (same_at.value)
      return common_failure("StaleDirectorOriginalOwnerScope", {},
                            ErrorKind::internal);
  }
  if (a.catalog.personal_appointment_attendance) {
    if (!a.appointment_attendance)
      return common_failure("DirectorOriginalAppointmentWakeSourcesRequired",
                            {}, ErrorKind::unsupported_capability);
    const auto &r = *a.appointment_attendance;
    if (&r.common != &a.common || &r.interaction != &x_.interaction ||
        (r.written_visits && &r.written_visits->tasks != &a.tasks))
      return common_failure("DirectorAppointmentCandidateMismatch", {},
                            ErrorKind::internal);
    auto valid = validate_director_appointment_wake_sources2(
        {r, *a.catalog.personal_appointment_attendance, a.state, a.catalog,
         now.value});
    if (!valid.ok)
      return valid;
  } else if (a.appointment_attendance)
    return common_failure("UnselectedDirectorAppointmentOwner", {},
                          ErrorKind::internal);
  for (const auto &row : a.catalog.admissions.rows()) {
    if (row.second.kind == DirectorAdmissionKind::household_cohorts &&
        (!x_.cohorts || !x_.cohort_purchase ||
         !x_.cohorts->bound_to(a.common, a.state)))
      return common_failure("DirectorOriginalCohortOwnerRequired", row.first,
                            ErrorKind::unsupported_capability);
    if (row.second.kind == DirectorAdmissionKind::approved_organizations &&
        (!x_.institutions || !x_.institutions->bound_to(a.common, a.state)))
      return common_failure("DirectorOriginalInstitutionalOwnerRequired",
                            row.first, ErrorKind::unsupported_capability);
  }
  return common_success();
}
Outcome<std::vector<SubjectAdmissionRead2>>
NativeDirectorSubjectOwnerRead::selected_admissions() const {
  auto valid = binding();
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto &a = x_.actors;
  std::vector<SubjectAdmissionRead2> out;
  auto append = [&](const SubjectAdmissionRead2 &r) -> Status {
    if (out.size() >= 4096)
      return common_failure("DirectorSubjectLimit2");
    for (const auto &v : out)
      if (same_social_subject(v.registration.subject, r.registration.subject))
        return common_failure("DuplicateDirectorOriginalAdmission",
                              r.registration.subject.id);
    out.push_back(r);
    return common_success();
  };
  for (const auto &row : a.common.state().actors.rows()) {
    auto original = read_native_director_actor_admission2(
        a.common, a.state, a.catalog, a.organization, a.tasks, x_.activity,
        row.first, x_.conversation_response, [&](const StableId &actor) {
          return read_original_authored_director_age(x_.conditions, actor);
        });
    if (!original.ok)
      return {false, {}, original.error};
    if (original.value) {
      valid = append(*original.value);
      if (!valid.ok)
        return {false, {}, valid.error};
    }
  }
  bool cohort = false, institution = false;
  for (const auto &row : a.catalog.admissions.rows()) {
    cohort =
        cohort || row.second.kind == DirectorAdmissionKind::household_cohorts;
    institution =
        institution ||
        row.second.kind == DirectorAdmissionKind::approved_organizations;
  }
  if (cohort) {
    valid = x_.cohorts->visit_admissions(append);
    if (!valid.ok)
      return {false, {}, valid.error};
  }
  if (institution) {
    valid = x_.institutions->visit_admissions(append);
    if (!valid.ok)
      return {false, {}, valid.error};
  }
  return {true, std::move(out), {}};
}
Outcome<SubjectAdmissionRead2>
NativeDirectorSubjectOwnerRead::current_admission(const SubjectRef &s) const {
  auto valid = binding();
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto &a = x_.actors;
  if (s.kind == SubjectKind::actor) {
    auto original = read_native_director_actor_admission2(
        a.common, a.state, a.catalog, a.organization, a.tasks, x_.activity,
        s.id, x_.conversation_response, [&](const StableId &actor) {
          return read_original_authored_director_age(x_.conditions, actor);
        });
    if (!original.ok)
      return {false, {}, original.error};
    return original.value
               ? Outcome<SubjectAdmissionRead2>{true,
                                                std::move(*original.value),
                                                {}}
               : fail<SubjectAdmissionRead2>("DirectorOriginalAdmissionMissing",
                                             s.id);
  }
  if (s.kind == SubjectKind::cohort) {
    if (!x_.cohorts)
      return fail<SubjectAdmissionRead2>("DirectorOriginalCohortOwnerRequired",
                                         s.id,
                                         ErrorKind::unsupported_capability);
    auto original = x_.cohorts->admission(s);
    if (!original.ok)
      return {false, {}, original.error};
    return original.value
               ? Outcome<SubjectAdmissionRead2>{true,
                                                std::move(*original.value),
                                                {}}
               : fail<SubjectAdmissionRead2>("DirectorOriginalAdmissionMissing",
                                             s.id);
  }
  if (s.kind == SubjectKind::organization && x_.institutions)
    return x_.institutions->admission(s);
  return fail<SubjectAdmissionRead2>(
      "DirectorOriginalInstitutionalOwnerRequired", s.id,
      ErrorKind::unsupported_capability);
}
Outcome<DirectorKnownSubject2>
NativeDirectorSubjectOwnerRead::known_subject(const SubjectRef &s) const {
  auto valid = binding();
  if (!valid.ok)
    return {false, {}, valid.error};
  if (s.kind == SubjectKind::actor) {
    auto known = read_known_director_actor2(x_.actors.common, x_.social, s.id,
                                            x_.interaction, x_.results,
                                            x_.stamp, x_.clock);
    return known.ok
               ? Outcome<DirectorKnownSubject2>{true,
                                                DirectorKnownSubject2{
                                                    std::move(known.value)},
                                                {}}
               : Outcome<DirectorKnownSubject2>{false, {}, known.error};
  }
  if (s.kind == SubjectKind::cohort && x_.cohorts) {
    auto known = x_.cohorts->known(s);
    return known.ok
               ? Outcome<DirectorKnownSubject2>{true,
                                                DirectorKnownSubject2{
                                                    std::move(known.value)},
                                                {}}
               : Outcome<DirectorKnownSubject2>{false, {}, known.error};
  }
  if (s.kind == SubjectKind::organization && x_.institutions) {
    auto known = x_.institutions->known(s);
    return known.ok
               ? Outcome<DirectorKnownSubject2>{true,
                                                DirectorKnownSubject2{
                                                    std::move(known.value)},
                                                {}}
               : Outcome<DirectorKnownSubject2>{false, {}, known.error};
  }
  return fail<DirectorKnownSubject2>("DirectorOriginalKnownSubjectRequired",
                                     s.id);
}
Outcome<std::optional<SubjectDecisionProposal2>>
NativeDirectorSubjectOwnerRead::propose_actor(const DirectorRegistration2 &r,
                                              const KnownSubjectView &k,
                                              Integer n) const {
  auto valid = binding();
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto *p = x_.actors.catalog.policies.find(r.policy_id);
  if (!p)
    return fail<std::optional<SubjectDecisionProposal2>>(
        "UnknownDirectorPolicy", r.policy_id);
  for (auto t : p->templates)
    if (static_cast<Integer>(t) >
        static_cast<Integer>(DirectorTemplate::office_resignation_response))
      return fail<std::optional<SubjectDecisionProposal2>>(
          "NOT_IMPLEMENTED", director_template_id(t),
          ErrorKind::unsupported_capability);
  return propose_native_director_actor2(x_.actors, r, k, n);
}
Outcome<std::optional<SubjectDecisionProposal2>>
NativeDirectorSubjectOwnerRead::propose_cohort_purchase(
    const DirectorRegistration2 &r, const CohortKnownSubjectView &k,
    Integer n) const {
  auto valid = binding();
  if (!valid.ok)
    return {false, {}, valid.error};
  return x_.cohort_purchase
             ? x_.cohort_purchase->propose(r, k, n)
             : fail<std::optional<SubjectDecisionProposal2>>(
                   "DirectorOriginalCohortOwnerRequired", r.subject.id);
}
Outcome<std::optional<SubjectDecisionProposal2>>
NativeDirectorSubjectOwnerRead::propose_cohort_household_routine(
    const DirectorRegistration2 &r, const CohortKnownSubjectView &,
    Integer) const {
  return fail<std::optional<SubjectDecisionProposal2>>(
      "DirectorOriginalHouseholdRoutineRequired", r.subject.id);
}
Outcome<std::optional<SubjectDecisionProposal2>>
NativeDirectorSubjectOwnerRead::propose_institutional_continuation(
    const DirectorRegistration2 &r, const OrganizationKnownSubjectView &k,
    Integer n) const {
  auto valid = binding();
  if (!valid.ok)
    return {false, {}, valid.error};
  return x_.institutions
             ? x_.institutions->propose(
                   r, k, DirectorDecisionRoute2::institutional_continuation, n)
             : fail<std::optional<SubjectDecisionProposal2>>(
                   "DirectorOriginalInstitutionalOwnerRequired", r.subject.id);
}
Outcome<std::optional<SubjectDecisionProposal2>>
NativeDirectorSubjectOwnerRead::propose_await_officer(
    const DirectorRegistration2 &r, const OrganizationKnownSubjectView &k,
    Integer n) const {
  auto valid = binding();
  if (!valid.ok)
    return {false, {}, valid.error};
  return x_.institutions
             ? x_.institutions->propose(
                   r, k, DirectorDecisionRoute2::await_officer, n)
             : fail<std::optional<SubjectDecisionProposal2>>(
                   "DirectorOriginalInstitutionalOwnerRequired", r.subject.id);
}
Status NativeDirectorSubjectOwnerRead::validate_plan_binding(
    const SubjectPlanBinding &b) const {
  auto valid = binding();
  if (!valid.ok)
    return valid;
  if (const auto *actor = std::get_if<ActorExecutionBinding>(&b.binding)) {
    if (b.subject.kind != SubjectKind::actor || b.continuation_ref)
      return common_failure("DirectorOriginalActorBindingMismatch", b.plan_id);
    valid = x_.actor_effects.validate_actor_command_effect(
        b.subject.id, actor->original_business_ref);
    if (!valid.ok)
      return valid;
    return actor->execution_ref
               ? x_.actor_effects.validate_actor_command_effect(
                     b.subject.id, *actor->execution_ref)
               : common_success();
  }
  if (std::holds_alternative<CohortPurchaseBinding>(b.binding))
    return x_.cohort_purchase
               ? x_.cohort_purchase->validate_binding(b)
               : common_failure("DirectorOriginalCohortOwnerRequired",
                                b.plan_id);
  if (b.subject.kind == SubjectKind::organization && x_.institutions)
    return x_.institutions->validate_binding(b);
  return common_failure("DirectorOriginalBindingProviderRequired", b.plan_id);
}
Status
NativeDirectorSubjectOwnerRead::validate_plan(const FinitePlan &p) const {
  auto valid = binding();
  if (!valid.ok)
    return valid;
  const auto *policy = x_.actors.catalog.policies.find(p.policy_id);
  if (!policy)
    return common_failure("UnknownDirectorPolicy", p.policy_id);
  if (p.subject.kind == SubjectKind::actor &&
      p.template_id == "life_growth_care_v1")
    return common_failure("NOT_IMPLEMENTED", p.id,
                          ErrorKind::unsupported_capability);
  if (p.subject.kind == SubjectKind::actor) {
    valid = validate_director_actor_plan_fields(
        x_.actors.common, x_.actors.catalog, *policy, p,
        p.phase == PlanPhase::completed ? nullptr : x_.actors.life);
    if (!valid.ok)
      return valid;
    if (x_.actors.catalog.personal_appointment_attendance) {
      auto now = x_.clock.read_current_moment();
      if (!now.ok)
        return {false, {}, now.error};
      return validate_director_appointment_plan2(
          {*x_.actors.appointment_attendance,
           *x_.actors.catalog.personal_appointment_attendance, x_.actors.state,
           x_.actors.catalog, now.value},
          p);
    }
    return common_success();
  }
  if (p.subject.kind == SubjectKind::organization)
    return x_.institutions
               ? x_.institutions->validate_plan(p)
               : common_failure("DirectorOriginalInstitutionalOwnerRequired",
                                p.id);
  if (p.subject.kind != SubjectKind::cohort ||
      p.template_id != "economic_household/1" || p.step_index < 0 ||
      p.step_index > 4)
    return common_failure("DirectorOriginalCohortPlanRequired", p.id);
  const auto *b = x_.actors.state.subject2->subject_plan_bindings.find(p.id);
  if (!b)
    return p.phase == PlanPhase::waiting && p.step_index == 0 &&
                   p.commitment_ids.empty()
               ? common_success()
               : common_failure("DirectorCohortPlanBindingMissing", p.id);
  valid = validate_plan_binding(*b);
  if (!valid.ok)
    return valid;
  const auto *purchase = std::get_if<CohortPurchaseBinding>(&b->binding);
  if (!purchase ||
      (!p.goal_ref.empty() && p.goal_ref != purchase->purchase_plan_ref.id))
    return common_failure("DirectorCohortPlanGoalMismatch", p.id);
  for (const auto &id : p.commitment_ids)
    if (id != purchase->purchase_plan_ref.id)
      return common_failure("DirectorCohortPlanCommitmentMismatch", p.id);
  return common_success();
}
Outcome<const StableId *>
NativeDirectorSubjectOwnerRead::read_family_applied_goal5(
    const SubjectDecisionProposal2 &p,
    const SubjectDecisionApplied2 &applied) const {
  auto valid = binding();
  if (!valid.ok)
    return {false, nullptr, valid.error};
  (void)applied;
  return {false, nullptr,
          common_failure("NOT_IMPLEMENTED", p.plan_id,
                         ErrorKind::unsupported_capability)
              .error};
}
Status NativeDirectorSubjectOwnerRead::validate_applied_effect(
    const SubjectRef &s, DirectorDecisionRoute2 route,
    const SourceRef &source) const {
  auto valid = binding();
  if (!valid.ok)
    return valid;
  if (route == DirectorDecisionRoute2::actor && s.kind == SubjectKind::actor)
    return x_.actor_effects.validate_actor_command_effect(s.id, source);
  if (route == DirectorDecisionRoute2::cohort_purchase &&
      s.kind == SubjectKind::cohort && x_.cohort_purchase)
    return x_.cohort_purchase->validate_effect(s, source);
  if (s.kind == SubjectKind::organization && x_.institutions)
    return x_.institutions->validate_effect(s, route, source);
  return common_failure("DirectorOriginalAppliedEffectProviderRequired",
                        source.id);
}
Status NativeDirectorSubjectOwnerRead::validate_continuation(
    const SubjectContinuation &c) const {
  auto valid = binding();
  if (!valid.ok)
    return valid;
  return c.prior_subject.kind == SubjectKind::organization &&
                 c.current_subject.kind == SubjectKind::organization &&
                 x_.institutions
             ? x_.institutions->validate_continuation(c)
             : common_failure("DirectorOriginalContinuationMappingRequired",
                              c.original_continuation_ref.id);
}
Status
NativeDirectorSubjectOwnerRead::validate_wake(const SubjectWake2 &w) const {
  auto valid = binding();
  if (!valid.ok)
    return valid;
  auto now = x_.clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  if (director_appointment_wake_domain2(w.actual_source) &&
      x_.actors.catalog.personal_appointment_attendance)
    return validate_director_appointment_wake2(
        {*x_.actors.appointment_attendance,
         *x_.actors.catalog.personal_appointment_attendance, x_.actors.state,
         x_.actors.catalog, now.value},
        w);
  if (w.actual_source.domain == "social")
    return validate_director_known_report_wake2(
        x_.actors.common, x_.social, x_.results, x_.stamp, w, now.value);
  if (w.actual_source.domain == "tasks") {
    if (work_kind(w.actual_source.kind)) {
      if (!x_.work)
        return common_failure("DirectorOriginalWorkWakeOwnerRequired",
                              w.actual_source.id,
                              ErrorKind::unsupported_capability);
      return validate_director_work_wake2(*x_.work, w);
    }
    return validate_director_task_commitment_wake2(
        x_.actors.common, x_.actors.tasks, x_.actors.affairs, w, now.value);
  }
  if (w.actual_source.domain == "organization")
    return validate_director_authority_wake2(x_.authorities, w);
  if (w.actual_source.domain == "actor_foundation")
    return validate_director_capability_wake2(x_.conditions, w);
  if (w.subject.kind == SubjectKind::organization && x_.institutions)
    return x_.institutions->validate_wake(w);
  return common_failure("DirectorOriginalWakeProviderRequired",
                        w.actual_source.id);
}
Status NativeDirectorSubjectOwnerRead::visit_actual_wakes(
    const DirectorOriginalWakeVisitor2 &visit) const {
  auto valid = binding();
  if (!valid.ok)
    return valid;
  if (!visit)
    return common_failure("DirectorOriginalWakeVisitorRequired");
  auto now = x_.clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  const auto &a = x_.actors;
  auto selected = [&](const SubjectWake2 &w, const WorldMoment &at) -> Status {
    const auto *r =
        a.state.subject2->registrations.find(director_subject_key2(w.subject));
    return !r ? common_success() : visit(w, at);
  };
  if (!x_.social.dynamic || !x_.social.dynamic->time2)
    return common_failure("ExactSocialStateRequired");
  for (const auto &owner : x_.social.dynamic->known_reports.rows())
    for (const auto &row : owner.second.rows()) {
      const auto &known = row.second;
      if (!a.state.subject2->registrations.find(
              director_subject_key2(known.receiver)))
        continue;
      auto wake = read_director_known_report_wake2(
          a.common, x_.social, x_.results, x_.stamp,
          {known.receiver, known.report_id}, now.value);
      if (!wake.ok)
        return {false, {}, wake.error};
      auto receipt = read_knowledge_receipt_view_v2(a.common, x_.social,
                                                    x_.results, known.receiver,
                                                    known.report_id, x_.stamp);
      if (!receipt.ok)
        return {false, {}, receipt.error};
      if (!receipt.value)
        return common_failure("DirectorKnowledgeReceiptRequired",
                              known.report_id);
      valid = selected(wake.value, receipt.value->learned_at);
      if (!valid.ok)
        return valid;
    }
  valid = visit_task_commitment_wakes2(
      a.common, a.tasks, a.affairs, now.value, [&](const SubjectWake2 &w) {
        auto at = read_task_result_source_moment(
            a.common, a.tasks, a.affairs, {w.actual_source, w.source_schema});
        if (!at.ok)
          return Status{false, {}, at.error};
        return selected(w, at.value);
      });
  if (!valid.ok)
    return valid;
  if (a.catalog.personal_appointment_attendance) {
    valid = visit_director_appointment_wakes2(
        {*a.appointment_attendance, *a.catalog.personal_appointment_attendance,
         a.state, a.catalog, now.value},
        selected);
    if (!valid.ok)
      return valid;
  }
  if (x_.work) {
    valid = visit_director_work_wakes2(*x_.work, selected);
    if (!valid.ok)
      return valid;
  }
  valid = visit_director_authority_wakes2(x_.authorities, selected);
  if (!valid.ok)
    return valid;
  valid = visit_director_capability_wakes2(x_.conditions, selected);
  if (!valid.ok)
    return valid;
  return x_.institutions ? x_.institutions->visit_wakes(selected)
                         : common_success();
}
} // namespace hanmo::resident
