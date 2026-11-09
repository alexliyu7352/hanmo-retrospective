#pragma once
#include "work_execution_contract.hpp"
#include "resident/domains/local_economy/governance_work/governance_work_expense_contract.hpp"
namespace hanmo::resident {
// This lease only composes the existing S5 accept writer in the S3 candidate.
// It does not create a fee policy, approve money, or retain a ReadStamp.
class NativeWorkExpenseAcceptanceWrite final:public WorkExpenseAcceptanceWrite {
 CommonStore& common_;GovernanceWorkExpenseState& expenses_;const GovernanceWorkExpenseOriginalRead& originals_;
 const WorkFeeQuote& quote_;const CurrentMomentRead& clock_;const DomainReadVersionSource& versions_;
 BudgetExecutionState& budget_;const BudgetAuthorizationRead& authorization_;const BudgetResponsibilityRead& responsibilities_;const GovernanceWorkExpenseReadContext& context_;
public:
 NativeWorkExpenseAcceptanceWrite(CommonStore&common,GovernanceWorkExpenseState&expenses,const GovernanceWorkExpenseOriginalRead&originals,const WorkFeeQuote&quote,const CurrentMomentRead&clock,const DomainReadVersionSource&versions,BudgetExecutionState&budget,const BudgetAuthorizationRead&authorization,const BudgetResponsibilityRead&responsibilities,const GovernanceWorkExpenseReadContext&context):common_(common),expenses_(expenses),originals_(originals),quote_(quote),clock_(clock),versions_(versions),budget_(budget),authorization_(authorization),responsibilities_(responsibilities),context_(context){}
 Outcome<SourceRef> accept_work_expense(const StableId&,const SourceRef&,const WorkAcceptedRead&,const WorldMoment&)override;
 Status finalize_work_expense_acceptance(const WorkAcceptanceSource&)override;
};
}
