#include "director_native_executor2.hpp"
#include "director_actor_proposal2.hpp"
namespace hanmo::resident {
namespace {
Outcome<SubjectDecisionApplied2> failed(const Status &status) {
  return {false, {}, status.error};
}
} // namespace
bool NativeDirectorSubjectExecutor::bound_to(
    const CommonStore &c, const DirectorState &d) const noexcept {
  return &c == &x_.common && &d == &x_.director &&
         x_.actor_commands.bound_to(c, d);
}
Status NativeDirectorSubjectExecutor::check(const SubjectDecisionProposal2 &p,
                                            DirectorDecisionRoute2 route,
                                            SubjectKind kind) const {
  if (!bound_to(x_.common, x_.director))
    return common_failure("DirectorExecutorCandidateMismatch");
  if (!x_.director.subject2 || p.route != route || p.subject.kind != kind ||
      p.subject.id.empty() || !p.wait_reason.empty() ||
      p.registration_epoch < 1 || p.decision_seq < 1)
    return common_failure("DirectorExecutorProposalMismatch", p.plan_id);
  const auto *registration = x_.director.subject2->registrations.find(
      director_subject_key2(p.subject));
  const auto *schedule =
      x_.director.schedules.find(director_subject_key2(p.subject));
  if (!registration || !schedule ||
      registration->lifecycle != DirectorLifecycle::active ||
      registration->epoch != p.registration_epoch ||
      registration->policy_id != p.policy_id ||
      schedule->decision_seq != p.decision_seq - 1)
    return common_failure("StaleDirectorExecutorProposal", p.plan_id);
  auto now = x_.clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  if (schedule->last_decision_tick >= now.value.half_day_tick ||
      schedule->next_tick > now.value.half_day_tick)
    return common_failure("DirectorExecutorReviewNotDue", p.plan_id);
  if (kind == SubjectKind::actor)
    return validate_director_actor_proposal2(p);
  if (p.actor_proposal || !p.binding ||
      p.binding->index() != static_cast<std::size_t>(route))
    return common_failure("NonActorCommandForbidden", p.plan_id);
  return common_success();
}
Outcome<SubjectDecisionApplied2>
NativeDirectorSubjectExecutor::apply_actor(const SubjectDecisionProposal2 &p) {
  auto valid = check(p, DirectorDecisionRoute2::actor, SubjectKind::actor);
  if (!valid.ok)
    return failed(valid);
  return x_.actor_commands.apply_actor_command_in_candidate(p);
}
Outcome<SubjectDecisionApplied2>
NativeDirectorSubjectExecutor::apply_cohort_purchase(
    const SubjectDecisionProposal2 &p) {
  auto valid =
      check(p, DirectorDecisionRoute2::cohort_purchase, SubjectKind::cohort);
  if (!valid.ok)
    return failed(valid);
  if (!x_.cohort_purchase)
    return failed(
        common_failure("DirectorCohortPurchaseWriterRequired", p.plan_id));
  auto &b = *x_.cohort_purchase;
  const auto &c = b.original;
  if (&c.common != &x_.common || &c.economy != &b.economy ||
      &c.state != &b.purchases || &c.claims != &b.claims ||
      &c.elapsed != &b.elapsed || &c.retail.common != &x_.common ||
      &c.retail.state != &b.retail)
    return failed(
        common_failure("DirectorCohortWriterCandidateMismatch", p.plan_id));
  auto now = x_.clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  auto same = compare_world_moments(c.now, now.value);
  if (!same.ok)
    return {false, {}, same.error};
  if (same.value)
    return failed(
        common_failure("DirectorCohortWriterMomentMismatch", p.plan_id));
  return apply_director_cohort_purchase_in_candidate(
      x_.common, b.economy, b.purchases, b.claims, b.elapsed, b.retail, c,
      b.knowledge, p);
}
Outcome<SubjectDecisionApplied2>
NativeDirectorSubjectExecutor::apply_cohort_household_routine(
    const SubjectDecisionProposal2 &p) {
  auto valid = check(p, DirectorDecisionRoute2::cohort_household_routine,
                     SubjectKind::cohort);
  if (!valid.ok)
    return failed(valid);
  if (!x_.household_routine)
    return failed(
        common_failure("DirectorHouseholdRoutineWriterRequired", p.plan_id));
  if (!x_.household_routine->bound_to(x_.common, x_.director))
    return failed(
        common_failure("DirectorHouseholdRoutineCandidateMismatch", p.plan_id));
  return x_.household_routine->apply_household_routine_in_candidate(p);
}
Outcome<SubjectDecisionApplied2>
NativeDirectorSubjectExecutor::apply_institutional_continuation(
    const SubjectDecisionProposal2 &p) {
  auto valid = check(p, DirectorDecisionRoute2::institutional_continuation,
                     SubjectKind::organization);
  if (!valid.ok)
    return failed(valid);
  if (!x_.institutions)
    return failed(
        common_failure("DirectorInstitutionalWriterRequired", p.plan_id));
  if (!x_.institutions->bound_to(x_.common, x_.director))
    return failed(
        common_failure("DirectorInstitutionalCandidateMismatch", p.plan_id));
  return x_.institutions->apply_institutional_continuation_in_candidate(p);
}
Outcome<SubjectDecisionApplied2>
NativeDirectorSubjectExecutor::apply_await_officer(
    const SubjectDecisionProposal2 &p) {
  auto valid = check(p, DirectorDecisionRoute2::await_officer,
                     SubjectKind::organization);
  if (!valid.ok)
    return failed(valid);
  if (!x_.institutions)
    return failed(
        common_failure("DirectorInstitutionalWriterRequired", p.plan_id));
  if (!x_.institutions->bound_to(x_.common, x_.director))
    return failed(
        common_failure("DirectorInstitutionalCandidateMismatch", p.plan_id));
  return x_.institutions->apply_await_officer_in_candidate(p);
}
Status NativeDirectorSubjectExecutor::finalize_actor_adoption_in_candidate(
    const SubjectDecisionProposal2 &p, const DirectorAdoptionResult2 &adoption,
    const WorldMoment &at) {
  if (!bound_to(x_.common, x_.director) || !x_.director.subject2 ||
      p.route != DirectorDecisionRoute2::actor ||
      p.subject.kind != SubjectKind::actor)
    return common_failure("DirectorActorFinalizerCandidateMismatch", p.plan_id);
  const auto *original = x_.director.subject2->adoption_results.find(
      director_adoption_key2(p.subject, p.registration_epoch, p.decision_seq));
  if (original != &adoption ||
      adoption.status != DirectorAdoptionStatus2::adopted)
    return common_failure("DirectorActualAdoptionBorrowRequired", p.plan_id);
  if (!adoption.accepted_command)
    return common_failure("DirectorAcceptedCommandMissing", p.plan_id);
  auto now = x_.clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  auto same = compare_world_moments(at, now.value);
  if (!same.ok)
    return {false, {}, same.error};
  if (same.value)
    return common_failure("DirectorActorFinalizerMomentMismatch", p.plan_id);
  return x_.actor_commands.finalize_actor_adoption_in_candidate(p, adoption,
                                                                at);
}
} // namespace hanmo::resident
