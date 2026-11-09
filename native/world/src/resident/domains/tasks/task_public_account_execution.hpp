// 公库军事接管仅改变明确执行许可；原Task主体、受托人与货物受益权始终不变。
#pragma once
#include "task_public_account_dependencies.hpp"
#include "task_public_account_state.hpp"
namespace hanmo::resident {
class PoliticalTakeoverRead;
struct TaskPublicExecutionAccount {
    TaskAccountPurpose purpose=TaskAccountPurpose::funding;
    StableId container_id;bool inventory=false,affected=false;
    Owner execution_owner;
};
struct TaskPublicExecution {
    StableId responsibility_id,registration_fact_id,decision_fact_id,code;
    Owner original_issuer;SubjectRef original_assignee;
    std::optional<SubjectRef> original_delivery_beneficiary;
    bool may_progress=true,may_settle=true;
    std::vector<TaskPublicExecutionAccount> accounts;
};
// 本口自行读取真实Task绑定；不接收玩家提供的责任主体、付款人或容器列表。
Outcome<TaskPublicExecution> read_task_public_account_execution(
    const CommonStore&,const TaskState&,const TaskCatalog&,const PoliticalTakeoverRead*,const StableId& responsibility_id,const DynamicStepInput* phase=nullptr);
// 冷资产允许真实pending登记覆盖的公库转属，但pending绝不授权付款/履约。
Status validate_task_public_account_execution_assets(const CommonStore&,const TaskPublicExecution&);
// 只有真实具名支付事实才可核它之前的政治决定；历史不回查今日Owner。
Outcome<TaskPublicExecution> read_task_public_account_payment_authority(
    const CommonStore&,const TaskState&,const TaskCatalog&,const PoliticalTakeoverRead&,const TaskContractPaymentSource&,const DynamicStepInput* phase=nullptr);
// v2静态付款仍以原Task为责任，只接受具名真实现金事实；金额与阶段由静态冷验证另核。
Outcome<TaskPublicExecution> read_task_static_payment_authority(
    const CommonStore&,const TaskState&,const TaskCatalog&,const PoliticalTakeoverRead&,const TaskStaticPaymentSource&,const DynamicStepInput* phase=nullptr);
}
