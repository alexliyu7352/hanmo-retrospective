// 只有完整原去程/返报、原取消实物及真实生命源同时选择，才解释新的独立回书段。
#pragma once
#include "task_message_return_state.hpp"
namespace hanmo::resident {
Outcome<Integer> task_message_return_selected_version(const std::vector<std::pair<StableId,StableId>>&);
Outcome<BoundaryValue> task_message_return_action_payloads(const CommonStore&,const TaskMessageReturnState*);
Outcome<BoundaryValue> task_message_return_fact_payloads(const CommonStore&,const TaskMessageReturnState*);
Status validate_task_message_return_json_payloads(const CommonStore&,const TaskMessageReturnState*,const BoundaryValue& state_json);
Status validate_task_message_return_layout(const CommonStore&,const std::vector<std::pair<StableId,StableId>>&,const TaskMessageReturnState*);
}
