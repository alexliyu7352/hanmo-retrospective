// 原GovernanceProgramState中的后继执行批准；不拥有Work、费用或elapsed副本。
#pragma once
#include "governance_supervision_scope.hpp"
#include "resident/domains/tasks/work_execution/work_supervision_sources.hpp"
namespace hanmo::resident {
// 工厂先造原readers，最后在实际调用栈拼此借用；不捕获循环provider或跨clone缓存。
struct GovernanceSupervisionApprovalValidationContext {
 const GovernanceWorkHistoryContext& history;
 const GovernanceSupervisionContinuityRead& continuity;
 const SupervisionSourceContext& executions;
};
struct ApproveGovernanceSupervisionExecution {
 ActingAuthority authority;StableId work_id;Integer execution_ordinal=0;
 ActorFoundationRuleRef quote_ref;Integer expected_target_revision=0;
 std::optional<SourceRef> successor_acceptance_ref;
};
// 命令只选已登记quote和本Work下一ordinal；H、原债主体及预算均由原源取回。
Status validate_governance_supervision_execution_request(
 const GovernanceSupervisionApprovalValidationContext&,const ApproveGovernanceSupervisionExecution&,
 const StableId& authenticated_actor,const ReadStamp&);
Outcome<SourceRef> approve_governance_supervision_execution_in_candidate(
 CommonStore&,OrganizationRuntime&,GovernanceProgramState&,
 const GovernanceSupervisionApprovalValidationContext&,const ApproveGovernanceSupervisionExecution&,
 const StableId& authenticated_actor,const ReadStamp&);
// 验原实际签发、单Grant、真实前序终局和同版quote；依赖只降至ordinal-1。
Outcome<const GovernanceSupervisionExecutionApproval*> read_verified_governance_supervision_approval(
 const GovernanceSupervisionApprovalValidationContext&,const SourceRef&);
Status validate_governance_supervision_approvals(const GovernanceSupervisionApprovalValidationContext&);
}
