// 受领是独立显式尾段，原Q01/返报选择不隐式生成新source。
#pragma once
#include "task_personal_acceptance_contract.hpp"
#include "resident/domains/tasks/extensions/task_extension.hpp"
namespace hanmo::resident {
Outcome<bool> decode_task_personal_acceptance_profile(const BoundaryValue&,const std::vector<StableId>&);
Outcome<Integer> task_personal_acceptance_selected_version(const std::vector<std::pair<StableId,StableId>>&);
Status validate_task_personal_acceptance_layout(const CommonStore&,const std::vector<std::pair<StableId,StableId>>&,const TaskPersonalAcceptanceState*);
Status validate_task_personal_acceptance_extension_pair(const TaskPersonalAcceptanceCatalog&,const TaskExtensionCatalog*);
Outcome<BoundaryValue> task_personal_acceptance_fact_payloads(const CommonStore&,const TaskPersonalAcceptanceState*);
Status validate_task_personal_acceptance_json_payloads(const CommonStore&,const TaskPersonalAcceptanceState*,const BoundaryValue&);
}
