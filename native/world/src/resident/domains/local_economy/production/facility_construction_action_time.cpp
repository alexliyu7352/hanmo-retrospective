// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
// 原开工与单阶段完工反读；身体/地点规则复用服务唯一正文，不写第二工量。
#include "facility_construction_action_time.hpp"
#include "facility_construction_result_read.hpp"
#include "hanmo/resident_world_api.hpp"
#include "resident/kernel/world_copy_plan.hpp"

namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
int compare(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b));
}
// 注册只认本域原编译行，作者不能自填一个近似 F 配置绕过 owner。
void registered(const ActionTimeRegistration &r) {
  const auto &actual = facility_construction_action_time_registration();
  if (r.action_kind != actual.action_kind ||
      r.owner_domain != actual.owner_domain ||
      r.profile_id != actual.profile_id ||
      r.profile_version != actual.profile_version ||
      r.source_schema != actual.source_schema || r.mode != actual.mode ||
      r.remaining_meaning != actual.remaining_meaning || r.shared_gate_kind ||
      !r.retained_wait_phases.empty())
    fail("ConstructionOriginalActionTimeRegistrationRequired");
}
// 临时 source 先预留，再进入原唯一 stage reader；返回只借实际业务行。
const ConstructionStageResult &
stage(const FacilityConstructionServiceContext &x, const StableId &id) {


  auto memory = CheckpointMemoryCharge(0);
  return *take(read_facility_construction_stage_service_evidence(
      x, {facility_construction_stage_source(id), 1}));
}
const ConstructionStageResult *
terminal(const FacilityConstructionServiceContext &x,
         const StableId &construction) {
  const ConstructionStageResult *result = nullptr;
  for (const auto &row : x.adoption.state.stages)
    if (row.construction_id == construction) {
      if (result)
        fail("ConstructionDuplicateTerminalStage", construction);
      result = &stage(x, row.result_id);
    }
  return result;
}
// 原专业采用决定身份，phase 由真实原阶段或当前身体推导，不从 elapsed 猜。
ActionTimeOwnerProof owner(const FacilityConstructionServiceContext &x,
                           const StableId &id) {
  const auto &row = *take(
      read_facility_construction_by_id_for_original_dependency(x.adoption, id));
  CheckpointVector<std::string_view> participants, gates;
  participants.reserve(1);
  participants.emplace_back(row.builder_actor_id);
  ActionTimeOwnerPhase phase;
  std::optional<ElapsedTimeSourceBuildView> ended;
  std::optional<std::string_view> result;
  if (const auto *done = terminal(x, id)) {
    phase = ActionTimeOwnerPhase::terminal;
    ended = MomentOwnerSourceBuildView{
        {"local_economy", "facility_construction_stage", done->result_id}, 1};
    result = done->result_id;
  } else {
    const auto *action = x.adoption.common.action(id);
    const auto *actor = x.adoption.common.actor(row.builder_actor_id);
    if (!action || !actor || action->kind != "facility_construction" ||
        action->actor_id != row.builder_actor_id ||
        action->participants.size() != 1 ||
        action->participants.front() != row.builder_actor_id ||
        action->start_tick != row.started_at.half_day_tick ||
        actor->action_id != id)
      fail("ConstructionActualExclusiveActionRequired", id);
    const auto capability =
        take(read_facility_construction_current_worker_capability(x, id));
    phase = capability.allowed && capability.efficiency_bp > 0
                ? ActionTimeOwnerPhase::working
                : ActionTimeOwnerPhase::suspended;
  }
  const SourceRefBuildView execution{"local_economy",
                                     "facility_construction_started", id};
  return ActionTimeOwnerProof{
      ActionTimeOwnerProofBuildView{id,
                                    "facility_construction",
                                    row.builder_actor_id,
                                    {participants},
                                    execution,
                                    1,
                                    MomentOwnerSourceBuildView{execution, 1},
                                    phase,
                                    std::nullopt,
                                    ended,
                                    result,
                                    {gates},
                                    std::nullopt}};
}
bool same_moment_source(const ElapsedTimeSource &a,
                        const ElapsedTimeSource &b) {
  const auto *left = std::get_if<MomentOwnerSource>(&a);
  const auto *right = std::get_if<MomentOwnerSource>(&b);
  return left && right && left->source_schema == right->source_schema &&
         same_source_ref(left->source_ref, right->source_ref);
}
// 所有原 proof 身份字段均反读，不只检查 action ID。
void proof(const ActionTimeRegistration &r, const ActionTimeOwnerProof &a,
           const ActionTimeOwnerProof &b) {
  take(validate_action_time_owner_proof(r, a));
  if (a.action_ref != b.action_ref || a.action_kind != b.action_kind ||
      a.actor_id != b.actor_id ||
      a.participant_actor_ids != b.participant_actor_ids ||
      !same_source_ref(a.original_execution_source,
                       b.original_execution_source) ||
      a.source_schema != b.source_schema ||
      !same_moment_source(a.started_source, b.started_source) ||
      a.phase != b.phase || a.retained_wait_source || a.suspended_source ||
      a.terminal_result_ref != b.terminal_result_ref ||
      !a.retained_gate_result_refs.empty() ||
      a.ended_source.has_value() != b.ended_source.has_value() ||
      (a.ended_source && !same_moment_source(*a.ended_source, *b.ended_source)))
    fail("ConstructionOriginalOwnerProofMismatch", b.action_ref);
}
} // namespace

