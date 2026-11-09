// IC8 原 Task 终局与原 S5 财务证据的只读共同结果；无服务/退款新根。
#pragma once
#include "service_actual_termination_result.hpp"
#include "resident/domains/tasks/work_execution/work_commission_terminal.hpp"
#include "resident/domains/local_economy/governance_work/governance_work_expense_contract.hpp"
namespace hanmo::resident {
// termination_id is the existing globally unique Task terminal fact ID. It is
// neither a newly allocated Common ID nor an independently writable terminal.
// Returns the explicit no-advance result /4; nested service identities stay /3.
Outcome<OwnedServiceActualTerminationResult> read_work_service_actual_termination(
 const WorkCommissionTerminalContext&,const GovernanceWorkExpenseReadContext&,
 const S3WorkCommission&,const WorkCommissionTerminal&,const ReadStamp&);
}
