// 原治理批准与后继执行批准共用唯一权源算法；两者各保自己的实际签发行。
#pragma once
#include "governance_program.hpp"
namespace hanmo::resident {
struct GovernanceApprovalAuthorityCapture {
 GovernanceTargetApprovalRead target;
 SingleGrantAuthorityRequest request;
 SingleGrantAuthorityRead proof;
 BudgetAuthorizationRef budget;
};
Outcome<GovernanceApprovalAuthorityCapture> prepare_governance_approval_authority(
 const GovernanceProgramReadContext&,const GovernanceProgramSpec&,const GovernanceExecutor&,
 const ActingAuthority&,const BudgetExecutionRef&,Integer expected_target_revision,const ReadStamp&);
Status bind_governance_approval_authority_in_candidate(CommonStore&,OrganizationRuntime&,
 const GovernanceProgramReadContext&,const GovernanceApprovalAuthorityCapture&,const SourceRef&);
Status validate_governance_approval_authority(const GovernanceProgramReadContext&,
 const GovernanceProgramSpec&,const ActingAuthority&,const AuthorityBasisRef&,
 const BudgetExecutionRef&,const SourceRef& actual_source);
}
