// 销约是独立显式尾段，原Q01/返报选择不隐式生成新source。
#pragma once
#include "task_personal_cancellation_contract.hpp"
#include "resident/domains/tasks/extensions/task_extension.hpp"
namespace hanmo::resident {
Outcome<bool> decode_task_personal_cancellation_profile(const BoundaryValue&,const std::vector<StableId>&);
Outcome<Integer> task_personal_cancellation_selected_version(const std::vector<std::pair<StableId,StableId>>&);
Status validate_task_personal_cancellation_layout(const CommonStore&,const std::vector<std::pair<StableId,StableId>>&,const TaskPersonalCancellationState*);
Status validate_task_personal_cancellation_extension_pair(const TaskPersonalCancellationCatalog&,const TaskExtensionCatalog*);
Outcome<BoundaryValue> task_personal_cancellation_fact_payloads(const CommonStore&,const TaskPersonalCancellationState*);
Status validate_task_personal_cancellation_json_payloads(const CommonStore&,const TaskPersonalCancellationState*,const BoundaryValue&);
}
