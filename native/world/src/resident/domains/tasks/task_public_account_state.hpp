// 公账履责只保存真实支付事实；原任务生命周期、主体和可花余额仍由Task/Common唯一持有。
#pragma once
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
inline constexpr const char* task_contract_payment_fact_kind="task_contract_payment";
inline constexpr const char* task_static_payment_fact_kind="task_static_payment";
enum class TaskContractPaymentPurpose { budget_return,reward_payment,reward_return };
struct TaskContractPaymentLeg {
    TaskContractPaymentPurpose purpose=TaskContractPaymentPurpose::budget_return;
    StableId from_wallet_id,to_wallet_id;Integer amount=0;
};
struct TaskContractPaymentSource {
    StableId fact_id,responsibility_id,task_id,registration_fact_id,decision_fact_id;
    Integer tick=0;std::vector<TaskContractPaymentLeg> legs;
};
// 静态任务沿原算法分别受领预算、结酬与退款；剩余预算奖励不冒充动态合同的本金退库。
enum class TaskStaticPaymentStage { acceptance,report_success,report_failure,cancellation };
enum class TaskStaticPaymentPurpose { funding,unused_budget_award,budget_return,reward_payment };
struct TaskStaticPaymentLeg {
    TaskStaticPaymentPurpose purpose=TaskStaticPaymentPurpose::funding;
    StableId from_wallet_id,to_wallet_id;Integer amount=0;
};
struct TaskStaticPaymentSource {
    StableId fact_id,task_id,source_action_id,registration_fact_id,decision_fact_id;
    TaskStaticPaymentStage stage=TaskStaticPaymentStage::acceptance;
    Integer tick=0;std::vector<TaskStaticPaymentLeg> legs;
};
// 独立段v1原字节不变；v2显式追加静态履责收据，不向旧Tasks/dynamic_tasks补默认字节。
struct TaskPublicAccountExecutionState {
    Integer version=1;
    OrderedTable<TaskContractPaymentSource> payments;
    OrderedTable<TaskStaticPaymentSource> static_payments;
};
Status visit_task_public_account_execution_checkpoint(CodecVisitor&,TaskPublicAccountExecutionState&);
BoundaryValue encode_task_public_account_execution_state(const TaskPublicAccountExecutionState&);
Outcome<TaskPublicAccountExecutionState> decode_task_public_account_execution_state(const BoundaryValue&);
}
