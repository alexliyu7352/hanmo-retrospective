// handoff3与本段严格成对；旧handoff1/2的字段/字节和岗位限制不由本选择口放宽。
#pragma once
#include "task_personal_message_catalog.hpp"
#include "task_personal_message_state.hpp"
namespace hanmo::resident {
Outcome<bool> decode_task_personal_message_profile(const BoundaryValue& scenario,const std::vector<StableId>& capabilities);
Outcome<Integer> task_personal_message_selected_version(const std::vector<std::pair<StableId,StableId>>&);
Status validate_task_personal_message_layout(const CommonStore&,const std::vector<std::pair<StableId,StableId>>&,const TaskPersonalMessageState*);
Outcome<BoundaryValue> task_personal_message_fact_payloads(const CommonStore&,const TaskPersonalMessageState*);
Status validate_task_personal_message_json_payloads(const CommonStore&,const TaskPersonalMessageState*,const BoundaryValue& state_json);
}
