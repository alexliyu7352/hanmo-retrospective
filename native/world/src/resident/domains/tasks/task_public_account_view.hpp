// 本人任务只读当前公账履责许可；不公开远方任务或任何他人的私人余额。
#pragma once
#include "task_public_account_execution.hpp"
namespace hanmo::resident {
Outcome<BoundaryValue> query_task_public_account_execution(const CommonStore&,const TaskState&,
    const TaskCatalog&,const PoliticalTakeoverRead&,Integer cursor=0);
}