Status NativeFacilityConstructionActionTimeRead::enumerate_action_time_owners(
    const ActionTimeRegistration &r, ActionTimeOwnerIdSink &sink) const {
  try {
    registered(r);
    for (const auto &row : x_.adoption.state.constructions) {
      take(read_facility_construction_by_id_for_original_dependency(
          x_.adoption, row.construction_id));
      take(sink.append(row.construction_id));
    }
    return common_success();
  } catch (const Failure &failure) {
    return {false, {}, failure.error};
  }
}
Outcome<ActionTimeOwnerProof>
NativeFacilityConstructionActionTimeRead::read_action_time_owner(
    const ActionTimeRegistration &r, const StableId &id) const {
  try {
    registered(r);
    return {true, owner(x_, id), {}};
  } catch (const Failure &failure) {
    return {false, {}, failure.error};
  }
}
Outcome<WorldMoment>
NativeFacilityConstructionActionTimeRead::read_action_time_source(
    const ElapsedTimeSource &source) const {
  try {
    const auto *actual = std::get_if<MomentOwnerSource>(&source);
    if (!actual || actual->source_schema != 1 ||
        actual->source_ref.domain != "local_economy")
      fail("ConstructionExactMomentSourceRequired");
    if (actual->source_ref.kind == "facility_construction_started")
      return {true,
              take(read_facility_construction_adoption_for_original_dependency(
                       x_.adoption, *actual))
                  ->started_at,
              {}};
    if (actual->source_ref.kind == "facility_construction_stage")
      return {true, stage(x_, actual->source_ref.id).occurred_at, {}};
    fail("ConstructionExactMomentSourceRequired", actual->source_ref.id);
  } catch (const Failure &failure) {
    return {false, {}, failure.error};
  }
}
Status NativeFacilityConstructionActionTimeRead::validate_fixed_action_sources(
    const ActionTimeRegistration &r, const ActionTimeOwnerProof &p,
    const ActionElapsedState &entry,
    const std::vector<FixedServiceTransition> &history) const {
  return validate_fixed_action_sources_in_candidate(r, p, entry, history,
                                                    x_.elapsed);
}
Status NativeFacilityConstructionActionTimeRead::
    validate_fixed_action_sources_in_candidate(
        const ActionTimeRegistration &r, const ActionTimeOwnerProof &p,
        const ActionElapsedState &entry,
        const std::vector<FixedServiceTransition> &history,
        const ActionElapsedStore &candidate) const {
  try {
    registered(r);
    const auto actual = owner(x_, entry.action_ref);
    proof(r, p, actual);
    const auto &row =
        *take(read_facility_construction_by_id_for_original_dependency(
            x_.adoption, entry.action_ref));
    const auto &terms = *take(
        read_facility_construction_terms(x_.adoption.catalog, row.terms_ref));
    const auto required = take(fixed_duration_work_units(
        terms.duration_halfdays, x_.scale.units_per_half_day,
        x_.spatial.budget));
    if (entry.execution_source_ref != row.construction_id ||
        entry.duration_definition_ref != terms.terms_ref.id ||
        entry.owner_cost_state_ref != row.actual_cost_ref ||
        entry.owner_work_remainder ||
        compare(entry.started_at, row.started_at) ||
        take(compare_fractions(required.value, entry.required_units.value,
                               x_.spatial.budget)))
      fail("ConstructionOriginalElapsedMismatch", row.construction_id);
    const auto *retained = find_fixed_elapsed(candidate, row.construction_id);
    if (!retained || compare(retained->started_at, entry.started_at) ||
        compare(retained->last_settled_at, entry.last_settled_at) ||
        take(compare_fractions(retained->credited_units.value,
                               entry.credited_units.value,
                               x_.spatial.budget)) ||
        retained->timing.index() != entry.timing.index())
      fail("ConstructionActualElapsedCandidateRequired", row.construction_id);
    bool running = true, ended = false;
    std::size_t count = 0;
    CheckpointVector<std::string_view> condition_changes;
    for (const auto &t : history) {
      if (t.action_ref != row.construction_id)
        continue;
      if (ended)
        fail("ConstructionTransitionAfterTerminal", t.transition_id);
      const auto *saved = take(read_fixed_service_transition_source(
          candidate, {t.transition_id, t.source_schema}));
      if (!take(same_fixed_service_transition(t, *saved, x_.spatial.budget)) ||
          t.execution_source_schema != 1 ||
          !same_source_ref(t.execution_source_ref,
                           row.execution_binding.source_owner_ref))
        fail("ConstructionActualTransitionCandidateMismatch", t.transition_id);
      if (t.kind == FixedTransitionKind::terminal) {
        const auto *cause = std::get_if<FixedOwnerTerminated>(&t.cause);
        const auto *done = terminal(x_, row.construction_id);
        if (!count || !cause || !done || cause->owner_result_schema != 1 ||
            cause->owner_result_ref.domain != "local_economy" ||
            cause->owner_result_ref.kind != "facility_construction_stage" ||
            cause->owner_result_ref.id != done->result_id ||
            t.transition_id != done->result_id ||
            compare(t.occurred_at, done->occurred_at) ||
            !t.input_basis_refs.empty() ||
            t.terminal_disposition != std::optional<FixedTerminalDisposition>{
                                          FixedTerminalDisposition::completed})
          fail("ConstructionActualTerminalStageRequired", t.transition_id);
        ended = true;
        running = false;
        ++count;
        continue;
      }
      const auto capability = take(read_facility_construction_transition_worker(
          x_, row.construction_id, t));
      if (!count) {
        const auto *cause = std::get_if<FixedOwnerAdopted>(&t.cause);
        if (t.kind != FixedTransitionKind::started ||
            t.transition_id != row.construction_id || !cause ||
            cause->owner_source_schema != 1 ||
            !same_source_ref(cause->owner_source_ref,
                             row.execution_binding.source_owner_ref) ||
            compare(t.occurred_at, row.started_at) || !capability.allowed ||
            capability.efficiency_bp <= 0)
          fail("ConstructionActualStartedTransitionRequired", t.transition_id);
      } else {
        const auto *cause = std::get_if<FixedEligibilityChanged>(&t.cause);
        const auto *body =
            std::get_if<FixedAppliedInput>(&t.input_basis_refs[1]);
        if (!cause ||
            cause->original_owner_execution_ref != row.construction_id ||
            cause->original_cause_schema != 2 ||
            cause->original_cause_ref.domain != "actor_foundation" ||
            cause->original_cause_ref.kind != "condition_receipt" ||
            cause->transition_rule_ref != capability.rule_ref.id ||
            t.transition_id != cause->original_cause_ref.id || !body ||
            body->source_schema != 2 ||
            !same_source_ref(body->source_ref, cause->original_cause_ref))
          fail("ConstructionActualConditionTransitionRequired",
               t.transition_id);
        const auto changed = take(read_character_condition_source(
            x_.body, cause->original_cause_ref, 2));
        if (changed.actor_id != row.builder_actor_id ||
            compare(changed.occurred_at, t.occurred_at))
          fail("ConstructionConditionTransitionMomentMismatch",
               t.transition_id);
        const bool allowed = capability.allowed && capability.efficiency_bp > 0;
        if (t.kind == FixedTransitionKind::suspended) {
          if (!running || allowed)
            fail("ConstructionActualWorkerStopRequired", t.transition_id);
          running = false;
        } else if (t.kind == FixedTransitionKind::resumed) {
          if (running || !allowed)
            fail("ConstructionActualWorkerResumeRequired", t.transition_id);
          running = true;
        } else if (t.kind != FixedTransitionKind::input_rebound ||
                   running != allowed)
          fail("ConstructionConditionTransitionPhaseMismatch", t.transition_id);
        condition_changes.emplace_back(cause->original_cause_ref.id);
      }
      ++count;
    }
    if (!count ||
        (actual.phase == ActionTimeOwnerPhase::working) !=
            std::holds_alternative<ElapsedRunning>(entry.timing) ||
        (actual.phase == ActionTimeOwnerPhase::suspended) !=
            std::holds_alternative<ElapsedSuspended>(entry.timing) ||
        (actual.phase == ActionTimeOwnerPhase::terminal) !=
            std::holds_alternative<ElapsedTerminal>(entry.timing))
      fail("ConstructionOriginalPhaseMismatch", row.construction_id);
    // 已采用后每条原身体 revision 都须被本 action 的原转换覆盖；零净变化
    // 也不能删掉。终局同 M 无原应用次序源时明确拒绝，不能假定它在终局之后。
    for (const auto &saved :
         x_.body.state.character_condition_extensions.applied_receipts.rows()) {
      const auto &change = saved.second;
      if (change.actor_id != row.builder_actor_id ||
          change.condition_revision_after <=
              row.accepted_condition_head.condition_revision)
        continue;
      if (change.effective_at.kind != ActorFoundationAt::Kind::exact_moment)
        fail("ConstructionExactConditionChangeRequired", saved.first);
      const auto relation =
          compare(change.effective_at.exact_moment, entry.last_settled_at);
      if (relation > 0)
        continue;
      bool covered = false;
      for (const auto id : condition_changes)
        covered |= id == change.result_id;
      if (!covered && relation == 0 && ended)
        fail("ConstructionTerminalConditionOrderSourceRequired",
             change.result_id);
      if (!covered)
        fail("ConstructionConditionTransitionCoverageMismatch",
             change.result_id);
    }
    const auto now = take(x_.adoption.clock.read_current_moment());
    const bool due = std::holds_alternative<ElapsedRunning>(entry.timing) &&
                     take(compare_fractions(entry.credited_units.value,
                                            entry.required_units.value,
                                            x_.spatial.budget)) == 0;
    const auto intervals =
        take(due ? read_fixed_service_intervals_at_completion_due(
                       candidate, row.construction_id, entry.started_at,
                       entry.last_settled_at, now, x_.scale, x_.spatial.budget)
                 : read_fixed_service_intervals(candidate, row.construction_id,
                                                entry.started_at,
                                                entry.last_settled_at, now,
                                                x_.scale, x_.spatial.budget));
    take(validate_facility_construction_provided_intervals(
        x_, row.construction_id, intervals));
    return common_success();
  } catch (const Failure &failure) {
    return {false, {}, failure.error};
  }
}
Status NativeFacilityConstructionActionTimeRead::validate_shared_action_sources(
    const ActionTimeRegistration &, const ActionTimeOwnerProof &,
    const SharedWorkElapsedState &) const {
  return common_failure("ConstructionSharedWorkNotSupported", {},
                        ErrorKind::unsupported_capability);
}
Outcome<RunStop>
NativeFacilityConstructionActionTimeRead::read_action_continuation_stop(
    const ActionTimeRegistration &r, const ActionTimeOwnerProof &p,
    const WorldMoment &at) const {
  try {
    registered(r);
    if (compare(at, take(x_.adoption.clock.read_current_moment())))
      fail("ConstructionActualContinuationMomentRequired", p.action_ref);
    const auto actual = owner(x_, p.action_ref);
    proof(r, p, actual);
    return {true,
            actual.phase == ActionTimeOwnerPhase::terminal ? RunStop::completed
            : actual.phase == ActionTimeOwnerPhase::suspended
                ? RunStop::paused
                : RunStop::awaiting_permit,
            {}};
  } catch (const Failure &failure) {
    return {false, {}, failure.error};
  }
}
Outcome<OriginalActionRemaining>
NativeFacilityConstructionActionTimeRead::read_original_action_remaining(
    const ActionTimeRegistration &r, const ActionTimeOwnerProof &p,
    const WorldMoment &at) const {
  try {
    registered(r);
    if (compare(at, take(x_.adoption.clock.read_current_moment())))
      fail("ConstructionActualRemainingMomentRequired", p.action_ref);
    const auto actual = owner(x_, p.action_ref);
    proof(r, p, actual);
    const auto *entry = find_fixed_elapsed(x_.elapsed, p.action_ref);
    if (!entry)
      fail("ConstructionOriginalElapsedRequired", p.action_ref);
    auto remaining = take(scale_units_to_duration(
        take(subtract_work_units(entry->required_units, entry->credited_units,
                                 x_.spatial.budget)),
        x_.scale.units_per_half_day, x_.spatial.budget));
    return {true,
            ActualServiceRemaining{ActualServiceRemainingBuildView{
                remaining,
                {actual.original_execution_source.domain,
                 actual.original_execution_source.kind,
                 actual.original_execution_source.id},
                1}},
            {}};
  } catch (const Failure &failure) {
    return {false, {}, failure.error};
  }
}
Outcome<StableId> NativeFacilityConstructionActionTimeRead::read_gate_result_id(
    const SharedWorkGateResult &) const {
  return {false,
          {},
          common_failure("ConstructionSharedGateNotSupported", {},
                         ErrorKind::unsupported_capability)
              .error};
}
Outcome<SharedWorkGateResult>
NativeFacilityConstructionActionTimeRead::read_original_gate_result(
    const StableId &id) const {
  return {false,
          {},
          common_failure("ConstructionSharedGateNotSupported", id,
                         ErrorKind::unsupported_capability)
              .error};
}
Status NativeFacilityConstructionActionTimeRead::validate_original_gate_result(
    const SharedWorkGateResult &) const {
  return common_failure("ConstructionSharedGateNotSupported", {},
                        ErrorKind::unsupported_capability);
}
} // namespace hanmo::resident
