// Original actual termination: preserve earned debts and release unpaid reservations.
#pragma once
#include "governance_work_expense_contract.hpp"
namespace hanmo::resident {
Outcome<Integer> read_work_fee_reserved_cash(const GovernanceWorkExpenseState&,const GovernanceWorkExpenseTerms&,const LocalEconomyState*);
Outcome<bool> read_work_scope_financial_termination(const GovernanceWorkExpenseState&,const GovernanceWorkExpenseTerms&);
Outcome<SourceRef> consume_work_service_actual_termination_in_candidate(CommonStore&,LocalEconomyState&,GovernanceWorkExpenseState&,const GovernanceWorkExpenseReadContext&,const S3WorkCommission&,const WorkCommissionTerminal&);
Status validate_governance_work_settlements(const GovernanceWorkExpenseReadContext&);
Outcome<std::optional<WorldMoment>> read_governance_work_settlement_moment(const GovernanceWorkExpenseReadContext&,const SourceRef&);
}
