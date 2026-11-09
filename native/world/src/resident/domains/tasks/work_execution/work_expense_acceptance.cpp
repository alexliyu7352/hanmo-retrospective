#include "work_expense_acceptance.hpp"
#include "work_execution_detail.hpp"
namespace hanmo::resident {
// 费用冻结只消费本次真实 Task 接受及已经分配的 Work 引用；此时 Work
// 尚未插表，禁止在这里调用要求历史 Work 已存在的预算责任 reader。
Outcome<SourceRef> NativeWorkExpenseAcceptanceWrite::accept_work_expense(
 const StableId& work,const SourceRef& acceptance,const WorkAcceptedRead& accepted,
 const WorldMoment& at){
 auto now=clock_.read_current_moment();if(!now.ok)return {false,{},now.error};
 auto same=compare_world_moments(now.value,at);if(!same.ok)return {false,{},same.error};
 if(same.value||accepted.actual_task_acceptance.source_schema!=1)
  return work_detail::fail<SourceRef>("WorkExpenseAcceptanceContextMismatch",work);
 if(context_.budget_execution!=&budget_||&context_.state!=&expenses_||
    &context_.common!=&common_||&context_.original!=&originals_)
  return work_detail::fail<SourceRef>("WorkBudgetCandidateMismatch",work);
 WorkExpenseAcceptance value;
 value.work_id=work;value.accepted_task_id=accepted.frozen_request.authorization.accepted_task_id;
 value.task_acceptance_source=accepted.actual_task_acceptance.source_ref;
 value.work_acceptance_source=acceptance;value.authorization=accepted.frozen_request.authorization;
 return accept_governance_work_expense_in_candidate(common_,expenses_,originals_,quote_,value,clock_,versions_);
}
// 仅原接受事务尾调用：S3/S6 已经写入真实来源，S5 保持原 existing-source
// 严格反读，再绑定唯一预算。全量费用冷门失败仍撤整候选，包括本次预付。
Status NativeWorkExpenseAcceptanceWrite::finalize_work_expense_acceptance(
 const WorkAcceptanceSource& accepted){
 if(context_.budget_execution!=&budget_||&context_.state!=&expenses_||
    &context_.common!=&common_||&context_.original!=&originals_)
  return common_failure("WorkBudgetCandidateMismatch",accepted.work_id);
 auto now=clock_.read_current_moment();if(!now.ok)return {false,{},now.error};
 auto same=compare_world_moments(now.value,accepted.accepted_at);if(!same.ok)return {false,{},same.error};
 if(same.value)return common_failure("WorkExpenseFinalizationOutsideAcceptance",accepted.work_id);
 auto terms=read_governance_work_expense_terms(context_,accepted.expense_terms_ref,
  accepted.work_id,accepted.authorization.accepted_task_id);
 if(!terms.ok)return {false,{},terms.error};
 const SourceRef work_source{"tasks","work_accepted",accepted.fact_id};
 if(!work_detail::same(terms.value->work_acceptance_source,work_source)||
    !work_detail::same(terms.value->task_acceptance_source,accepted.original_task_acceptance.source_ref)||
    accepted.original_task_acceptance.source_schema!=1)
  return common_failure("WorkExpenseFinalizationSourceMismatch",accepted.work_id);
 auto bound=bind_governance_work_budget_in_candidate(common_,budget_,authorization_,
  responsibilities_,context_,accepted.expense_terms_ref);
 if(!bound.ok)return {false,{},bound.error};
 return validate_governance_work_expenses(context_);
}
}
