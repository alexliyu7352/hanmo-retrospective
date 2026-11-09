#include "director_subject2.hpp"
#include "director_actor_proposal2.hpp"
#include "director_actor_read.hpp"
#include "director_authority_wake2.hpp"
#include "director_command_acceptance.hpp"
#include "director_knowledge_wake2.hpp"
#include "director_native_wakes2.hpp"
#include "director_task_wake2.hpp"
#include "resident/domains/social/propagation.hpp"
#include "resident/kernel/command_types.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace hanmo::resident {
namespace {
bool same(const SubjectRef &a, const SubjectRef &b) {
  return a.kind == b.kind && a.id == b.id;
}
bool same(const SourceRef &a, const SourceRef &b) {
  return a.domain == b.domain && a.kind == b.kind && a.id == b.id;
}
bool present(const SourceRef &s) {
  return !s.domain.empty() && !s.kind.empty() && !s.id.empty();
}
Status fail(const StableId &code, const StableId &id = {}) {
  return common_failure(code, id);
}
bool route_matches(SubjectKind kind, DirectorDecisionRoute2 route) {
  switch (kind) {
  case SubjectKind::actor:
    return route == DirectorDecisionRoute2::actor;
  case SubjectKind::cohort:
    return route == DirectorDecisionRoute2::cohort_purchase ||
           route == DirectorDecisionRoute2::cohort_household_routine;
  case SubjectKind::organization:
    return route == DirectorDecisionRoute2::institutional_continuation ||
           route == DirectorDecisionRoute2::await_officer;
  }
  return false;
}
bool admission_equal(const DirectorAdmissionSource2 &a,
                     const DirectorAdmissionSource2 &b) {
  if (a.index() != b.index())
    return false;
  return std::visit(
      [&](const auto &x) {
        using T = std::decay_t<decltype(x)>;
        const auto &y = std::get<T>(b);
        if constexpr (std::is_same_v<T, ActorDirectorAdmission>)
          return x.actor_id == y.actor_id && x.admission_id == y.admission_id;
        else if constexpr (std::is_same_v<T, CohortDirectorAdmission>)
          return x.source_revision == y.source_revision &&
                 same(x.accepted_household_policy_ref,
                      y.accepted_household_policy_ref);
        else
          return x.source_revision == y.source_revision &&
                 same(x.approved_responsibility_ref,
                      y.approved_responsibility_ref);
      },
      a);
}
Status shape(const DirectorRegistration2 &r, const DirectorCatalog &catalog) {
  if (!catalog.subjects2 || !catalog.dynamic_subjects)
    return fail("DirectorSubjectsProfileRequired");
  const auto *authored = catalog.admissions.find(r.admission_id);
  if (!authored || authored->policy_id != r.policy_id ||
      (r.subject.kind == SubjectKind::actor &&
       (authored->kind != DirectorAdmissionKind::all_actors &&
        authored->kind != DirectorAdmissionKind::organization_members)) ||
      (r.subject.kind == SubjectKind::cohort &&
       authored->kind != DirectorAdmissionKind::household_cohorts) ||
      (r.subject.kind == SubjectKind::organization &&
       authored->kind != DirectorAdmissionKind::approved_organizations))
    return fail("DirectorAdmissionKindMismatch2", r.subject.id);
  if (r.subject.id.empty() || r.policy_id.empty() || r.admission_id.empty() ||
      !catalog.policies.find(r.policy_id) || r.epoch < 1 || r.reason.empty() ||
      r.entered_tick < 0 || r.changed_tick < r.entered_tick ||
      static_cast<Integer>(r.lifecycle) < 0 ||
      static_cast<Integer>(r.lifecycle) > 2)
    return fail("InvalidDirectorRegistration2", r.subject.id);
  switch (r.subject.kind) {
  case SubjectKind::actor: {
    const auto *p = std::get_if<ActorDirectorAdmission>(&r.admission_source);
    if (!p || p->actor_id != r.subject.id || p->admission_id != r.admission_id)
      return fail("InvalidActorAdmission2", r.subject.id);
    break;
  }
  case SubjectKind::cohort: {
    const auto *p = std::get_if<CohortDirectorAdmission>(&r.admission_source);
    if (!p || !present(p->accepted_household_policy_ref) ||
        p->source_revision < 0)
      return fail("InvalidCohortAdmission2", r.subject.id);
    break;
  }
  case SubjectKind::organization: {
    const auto *p =
        std::get_if<OrganizationDirectorAdmission>(&r.admission_source);
    if (!p || !present(p->approved_responsibility_ref) ||
        p->source_revision < 0)
      return fail("InvalidOrganizationAdmission2", r.subject.id);
    break;
  }
  default:
    return fail("InvalidDirectorSubjectKind2", r.subject.id);
  }
  return common_success();
}
Status proposal_shape(const SubjectDecisionProposal2 &p) {
  if (p.route == DirectorDecisionRoute2::actor &&
      p.template_id == "life_growth_care_v1")
    return common_failure("NOT_IMPLEMENTED", p.plan_id,
                          ErrorKind::unsupported_capability);
  if (p.subject.id.empty() || p.registration_epoch < 1 || p.decision_seq < 1 ||
      p.policy_id.empty() || p.plan_id.empty() || p.template_id.empty() ||
      p.next_step_index < 0 || !route_matches(p.subject.kind, p.route) ||
      static_cast<Integer>(p.phase) < 0 ||
      static_cast<Integer>(p.phase) >
          static_cast<Integer>(PlanPhase::completed))
    return fail("InvalidDirectorProposal2", p.subject.id);
  if (p.route == DirectorDecisionRoute2::actor) {
    auto original = validate_director_actor_proposal2(p);
    if (!original.ok)
      return original;
  } else if (p.actor_proposal)
    return fail("NonActorCommandForbidden", p.subject.id);
  if (!present(p.policy_source_ref) || p.owner_source_revision < 0)
    return fail("DirectorProposalPolicySourceRequired", p.subject.id);
  if (p.cohort_purchase_quote &&
      p.route != DirectorDecisionRoute2::cohort_purchase)
    return fail("DirectorUnexpectedPurchaseQuote", p.subject.id);
  if (p.binding && p.binding->index() != static_cast<std::size_t>(p.route))
    return fail("DirectorRouteBindingMismatch", p.subject.id);
  if (!p.wait_reason.empty() && p.phase != PlanPhase::waiting)
    return fail("InvalidDirectorWaitingPhase", p.subject.id);
  return common_success();
}
Status validate_plan_shape2(const FinitePlan &p, const DirectorCatalog &catalog,
                            const DirectorSubjectOwnerRead &read) {
  const auto *policy = catalog.policies.find(p.policy_id);
  if (!policy || p.id.empty() || p.subject.id.empty() || p.step_index < 0 ||
      p.next_review_tick < 0)
    return fail("InvalidDirectorPlan2", p.id);
  bool selected = false;
  for (auto kind : policy->templates) {
    const auto *name = director_template_id(kind);
    if (name && p.template_id == name)
      selected = true;
  }
  if (!selected)
    return fail("DirectorPlanTemplateNotSelected2", p.id);
  if (p.subject.kind != SubjectKind::actor &&
      (p.current_action_id || p.pending_life_action_id || p.last_life_fact_id ||
       p.livelihood))
    return fail("NonActorPersonalActionForbidden", p.id);
  return read.validate_plan(p);
}
SourceRef policy_source(const DirectorRegistration2 &r) {
  return std::visit(
      [&](const auto &x) -> SourceRef {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, ActorDirectorAdmission>)
          return {"director", "actor_admission", x.admission_id};
        else if constexpr (std::is_same_v<T, CohortDirectorAdmission>)
          return x.accepted_household_policy_ref;
        else
          return x.approved_responsibility_ref;
      },
      r.admission_source);
}
Integer admission_revision(const DirectorRegistration2 &r) {
  return std::visit(
      [](const auto &x) -> Integer {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, ActorDirectorAdmission>)
          return 0;
        else
          return x.source_revision;
      },
      r.admission_source);
}
SubjectPlanBinding plan_binding(const SubjectDecisionProposal2 &p,
                                const SubjectExecutionBinding &b) {
  return {p.plan_id, p.subject,           p.registration_epoch,
          b,         p.policy_source_ref, p.owner_source_revision,
          {}};
}
StableId wake_key(const SubjectWake2 &w) {
  auto part = [](const StableId &v) {
    return std::to_string(v.size()) + ":" + v;
  };
  return director_subject_key2(w.subject) + ":" +
         std::to_string(static_cast<Integer>(w.kind)) + ":" +
         std::to_string(w.source_schema) + ":" +
         std::to_string(w.source_revision) + ":" +
         part(w.actual_source.domain) + part(w.actual_source.kind) +
         part(w.actual_source.id);
}
void pin(std::vector<SourceRef> &out, const SourceRef &s) {
  if (present(s) && std::none_of(out.begin(), out.end(),
                                 [&](const auto &x) { return same(x, s); }))
    out.push_back(s);
}
void pins(std::vector<SourceRef> &out, const SubjectExecutionBinding &b) {
  std::visit(
      [&](const auto &x) {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, ActorExecutionBinding>)
          pin(out, x.original_business_ref);
        else if constexpr (std::is_same_v<T, CohortPurchaseBinding>) {
          pin(out, x.purchase_plan_ref);
          if (x.execution_ref)
            pin(out, *x.execution_ref);
        } else if constexpr (std::is_same_v<T, CohortHouseholdRoutineBinding>) {
          pin(out, x.need_source_ref);
          if (x.execution_ref)
            pin(out, *x.execution_ref);
        } else if constexpr (std::is_same_v<T, InstitutionalExecutionBinding>) {
          pin(out, x.approved_responsibility_ref);
          if (x.execution_ref)
            pin(out, *x.execution_ref);
        } else
          pin(out, x.approved_goal_ref);
      },
      b);
}
} // namespace
StableId director_subject_key2(const SubjectRef &s) {
  return std::to_string(static_cast<Integer>(s.kind)) + ":" +
         std::to_string(s.id.size()) + ":" + s.id;
}
StableId director_adoption_key2(const SubjectRef &subject, Integer epoch,
                                Integer sequence) {
  return director_subject_key2(subject) + ":" + std::to_string(epoch) + ":" +
         std::to_string(sequence);
}
const FinitePlan *find_director_plan2(const DirectorState &s,
                                      const StableId &id) {
  if (const auto *p = s.plans.find(id))
    return p;
  return s.archived_plans.find(id);
}
Status reconcile_director_subjects2(const CommonStore &c, DirectorState &s,
                                    const DirectorCatalog &catalog,
                                    const DirectorSubjectOwnerRead &read,
                                    const WorldMoment &actual) {
  auto actual_shape = compare_world_moments(actual, actual);
  if (!actual_shape.ok)
    return {false, {}, actual_shape.error};
  if (!s.subject2)
    return fail("DirectorSubject2NotSelected");
  if (s.subject2->command_binding_version !=
      (catalog.command_acceptance_bindings ? 1 : 0))
    return fail("DirectorCommandPhysicalSelectionMismatch");
  if (catalog.decision_budget < 1 || catalog.decision_budget > 64 ||
      catalog.candidates_per_subject < 1 ||
      catalog.candidates_per_subject > 256 ||
      catalog.review_interval_ticks < 1 || catalog.retry_delay_ticks < 1)
    return fail("InvalidDirectorBudget2");
  auto admissions = read.selected_admissions();
  if (!admissions.ok)
    return {false, {}, admissions.error};
  if (admissions.value.size() > 4096)
    return fail("DirectorSubjectLimit2");
  std::set<StableId> seen;
  // Validate every selected original source before changing any registration.
  // The enclosing world candidate owns all rollback.
  for (const auto &a : admissions.value) {
    auto r = shape(a.registration, catalog);
    if (!r.ok)
      return r;
    const auto key = director_subject_key2(a.registration.subject);
    if (!seen.insert(key).second ||
        !route_matches(a.registration.subject.kind, a.route))
      return fail("DuplicateDirectorAdmission2", key);
    if (a.registration.changed_tick > actual.half_day_tick)
      return fail("FutureDirectorAdmission2", key);
  }
  for (const auto &row : s.subject2->registrations.rows())
    if (!seen.count(row.first)) {
      auto current = read.current_admission(row.second.subject);
      if (!current.ok)
        return {false, {}, current.error};
      if (current.value.source_active)
        return fail("IncompleteSelectedDirectorAdmissions2", row.first);
      admissions.value.push_back(std::move(current.value));
    }
  for (const auto &a : admissions.value) {
    const auto key = director_subject_key2(a.registration.subject);
    auto *prior = s.subject2->registrations.find(key);
    DirectorRegistration2 next = a.registration;
    if (prior) {
      next.entered_tick = prior->entered_tick;
      next.epoch = prior->epoch;
      next.changed_tick = prior->changed_tick;
      const bool revoked =
          !a.source_active || prior->policy_id != next.policy_id ||
          !admission_equal(prior->admission_source, next.admission_source);
      if (revoked && a.has_frozen_responsibility) {
        next.policy_id = prior->policy_id;
        next.admission_id = prior->admission_id;
        next.admission_source = prior->admission_source;
        next.obligations_only = true;
        next.reason = "ExistingResponsibilityOnly";
      } else
        next.obligations_only = false;
      if (!a.source_active && !a.has_frozen_responsibility) {
        next.lifecycle = DirectorLifecycle::retired;
        next.reason = "AdmissionWithdrawn";
      }
      const bool changed =
          prior->policy_id != next.policy_id ||
          prior->admission_id != next.admission_id ||
          !admission_equal(prior->admission_source, next.admission_source) ||
          prior->lifecycle != next.lifecycle ||
          prior->obligations_only != next.obligations_only ||
          prior->reason != next.reason;
      if (changed) {
        if (prior->epoch == std::numeric_limits<Integer>::max())
          return fail("DirectorEpochOverflow2", key);
        ++next.epoch;
        next.changed_tick = actual.half_day_tick;
      }
    } else {
      if (!a.source_active)
        return fail("InactiveNewDirectorAdmission2", key);
      next.epoch = 1;
      next.entered_tick = next.changed_tick = actual.half_day_tick;
    }
    // Original Actor admission withdrawal/policy replacement retires only the
    // optional finite plan. Frozen business responsibilities stay under their
    // original policy above; a live original Action cannot be silently
    // archived.
    if (prior && next.subject.kind == SubjectKind::actor &&
        !a.has_frozen_responsibility &&
        (!a.source_active || prior->policy_id != next.policy_id ||
         !admission_equal(prior->admission_source, next.admission_source))) {
      auto current_plan = read_director_actor_plan(s, next.subject.id);
      if (!current_plan.ok)
        return {false, {}, current_plan.error};
      if (const auto *p = current_plan.value) {
        if ((p->current_action_id && c.action(*p->current_action_id)) ||
            p->pending_life_action_id)
          return fail("DirectorPlanStillActive", next.subject.id);
        if (s.archived_plans.find(p->id))
          return fail("DuplicateArchivedDirectorPlan", p->id);
        FinitePlan archived = *p;
        archived.phase = PlanPhase::completed;
        archived.current_action_id.reset();
        archived.pending_life_action_id.reset();
        const auto plan_id = archived.id;
        s.archived_plans.insert(plan_id, std::move(archived));
        s.plans.erase(plan_id);
      }
    }
    if (next.subject.kind == SubjectKind::actor &&
        next.subject.id == c.state().metadata.player_actor_id &&
        next.lifecycle == DirectorLifecycle::active)
      return fail("PlayerDirectorOverlap2", key);
    s.subject2->registrations.set(key, next);
    if (!s.schedules.find(key))
      s.schedules.insert(key, {next.subject,
                               actual.half_day_tick,
                               0,
                               actual.half_day_tick,
                               -1,
                               {},
                               {}});
    for (const auto &row : s.subject2->subject_plan_bindings.rows())
      if (same(row.second.subject, next.subject) &&
          row.second.registration_epoch > next.epoch)
        return fail("DirectorPlanEpochMismatch2", row.first);
  }
  if (!s.subject2->wake_cache_ready)
    return rebuild_director_wake_cache2(s, read, actual);
  return common_success();
}
namespace {
Status record_verified_director_wake2(DirectorState &s, const SubjectWake2 &w,
                                      const WorldMoment &actual) {
  const Integer tick = actual.half_day_tick;
  if (!s.subject2 || !s.subject2->wake_cache_ready || tick < 0 ||
      !present(w.actual_source) || w.source_schema < 1 ||
      w.source_revision < 0 || static_cast<Integer>(w.kind) < 0 ||
      static_cast<Integer>(w.kind) > 4)
    return fail("InvalidDirectorWake2");
  const auto key = director_subject_key2(w.subject);
  const auto *r = s.subject2->registrations.find(key);
  auto *schedule = s.schedules.find(key);
  if (!r || !schedule || !same(schedule->subject, w.subject))
    return fail("UnknownDirectorWakeSubject2", key);
  if (r->lifecycle == DirectorLifecycle::retired)
    return common_success();
  const StableId reason = wake_key(w);
  if (s.subject2->wake_sources.find(reason))
    return common_success();
  auto encoded = compare_world_moments(actual, actual);
  if (!encoded.ok)
    return {false, {}, encoded.error};
  if (schedule->wake_reasons.size() >= 256)
    return fail("DirectorWakeLimit2", key);
  if (!s.subject2->wake_sources.insert(reason, {w, actual, {}}))
    return fail("DuplicateDirectorWakeSource2", reason);
  if (std::find(schedule->wake_reasons.begin(), schedule->wake_reasons.end(),
                reason) == schedule->wake_reasons.end()) {
    if (schedule->wake_reasons.size() >= 256)
      return fail("DirectorWakeLimit2", key);
    schedule->wake_reasons.push_back(std::move(reason));
  }
  if (schedule->last_decision_tick < tick)
    schedule->next_tick = std::min(schedule->next_tick, tick);
  return common_success();
}
} // namespace
Status rebuild_director_wake_cache2(DirectorState &s,
                                    const DirectorSubjectOwnerRead &read,
                                    const WorldMoment &actual) {
  if (!s.subject2)
    return fail("DirectorSubject2NotSelected");
  s.subject2->wake_cache_ready = false;
  s.subject2->wake_sources = OrderedTable<DirectorWakeRecord2>{};
  auto visited = read.visit_actual_wakes([&](const SubjectWake2 &w,
                                             const WorldMoment &at) {
    const auto key = director_subject_key2(w.subject);
    const auto *r = s.subject2->registrations.find(key);
    const auto *schedule = s.schedules.find(key);
    if (!r || !schedule)
      return fail("DirectorWakeSourceRegistrationMissing", key);
    auto past = compare_world_moments(at, actual);
    if (!past.ok)
      return Status{false, {}, past.error};
    if (past.value > 0)
      return fail("DirectorWakeSourceFromFuture", key);
    auto valid = read.validate_wake(w);
    if (!valid.ok)
      return valid;
    const auto id = wake_key(w);
    if (s.subject2->wake_sources.find(id))
      return common_success();
    const bool pending =
        std::find(schedule->wake_reasons.begin(), schedule->wake_reasons.end(),
                  id) != schedule->wake_reasons.end();
    DirectorWakeRecord2 row{
        w, at,
        pending ? std::nullopt
                : std::optional<Integer>{schedule->decision_seq}};
    s.subject2->wake_sources.insert(id, std::move(row));
    return common_success();
  });
  if (!visited.ok)
    return visited;
  for (const auto &row : s.schedules.rows())
    for (const auto &id : row.second.wake_reasons) {
      const auto *source = s.subject2->wake_sources.find(id);
      if (!source || source->consumed_decision_seq ||
          !same(source->wake.subject, row.second.subject))
        return fail("DirectorPendingWakeSourceMissing", id);
    }
  s.subject2->wake_cache_ready = true;
  return common_success();
}
Status wake_director_subject2(DirectorState &s, const SubjectWake2 &w,
                              const WorldMoment &actual,
                              const DirectorSubjectOwnerRead &read) {
  auto valid = read.validate_wake(w);
  if (!valid.ok)
    return valid;
  return record_verified_director_wake2(s, w, actual);
}
Status collect_director_owner_wakes2(DirectorState &s,
                                     const DirectorSubjectOwnerRead &read,
                                     const WorldMoment &actual) {
  if (!s.subject2 || !s.subject2->wake_cache_ready)
    return fail("DirectorWakeCacheRebuildRequired");
  return read.visit_actual_wakes(
      [&](const SubjectWake2 &w, const WorldMoment &at) {
        const auto *r =
            s.subject2->registrations.find(director_subject_key2(w.subject));
        if (!r || r->lifecycle == DirectorLifecycle::retired)
          return common_success();
        auto past = compare_world_moments(at, actual);
        if (!past.ok)
          return Status{false, {}, past.error};
        if (past.value > 0)
          return fail("DirectorWakeSourceFromFuture", w.actual_source.id);
        auto valid = read.validate_wake(w);
        if (!valid.ok)
          return valid;
        return record_verified_director_wake2(s, w, actual);
      });
}
Status collect_director_knowledge_wakes2(const CommonStore &common,
                                         const SocialState &social,
                                         const ExactResultTimeRead &source,
                                         const ExactResultReadStamp &stamp,
                                         DirectorState &s,
                                         const WorldMoment &actual) {
  if (!s.subject2)
    return common_failure("DirectorSubject2NotSelected");
  if (!social.dynamic || !social.dynamic->time2)
    return common_failure("ExactSocialStateRequired");
  for (const auto &owner : social.dynamic->known_reports.rows())
    for (const auto &row : owner.second.rows()) {
      const SocialKnowledgeKey key{row.second.receiver, row.second.report_id};
      const auto *r =
          s.subject2->registrations.find(director_subject_key2(key.receiver));
      if (!r || r->lifecycle == DirectorLifecycle::retired)
        continue;
      auto wake = read_director_known_report_wake2(common, social, source,
                                                   stamp, key, actual);
      if (!wake.ok)
        return {false, {}, wake.error};
      auto wrote = record_verified_director_wake2(s, wake.value, actual);
      if (!wrote.ok)
        return wrote;
    }
  return common_success();
}

