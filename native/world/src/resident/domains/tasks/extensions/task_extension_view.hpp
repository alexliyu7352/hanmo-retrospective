// 本人只看自己已获延期，或当前原委托人实际say正在提出的有限同意，不从远方目录泄露意图。
#pragma once
#include "task_extension_boundary.hpp"
namespace hanmo::resident {
struct TaskExtensionViewAccess {
    const CommonCatalog& common_catalog;
    const TaskCatalog& task_catalog;
    const InteractionState& interaction;
    const InteractionCatalog& interaction_catalog;
    const InteractionAccess& interaction_access;
    const TaskExtensionCatalog& catalog;
    const TaskExtensionState& state;
    TaskExtensionHistorySources history;
};
Outcome<BoundaryValue> query_task_extensions(const CommonStore&,const TaskState&,const TaskExtensionViewAccess&,Integer cursor=0);
}
