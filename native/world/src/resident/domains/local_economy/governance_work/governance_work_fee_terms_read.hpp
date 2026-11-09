// Concrete quote/acceptance adapter. It does not evaluate service, move money,
// invent cost legs, or turn a Work result into a Task financial terminal.
#pragma once
#include "governance_work_fee_policy.hpp"
#include "resident/domains/tasks/work_execution/work_task_sources.hpp"
namespace hanmo::resident {
// Non-service cost owners must prove canonical original responsibilities and
// included/separate coverage. No adapter is needed when no such legs were quoted.
class GovernanceWorkOriginalCostRead {
public:virtual ~GovernanceWorkOriginalCostRead()=default;
 virtual Status validate_work_original_cost_coverage(const ProposedWorkExpenseTerms&,const WorldMoment& accepted_or_quote_at,bool existing_acceptance)const=0;
};
struct GovernanceWorkFeeTermsContext {
 const WorkTaskSourceContext& task_sources;const GovernanceWorkFeeCatalog& catalog;
 const GovernanceWorkExpenseState& expenses;const GovernanceWorkOriginalCostRead* costs=nullptr;
};
class NativeGovernanceWorkFeeTermsRead {
 GovernanceWorkFeeTermsContext x_;WorkTaskRead tasks_;
public:
 explicit NativeGovernanceWorkFeeTermsRead(GovernanceWorkFeeTermsContext x):x_(x),tasks_(x.task_sources){}
 Outcome<ProposedWorkExpenseTerms> read_work_fee_offer(const PrepareWorkFeeQuote&,const ReadStamp&)const;
 Status validate_work_expense_acceptance(const ProposedWorkExpenseTerms&,const WorkExpenseAcceptance&,const WorldMoment&,bool existing_source)const;
 Status validate_work_fee_cost_coverage(const GovernanceWorkExpenseTerms&)const;
};
}
