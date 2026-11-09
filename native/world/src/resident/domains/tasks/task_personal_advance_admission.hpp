// 只在原动态受领Action同候选登记作者垫款授权；不另建任务、钱包或接受时耗。
#pragma once
#include "task_personal_advance_catalog.hpp"
namespace hanmo::resident {
Outcome<bool> authorize_task_personal_advance_in_candidate(
    const CommonStore&,TaskState&,const TaskCatalog&,const TaskPersonalAdvanceCatalog&,
    TaskPersonalAdvanceState&,const StableId& accepting_action_id);
}
