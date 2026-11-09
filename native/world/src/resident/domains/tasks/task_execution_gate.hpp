// 新采购支出额外检查原合同时间窗；账目清算与旧任务状态仍由原所有者处理。
#pragma once
#include "task_model.hpp"
namespace hanmo::resident {
bool task_procurement_authorized_now(const CommonRead&,const TaskState&,const TaskCatalog&,
    const StableId& actor,const StableId& task,const StableId& item);
std::vector<std::pair<StableId,StableId>> task_procurement_payers_now(const CommonRead&,
    const TaskState&,const TaskCatalog&,const StableId& actor);
}
