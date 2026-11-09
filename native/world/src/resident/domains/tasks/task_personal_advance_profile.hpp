// 垫款只能由所选原生情景明确开启；旧Task/合同段不加字节，新收据不能藏进禁用旧档。
#pragma once
#include "task_personal_advance_catalog.hpp"
namespace hanmo::resident {
Outcome<std::optional<TaskPersonalAdvanceCatalog>> decode_task_personal_advance_profile(const BoundaryValue& selected_scenario,const std::vector<StableId>& required_capabilities);
Status validate_task_personal_advance_layout(bool enabled,const std::vector<std::pair<StableId,StableId>>& modules,const TaskPersonalAdvanceState*);
Status validate_task_personal_advance_history(const CommonStore&,bool enabled);
}