Status collect_director_task_commitment_wakes2(const CommonStore &common,
                                               const TaskState &tasks,
                                               const AffairsState &affairs,
                                               DirectorState &s,
                                               const WorldMoment &actual) {
  if (!s.subject2)
    return common_failure("DirectorSubject2NotSelected");
  return visit_task_commitment_wakes2(
      common, tasks, affairs, actual, [&](const SubjectWake2 &wake) {
        const auto *r =
            s.subject2->registrations.find(director_subject_key2(wake.subject));
        return !r || r->lifecycle == DirectorLifecycle::retired
                   ? common_success()
                   : record_verified_director_wake2(s, wake, actual);
      });
}
Status collect_director_authority_wakes2(const DirectorAuthorityWakeRead2 &read,
                                         DirectorState &s) {
  if (!s.subject2)
    return common_failure("DirectorSubject2NotSelected");
  return visit_director_authority_wakes2(
      read, [&](const SubjectWake2 &wake, const WorldMoment &at) {
        const auto *r =
            s.subject2->registrations.find(director_subject_key2(wake.subject));
        return !r || r->lifecycle == DirectorLifecycle::retired
                   ? common_success()
                   : record_verified_director_wake2(s, wake, read.actual);
      });
}
Outcome<std::vector<SubjectRef>>
director_due_subjects2(const DirectorState &s, const DirectorCatalog &catalog,
                       Integer tick) {
  if (!s.subject2 || tick < 0 || s.fairness_cursor < 0 ||
      catalog.decision_budget < 1 || catalog.decision_budget > 64)
    return {false, {}, fail("InvalidDirectorBudget2").error};
  Integer reviewed = 0;
  for (const auto &row : s.schedules.rows())
    if (row.second.last_decision_tick == tick)
      ++reviewed;
  const Integer available =
      std::max<Integer>(0, catalog.decision_budget - reviewed);
  std::vector<SubjectRef> due;
  const auto &order = s.subject2->registrations.order();
  for (const auto &row : s.subject2->registrations.rows()) {
    const auto *schedule = s.schedules.find(row.first);
    if (!schedule)
      return {false, {}, fail("MissingDirectorSchedule2", row.first).error};
    if (row.second.lifecycle == DirectorLifecycle::active &&
        schedule->next_tick <= tick && schedule->last_decision_tick < tick)
      due.push_back(row.second.subject);
  }
  const auto n = std::max<std::size_t>(1, order.size());
  auto rank = [&](const SubjectRef &x) {
    const auto pos =
        std::find(order.begin(), order.end(), director_subject_key2(x));
    return (static_cast<std::size_t>(pos - order.begin()) + n -
            static_cast<std::size_t>(s.fairness_cursor %
                                     static_cast<Integer>(n))) %
           n;
  };
  std::stable_sort(due.begin(), due.end(), [&](const auto &a, const auto &b) {
    const auto *x = s.schedules.find(director_subject_key2(a));
    const auto *y = s.schedules.find(director_subject_key2(b));
    if (x->waiting_since != y->waiting_since)
      return x->waiting_since < y->waiting_since;
    if (x->next_tick != y->next_tick)
      return x->next_tick < y->next_tick;
    return rank(a) < rank(b);
  });
  if (due.size() > static_cast<std::size_t>(available))
    due.resize(static_cast<std::size_t>(available));
  return {true, std::move(due), {}};
}
Outcome<std::optional<SubjectDecisionProposal2>>
propose_director_subject2(const DirectorState &s,
                          const DirectorCatalog &catalog,
                          const SubjectRef &subject, Integer tick,
                          const DirectorSubjectOwnerRead &read) {
  const auto key = director_subject_key2(subject);
  if (!s.subject2)
    return {false, {}, fail("DirectorSubject2NotSelected").error};
  const auto *r = s.subject2->registrations.find(key);
  const auto *schedule = s.schedules.find(key);
  if (!r || !schedule || r->lifecycle != DirectorLifecycle::active ||
      schedule->next_tick > tick || schedule->last_decision_tick >= tick)
    return {true, std::nullopt, {}};
  if (schedule->decision_seq == std::numeric_limits<Integer>::max())
    return {false, {}, fail("DirectorSequenceOverflow2", key).error};
  auto admission = read.current_admission(subject);
  if (!admission.ok)
    return {false, {}, admission.error};
  if (!admission.value.source_active && !r->obligations_only)
    return {false, {}, fail("StaleDirectorAdmission2", key).error};
  auto view = read.known_subject(subject);
  if (!view.ok)
    return {false, {}, view.error};
  Outcome<std::optional<SubjectDecisionProposal2>> result;
  switch (admission.value.route) {
  case DirectorDecisionRoute2::actor: {
    auto *p = std::get_if<KnownSubjectView>(&view.value);
    if (!p || !same(p->subject, subject))
      return {false, {}, fail("DirectorKnownSubjectMismatch2", key).error};
    result = read.propose_actor(*r, *p, catalog.candidates_per_subject);
    break;
  }
  case DirectorDecisionRoute2::cohort_purchase:
  case DirectorDecisionRoute2::cohort_household_routine: {
    auto *p = std::get_if<CohortKnownSubjectView>(&view.value);
    if (!p || !same(p->subject, subject))
      return {false, {}, fail("DirectorKnownSubjectMismatch2", key).error};
    result = admission.value.route == DirectorDecisionRoute2::cohort_purchase
                 ? read.propose_cohort_purchase(*r, *p,
                                                catalog.candidates_per_subject)
                 : read.propose_cohort_household_routine(
                       *r, *p, catalog.candidates_per_subject);
    break;
  }
  case DirectorDecisionRoute2::institutional_continuation:
  case DirectorDecisionRoute2::await_officer: {
    auto *p = std::get_if<OrganizationKnownSubjectView>(&view.value);
    if (!p || !same(p->subject, subject))
      return {false, {}, fail("DirectorKnownSubjectMismatch2", key).error};
    result = admission.value.route ==
                     DirectorDecisionRoute2::institutional_continuation
                 ? read.propose_institutional_continuation(
                       *r, *p, catalog.candidates_per_subject)
                 : read.propose_await_officer(*r, *p,
                                              catalog.candidates_per_subject);
    break;
  }
  default:
    return {false, {}, fail("UnsupportedDirectorRoute2", key).error};
  }
  if (!result.ok)
    return result;
  if (!result.value) {
    SubjectDecisionProposal2 waiting;
    waiting.subject = subject;
    waiting.registration_epoch = r->epoch;
    waiting.decision_seq = schedule->decision_seq + 1;
    waiting.policy_id = r->policy_id;
    waiting.plan_id = "subject2-plan:" + key + ":" + std::to_string(r->epoch);
    waiting.route = admission.value.route;
    waiting.template_id = admission.value.template_id;
    waiting.policy_source_ref = policy_source(*r);
    waiting.owner_source_revision = admission_revision(*r);
    waiting.phase = PlanPhase::waiting;
    waiting.wait_reason = "NoKnownExecutableWork";
    if (waiting.route == DirectorDecisionRoute2::actor) {
      DecisionProposal old;
      old.subject_id = subject.id;
      old.plan_id = waiting.plan_id;
      waiting.actor_proposal = std::move(old);
    }
    result.value = std::move(waiting);
  }
  const auto &p = *result.value;
  auto valid = proposal_shape(p);
  if (!valid.ok)
    return {false, {}, valid.error};
  if (!same(p.subject, subject) || p.registration_epoch != r->epoch ||
      p.decision_seq != schedule->decision_seq + 1 ||
      p.policy_id != r->policy_id || p.route != admission.value.route ||
      !same(p.policy_source_ref, policy_source(*r)))
    return {false, {}, fail("MismatchedDirectorProposal2", key).error};
  if (p.binding) {
    valid = read.validate_plan_binding(plan_binding(p, *p.binding));
    if (!valid.ok)
      return {false, {}, valid.error};
  }
  return result;
}
Outcome<const DirectorAdoptionResult2 *>
read_director_adoption_result2(const DirectorState &s,
                               const SubjectDecisionProposal2 &p,
                               const DirectorSubjectOwnerRead &read) {
  if (!s.subject2)
    return {false, nullptr, fail("DirectorSubject2NotSelected").error};
  const auto key = director_subject_key2(p.subject);
  const auto *row = s.subject2->adoption_results.find(
      director_adoption_key2(p.subject, p.registration_epoch, p.decision_seq));
  if (!row)
    return {true, nullptr, {}};
  if (!same(row->subject, p.subject) || !row->plan_id ||
      *row->plan_id != p.plan_id ||
      row->typed_proposal_kind != static_cast<Integer>(p.route) ||
      !same(row->source_policy_ref, p.policy_source_ref))
    return {false, nullptr, fail("DirectorAdoptionReplayMismatch2", key).error};
  const auto *schedule = s.schedules.find(key);
  if (!schedule || schedule->decision_seq < row->decision_seq ||
      schedule->last_decision_tick < row->reviewed_tick)
    return {false, nullptr,
            fail("DirectorAdoptionReplayScheduleMismatch2", key).error};
  if (row->status == DirectorAdoptionStatus2::adopted) {
    if (row->actual_outcome_refs.empty())
      return {false, nullptr,
              fail("DirectorAdoptionWithoutAppliedEffect", key).error};
    for (const auto &source : row->actual_outcome_refs) {
      auto valid = read.validate_applied_effect(row->subject, p.route, source);
      if (!valid.ok)
        return {false, nullptr, valid.error};
    }
  } else if (!row->actual_outcome_refs.empty())
    return {false, nullptr, fail("InvalidDirectorNonAdoption2", key).error};
  return {true, row, {}};
}
Status adopt_director_subject2(const CommonStore &c, DirectorState &s,
                               const DirectorCatalog &catalog,
                               const SubjectDecisionProposal2 &p,
                               const DirectorSubjectOwnerRead &read,
                               DirectorSubjectExecutor &executor,
                               const WorldMoment &actual) {
  auto actual_shape = compare_world_moments(actual, actual);
  if (!actual_shape.ok)
    return {false, {}, actual_shape.error};
  if (!s.subject2)
    return fail("DirectorSubject2NotSelected");
  if (s.subject2->command_binding_version !=
      (catalog.command_acceptance_bindings ? 1 : 0))
    return fail("DirectorCommandPhysicalSelectionMismatch");
  auto valid = proposal_shape(p);
  if (!valid.ok)
    return valid;
  auto replay = read_director_adoption_result2(s, p, read);
  if (!replay.ok)
    return {false, {}, replay.error};
  if (replay.value)
    return common_success();
  const auto key = director_subject_key2(p.subject);
  auto *r = s.subject2->registrations.find(key);
  auto *schedule = s.schedules.find(key);
  if (!r || !schedule || r->lifecycle != DirectorLifecycle::active ||
      r->epoch != p.registration_epoch || r->policy_id != p.policy_id ||
      schedule->last_decision_tick >= actual.half_day_tick ||
      schedule->next_tick > actual.half_day_tick ||
      schedule->decision_seq == std::numeric_limits<Integer>::max() ||
      p.decision_seq != schedule->decision_seq + 1 ||
      s.fairness_cursor == std::numeric_limits<Integer>::max())
    return fail("StaleDirectorProposal2", key);
  const auto delay = p.wait_reason.empty() ? catalog.review_interval_ticks
                                           : catalog.retry_delay_ticks;
  if (delay < 1 ||
      actual.half_day_tick > std::numeric_limits<Integer>::max() -
                                 std::max(catalog.review_interval_ticks,
                                          catalog.retry_delay_ticks))
    return fail("DirectorReviewOverflow2", key);
  auto admission = read.current_admission(p.subject);
  if (!admission.ok)
    return {false, {}, admission.error};
  if ((!admission.value.source_active && !r->obligations_only) ||
      p.route != admission.value.route ||
      !same(p.policy_source_ref, policy_source(*r)))
    return fail("StaleDirectorRoute2", key);
  for (const auto &version : p.expected_versions)
    if (c.read_version(version.resource).version != version.version)
      return fail("StaleDirectorReadVersion2", key);
  if (p.binding) {
    valid = read.validate_plan_binding(plan_binding(p, *p.binding));
    if (!valid.ok)
      return valid;
  }
  if (s.archived_plans.find(p.plan_id))
    return fail("DirectorArchivedPlanCannotRestart2", p.plan_id);
  if (const auto *old = s.plans.find(p.plan_id))
    if (!same(old->subject, p.subject))
      return fail("DirectorPlanOwnerMismatch2", p.plan_id);
  Integer reviewed = 0;
  for (const auto &row : s.schedules.rows())
    if (row.second.last_decision_tick == actual.half_day_tick)
      ++reviewed;
  if (reviewed >= catalog.decision_budget)
    return common_failure("DirectorBoundaryBudgetExhausted", key,
                          ErrorKind::rule);
  SubjectDecisionApplied2 applied;
  DirectorAdoptionStatus2 status = DirectorAdoptionStatus2::waiting;
  StableId code = p.wait_reason;
  if (p.wait_reason.empty()) {
    Outcome<SubjectDecisionApplied2> result;
    switch (p.route) {
    case DirectorDecisionRoute2::actor:
      result = executor.apply_actor(p);
      break;
    case DirectorDecisionRoute2::cohort_purchase:
      result = executor.apply_cohort_purchase(p);
      break;
    case DirectorDecisionRoute2::cohort_household_routine:
      result = executor.apply_cohort_household_routine(p);
      break;
    case DirectorDecisionRoute2::institutional_continuation:
      result = executor.apply_institutional_continuation(p);
      break;
    case DirectorDecisionRoute2::await_officer:
      result = executor.apply_await_officer(p);
      break;
    }
    if (!result.ok)
      return {false, {}, result.error};
    applied = std::move(result.value);
    if (applied.outcome.kind == DecisionOutcome::Kind::internal_failure)
      return common_failure(applied.outcome.code, key, ErrorKind::internal);
    if (applied.outcome.kind == DecisionOutcome::Kind::adopted) {
      if (!applied.applied_effect_ref ||
          !present(*applied.applied_effect_ref) || !applied.actual_binding)
        return common_failure("DirectorAdoptionWithoutAppliedEffect", key,
                              ErrorKind::internal);
      valid = read.validate_applied_effect(p.subject, p.route,
                                           *applied.applied_effect_ref);
      if (!valid.ok)
        return valid;
      valid =
          read.validate_plan_binding(plan_binding(p, *applied.actual_binding));
      if (!valid.ok)
        return valid;
      if (applied.actual_binding->index() != static_cast<std::size_t>(p.route))
        return common_failure("DirectorAppliedRouteMismatch", key,
                              ErrorKind::internal);
      status = DirectorAdoptionStatus2::adopted;
      code = applied.outcome.code;
    } else {
      if (applied.applied_effect_ref)
        return common_failure("RejectedDirectorHasAppliedEffect", key,
                              ErrorKind::internal);
      status = DirectorAdoptionStatus2::rejected;
      code = applied.outcome.code.empty() ? "Rejected" : applied.outcome.code;
    }
  }
  valid = validate_director_command_application(c, catalog, p, applied, actual);
  if (!valid.ok)
    return valid;
  // Only a real writer result advances execution. An ordinary wait is durable,
  // consumes this boundary's budget, and has no adoption source.
  // 暂定内部字段映射：v1e§6.1要求原来源地址；首写A在执行后才有ID。
  // 只由有限typed命令/真实结果及本次NPC路由返回原A；B仍指A，不预分配。
  CheckpointMemoryCharge family_goal_memory;
  const StableId *family_goal = nullptr;
  if (p.template_id == "life_growth_care_v1" &&
      status == DirectorAdoptionStatus2::adopted) {
    auto original_goal = read.read_family_applied_goal5(p, applied);
    if (!original_goal.ok)
      return {false, {}, original_goal.error};
    if (!original_goal.value || original_goal.value->empty())
      return fail("DirectorFamilyAppliedGoalReaderRequired", p.plan_id);
    family_goal = original_goal.value;


    (family_goal_memory).grow(0);
  }
  FinitePlan plan;
  if (const auto *old = s.plans.find(p.plan_id))
    plan = *old;
  plan.id = p.plan_id;
  plan.template_id = p.template_id;
  if (family_goal)
    plan.goal_ref = checkpoint_fresh_string_copy(*family_goal);
  else
    plan.goal_ref = p.goal_ref;
  plan.subject = p.subject;
  plan.policy_id = p.policy_id;
  plan.next_review_tick =
      actual.half_day_tick + (status == DirectorAdoptionStatus2::adopted
                                  ? catalog.review_interval_ticks
                                  : catalog.retry_delay_ticks);
  if (status == DirectorAdoptionStatus2::adopted) {
    plan.step_index = p.next_step_index;
    plan.phase = p.phase;
    plan.current_action_id = applied.outcome.action_id;
    plan.org_activity_id = applied.outcome.org_activity_id;
    plan.commitment_ids = p.commitment_ids;
    for (const auto &id : p.known_report_ids)
      if (!id.empty() &&
          std::find(plan.known_report_ids.begin(), plan.known_report_ids.end(),
                    id) == plan.known_report_ids.end())
        plan.known_report_ids.push_back(id);
    plan.retry_reason.clear();
    s.subject2->subject_plan_bindings.set(
        p.plan_id, plan_binding(p, *applied.actual_binding));
  } else {
    plan.phase = PlanPhase::waiting;
    plan.retry_reason = code;
  }
  if (p.route == DirectorDecisionRoute2::actor &&
      p.actor_proposal->transition) {
    const auto &t = *p.actor_proposal->transition;
    if (t.next_review_tick <= actual.half_day_tick)
      return fail("DirectorOriginalActorReviewBoundary", p.subject.id);
    plan.next_review_tick = std::max(plan.next_review_tick, t.next_review_tick);
    if (s.regional_life) {
      plan.livelihood = t.livelihood;
      plan.pending_life_action_id = t.pending_life_action_id;
      plan.last_life_fact_id = t.last_life_fact_id;
    }
    if (status == DirectorAdoptionStatus2::adopted) {
      if (!plan.current_action_id)
        plan.current_action_id = t.current_action_id;
      if (s.regional_life && t.template_id == "private_livelihood" &&
          p.actor_proposal->command &&
          (p.actor_proposal->command->kind == "service" ||
           p.actor_proposal->command->kind == "trade")) {
        if (!applied.outcome.action_id)
          return fail("MissingLifeAction", p.subject.id);
        plan.pending_life_action_id = applied.outcome.action_id;
      }
    } else
      plan.current_action_id.reset();
  }
  valid = validate_plan_shape2(plan, catalog, read);
  if (!valid.ok)
    return valid;
  schedule->decision_seq = p.decision_seq;
  schedule->last_decision_tick = actual.half_day_tick;
  schedule->next_tick = plan.next_review_tick;
  schedule->waiting_since = actual.half_day_tick;
  schedule->last_outcome =
      status == DirectorAdoptionStatus2::adopted ? "adopted" : code;
  for (auto &row : s.subject2->wake_sources.rows())
    if (same(row.second.wake.subject, p.subject) &&
        !row.second.consumed_decision_seq)
      row.second.consumed_decision_seq = p.decision_seq;
  schedule->wake_reasons.clear();
  if (plan.phase == PlanPhase::completed) {
    s.archived_plans.set(p.plan_id, std::move(plan));
    s.plans.erase(p.plan_id);
  } else
    s.plans.set(p.plan_id, std::move(plan));
  DirectorAdoptionResult2 receipt;
  receipt.subject = p.subject;
  receipt.registration_epoch = p.registration_epoch;
  receipt.decision_seq = p.decision_seq;
  receipt.plan_id = p.plan_id;
  receipt.typed_proposal_kind = static_cast<Integer>(p.route);
  receipt.source_policy_ref = p.policy_source_ref;
  receipt.status = status;
  if (applied.applied_effect_ref)
    receipt.actual_outcome_refs.push_back(*applied.applied_effect_ref);
  receipt.reviewed_tick = actual.half_day_tick;
  receipt.next_review_tick = schedule->next_tick;
  receipt.accepted_command = std::move(applied.accepted_command);
  const auto adoption_key =
      director_adoption_key2(p.subject, p.registration_epoch, p.decision_seq);
  if (!s.subject2->adoption_results.insert(adoption_key, std::move(receipt)))
    return fail("DuplicateDirectorAdoptionKey2", adoption_key);
  ++s.fairness_cursor;
  s.last_boundary_tick = actual.half_day_tick;
  // 只保原物理6接受尾；未安装 Family 的物理5不能绕过真实命令接受。
  if (p.route == DirectorDecisionRoute2::actor &&
      status == DirectorAdoptionStatus2::adopted &&
      catalog.command_acceptance_bindings) {
    const auto *actual_adoption =
        s.subject2->adoption_results.find(adoption_key);
    if (!actual_adoption)
      return fail("DirectorAcceptedCommandMissing", adoption_key);
    if (!actual_adoption->accepted_command)
      return fail("DirectorAcceptedCommandMissing", adoption_key);
    valid = executor.finalize_actor_adoption_in_candidate(p, *actual_adoption,
                                                          actual);
    if (!valid.ok)
      return valid;
  }
  return common_success();
}
Status continue_director_subject2(DirectorState &s,
                                  const SubjectContinuation &continuation,
                                  const DirectorSubjectOwnerRead &read,
                                  const WorldMoment &actual) {
  if (!s.subject2 ||
      same(continuation.prior_subject, continuation.current_subject) ||
      !present(continuation.original_continuation_ref) ||
      continuation.plan_ids.empty())
    return fail("InvalidSubjectContinuation2");
  auto moment = compare_world_moments(actual, actual);
  if (!moment.ok)
    return {false, {}, moment.error};
  const auto id = continuation.original_continuation_ref.id;
  if (const auto *old = s.subject2->subject_continuations.find(id)) {
    if (!same(old->original_continuation_ref,
              continuation.original_continuation_ref) ||
        !same(old->prior_subject, continuation.prior_subject) ||
        !same(old->current_subject, continuation.current_subject) ||
        old->old_registration_epoch != continuation.old_registration_epoch ||
        old->new_registration_epoch != continuation.new_registration_epoch ||
        old->plan_ids != continuation.plan_ids)
      return fail("SubjectContinuationReplayConflict2", id);
    return read.validate_continuation(*old);
  }
  auto valid = read.validate_continuation(continuation);
  if (!valid.ok)
    return valid;
  const auto prior = director_subject_key2(continuation.prior_subject),
             current = director_subject_key2(continuation.current_subject);
  auto *r = s.subject2->registrations.find(current);
  auto *old_registration = s.subject2->registrations.find(prior);
  if (!r || !old_registration || r->lifecycle == DirectorLifecycle::retired ||
      continuation.old_registration_epoch != old_registration->epoch ||
      continuation.new_registration_epoch != r->epoch ||
      old_registration->epoch == std::numeric_limits<Integer>::max())
    return fail("UnregisteredContinuationSubject2", current);
  // The original owner validates each named business mapping. No all-subject
  // move, quantity split, or cloned obligation is inferred from a population
  // transition.
  for (std::size_t i = 0; i < continuation.plan_ids.size(); ++i) {
    const auto &plan_id = continuation.plan_ids[i];
    for (std::size_t j = 0; j < i; ++j)
      if (continuation.plan_ids[j] == plan_id)
        return fail("DuplicateContinuationPlan2", plan_id);
    const auto *plan = find_director_plan2(s, plan_id);
    const auto *binding = s.subject2->subject_plan_bindings.find(plan_id);
    if (!plan || !binding || !same(plan->subject, continuation.prior_subject) ||
        !same(binding->subject, continuation.prior_subject) ||
        binding->plan_id != plan_id ||
        binding->registration_epoch > continuation.old_registration_epoch)
      return fail("InvalidContinuationPlans2", plan_id);
    SubjectPlanBinding moved = *binding;
    moved.subject = continuation.current_subject;
    moved.registration_epoch = r->epoch;
    moved.continuation_ref = continuation.original_continuation_ref;
    valid = read.validate_plan_binding(moved);
    if (!valid.ok)
      return valid;
  }
  auto prior_source = read.current_admission(continuation.prior_subject);
  if (!prior_source.ok)
    return {false, {}, prior_source.error};
  for (const auto &plan_id : continuation.plan_ids) {
    auto *plan = s.plans.find(plan_id);
    if (!plan)
      plan = s.archived_plans.find(plan_id);
    auto *binding = s.subject2->subject_plan_bindings.find(plan_id);
    plan->subject = continuation.current_subject;
    binding->subject = continuation.current_subject;
    binding->registration_epoch = r->epoch;
    binding->continuation_ref = continuation.original_continuation_ref;
  }
  ++old_registration->epoch;
  old_registration->changed_tick = actual.half_day_tick;
  if (!prior_source.value.source_active) {
    old_registration->obligations_only =
        prior_source.value.has_frozen_responsibility;
    old_registration->lifecycle = prior_source.value.has_frozen_responsibility
                                      ? DirectorLifecycle::active
                                      : DirectorLifecycle::retired;
    old_registration->reason = prior_source.value.has_frozen_responsibility
                                   ? "ExistingResponsibilityOnly"
                                   : "SourceContinued";
  }
  if (!s.subject2->subject_continuations.insert(id, continuation))
    return fail("DuplicateSubjectContinuation2", id);
  return common_success();
}
Status validate_director_subjects2(const CommonStore &c, const DirectorState &s,
                                   const DirectorCatalog &catalog,
                                   const DirectorSubjectOwnerRead &read,
                                   const WorldMoment &actual,
                                   const SocialState *actual_social) {
  auto actual_shape = compare_world_moments(actual, actual);
  if (!actual_shape.ok)
    return {false, {}, actual_shape.error};
  if (!catalog.subjects2 || !catalog.dynamic_subjects)
    return fail("DirectorSubjectsProfileRequired");
  if (!s.subject2 || !s.subject2->wake_cache_ready)
    return fail("DirectorWakeCacheRebuildRequired");
  if (s.subject2->registrations.size() > 4096 ||
      s.subject2->registrations.size() != s.schedules.size() ||
      s.fairness_cursor < 0 || s.last_boundary_tick > actual.half_day_tick)
    return fail("InvalidDirectorState2");
  for (const auto &row : s.subject2->registrations.rows()) {
    const auto &r = row.second;
    auto valid = shape(r, catalog);
    if (!valid.ok)
      return valid;
    if (row.first != director_subject_key2(r.subject) ||
        r.changed_tick > actual.half_day_tick)
      return fail("InvalidDirectorRegistrationKey2", row.first);
    const auto *schedule = s.schedules.find(row.first);
    if (!schedule || !same(schedule->subject, r.subject) ||
        schedule->decision_seq < 0 || schedule->next_tick < 0 ||
        schedule->last_decision_tick > actual.half_day_tick ||
        schedule->waiting_since < 0 ||
        schedule->waiting_since > actual.half_day_tick ||
        schedule->wake_reasons.size() > 256)
      return fail("InvalidDirectorSchedule2", row.first);
    auto admission = read.current_admission(r.subject);
    if (!admission.ok)
      return {false, {}, admission.error};
    if (r.lifecycle == DirectorLifecycle::active &&
        !admission.value.source_active && !r.obligations_only)
      return fail("InactiveDirectorSource2", row.first);
    if (r.subject.kind == SubjectKind::actor &&
        r.lifecycle == DirectorLifecycle::active &&
        r.subject.id == c.state().metadata.player_actor_id)
      return fail("PlayerDirectorOverlap2", row.first);
  }
  for (const auto &row : s.subject2->wake_sources.rows()) {
    const auto &v = row.second;
    const auto key = director_subject_key2(v.wake.subject);
    const auto *schedule = s.schedules.find(key);
    if (row.first != wake_key(v.wake) || !schedule ||
        v.wake.source_schema < 1 || v.wake.source_revision < 0)
      return fail("InvalidDirectorWakeSource2", row.first);
    auto past = compare_world_moments(v.observed_at, actual);
    if (!past.ok)
      return {false, {}, past.error};
    if (past.value > 0)
      return fail("FutureDirectorWakeSource2", row.first);
    auto valid = read.validate_wake(v.wake);
    if (!valid.ok)
      return valid;
    const auto count = std::count(schedule->wake_reasons.begin(),
                                  schedule->wake_reasons.end(), row.first);
    if (v.consumed_decision_seq) {
      if (*v.consumed_decision_seq < 0 ||
          *v.consumed_decision_seq > schedule->decision_seq || count != 0)
        return fail("InvalidConsumedDirectorWake2", row.first);
    } else if (count != 1)
      return fail("UnscheduledDirectorWake2", row.first);
  }
  for (const auto &row : s.schedules.rows())
    for (const auto &reason : row.second.wake_reasons) {
      const auto *wake = s.subject2->wake_sources.find(reason);
      if (!wake || wake->consumed_decision_seq ||
          !same(wake->wake.subject, row.second.subject))
        return fail("UnprovenDirectorWake2", row.first);
    }
  for (const auto &row : s.plans.rows()) {
    if (row.first != row.second.id || s.archived_plans.find(row.first))
      return fail("DirectorPlanIdMismatch2", row.first);
    if (!s.subject2->registrations.find(
            director_subject_key2(row.second.subject)))
      return fail("DirectorPlanRegistrationMissing2", row.first);
    auto valid = validate_plan_shape2(row.second, catalog, read);
    if (!valid.ok)
      return valid;
  }
  for (const auto &row : s.archived_plans.rows()) {
    if (row.first != row.second.id ||
        row.second.phase != PlanPhase::completed ||
        !s.subject2->registrations.find(
            director_subject_key2(row.second.subject)))
      return fail("InvalidArchivedDirectorPlan2", row.first);
    auto valid = validate_plan_shape2(row.second, catalog, read);
    if (!valid.ok)
      return valid;
  }
  for (const auto &row : s.subject2->subject_plan_bindings.rows()) {
    const auto &b = row.second;
    const auto *r =
        s.subject2->registrations.find(director_subject_key2(b.subject));
    const auto *p = find_director_plan2(s, row.first);
    if (row.first != b.plan_id || !r || !p || b.plan_id != p->id ||
        !same(b.subject, r->subject) || !same(p->subject, b.subject) ||
        b.registration_epoch > r->epoch || b.registration_epoch < 1 ||
        !present(b.policy_source_ref) || b.last_source_revision < 0)
      return fail("InvalidSubjectPlanBinding2", row.first);
    auto valid = read.validate_plan_binding(b);
    if (!valid.ok)
      return valid;
  }
  for (const auto &row : s.subject2->adoption_results.rows()) {
    const auto &a = row.second;
    const auto subject_key = director_subject_key2(a.subject);
    const auto *r = s.subject2->registrations.find(subject_key);
    const auto *schedule = s.schedules.find(subject_key);
    if (row.first != director_adoption_key2(a.subject, a.registration_epoch,
                                            a.decision_seq) ||
        !r || !schedule || !same(a.subject, r->subject) ||
        a.registration_epoch < 1 || a.registration_epoch > r->epoch ||
        a.decision_seq < 1 || a.decision_seq > schedule->decision_seq ||
        a.reviewed_tick > schedule->last_decision_tick ||
        a.reviewed_tick > actual.half_day_tick ||
        a.next_review_tick <= a.reviewed_tick || !a.plan_id ||
        a.plan_id->empty() || !present(a.source_policy_ref) ||
        a.typed_proposal_kind < 0 || a.typed_proposal_kind > 4 ||
        !route_matches(a.subject.kind, static_cast<DirectorDecisionRoute2>(
                                           a.typed_proposal_kind)) ||
        static_cast<Integer>(a.status) < 0 ||
        static_cast<Integer>(a.status) > 2)
      return fail("InvalidDirectorAdoptionResult2", row.first);
    if (a.status == DirectorAdoptionStatus2::adopted) {
      if (a.actual_outcome_refs.empty())
        return fail("DirectorAdoptionWithoutAppliedEffect", row.first);
      const auto *b = s.subject2->subject_plan_bindings.find(*a.plan_id);
      if (!b || b->plan_id != *a.plan_id)
        return fail("MissingAdoptedPlanBinding2", row.first);
      for (const auto &effect : a.actual_outcome_refs) {
        auto valid = read.validate_applied_effect(
            a.subject,
            static_cast<DirectorDecisionRoute2>(a.typed_proposal_kind), effect);
        if (!valid.ok)
          return valid;
      }
    } else if (!a.actual_outcome_refs.empty())
      return fail("InvalidDirectorNonAdoption2", row.first);
  }

  for (const auto &row : s.schedules.rows())
    if (row.second.decision_seq > 0) {
      const DirectorAdoptionResult2 *latest = nullptr;
      for (const auto &entry : s.subject2->adoption_results.rows())
        if (same(entry.second.subject, row.second.subject) &&
            entry.second.decision_seq == row.second.decision_seq) {
          if (latest)
            return fail("DuplicateDirectorDecisionSequence2", row.first);
          latest = &entry.second;
        }
      const SocialPropagationDecision *propagation = nullptr;
      if (actual_social && actual_social->dynamic &&
          actual_social->dynamic->time2)
        for (const auto &entry :
             actual_social->dynamic->time2->propagation_decisions.rows())
          if (same(entry.second.subject, row.second.subject) &&
              entry.second.decision_seq == row.second.decision_seq) {
            if (propagation)
              return fail("DuplicateDirectorDecisionSequence2", row.first);
            propagation = &entry.second;
          }
      if (latest && propagation)
        return fail("DuplicateDirectorDecisionSequence2", row.first);
      if (propagation) {
        if (propagation->evaluated_at != row.second.last_decision_tick ||
            propagation->next_review_tick < row.second.next_tick ||
            row.second.last_outcome !=
                "propagation:" +
                    propagation_decision_key(propagation->subject,
                                             propagation->registration_epoch,
                                             propagation->decision_seq))
          return fail("DirectorPropagationScheduleMismatch", row.first);
      } else if (!latest ||
                 latest->reviewed_tick != row.second.last_decision_tick ||
                 latest->next_review_tick < row.second.next_tick)
        return fail("DirectorLatestAdoptionMissing2", row.first);
    }
  for (const auto &row : s.subject2->subject_continuations.rows()) {
    const auto &v = row.second;
    if (row.first != v.original_continuation_ref.id || v.plan_ids.empty())
      return fail("InvalidContinuationKey2", row.first);
    const auto *old =
        s.subject2->registrations.find(director_subject_key2(v.prior_subject));
    const auto *current = s.subject2->registrations.find(
        director_subject_key2(v.current_subject));
    if (!old || !current || v.old_registration_epoch < 1 ||
        v.old_registration_epoch >= old->epoch ||
        v.new_registration_epoch < 1 ||
        v.new_registration_epoch > current->epoch)
      return fail("InvalidContinuationEpoch2", row.first);
    for (std::size_t i = 0; i < v.plan_ids.size(); ++i) {
      for (std::size_t j = 0; j < i; ++j)
        if (v.plan_ids[j] == v.plan_ids[i])
          return fail("DuplicateContinuationPlan2", v.plan_ids[i]);
      if (!find_director_plan2(s, v.plan_ids[i]))
        return fail("ContinuationPlanMissing2", v.plan_ids[i]);
    }
    auto valid = read.validate_continuation(v);
    if (!valid.ok)
      return valid;
  }
  return common_success();
}
std::vector<SourceRef> director_subject_source_pins2(const DirectorState &s) {
  std::vector<SourceRef> out;
  if (!s.subject2)
    return out;
  for (const auto &row : s.subject2->registrations.rows()) {
    if (row.second.lifecycle == DirectorLifecycle::retired &&
        !row.second.obligations_only)
      continue;
    std::visit(
        [&](const auto &x) {
          using T = std::decay_t<decltype(x)>;
          if constexpr (std::is_same_v<T, CohortDirectorAdmission>)
            pin(out, x.accepted_household_policy_ref);
          else if constexpr (std::is_same_v<T, OrganizationDirectorAdmission>)
            pin(out, x.approved_responsibility_ref);
        },
        row.second.admission_source);
  }
  for (const auto &row : s.subject2->subject_plan_bindings.rows()) {
    const auto *p = find_director_plan2(s, row.first);
    if (p && p->phase != PlanPhase::completed) {
      pins(out, row.second.binding);
      pin(out, row.second.policy_source_ref);
      if (row.second.continuation_ref)
        pin(out, *row.second.continuation_ref);
    }
  }
  for (const auto &row : s.subject2->adoption_results.rows()) {
    for (const auto &source : row.second.actual_outcome_refs)
      pin(out, source);
    if (row.second.accepted_command)
      pin(out,
          {"common", "command_result", row.second.accepted_command->result_id});
  }
  for (const auto &row : s.subject2->subject_continuations.rows())
    pin(out, row.second.original_continuation_ref);
  return out;
}
} // namespace hanmo::resident
