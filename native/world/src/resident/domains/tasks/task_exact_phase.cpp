#include "message_notices/task_message_notice_world.hpp"
#include "resident/domains/social/social_relation_execution.hpp"
#include "resident/kernel/exact_complete_world.hpp"
#include "resident/kernel/native_task_work_catalog.hpp"
#include "resident/kernel/political_takeover_execution_world.hpp"
#include "resident/kernel/world_bundle.hpp"
#include "task_exact_execution.hpp"
#include "work_execution/work_exact_runtime.hpp"
namespace hanmo::resident {
namespace {
template <class T> Outcome<T> bad(const char *c, const StableId &id = {}) {
  return {false, {}, common_failure(c, id).error};
}
Status selected(const WorldBundle &w) {
  return w.world_timeline && w.action_elapsed && w.tasks.exact_execution &&
                 w.catalog->tasks
             ? common_success()
             : common_failure("TaskExactSourcePairRequired");
}
const TaskExactTerminalSource *terminal(const TaskState &s,
                                        const StableId &action) {
  const TaskExactTerminalSource *out = nullptr;
  for (const auto &row : s.exact_execution->terminals.rows())
    if (row.second.action_id == action) {
      if (out)
        return nullptr;
      out = &row.second;
    }
  return out;
}
} // namespace
ExactPhaseRegistration make_task_exact_phase_registration() {
  return make_task_exact_phase_registration(TaskWorkWorldFactory{});
}
ExactPhaseRegistration
make_task_exact_phase_registration(TaskWorkWorldFactory work, Integer version) {
  ExactPhaseRegistration r;
  r.provider = ExactProviderId::tasks;
  r.profile_id = "task_execution_time2";
  r.version = version;
  r.stages = {ExactStage::interval_motion_work, ExactStage::action_completions};
  r.interval_writer = true;
  r.effect_writer_keys = {"task_fixed_action_progress",
                          "task_acceptance_handover_result"};
  if (work.read || work.write) {
    r.stages.push_back(ExactStage::work_inspection);
    r.stages.push_back(ExactStage::political_due_close);
    r.causal_edges.push_back(
        {ExactStage::work_inspection, ExactStage::political_due_close});
    r.effect_writer_keys.push_back("original_work_deadline_termination");
    r.effect_writer_keys.push_back("original_supervision_work_period");
    r.effect_writer_keys.push_back("original_patrol_work_period");
    r.effect_writer_keys.push_back("original_patrol_checkpoint_completion");
    r.effect_writer_keys.push_back("original_supervision_fixed_execution");
  }
  r.validate_author_selection = [work, version](const WorldBundle &w) {
    auto v = selected(w);
    if (!v.ok)
      return v;
    if (!task_exact_execution_version(version) ||
        w.tasks.exact_execution->version != version)
      return common_failure("TaskExactExecutionSelectionMismatch");
    auto layout = validate_native_task_work_layout(*w.catalog, w.tasks);
    if (!layout.ok)
      return layout;
    if (w.tasks.work_execution && (!work.read || !work.write))
      return common_failure("TaskWorkWorldFactoryRequired");
    return common_success();
  };
  r.validate_world_sources = [work](const WorldBundle &w,
                                    const WorldMoment &now) {
    auto s = selected(w);
    if (!s.ok)
      return s;
    std::unique_ptr<CompleteExactWorldBindingOwner> owner;
    std::optional<CharacterConditionSources> body;
    std::optional<TaskExactLifeSources> life;
    if (w.tasks.exact_execution->version == 2) {
      auto lease = w.complete_exact_time_bindings();
      if (!lease.ok)
        return Status{false, {}, lease.error};
      owner = std::move(lease.value);
      if (!owner)
        return common_failure("TaskExactOriginalLifeSourcesRequired");
      auto sources = owner->reads().character_conditions.sources();
      if (!sources.ok)
        return Status{false, {}, sources.error};
      body.emplace(std::move(sources.value));
      if (&body->common != &w.common || body->deaths != &w.family_life ||
          !body->life_sources)
        return common_failure("TaskExactLifeCandidateMismatch");
      life.emplace(TaskExactLifeSources{w.common, w.family_life,
                                        *body->life_sources, body->actual_now});
    }
    s = validate_task_exact_execution(
        w.common, w.tasks, *w.catalog->tasks, *w.action_elapsed, now,
        w.world_timeline->scale_ref, w.catalog->identity,
        life ? &*life : nullptr);
    if (!s.ok)
      return s;
    // 普通 Task 的原读写不借 Work；未选 Work
    // 仍反扫事实、行动和生命周期结果，不能以空指针冒真空。
    return w.tasks.work_execution
               ? validate_supervision_world_sources(w, now, work)
               : validate_native_task_work_world(w);
  };
  r.predict = [work](const WorldBundle &w, const WorldMoment &from,
                     const WorldMoment &through)
      -> Outcome<std::vector<ExactEventPrediction>> {
    auto s = selected(w);
    if (!s.ok)
      return {false, {}, s.error};
    std::vector<ExactEventPrediction> out;
    for (const auto &row : w.tasks.exact_execution->starts.rows()) {
      const auto &source = row.second;
      if (terminal(w.tasks, source.action_id))
        continue;
      const auto *entry =
          find_fixed_elapsed(*w.action_elapsed, source.action_id);
      const auto *running =
          entry ? std::get_if<ElapsedRunning>(&entry->timing) : nullptr;
      if (!running)
        return bad<std::vector<ExactEventPrediction>>(
            "TaskExactRunningSourceRequired", source.action_id);
      auto lo = compare_world_moments(from, running->completion_due_at),
           hi = compare_world_moments(running->completion_due_at, through);
      if (!lo.ok)
        return {false, {}, lo.error};
      if (!hi.ok)
        return {false, {}, hi.error};
      if (lo.value > 0)
        return bad<std::vector<ExactEventPrediction>>(
            "TaskExactCompletionOverdue", source.action_id);
      if (hi.value <= 0)
        out.push_back({running->completion_due_at,
                       ExactStage::action_completions,
                       {"tasks", "task_action_started", source.id},
                       1,
                       0});
    }
    if (w.tasks.work_execution) {
      auto pending = predict_supervision_work(w, from, through, work);
      if (!pending.ok)
        return {false, {}, pending.error};
      for (auto &event : pending.value)
        out.push_back(std::move(event));
    }
    return {true, std::move(out), {}};
  };
  r.provide_interval = [work](WorldBundle &w, const MomentSpan &span,
                              ExactPhaseWriteContext &) {
    auto s = selected(w);
    if (!s.ok)
      return s;
    for (const auto &row : w.tasks.exact_execution->starts.rows()) {
      const auto &source = row.second;
      if (terminal(w.tasks, source.action_id))
        continue;
      auto *entry = find_fixed_elapsed(*w.action_elapsed, source.action_id);
      const auto *actual = w.common.action(source.action_id);
      if (!entry || !actual)
        return common_failure("TaskExactIntervalOwnerMissing",
                              source.action_id);
      auto portion = prepare_fixed_elapsed_slice(*entry, span,
                                                 w.world_timeline->scale_ref);
      if (!portion.ok)
        return Status{false, {}, portion.error};
      entry->credited_units = std::move(portion.value.credited_units);
      entry->last_settled_at = std::move(portion.value.last_settled_at);
      auto remaining =
          fixed_elapsed_remaining_ticks(*entry, w.world_timeline->scale_ref);
      if (!remaining.ok)
        return Status{false, {}, remaining.error};
      ActionMutation mutation;
      mutation.action = *actual;
      mutation.remaining_ticks = remaining.value;
      s = w.common.apply_action(mutation);
      if (!s.ok)
        return s;
    }
    return w.tasks.work_execution
               ? provide_supervision_action_interval(w, span, work)
               : common_success();
  };
  r.apply_due =
      [work](WorldBundle &w, const ExactEventPrediction &p,
             ExactPhaseWriteContext &context) -> Outcome<ExactEventApplied> {
    auto s = selected(w);
    if (!s.ok)
      return {false, {}, s.error};
    if (p.stage == ExactStage::work_inspection ||
        p.stage == ExactStage::political_due_close)
      return apply_supervision_work_due(w, p, work, context);
    if (p.stage != ExactStage::action_completions || p.source_schema != 1 ||
        p.actual_source_basis.domain != "tasks" ||
        p.actual_source_basis.kind != "task_action_started")
      return bad<ExactEventApplied>("TaskExactDueSourceMismatch",
                                    p.actual_source_basis.id);
    const auto *begun =
        w.tasks.exact_execution->starts.find(p.actual_source_basis.id);
    const auto *actual = begun ? w.common.action(begun->action_id) : nullptr;
    const auto *entry =
        actual ? find_fixed_elapsed(*w.action_elapsed, actual->id) : nullptr;
    const auto *running =
        entry ? std::get_if<ElapsedRunning>(&entry->timing) : nullptr;
    if (!begun || !actual || !running || terminal(w.tasks, actual->id))
      return bad<ExactEventApplied>("TaskExactDueOwnerMissing",
                                    p.actual_source_basis.id);
    auto due = compare_world_moments(running->completion_due_at, p.at);
    if (!due.ok)
      return {false, {}, due.error};
    if (due.value || actual->remaining_ticks != 0)
      return bad<ExactEventApplied>("TaskExactDueMomentMismatch", actual->id);
    const auto action = *actual;
    TaskExactWrite write{*w.action_elapsed, w.world_timeline->scale_ref, p.at,
                         w.catalog->identity};
    // The selected source-producing relation writer belongs to this exact
    // mutable world lease. Keep it alive through the real terminal and Action
    // release.
    std::unique_ptr<CompleteExactWorldBindingOwner> relation_owner;
    std::optional<CharacterConditionSources> body;
    std::optional<TaskExactLifeSources> life;
    if (w.social.original_relation_version == 2 ||
        w.tasks.exact_execution->version == 2) {
      auto lease = w.complete_exact_time_bindings();
      if (!lease.ok)
        return {false, {}, lease.error};
      relation_owner = std::move(lease.value);
      if (!relation_owner)
        return {false,
                {},
                common_failure("TaskOriginalRelationOwningLeaseRequired",
                               action.id, ErrorKind::unsupported_capability)
                    .error};
      if (w.tasks.exact_execution->version == 2) {
        auto sources = relation_owner->reads().character_conditions.sources();
        if (!sources.ok)
          return {false, {}, sources.error};
        body.emplace(std::move(sources.value));
        if (&body->common != &w.common || body->deaths != &w.family_life ||
            !body->life_sources)
          return bad<ExactEventApplied>("TaskExactLifeCandidateMismatch",
                                        action.id);
        life.emplace(TaskExactLifeSources{
            w.common, w.family_life, *body->life_sources, body->actual_now});
        write.life = &*life;
      }
      if (w.social.original_relation_version == 2) {
        write.report_relations =
            relation_owner->original_task_report_relation_writer();
        if (!write.report_relations)
          return {false,
                  {},
                  common_failure("TaskOriginalRelationWriteRequired", action.id,
                                 ErrorKind::unsupported_capability)
                      .error};
        if (!write.report_relations->bound_to(w.common, w.social, w.tasks))
          return {false,
                  {},
                  common_failure("TaskOriginalRelationCandidateMismatch",
                                 action.id, ErrorKind::internal)
                      .error};
      }
      auto now = w.actual_phase_moment();
      if (!now.ok)
        return {false, {}, now.error};
      auto actual_at = compare_world_moments(now.value, p.at);
      if (!actual_at.ok)
        return {false, {}, actual_at.error};
      if (actual_at.value)
        return {false,
                {},
                common_failure("TaskOriginalRelationActualMomentMismatch",
                               action.id, ErrorKind::internal)
                    .error};
    }
    // These original readers observe the current candidate. No invented
    // one-half-day DynamicStepInput is passed at fractional M.
    auto takeover = PoliticalTakeoverExecutionWorldAccess::create(w);
    if (!takeover.ok)
      return {false, {}, takeover.error};
    auto notice = make_task_message_notice_services(w);
    if (!notice.ok)
      return {false, {}, notice.error};
    auto returns = TaskMessageReturnWorldAccess::create(w);
    if (!returns.ok)
      return {false, {}, returns.error};
    auto personal = TaskPersonalMessageWorldAccess::create(
        w, nullptr,
        relation_owner ? &relation_owner->reads().character_conditions
                       : nullptr);
    if (!personal.ok)
      return {false, {}, personal.error};
    auto reports = TaskPersonalReportWorldAccess::create(
        w, nullptr,
        relation_owner ? &relation_owner->reads().character_conditions
                       : nullptr);
    if (!reports.ok)
      return {false, {}, reports.error};
    auto acceptances = TaskPersonalAcceptanceWorldAccess::create(
        w, nullptr,
        relation_owner ? &relation_owner->reads().character_conditions
                       : nullptr);
    if (!acceptances.ok)
      return {false, {}, acceptances.error};

    auto finished = finish_task_action(
        w.common, w.tasks, w.social, *w.catalog->tasks, w.interaction,
        *w.catalog->interaction, w.services.interaction_access(), action,
        p.at.half_day_tick, w.dynamic_enabled() ? &w.affairs : nullptr,
        takeover.value->tasks(), notice.value ? &*notice.value : nullptr,
        returns.value ? &returns.value->services() : nullptr,
        personal.value ? &personal.value->services() : nullptr,
        reports.value ? &reports.value->services() : nullptr,
        acceptances.value ? &acceptances.value->services() : nullptr, &write);
    if (!finished.ok)
      return {false, {}, finished.error};
    const auto *result = terminal(w.tasks, action.id);
    if (!result)
      return bad<ExactEventApplied>("TaskExactAppliedTerminalMissing",
                                    action.id);
    const StableId result_id = result->id;
    ActionCompletion completion;
    completion.completed = true;
    completion.success = finished.value.success;
    completion.task = std::move(finished.value);
    if (!context.advance.completed_actions.insert(action.id,
                                                  std::move(completion)))
      return bad<ExactEventApplied>("DuplicateTaskExactAdvanceResult",
                                    action.id);
    s = release_action(w, action.id);
    if (!s.ok)
      return {false, {}, s.error};
    return {
        true,
        {{"tasks", "task_action_finished", result_id},
         task_exact_terminal_source_schema(w.tasks.exact_execution->version)},
        {}};
  };
  r.validate_applied_source = [work](const WorldBundle &w,
                                     const ExactEventApplied &source) {
    if (source.applied_result.domain == "tasks" &&
        (supervision_source_kind(source.applied_result.kind) ||
         source.applied_result.kind == "work_period_receipt" ||
         source.applied_result.kind == "supervised_work_result" ||
         source.applied_result.kind == "patrol_work_result"))
      return validate_supervision_applied_source(w, source, work);
    if (source.applied_result.kind != "task_action_finished")
      return common_failure("TaskExactAppliedKindMismatch",
                            source.applied_result.id);
    auto at = read_task_exact_source_moment(
        w.common, w.tasks, source.applied_result, source.source_schema);
    if (!at.ok)
      return Status{false, {}, at.error};
    const auto *result =
        w.tasks.exact_execution->terminals.find(source.applied_result.id);
    if (!result || w.common.action(result->action_id) ||
        w.tasks.actions.find(result->action_id))
      return common_failure("TaskExactAppliedActionNotReleased",
                            source.applied_result.id);
    return common_success();
  };
  return r;
}
} // namespace hanmo::resident
