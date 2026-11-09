// 作者授权按具名动态采购模板选择；目录不产生债务，也不默认启用其它情景的垫款。
#pragma once
#include "task_personal_advance.hpp"
namespace hanmo::resident {
struct TaskPersonalAdvanceCatalog {Integer version=1;OrderedTable<TaskPersonalAdvancePolicy> policies;};
Outcome<TaskPersonalAdvanceCatalog> decode_task_personal_advance_catalog(const BoundaryValue&);
Status validate_task_personal_advance_catalog(const TaskPersonalAdvanceCatalog&,const TaskCatalog&);
Status validate_task_personal_advance_catalog_links(const TaskPersonalAdvanceCatalog&,const TaskCatalog&,const TaskPersonalAdvanceState&);
Outcome<std::optional<TaskPersonalAdvancePolicy>> task_personal_advance_policy_for_offer(
    const TaskPersonalAdvanceCatalog&,const TaskCatalog&,const TaskState&,const StableId& offer_id);
}
