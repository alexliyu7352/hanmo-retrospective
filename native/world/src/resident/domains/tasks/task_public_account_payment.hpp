// 动态合同先核政治执行来源再退预算/结酬；暂停保债且不产生空收据。
#pragma once
#include "task_public_account_execution.hpp"
#include "task_personal_advance.hpp"
namespace hanmo::resident {
struct TaskContractPaymentAmounts {Integer budget_return=0,reward_payment=0,reward_return=0;};
enum class TaskContractPaymentKind { no_payment,pending,paid };
struct TaskContractPaymentAttempt {
    TaskContractPaymentKind kind=TaskContractPaymentKind::no_payment;
    StableId code;std::optional<StableId> payment_fact_id;TaskContractPaymentAmounts amounts;
};
// 调用方持完整World候选并按返回额更新原合同唯一累计账；晚端失败须一起回滚。
Outcome<TaskContractPaymentAttempt> pay_task_contract_in_candidate(
    CommonStore&,const TaskState&,const TaskCatalog&,TaskPublicAccountExecutionState&,
    const PoliticalTakeoverRead&,const TaskPersonalAdvanceRead*,const StableId& contract_id,const DynamicStepInput* phase=nullptr);
// 每笔Common事实与本域载荷双向覆盖，并反核原Contract累计退款/报酬，不能删账免债。
Status validate_task_public_account_payments(const CommonStore&,const TaskState&,const TaskCatalog&,
    const TaskPublicAccountExecutionState&,const PoliticalTakeoverRead&,const DynamicStepInput* phase=nullptr);
// 政治/经济/Task三模块同启用由组合层核验；本口守自己的marker、段和事实。
Status validate_task_public_account_execution_layout(const CommonStore&,bool enabled,
    const std::vector<std::pair<StableId,StableId>>&,const TaskPublicAccountExecutionState*);
}
