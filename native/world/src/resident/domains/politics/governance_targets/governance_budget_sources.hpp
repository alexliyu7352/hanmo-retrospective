// 原预算批准的真实签发截面；不建立BudgetExecutionBinding或新增应付/预付款。
#pragma once
#include "governance_program.hpp"
#include "resident/domains/politics/budget/political_budget_read.hpp"
namespace hanmo::resident {
struct GovernanceBudgetSourceContext {
 const CommonStore& common;const PoliticalBudgetState& state;const PoliticalBudgetRead& budgets;
 const GovernanceProgramState& programs;const OrganizationAuthorityClock& clock;
 const ReadStampAuthority& stamp_authority;const DomainReadVersionSource& versions;
};
// 只解决Work型目标的原预算腿。专业目标、资源recipe、队伍/资质/同意仍由其原owner验证。
Outcome<GovernanceTargetApprovalRead> read_current_governance_work_budget(
 const GovernanceBudgetSourceContext&,const GovernanceProgramSpec&,const BudgetExecutionRef&,
 const StableId& actual_payer_wallet,const ReadStamp&);
Status validate_historical_governance_work_budget(const GovernanceBudgetSourceContext&,
 const GovernanceProgramSpec&,const GovernanceProgramApproval&);
Status validate_historical_governance_supervision_budget(const GovernanceBudgetSourceContext&,
 const GovernanceProgramSpec&,const GovernanceProgramApproval&,const GovernanceSupervisionExecutionApproval&);
}
