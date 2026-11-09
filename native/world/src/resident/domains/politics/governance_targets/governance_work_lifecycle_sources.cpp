#include "resident/domains/military/duty/military_duty_assignment.hpp"
// 再委任和绝对期限是原已接受 Work 的真实上游源，不以 WorkResult
// 或今日空绑定自证。
#include "governance_work_lifecycle_sources.hpp"
#include "governance_work_deadline_stop_boundary.hpp"
#include "governance_approval_authority.hpp"
#include "resident/domains/tasks/work_execution/work_supervision_sources.hpp"
#include "resident/domains/tasks/work_execution/work_execution_detail.hpp"
#include <algorithm>
#include <limits>
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
void take(Status value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
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
bool same(const ActorFoundationRuleRef &a, const ActorFoundationRuleRef &b) {
  return a.id == b.id && a.version == b.version &&
         a.catalog_digest == b.catalog_digest;
}
bool same(const BudgetExecutionRef &a, const BudgetExecutionRef &b) {
  return a.budget_id == b.budget_id && a.budget_revision == b.budget_revision &&
         a.policy_revision == b.policy_revision &&
         a.approval_fact_id == b.approval_fact_id;
}
bool same(const BudgetPeriod &a, const BudgetPeriod &b) {
  return a.period_from_tick == b.period_from_tick &&
         a.period_until_tick == b.period_until_tick;
}
bool same(const ActingAuthority &a, const ActingAuthority &b) {
  return a.actor_id == b.actor_id && a.organization_id == b.organization_id &&
         a.office_id == b.office_id && a.authority_epoch == b.authority_epoch;
}
std::size_t pos(const CommonStore &c, const SourceRef &ref) {
  const auto *h = c.state().facts.find(ref.id);
  const auto &order = c.state().facts.order();
  const auto found = std::find(order.begin(), order.end(), ref.id);
  if (!h || h->id != ref.id || h->domain != ref.domain || h->kind != ref.kind ||
      found == order.end())
    fail("GovernanceWorkLifecycleSourceMissing", ref.id);
  return static_cast<std::size_t>(found - order.begin());
}
// 原 stamp、clock、Task 与 Patrol 都必须在同候选根，不能借另一个已验证图。
const TaskWorkExecutionState &
selected(const GovernanceWorkLifecycleSourceContext &x,
         const ReadStamp &stamp) {
  const auto &h = x.history;
  const auto &p = h.programs;
  const auto &c = x.continuity;
  if (p.state.schema_version != 3 || p.catalog.schema_version != 4 ||
      !h.tasks.work_execution ||
      !task_work_lifecycle_sources_version(h.tasks.work_execution->version))
    fail("GovernanceWorkLifecycleProfileRequired");
  if (&c.history != &h || &x.patrol.common != &p.common ||
      &x.patrol.tasks != &h.tasks ||
      &x.patrol.definitions != &h.task_definitions ||
      x.patrol.work_history != &h ||
      !h.task_definitions.bound_to(p.common, h.tasks) ||
      &x.patrol.duty_context.scale != &x.scale || &c.body.common != &p.common ||
      c.body.elapsed != &x.elapsed || c.body.scale != &x.scale ||
      cmp(take(p.clock.read_organization_authority_now()),
          take(x.patrol.clock.read_current_moment())) ||
      cmp(c.body.actual_now, take(p.clock.read_organization_authority_now())))
    fail("GovernanceWorkLifecycleCandidateMismatch");
  if (stamp.mode() != ReadMode::phase_current &&
      stamp.mode() != ReadMode::committed_current)
    fail("GovernanceWorkLifecycleActualStampRequired");
  take(c.stamp_authority.validate(stamp, p.common.state().identity,
                                  stamp.mode(), p.common, c.versions));
  return *h.tasks.work_execution;
}
const WorkEpisode &episode(const GovernanceWorkLifecycleSourceContext &x,
                           const StableId &id) {
  const auto *e = x.history.tasks.work_execution->episodes.find(id);
  if (!e || e->work_id != id)
    fail("GovernanceWorkLifecycleWorkMissing", id);
  return *e;
}
// 只排除实际 cut 以前的真实劳动终局，今天的后继结果不抹去原决定。
void unfinished(const GovernanceWorkLifecycleSourceContext &x,
                const WorkEpisode &e, const SourceRef *cut) {
  const auto &s = *x.history.tasks.work_execution;
  const auto &c = x.history.programs.common;
  const auto limit = cut ? pos(c, *cut) : c.state().facts.order().size();
  for (const auto &row : s.results.rows())
    if (row.second.work_id == e.work_id) {
      if (row.first != row.second.fact_id)
        fail("GovernanceWorkResultKeyMismatch", row.first);
      if (pos(c, {"tasks", work_detail::result_kind(row.second), row.first}) <
          limit)
        fail("GovernanceWorkAlreadyTerminal", e.work_id);
    }
  for (const auto &row :
       x.history.programs.state.work_deadline_terminations.rows())
    if (row.second.work_id == e.work_id && (!cut || row.first != cut->id) &&
        pos(c, {"politics", "governance_work_deadline_terminated", row.first}) <
            limit)
      fail("GovernanceWorkAlreadyTerminated", e.work_id);
}
// 原明确 permitted 集合不以空集通配；这一步不伪造实际身体、技能或到场。
void permitted(const GovernanceAcceptedWorkHistory &h, const WorkEpisode &e,
               const StableId &actor) {
  if (actor.empty() || !h.specification->approved_work_terms)
    fail("GovernanceWorkReplacementTermsRequired", e.work_id);
  const std::vector<StableId> *actors = nullptr;
  if (h.patrol) {
    const auto *p = std::get_if<GovernancePatrolWorkTerms>(
        &*h.specification->approved_work_terms);
    if (!p)
      fail("GovernanceWorkReplacementKindMismatch", e.work_id);
    actors = &p->permitted_leader_actor_ids;
  } else {
    const auto *p = std::get_if<GovernanceSupervisionWorkTerms>(
        &*h.specification->approved_work_terms);
    if (!p || !h.supervision || p->kind != h.supervision->kind)
      fail("GovernanceWorkReplacementKindMismatch", e.work_id);
    actors = &p->permitted_supervisor_actor_ids;
  }
  if (std::find(actors->begin(), actors->end(), actor) == actors->end())
    fail("GovernanceWorkReplacementNotPermitted", actor);
}
// 临时结果拥有真正 Duty 读回的值，不返回指向该临时的借用地址。
struct Boundary {
  bool active = false;
  std::optional<MomentOwnerSource> terminal;
  std::optional<WorldMoment> ended_at;
};
// 读取本次实际执行边界；缺失、交叉来源或终局时间错位仍按原规则拒绝。
Boundary
governance_execution_boundary(const GovernanceWorkLifecycleSourceContext &x,
                              const WorkEpisode &e, const WorldMoment &at,
                              const SourceRef *cut) {
  const auto &common = x.history.programs.common;
  Boundary out;
  if (std::holds_alternative<SupervisedWorkPlan>(e.frozen_plan)) {
    const auto b = take(read_supervision_execution_boundary_before_cut(
        common, x.history.tasks, x.elapsed, x.scale, e, cut));
    if (b.state == WorkExecutionBoundaryKind::unresolved)
      fail("GovernanceWorkExecutionBoundaryUnresolved", e.work_id);
    out.active = b.state == WorkExecutionBoundaryKind::active;
    if (b.state == WorkExecutionBoundaryKind::ended) {
      if (!b.terminal_header || !b.ended_at)
        fail("GovernanceWorkExecutionBoundaryMissing", e.work_id);
      out.terminal =
          MomentOwnerSource{{b.terminal_header->domain, b.terminal_header->kind,
                             b.terminal_header->id},
                            1};
      out.ended_at = *b.ended_at;
    }
  } else {
    const auto limit =
        cut ? pos(common, *cut) : common.state().facts.order().size();
    std::size_t latest = 0, covered = 0;
    for (const auto &row : x.patrol.duties.executions.rows()) {
      const auto *work =
          take(read_work_assigned_to_patrol_duty(x.patrol, row.first));
      if (work != &e)
        continue;
      // 先按实际绑定头筛 cut，再调用完整军事冷读；未来新 Duty
      // 不能反证自己的批准。
      const FactRecord *binding_head = nullptr;
      if (row.second.actual_binding) {
        binding_head = common.state().facts.find(row.second.actual_binding->id);
        if (!binding_head ||
            binding_head->id != row.second.actual_binding->id ||
            binding_head->domain != "military" ||
            binding_head->kind != "military_duty_execution_bound")
          fail("GovernancePatrolBindingMissing", row.second.actual_binding->id);
      } else
        for (const auto &station : x.patrol.duties.station_sources.rows())
          if (station.second.duty_action_id == row.first) {
            if (binding_head)
              fail("GovernancePatrolBindingAmbiguous", row.first);
            binding_head = common.state().facts.find(station.first);
            if (station.first != station.second.id || !binding_head ||
                binding_head->id != station.first ||
                binding_head->domain != "military" ||
                binding_head->kind != "military_station_source")
              fail("GovernancePatrolBindingMissing", station.first);
          }
      if (!binding_head)
        continue;
      const auto binding_order = pos(
          common, {binding_head->domain, binding_head->kind, binding_head->id});
      if (binding_order >= limit)
        continue;
      // 在可回读 Work 的完整 S2 reader 前，证明此次 assignment
      // 严格属于更早的实际源。
      const auto *accepted =
          x.patrol.duties.accepted_sources.find(row.second.accepted_source_ref);
      if (row.second.duty_action_id != row.first || !accepted ||
          accepted->id != row.second.accepted_source_ref ||
          accepted->duty_action_id != row.first)
        fail("GovernancePatrolAssignmentSourceMismatch", row.first);
      const auto assignment =
          take(read_military_duty_work_assignment_association(
              common, x.patrol.duties, accepted->original_assignment_source,
              accepted->assignment_source_schema,
              x.patrol.duty_context.selected_dynamic_military));
      if (!assignment.source || assignment.source_schema != 1)
        fail("GovernancePatrolAssignmentSourceMismatch", row.first);
      const auto terms_order =
          pos(common, {"military", "military_duty_terms", accepted->id});
      const auto assignment_order =
          pos(common, accepted->original_assignment_source);
      if (assignment.assignment &&
          pos(common, *assignment.source) >= assignment_order)
        fail("GovernancePatrolAssignmentSourceOrder", row.first);
      if (assignment_order >= terms_order || terms_order >= binding_order)
        fail("GovernancePatrolAssignmentSourceOrder", row.first);
      auto actual = take(read_military_duty_execution(
          common, x.patrol.duties, x.patrol.duty_context, row.first));
      if (!actual)
        fail("GovernancePatrolBindingMissing", row.first);
      const auto order = pos(common, actual->actual_binding_source.source_ref);
      if (order >= limit)
        continue;
      if (std::count_if(
              e.original_execution_sources.begin(),
              e.original_execution_sources.end(), [&](const auto &source) {
                return work_detail::same(source, actual->actual_binding_source);
              }) != 1)
        fail("GovernancePatrolExecutionCoverage", e.work_id);
      ++covered;
      if (!actual->terminal ||
          pos(common, actual->terminal->actual_source.source_ref) >= limit) {
        out.active = true;
        continue;
      }
      if (order >= latest) {
        latest = order;
        out.terminal = actual->terminal->actual_source;
        out.ended_at = actual->terminal->ended_at;
      }
    }
    std::size_t expected = 0;
    for (const auto &source : e.original_execution_sources)
      if (pos(common, source.source_ref) < limit)
        ++expected;
    if (covered != expected)
      fail("GovernancePatrolExecutionCoverage", e.work_id);
  }
  if (!cut && e.current_execution && !out.active)
    fail("GovernanceWorkExecutionBindingNotClosed", e.work_id);
  if (out.ended_at && cmp(*out.ended_at, at) > 0)
    fail("GovernanceWorkTerminalAfterDecision", e.work_id);
  return out;
}
void continuity(const GovernanceWorkLifecycleSourceContext &x,
                const GovernanceAcceptedWorkHistory &h, const WorkEpisode &,
                const WorldMoment &at, const SourceRef *cut) {
  const auto &c = x.continuity;
  GovernanceOrganizationContinuityContext original{x.history.programs,
                                                   c.lifecycle_catalog,
                                                   c.lifecycle_sources,
                                                   c.office_sources,
                                                   c.deaths,
                                                   c.original_sources_stamp,
                                                   c.stamp_authority,
                                                   c.versions};
  take(validate_governance_reassignment_continuity(original, *h.approval,
                                                   *h.specification, at, cut));
}
// 校核原再委任源与实际终局；不匹配时保留原失败结果。
const GovernanceWorkReassignmentApproval &
reassignment(const GovernanceWorkLifecycleSourceContext &x,
             const SourceRef &ref, const ReadStamp &stamp) {
  selected(x, stamp);
  const auto &p = x.history.programs;
  const auto *a = p.state.work_reassignment_approvals.find(ref.id);
  if (ref.domain != "politics" ||
      ref.kind != "governance_work_reassignment_approved" || !a ||
      a->id != ref.id)
    fail("GovernanceWorkReassignmentSourceMissing", ref.id);
  const auto cut = pos(p.common, ref);
  const auto &e = episode(x, a->work_id);
  const auto h = take(read_governance_accepted_work_history(x.history, e));
  if (!work_detail::same(a->authorization, e.authorization) ||
      !work_detail::same(a->work_acceptance_ref,
                         {"tasks", "work_accepted", e.acceptance_source_ref}) ||
      !same(a->spec_ref, h.specification->spec_ref) ||
      a->kind != work_detail::kind(e.frozen_plan) ||
      !same(a->budget_execution, h.approval->budget_execution) ||
      !same(a->budget_period, h.approval->budget_period) ||
      a->issuer.organization_id != h.approval->issuer.organization_id ||
      a->expected_target_revision < 0 ||
      cmp(a->issued_at, h.work_acceptance->accepted_at) < 0 ||
      cmp(a->issued_at, take(p.clock.read_organization_authority_now())) > 0 ||
      pos(p.common, a->work_acceptance_ref) >= cut)
    fail("GovernanceWorkReassignmentMismatch", ref.id);
  if (h.supervision &&
      cmp(a->issued_at,
          WorldMoment{h.supervision->completion_deadline.tick, {}}) >= 0)
    fail("GovernanceWorkReassignmentOutsideWindow", e.work_id);
  const auto prior =
      take(read_governance_work_executor_before_cut(x.history, e, &ref));
  if (!prior.actor_id || !prior.predecessor ||
      a->old_actor_id != *prior.actor_id ||
      a->new_actor_id == a->old_actor_id ||
      a->executor_predecessor_ref.source_schema != 1 ||
      !work_detail::same(a->executor_predecessor_ref.source_ref,
                         {prior.predecessor->domain, prior.predecessor->kind,
                          prior.predecessor->id}))
    fail("GovernanceWorkReassignmentPredecessorMismatch", ref.id);
  permitted(h, e, a->new_actor_id);
  unfinished(x, e, &ref);
  const auto b = governance_execution_boundary(x, e, a->issued_at, &ref);
  if (b.active ||
      b.terminal.has_value() != a->preceding_terminal_ref.has_value() ||
      (b.terminal &&
       !work_detail::same(*b.terminal, *a->preceding_terminal_ref)))
    fail("GovernanceWorkReassignmentTerminalMismatch", ref.id);
  if (a->preceding_terminal_ref &&
      pos(p.common, a->preceding_terminal_ref->source_ref) >= cut)
    fail("GovernanceWorkReassignmentBeforeTerminal", ref.id);
  take(validate_governance_approval_authority(p, *h.specification, a->issuer,
                                              a->authority_basis_ref,
                                              a->budget_execution, ref));
  take(p.originals.validate_historical_governance_work_reassignment(
      *h.approval, *a, *h.specification));
  continuity(x, h, e, a->issued_at, &ref);
  return *a;
}
// 同一只读请求核供预检和候选 writer 使用；continuation 只在本栈同步消费借用。
template <class Existing, class Fresh>
auto reassignment_request(const GovernanceWorkLifecycleSourceContext &x,
                          const ApproveGovernanceWorkReassignment &request,
                          const ReadStamp &stamp, Existing &&existing,
                          Fresh &&fresh) {
  const auto &p = x.history.programs;
  const auto &e = episode(x, request.work_id);
  for (const auto &row : p.state.work_reassignment_approvals.rows())
    if (row.second.work_id == e.work_id &&
        work_detail::same(row.second.executor_predecessor_ref,
                          request.expected_executor_predecessor_ref) &&
        row.second.new_actor_id == request.new_actor_id) {
      if (!same(row.second.issuer, request.authority) ||
          row.second.expected_target_revision !=
              request.expected_target_revision)
        fail("GovernanceWorkReassignmentReplayChanged", row.first);
      auto ref = governance_work_reassignment_source(row.second);
      (void)reassignment(x, ref, stamp);
      return existing(std::move(ref));
    }
  const auto now = take(p.clock.read_organization_authority_now());
  const auto h = take(read_governance_accepted_work_history(x.history, e));
  const auto prior =
      take(read_governance_work_executor_before_cut(x.history, e, nullptr));
  if (!prior.actor_id || !prior.predecessor ||
      request.new_actor_id == *prior.actor_id ||
      request.expected_executor_predecessor_ref.source_schema != 1 ||
      !work_detail::same(request.expected_executor_predecessor_ref.source_ref,
                         {prior.predecessor->domain, prior.predecessor->kind,
                          prior.predecessor->id}) ||
      request.authority.organization_id != h.approval->issuer.organization_id)
    fail("GovernanceWorkReassignmentStale", e.work_id);
  if (h.supervision &&
      cmp(now, WorldMoment{h.supervision->completion_deadline.tick, {}}) >= 0)
    fail("GovernanceWorkReassignmentOutsideWindow", e.work_id);
  unfinished(x, e, nullptr);
  permitted(h, e, request.new_actor_id);
  const auto b = governance_execution_boundary(x, e, now, nullptr);
  if (b.active)
    fail("GovernanceWorkExecutionStillActive", e.work_id);
  continuity(x, h, e, now, nullptr);
  auto authority = take(prepare_governance_approval_authority(
      p, *h.specification, GovernanceNamedExecutor{request.new_actor_id},
      request.authority, h.approval->budget_execution,
      request.expected_target_revision, stamp));
  if (!same(authority.target.budget_period, h.approval->budget_period) ||
      authority.target.payer_wallet_id != h.approval->payer_wallet_id ||
      authority.target.contract_debtor.kind !=
          h.approval->contract_debtor.kind ||
      authority.target.contract_debtor.id != h.approval->contract_debtor.id)
    fail("GovernanceWorkOriginalExpenseBasisChanged", e.work_id);
  return fresh(e, h, prior, b, now, authority);
}
// 原 Patrol 的每个实际 Duty 窗须在决定之前关闭并由同一 Work 收据接入。
// 不用当前 last_result/phase 或未验证的 credited 值猜出“没有完成”。
void settled_patrol_deadline_prefix(
    const GovernanceWorkLifecycleSourceContext &x, const WorkEpisode &e,
    const WorldMoment &at, const SourceRef *cut) {
  if (!std::holds_alternative<PatrolPlan>(e.frozen_plan))
    return;
  const auto &common = x.history.programs.common;
  const auto &state = *x.history.tasks.work_execution;
  const auto limit =
      cut ? pos(common, *cut) : common.state().facts.order().size();
  std::vector<StableId> covered;
  Integer actual_credited = 0;
  for (const auto &row : x.patrol.duties.windows.rows()) {
    const auto &window = row.second;
    if (take(read_work_assigned_to_patrol_duty(
            x.patrol, window.window.duty_action_id)) != &e)
      continue;
    const auto opened =
        pos(common, {"military", "military_duty_window", row.first});
    if (opened >= limit)
      continue;
    if (window.id != row.first || cmp(window.window.opened_at, at) > 0)
      fail("GovernancePatrolDeadlineWindowMismatch", row.first);
    const auto *closed = std::get_if<DutyWindowClosed>(&window.state);
    if (!closed)
      fail("GovernancePatrolDeadlinePendingWindow", row.first);
    WorkMilitaryPeriodSource source{
        {"military", "military_duty_period", closed->result_ref}, 2};
    const auto period_order = pos(common, source.fact_ref);
    if (period_order <= opened || period_order >= limit)
      fail("GovernancePatrolDeadlinePendingPeriod", row.first);
    const auto actual = take(read_patrol_work_period(x.patrol, e, source));
    if (cmp(actual.actual_until, at) > 0)
      fail("GovernancePatrolDeadlinePendingSettlement", row.first);
    const WorkPeriodReceipt2 *receipt = nullptr;
    for (const auto &id : e.accepted_period_refs) {
      const auto *candidate = state.period_receipts.find(id);
      if (!candidate || candidate->fact_id != id ||
          candidate->work_id != e.work_id)
        fail("GovernanceWorkDeadlinePeriodCoverage", e.work_id);
      const auto *original =
          std::get_if<WorkMilitaryPeriodSource>(&candidate->source);
      if (!original || !work_detail::same(original->fact_ref, source.fact_ref))
        continue;
      if (receipt || original->source_schema != 2)
        fail("GovernancePatrolDeadlinePeriodAmbiguous", id);
      const auto adopted = pos(common, {"tasks", "work_period_receipt", id});
      if (adopted <= period_order || adopted >= limit)
        fail("GovernancePatrolDeadlinePendingPeriod", id);
      receipt = candidate;
    }
    if (!receipt || cmp(receipt->window.from, actual.window.from) ||
        cmp(receipt->window.to, actual.window.to) ||
        cmp(receipt->actual_until, actual.actual_until) ||
        receipt->leader_actor_id != actual.leader_actor_id ||
        !work_detail::same(receipt->execution_source, actual.execution_source))
      fail("GovernancePatrolDeadlineReceiptSourceMismatch", closed->result_ref);
    covered.push_back(receipt->fact_id);
    const auto &gate = work_detail::deadline(e.frozen_plan);
    if (actual.source_provided &&
        take(calendar_deadline_accepts(gate.tick, gate.inclusive,
                                       actual.actual_until))) {
      if (actual_credited == std::numeric_limits<Integer>::max())
        fail("GovernanceWorkDeadlineCreditOverflow", e.work_id);
      ++actual_credited;
    }
  }
  // 原窗口双向覆盖：不能凭造一个 Work 收据，或漏掉已发生 Duty 周期终止。
  for (const auto &row : state.period_receipts.rows())
    if (row.second.work_id == e.work_id &&
        pos(common, {"tasks", "work_period_receipt", row.first}) < limit)
      if (std::count(covered.begin(), covered.end(), row.first) != 1)
        fail("GovernancePatrolDeadlinePeriodCoverage", row.first);
  if (actual_credited >= work_detail::required(e.frozen_plan))
    fail("GovernanceWorkDeadlineCompletionNotSettled", e.work_id);
}
// 截止源只消费已发生的劳动前缀。真实 period 已结束但尚未纳入 Work
// 时不得抢先终止。
void settled_deadline_prefix(const GovernanceWorkLifecycleSourceContext &x,
                             const WorkEpisode &e, const WorldMoment &at,
                             const SourceRef *cut) {
  const auto &s = *x.history.tasks.work_execution;
  const auto &common = x.history.programs.common;
  const auto limit =
      cut ? pos(common, *cut) : common.state().facts.order().size();
  settled_patrol_deadline_prefix(x, e, at, cut);
  Integer credited = 0;
  for (const auto &row : s.period_receipts.rows())
    if (row.second.work_id == e.work_id &&
        pos(common, {"tasks", "work_period_receipt", row.first}) < limit) {
      if (row.first != row.second.fact_id ||
          std::count(e.accepted_period_refs.begin(),
                     e.accepted_period_refs.end(), row.first) != 1 ||
          row.second.credited_half_days < 0 ||
          row.second.credited_half_days > 1 ||
          cmp(row.second.actual_until, at) > 0 ||
          credited > std::numeric_limits<Integer>::max() -
                         row.second.credited_half_days)
        fail("GovernanceWorkDeadlinePeriodCoverage", e.work_id);
      credited += row.second.credited_half_days;
    }
  // 满劳动量仍待原目标结项时，不用未出现结果推断不能完成。
  if (credited >= work_detail::required(e.frozen_plan))
    fail("GovernanceWorkDeadlineCompletionNotSettled", e.work_id);
  for (const auto &row : s.supervision_starts.rows())
    if (row.second.work_id == e.work_id &&
        pos(common, {"tasks", "supervision_action_started", row.first}) <
            limit) {
      const auto *elapsed = find_fixed_elapsed(x.elapsed, row.second.action_id);
      if (!elapsed)
        fail("GovernanceWorkDeadlineElapsedMissing", row.first);
      // 只以该 cut 前真实终态缩短服务前缀；今日后继终态不改更早决定的
      // coverage。
      const WorldMoment *ended_at = nullptr;
      for (const auto &closed : s.supervision_window_closures.rows())
        if (closed.second.start_source_ref == row.first &&
            pos(common, {"tasks", "supervision_window_closed", closed.first}) <
                limit) {
          if (ended_at || closed.first != closed.second.fact_id)
            fail("GovernanceWorkDeadlineTerminalCoverage", row.first);
          ended_at = &closed.second.closed_at;
        }
      for (const auto &ended : s.supervision_terminals.rows())
        if (ended.second.start_source_ref == row.first &&
            pos(common, {"tasks", "supervision_action_ended", ended.first}) <
                limit) {
          if (ended_at || ended.first != ended.second.fact_id)
            fail("GovernanceWorkDeadlineTerminalCoverage", row.first);
          ended_at = &ended.second.ended_at;
        }
      const auto &through = ended_at ? *ended_at : at;
      if (cmp(through, row.second.started_at) < 0 || cmp(through, at) > 0 ||
          cmp(elapsed->last_settled_at, through) < 0)
        fail("GovernanceWorkDeadlinePendingSettlement", row.first);
      WorldMoment cursor = row.second.started_at;
      for (const auto &id : e.accepted_period_refs) {
        const auto *receipt = s.period_receipts.find(id);
        if (!receipt || receipt->work_id != e.work_id)
          fail("GovernanceWorkDeadlinePeriodCoverage", e.work_id);
        const auto *binding =
            std::get_if<WorkCommonActionBinding>(&receipt->execution_source);
        if (!binding || binding->action_id != row.second.action_id ||
            pos(common, {"tasks", "work_period_receipt", id}) >= limit)
          continue;
        if (cmp(receipt->window.from, cursor) ||
            cmp(receipt->actual_until, cursor) < 0 ||
            cmp(receipt->actual_until, through) > 0)
          fail("GovernanceWorkDeadlinePendingPeriod", e.work_id);
        cursor = receipt->actual_until;
      }
      if (cmp(cursor, through))
        fail("GovernanceWorkDeadlinePendingPeriod", e.work_id);
    }
}
// 校核原截止终止源及已结算前缀；未结算或来源不符时拒绝。
const GovernanceWorkDeadlineTermination &
deadline(const GovernanceWorkLifecycleSourceContext &x, const SourceRef &ref,
         const ReadStamp &stamp) {
  selected(x, stamp);
  const auto &p = x.history.programs;
  const auto *a = p.state.work_deadline_terminations.find(ref.id);
  if (ref.domain != "politics" ||
      ref.kind != "governance_work_deadline_terminated" || !a ||
      a->id != ref.id)
    fail("GovernanceWorkDeadlineSourceMissing", ref.id);
  pos(p.common, ref);
  const auto &e = episode(x, a->work_id);
  const auto h = take(read_governance_accepted_work_history(x.history, e));
  if ((!h.supervision && !h.patrol) ||
      !work_detail::same(a->authorization, e.authorization) ||
      !work_detail::same(a->work_acceptance_ref,
                         {"tasks", "work_accepted", e.acceptance_source_ref}) ||
      !same(a->spec_ref, h.specification->spec_ref) ||
      cmp(a->absolute_deadline,
          {work_detail::deadline(e.frozen_plan).tick, {}}) ||
      cmp(a->effective_at, a->absolute_deadline) < 0 ||
      cmp(a->effective_at, take(p.clock.read_organization_authority_now())) >
          0 ||
      pos(p.common, a->work_acceptance_ref) >= pos(p.common, ref))
    fail("GovernanceWorkDeadlineMismatch", ref.id);
  unfinished(x, e, &ref);
  const auto boundary =
      governance_execution_boundary(x, e, a->effective_at, &ref);
  if (h.patrol && boundary.active)
    fail("GovernanceWorkExecutionStillActive", e.work_id);
  settled_deadline_prefix(x, e, a->effective_at, &ref);
  for (const auto &row : p.state.work_deadline_terminations.rows())
    if (&row.second != a && row.second.work_id == e.work_id)
      fail("DuplicateGovernanceWorkDeadline", e.work_id);
  return *a;
}
} // namespace
Status validate_governance_work_lifecycle_bindings(
    const GovernanceWorkLifecycleSourceContext &x, const ReadStamp &stamp) {
  return guard<Done>([&] {
    selected(x, stamp);
    return Done{};
  });
}
// 只验证同候选的再委任接受边界；仍在执行或时点不符时拒绝。
Status validate_governance_work_reassignment_acceptance_boundary(
    const GovernanceWorkLifecycleSourceContext &x, const WorkEpisode &e,
    const WorldMoment &at, const SourceRef *cut, const ReadStamp &stamp) {
  return guard<Done>([&] {
    const auto &state = selected(x, stamp);
    const auto &p = x.history.programs;
    if (state.episodes.find(e.work_id) != &e)
      fail("GovernanceWorkAcceptanceBoundaryWorkMismatch", e.work_id);
    if (cut) {
      const auto *acceptance =
          state.work_reassignment_acceptances.find(cut->id);
      if (cut->domain != "tasks" || cut->kind != "work_reassignment_accepted" ||
          !acceptance || acceptance->fact_id != cut->id ||
          acceptance->work_id != e.work_id || cmp(acceptance->accepted_at, at))
        fail("GovernanceWorkAcceptanceBoundaryCutRequired", cut->id);
      pos(p.common, *cut);
    } else if (cmp(at, take(p.clock.read_organization_authority_now())))
      fail("GovernanceWorkAcceptanceBoundaryActualMomentRequired", e.work_id);
    unfinished(x, e, cut);
    const auto actual = governance_execution_boundary(x, e, at, cut);
    if (actual.active)
      fail("GovernanceWorkExecutionStillActive", e.work_id);
    return Done{};
  });
}
// Task Stop 只借真实 deadline 决定及其真实 result（若已产生）复核安全执行边界。
Status validate_governance_work_deadline_stop_boundary(
    const GovernanceWorkLifecycleSourceContext &x, const WorkEpisode &e,
    const MomentOwnerSource &cause, const WorldMoment &at,
    const ReadStamp &stamp) {
  return guard<Done>([&] {
    const auto &state = selected(x, stamp);
    if (state.episodes.find(e.work_id) != &e || cause.source_schema != 1)
      fail("GovernanceWorkDeadlineStopBindingMismatch", e.work_id);
    const auto &decision = deadline(x, cause.source_ref, stamp);
    if (decision.work_id != e.work_id || cmp(decision.effective_at, at))
      fail("GovernanceWorkDeadlineStopMismatch", e.work_id);
    SourceRef result_ref;
    const SourceRef *cut = nullptr;
    if (e.terminal_result_ref) {
      const auto *result = state.results.find(*e.terminal_result_ref);
      if (!result || result->fact_id != *e.terminal_result_ref ||
          result->work_id != e.work_id || !result->actual_stop_source ||
          !work_detail::same(*result->actual_stop_source, cause) ||
          cmp(result->finished_at, at))
        fail("GovernanceWorkDeadlineResultMismatch", e.work_id);
      result_ref = {"tasks", work_detail::result_kind(*result),
                    result->fact_id};
      cut = &result_ref;
      if (pos(x.history.programs.common, cause.source_ref) >=
          pos(x.history.programs.common, *cut))
        fail("GovernanceWorkDeadlineResultOrder", e.work_id);
    }
    const auto actual = governance_execution_boundary(x, e, at, cut);
    if (actual.active)
      fail("GovernanceWorkExecutionStillActive", e.work_id);
    settled_deadline_prefix(x, e, at, cut);
    return Done{};
  });
}
SourceRef governance_work_reassignment_source(
    const GovernanceWorkReassignmentApproval &a) {
  return {"politics", "governance_work_reassignment_approved", a.id};
}
SourceRef governance_work_deadline_termination_source(
    const GovernanceWorkDeadlineTermination &a) {
  return {"politics", "governance_work_deadline_terminated", a.id};
}
Outcome<const GovernanceWorkReassignmentApproval *>
read_governance_work_reassignment(const GovernanceWorkLifecycleSourceContext &x,
                                  const SourceRef &ref,
                                  const ReadStamp &stamp) {
  return guard<const GovernanceWorkReassignmentApproval *>(
      [&] { return &reassignment(x, ref, stamp); });
}
Outcome<const GovernanceWorkDeadlineTermination *>
read_governance_work_deadline_termination(
    const GovernanceWorkLifecycleSourceContext &x, const SourceRef &ref,
    const ReadStamp &stamp) {
  return guard<const GovernanceWorkDeadlineTermination *>(
      [&] { return &deadline(x, ref, stamp); });
}
Status validate_governance_work_reassignment_request(
    const GovernanceWorkLifecycleSourceContext &x,
    const ApproveGovernanceWorkReassignment &request, const ReadStamp &stamp) {
  return guard<Done>([&] {
    selected(x, stamp);
    return reassignment_request(
        x, request, stamp, [](const SourceRef &) { return Done{}; },
        [](const WorkEpisode &, const GovernanceAcceptedWorkHistory &,
           const auto &, const Boundary &, const WorldMoment &,
           const GovernanceApprovalAuthorityCapture &) { return Done{}; });
  });
}
Outcome<SourceRef> approve_governance_work_reassignment_in_candidate(
    CommonStore &common, OrganizationRuntime &runtime,
    GovernanceProgramState &state,
    const GovernanceWorkLifecycleSourceContext &x,
    const ApproveGovernanceWorkReassignment &request, const ReadStamp &stamp) {
  return guard<SourceRef>([&] {
    selected(x, stamp);
    const auto &p = x.history.programs;
    if (&common != &p.common || &runtime != &p.organization_runtime ||
        &state != &p.state)
      fail("GovernanceWorkLifecycleCandidateMismatch");
    return reassignment_request(
        x, request, stamp,
        [](SourceRef &&ref) -> SourceRef { return std::move(ref); },
        [&](const WorkEpisode &e, const GovernanceAcceptedWorkHistory &h,
            const auto &prior, const Boundary &b, const WorldMoment &now,
            const GovernanceApprovalAuthorityCapture &authority) -> SourceRef {
          GovernanceWorkReassignmentApproval a;
          a.work_id = e.work_id;
          a.authorization = e.authorization;
          a.work_acceptance_ref = {"tasks", "work_accepted",
                                   e.acceptance_source_ref};
          a.executor_predecessor_ref =
              request.expected_executor_predecessor_ref;
          a.preceding_terminal_ref = b.terminal;
          a.kind = work_detail::kind(e.frozen_plan);
          a.old_actor_id = *prior.actor_id;
          a.new_actor_id = request.new_actor_id;
          a.spec_ref = h.specification->spec_ref;
          a.issuer = request.authority;
          a.budget_execution = h.approval->budget_execution;
          a.budget_period = h.approval->budget_period;
          a.expected_target_revision = request.expected_target_revision;
          a.issued_at = now;
          const auto ref =
              take(append_governance_work_lifecycle_fact_in_candidate(
                  common, GovernanceWorkLifecycleFactKind::reassignment));
          a.id = ref.id;
          a.authority_basis_ref = {ref, 1};
          if (!state.work_reassignment_approvals.insert(ref.id, std::move(a)))
            fail("DuplicateGovernanceWorkReassignment", ref.id);
          take(bind_governance_approval_authority_in_candidate(
              common, runtime, p, authority, ref));
          (void)reassignment(x, ref, stamp);
          return ref;
        });
  });
}
// 在同候选登记已到期且已结算的终止源；不满足原前提时拒绝。
Outcome<SourceRef> record_governance_work_deadline_termination_in_candidate(
    CommonStore &common, GovernanceProgramState &state,
    const GovernanceWorkLifecycleSourceContext &x, const StableId &work,
    const ReadStamp &stamp) {
  return guard<SourceRef>([&] {
    selected(x, stamp);
    const auto &p = x.history.programs;
    if (&common != &p.common || &state != &p.state)
      fail("GovernanceWorkLifecycleCandidateMismatch");
    const auto &e = episode(x, work);
    for (const auto &row : state.work_deadline_terminations.rows())
      if (row.second.work_id == work) {
        const auto ref =
            governance_work_deadline_termination_source(row.second);
        (void)deadline(x, ref, stamp);
        return ref;
      }
    const auto now = take(p.clock.read_organization_authority_now());
    const auto h = take(read_governance_accepted_work_history(x.history, e));
    if (!h.supervision && !h.patrol)
      fail("GovernanceWorkDeadlineRequired", work);
    const WorldMoment absolute{work_detail::deadline(e.frozen_plan).tick, {}};
    if (cmp(now, absolute) < 0)
      fail("GovernanceWorkDeadlineNotReached", work);
    unfinished(x, e, nullptr);
    const auto boundary = governance_execution_boundary(x, e, now, nullptr);
    if (h.patrol && boundary.active)
      fail("GovernanceWorkExecutionStillActive", e.work_id);
    settled_deadline_prefix(x, e, now, nullptr);
    GovernanceWorkDeadlineTermination a;
    a.work_id = work;
    a.authorization = e.authorization;
    a.work_acceptance_ref = {"tasks", "work_accepted", e.acceptance_source_ref};
    a.spec_ref = h.specification->spec_ref;
    a.absolute_deadline = absolute;
    a.effective_at = now;
    const auto ref = take(append_governance_work_lifecycle_fact_in_candidate(
        common, GovernanceWorkLifecycleFactKind::deadline));
    a.id = ref.id;
    if (!state.work_deadline_terminations.insert(ref.id, std::move(a)))
      fail("DuplicateGovernanceWorkDeadline", ref.id);
    (void)deadline(x, ref, stamp);
    return ref;
  });
}
Status validate_governance_work_lifecycle_sources(
    const GovernanceWorkLifecycleSourceContext &x, const ReadStamp &stamp) {
  return guard<Done>([&] {
    selected(x, stamp);
    for (const auto &row :
         x.history.programs.state.work_reassignment_approvals.rows()) {
      if (row.first != row.second.id)
        fail("GovernanceWorkReassignmentKeyMismatch", row.first);
      (void)reassignment(x, governance_work_reassignment_source(row.second),
                         stamp);
    }
    for (const auto &row :
         x.history.programs.state.work_deadline_terminations.rows()) {
      if (row.first != row.second.id)
        fail("GovernanceWorkDeadlineKeyMismatch", row.first);
      (void)deadline(x, governance_work_deadline_termination_source(row.second),
                     stamp);
    }
    for (const auto &row : x.history.programs.common.state().facts.rows())
      if (row.second.domain == "politics" &&
          ((row.second.kind == "governance_work_reassignment_approved" &&
            !x.history.programs.state.work_reassignment_approvals.find(
                row.first)) ||
           (row.second.kind == "governance_work_deadline_terminated" &&
            !x.history.programs.state.work_deadline_terminations.find(
                row.first))))
        fail("GovernanceWorkLifecyclePayloadMissing", row.first);
    return Done{};
  });
}
} // namespace hanmo::resident
