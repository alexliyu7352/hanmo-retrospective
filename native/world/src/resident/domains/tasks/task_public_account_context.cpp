// 上下文只在当前命令/phase调用栈存活；冷验证仍由政治来源与Task原账独立完成。
#include "task_public_account_context.hpp"
namespace hanmo::resident {
Status validate_task_execution_services(const TaskState& tasks,const TaskExecutionServices* services){
    if(bool(tasks.takeover_execution)!=(services!=nullptr))return common_failure("TaskTakeoverExecutionContextRequired");
    if(!services)return common_success();
    if(!services->catalog||!services->politics||services->payments!=&*tasks.takeover_execution)return common_failure("TaskTakeoverExecutionContextMismatch");
    return common_success();
}
Outcome<TaskPublicExecution> read_task_execution_context(const CommonStore& common,const TaskState& tasks,const TaskCatalog& catalog,const TaskExecutionServices* services,const StableId& id){
    auto valid=validate_task_execution_services(tasks,services);if(!valid.ok)return {false,{},valid.error};
    if(!services)return {true,{}, {}};
    if(services->catalog!=&catalog)return {false,{},common_failure("TaskTakeoverExecutionCatalogMismatch").error};
    const auto* task=tasks.tasks.find(id);if(!task)return {false,{},common_failure("UnknownTaskResponsibility",id).error};
    return read_task_public_account_execution(common,tasks,catalog,services->politics,task->contract_id.value_or(id),services->phase);
}
}
