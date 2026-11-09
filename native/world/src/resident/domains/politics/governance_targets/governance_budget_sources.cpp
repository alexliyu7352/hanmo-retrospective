#include "governance_budget_sources.hpp"
#include "governance_reassignment_budget.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const StableId &code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> v) {
  if (!v.ok)
    throw Failure{v.error};
  return std::move(v.value);
}
template <class T, class F> Outcome<T> guard(F &&f) {
  try {
    return {true, f(), {}};
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
int cmp(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b));
}
bool period(const BudgetPeriod &a, const BudgetPeriod &b) {
  return a.period_from_tick == b.period_from_tick &&
         a.period_until_tick == b.period_until_tick;
}
bool same_budget(const BudgetExecutionRef &a, const BudgetExecutionRef &b) {
  return a.budget_id == b.budget_id && a.budget_revision == b.budget_revision &&
         a.policy_revision == b.policy_revision &&
         a.approval_fact_id == b.approval_fact_id;
}
std::size_t pos(const CommonStore &c, const SourceRef &source) {
  const auto *h = c.state().facts.find(source.id);
  const auto &order = c.state().facts.order();
  const auto at = std::find(order.begin(), order.end(), source.id);
  if (!valid_source_ref(source) || !h || h->domain != source.domain ||
      h->kind != source.kind || at == order.end())
    fail("GovernanceBudgetOriginalSourceMissing", source.id);
  return static_cast<std::size_t>(at - order.begin());
}
const BudgetAuthorizationSource &
read(const GovernanceBudgetSourceContext &x, const GovernanceProgramSpec &spec,
     const BudgetExecutionRef &ref, const StableId &wallet,
     const WorldMoment &at, const SourceRef *cut) {
  if (!x.budgets.bound_to(x.common, x.state))
    fail("GovernanceBudgetCandidateMismatch");
  if (cmp(take(x.budgets.read_organization_authority_now()),
          take(x.clock.read_organization_authority_now())))
    fail("GovernanceBudgetClockMismatch");
  if (!spec.approved_work_terms || !spec.expense_policy_ref ||
      (spec.target.index() != 1 && spec.target.index() != 2 &&
       spec.target.index() != 4 && spec.target.index() != 6))
    fail("GovernanceOriginalWorkBudgetRequired", spec.spec_ref.id);
  // 唯一预算owner全验权源/修订/父子容量；不用今日retained_cap反证旧批准。
  take(x.budgets.validate_sources());
  const auto *s =
      find_budget_authorization(x.state, ref.budget_id, ref.budget_revision);
  if (!s || s->closed || s->policy_revision != ref.policy_revision ||
      s->approval_source.id != ref.approval_fact_id ||
      s->scope.kind != spec.scope.kind || s->scope.id != spec.scope.id ||
      wallet.empty() || !x.common.wallet(wallet) ||
      at.half_day_tick < s->period.period_from_tick ||
      at.half_day_tick >= s->period.period_until_tick ||
      cmp(s->approved_at, at) > 0 ||
      cmp(at, take(x.clock.read_organization_authority_now())) > 0)
    fail("GovernanceWorkBudgetApprovalMismatch", ref.budget_id);
  if (std::find(s->purposes.begin(), s->purposes.end(),
                BudgetExecutionPurpose::governance_work_expense) ==
      s->purposes.end())
    fail("GovernanceWorkBudgetPurposeDenied", ref.budget_id);
  bool money = false;
  for (const auto &cap : s->total_caps)
    if (std::holds_alternative<BudgetMoneyQian>(cap.unit) &&
        std::find(cap.container_refs.begin(), cap.container_refs.end(),
                  wallet) != cap.container_refs.end())
      money = true;
  if (!money || std::find(s->container_refs.begin(), s->container_refs.end(),
                          wallet) == s->container_refs.end())
    fail("GovernanceWorkBudgetWalletDenied", wallet);
  const auto boundary =
      cut ? pos(x.common, *cut) : x.common.state().facts.order().size();
  if (pos(x.common, s->approval_source) >= boundary)
    fail("GovernanceWorkBudgetSourceOrder", ref.budget_id);
  for (const auto &next : x.state.authorization_history)
    if (next.budget_id == s->budget_id &&
        next.budget_revision > s->budget_revision &&
        pos(x.common, next.approval_source) < boundary)
      fail("GovernanceWorkBudgetWasNotCurrentAtIssue", ref.budget_id);
  if (!cut) {
    if (current_budget_authorization(x.state, s->budget_id) != s)
      fail("GovernanceWorkBudgetChanged", s->budget_id);
    const auto *actual = x.common.wallet(wallet);
    if (actual->owner.kind != Owner::Kind::organization ||
        actual->owner.id != s->principal_org_id)
      fail("GovernanceWorkBudgetPayerOwnershipChanged", wallet);
  }
  return *s;
}
void frozen(const BudgetAuthorizationSource &s,
            const GovernanceProgramApproval &a) {
  if (!period(s.period, a.budget_period) ||
      a.contract_debtor.kind != SubjectKind::organization ||
      a.contract_debtor.id != s.principal_org_id ||
      a.issuer.organization_id != s.principal_org_id)
    fail("GovernanceWorkFrozenBudgetMismatch", a.id);
}
} // namespace
Outcome<GovernanceTargetApprovalRead> read_current_governance_work_budget(
    const GovernanceBudgetSourceContext &x, const GovernanceProgramSpec &spec,
    const BudgetExecutionRef &ref, const StableId &wallet,
    const ReadStamp &stamp) {
  return guard<GovernanceTargetApprovalRead>([&] {
    if (stamp.mode() != ReadMode::phase_current &&
        stamp.mode() != ReadMode::committed_current)
      fail("GovernanceWorkBudgetCurrentStampRequired");
    take(x.stamp_authority.validate(stamp, x.common.state().identity,
                                    stamp.mode(), x.common, x.versions));
    const auto now = take(x.clock.read_organization_authority_now());
    const auto &s = read(x, spec, ref, wallet, now, nullptr);
    bool covered = false;
    for (const auto &r : stamp.resources())
      if (r.resource.kind == ResourceKey::Kind::wallet &&
          r.resource.id == wallet)
        covered = true;
    if (!covered ||
        std::find(stamp.source_ids().begin(), stamp.source_ids().end(),
                  s.approval_source.id) == stamp.source_ids().end())
      fail("GovernanceWorkBudgetStampCoverage", s.budget_id);
    return GovernanceTargetApprovalRead{
        s.period,
        wallet,
        {SubjectKind::organization, s.principal_org_id},
        {{ResourceKey::Kind::wallet, wallet}}};
  });
}
Status validate_historical_governance_work_budget(
    const GovernanceBudgetSourceContext &x, const GovernanceProgramSpec &spec,
    const GovernanceProgramApproval &a) {
  return guard<Done>([&] {
    if (x.programs.approvals.find(a.id) != &a ||
        a.spec_ref.id != spec.spec_ref.id ||
        a.spec_ref.version != spec.spec_ref.version ||
        a.spec_ref.catalog_digest != spec.spec_ref.catalog_digest)
      fail("GovernanceWorkActualApprovalRequired", a.id);
    const auto source = governance_program_source(a);
    const auto &s = read(x, spec, a.budget_execution, a.payer_wallet_id,
                         a.issued_at, &source);
    frozen(s, a);
    return Done{};
  });
}
Status validate_historical_governance_supervision_budget(
    const GovernanceBudgetSourceContext &x, const GovernanceProgramSpec &spec,
    const GovernanceProgramApproval &original,
    const GovernanceSupervisionExecutionApproval &a) {
  return guard<Done>([&] {
    take(validate_historical_governance_work_budget(x, spec, original));
    if ((x.programs.schema_version != 2 && x.programs.schema_version != 3) ||
        x.programs.supervision_execution_approvals.find(a.id) != &a ||
        !same_source_ref(a.authorization.program_ref,
                         governance_program_source(original)) ||
        !same_budget(a.budget_execution, original.budget_execution) ||
        !period(a.budget_period, original.budget_period) ||
        a.spec_ref.id != spec.spec_ref.id ||
        a.spec_ref.version != spec.spec_ref.version ||
        a.spec_ref.catalog_digest != spec.spec_ref.catalog_digest ||
        !spec.approved_work_terms ||
        !std::holds_alternative<GovernanceSupervisionWorkTerms>(
            *spec.approved_work_terms))
      fail("GovernanceSupervisionBudgetOriginalMismatch", a.id);
    const auto source = governance_supervision_execution_approval_source(a);
    const auto &s = read(x, spec, a.budget_execution, original.payer_wallet_id,
                         a.issued_at, &source);
    if (a.issuer.organization_id != s.principal_org_id)
      fail("GovernanceSupervisionBudgetIssuerMismatch", a.id);
    frozen(s, original);
    return Done{};
  });
}
// 同一原预算读核按再委任自己的真实 source cut 反查；不重签或消费预算。
Status validate_historical_governance_reassignment_budget(
    const GovernanceBudgetSourceContext &x, const GovernanceProgramSpec &spec,
    const GovernanceProgramApproval &original,
    const GovernanceWorkReassignmentApproval &approval) {
  return guard<Done>([&] {
    take(validate_historical_governance_work_budget(x, spec, original));
    if (x.programs.schema_version != 3 ||
        x.programs.work_reassignment_approvals.find(approval.id) != &approval ||
        !same_source_ref(approval.authorization.program_ref,
                         governance_program_source(original)) ||
        !same_budget(approval.budget_execution, original.budget_execution) ||
        !period(approval.budget_period, original.budget_period) ||
        approval.spec_ref.id != spec.spec_ref.id ||
        approval.spec_ref.version != spec.spec_ref.version ||
        approval.spec_ref.catalog_digest != spec.spec_ref.catalog_digest)
      fail("GovernanceReassignmentBudgetOriginalMismatch", approval.id);
    const auto source = governance_work_reassignment_source(approval);
    const auto &budget =
        read(x, spec, approval.budget_execution, original.payer_wallet_id,
             approval.issued_at, &source);
    if (approval.issuer.organization_id != budget.principal_org_id)
      fail("GovernanceReassignmentBudgetIssuerMismatch", approval.id);
    frozen(budget, original);
    return Done{};
  });
}

} // namespace hanmo::resident
