// 每个实际World候选临时借用政治/垫款读口，绝不把跨域指针写入持久状态或旧候选。
#pragma once
#include "task_public_account_payment.hpp"
namespace hanmo::resident {
class EconomicSupplyPolicyRead;
struct TaskExecutionServices {
    const TaskCatalog* catalog=nullptr;
    const PoliticalTakeoverRead* politics=nullptr;
    const TaskPublicAccountExecutionState* payments=nullptr;
    const TaskPersonalAdvanceRead* advances=nullptr;
    const DynamicStepInput* phase=nullptr;
    // 新发行政策的当前许可仅借同候选读；原合同结算不消费这个新签门。
    const EconomicSupplyPolicyRead* issued_supply=nullptr;
};
// 开启新段时必须传当前候选自己的sidecar；漏注册与旧对象指针均明确拒绝。
Status validate_task_execution_services(const TaskState&,const TaskExecutionServices*);
// 无接管profile保旧行为；有profile先按真正任务绑定读当前执行许可，不能信调用者自报账户。
Outcome<TaskPublicExecution> read_task_execution_context(const CommonStore&,const TaskState&,
    const TaskCatalog&,const TaskExecutionServices*,const StableId& task_id);
}
