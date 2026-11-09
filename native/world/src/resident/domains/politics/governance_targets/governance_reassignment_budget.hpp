// 再委任只核同一原批准所钉住的预算历史；不替代目标、接受或工作资格。
#pragma once
#include "governance_budget_sources.hpp"
namespace hanmo::resident {
// 仅接受实际 state/3 保留行；不能伪造旧批准更换 executor 后借旧历史过检。
Status validate_historical_governance_reassignment_budget(
    const GovernanceBudgetSourceContext &, const GovernanceProgramSpec &,
    const GovernanceProgramApproval &,
    const GovernanceWorkReassignmentApproval &);
} // namespace hanmo::resident
