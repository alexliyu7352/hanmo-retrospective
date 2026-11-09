#include "work_lifecycle_original.hpp"
#include "resident/domains/politics/governance_targets/governance_work_deadline_stop_boundary.hpp"
#include "work_execution_detail.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void reject(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
template <class T, class F> Outcome<T> guard(F &&f) {
  try {
    return {true, f(), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
int cmp(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b));
}
std::size_t position(const CommonStore &c, const SourceRef &source) {
  const auto *h = c.state().facts.find(source.id);
  const auto &ids = c.state().facts.order();
  auto at = std::find(ids.begin(), ids.end(), source.id);
  if (!h || h->id != source.id || h->domain != source.domain ||
      h->kind != source.kind || at == ids.end())
    reject("WorkLifecycleOriginalSourceMissing", source.id);
  return static_cast<std::size_t>(at - ids.begin());
}
} // namespace
// deadline 与真实 execution 安全终局分开证明，原 WorkResult
// 不能作上游释放依据。
Outcome<WorkStopRead> NativeGovernanceWorkLifecycleRead::read_actual_work_stop(
    const WorkEpisode &e, const MomentOwnerSource &cause,
    const WorldMoment &at) const {
  if (cause.source_ref.domain != "politics" ||
      cause.source_ref.kind != "governance_work_deadline_terminated") {
    if (other_originals_)
      return other_originals_->read_actual_work_stop(e, cause, at);
    // 中断、死亡或撤权的头本身不证明本 Work 的永久停止。
    return {false,
            {},
            common_failure("WorkStopOriginalSourceUnsupported",
                           cause.source_ref.id,
                           ErrorKind::unsupported_capability)
                .error};
  }
  return guard<WorkStopRead>([&] {
    if (cause.source_schema != 1)
      reject("WorkDeadlineSourceSchemaMismatch", cause.source_ref.id);
    // 原 S6 核已接受期限及每个真实执行窗口、Work 前缀；具名读口自行选择
    // 已存在的结果切口，不接任意调用者边界，也不把 Duty 到期冒为安全停止。
    take(validate_governance_work_deadline_stop_boundary(
        sources_.governance, e, cause, at, sources_.original_sources_stamp));
    return WorkStopRead{WorkStopReason::deadline_elapsed, cause, true};
  });
}
// 此处核实际三源关联；原 router 随后完整读取同一
// execution，保留原身体/权限/到场验证。
Status NativeGovernanceWorkLifecycleRead::validate_actual_work_leader_change(
    const WorkEpisode &e, const WorkLeaderChange &change) const {
  if (!sources_.tasks.tasks.work_execution ||
      !task_work_lifecycle_sources_version(
          sources_.tasks.tasks.work_execution->version)) {
    if (other_originals_)
      return other_originals_->validate_actual_work_leader_change(e, change);
    // 未选 /6 时不从当前控制人或空历史补造真正的替任授权。
    return common_failure("WorkReplacementOriginalSourceUnsupported", e.work_id,
                          ErrorKind::unsupported_capability);
  }
  return guard<Done>([&] {
    const auto &t = sources_.tasks;
    const auto &state = *t.tasks.work_execution;
    if (state.episodes.find(e.work_id) != &e || change.work_id != e.work_id ||
        change.authority_source.source_schema != 1 ||
        change.actual_consent_source.source_schema != 1 ||
        change.original_execution_source.source_schema != 1 ||
        !work_detail::same(change.actual_arrival_source,
                           change.original_execution_source))
      reject("WorkReplacementOriginalJoinRequired", e.work_id);
    SourceRef cut;
    const SourceRef *boundary = nullptr;
    if (!change.fact_id.empty()) {
      if (state.leader_changes.find(change.fact_id) != &change)
        reject("WorkReplacementRetainedSourceRequired", change.fact_id);
      cut = {"tasks", "work_leader_changed", change.fact_id};
      position(t.common, cut);
      boundary = &cut;
    }
    const auto *authority = take(read_governance_work_reassignment(
        sources_.governance, change.authority_source.source_ref,
        sources_.original_sources_stamp));
    const auto *accepted = take(read_work_reassignment_acceptance(
        sources_, change.actual_consent_source.source_ref));
    const auto current = take(read_governance_work_executor_before_cut(
        sources_.governance.history, e, boundary));
    if (!current.actor_id || !current.predecessor ||
        authority->work_id != e.work_id ||
        authority->old_actor_id != *current.actor_id ||
        authority->old_actor_id != change.old_actor_id ||
        authority->new_actor_id != change.new_actor_id ||
        accepted->new_actor_id != change.new_actor_id ||
        !work_detail::same(accepted->reassignment_approval_ref,
                           change.authority_source.source_ref) ||
        !work_detail::same(authority->executor_predecessor_ref.source_ref,
                           {current.predecessor->domain,
                            current.predecessor->kind,
                            current.predecessor->id}) ||
        cmp(accepted->earliest_service_at, change.replacement_at) > 0 ||
        cmp(change.replacement_at, take(t.clock.read_current_moment())) > 0)
      reject("WorkReplacementOriginalIdentityMismatch", e.work_id);
    if (position(t.common, change.actual_consent_source.source_ref) >=
            position(t.common, change.original_execution_source.source_ref) ||
        (boundary &&
         position(t.common, change.original_execution_source.source_ref) >=
             position(t.common, *boundary)))
      reject("WorkReplacementOriginalOrderMismatch", e.work_id);
    if (std::holds_alternative<SupervisedWorkPlan>(e.frozen_plan)) {
      const auto *start = state.supervision_starts.find(
          change.original_execution_source.source_ref.id);
      const auto *binding =
          std::get_if<WorkCommonActionBinding>(&change.new_execution_source);
      if (change.original_execution_source.source_ref.domain != "tasks" ||
          change.original_execution_source.source_ref.kind !=
              "supervision_action_started" ||
          !start || start->work_id != e.work_id ||
          start->actor_id != change.new_actor_id || !binding ||
          binding->action_id != start->action_id ||
          cmp(start->started_at, change.replacement_at))
        reject("WorkReplacementActualStartMismatch", e.work_id);
      const auto identity =
          take(read_governance_supervision_executor_identity_at(
              sources_.governance.history, e, start->started_at,
              &change.original_execution_source.source_ref));
      if (identity.acceptance != accepted || !identity.actor_id ||
          *identity.actor_id != change.new_actor_id)
        reject("WorkReplacementAcceptedStartMismatch", e.work_id);
    } else {
      const auto actual = take(read_patrol_work_execution(
          sources_.governance.patrol, e, change.original_execution_source));
      if (actual.actual_leader_actor_id != change.new_actor_id ||
          !work_detail::same(actual.binding, change.new_execution_source) ||
          cmp(actual.started_at, change.replacement_at))
        reject("WorkReplacementActualDutyMismatch", e.work_id);
    }
    return Done{};
  });
}
} // namespace hanmo::resident
